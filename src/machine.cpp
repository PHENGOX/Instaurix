#include "machine.hpp"
#include "program_loader.hpp"
#include <stdexcept>
#include <new>
#include <utility>

Machine::Machine(bool enableTestProtocol, bool enableReferenceHle, bool enableHomebrewEntry,Gpu::ProfileId gpuProfile) : ram(RAM_SIZE), bus(ram), cpu(bus), timer(interrupts, TIMER_IRQ), testProtocolEnabled(enableTestProtocol), referenceHleEnabled(enableReferenceHle), homebrewEntryEnabled(enableHomebrewEntry),selectedGpuProfile(gpuProfile)
{
    if (!bus.mapDevice(DEVICE_BASE, device.size(), device, "TestDevice") ||
        !bus.mapDevice(IRQ_BASE, interrupts.size(), interrupts, "IRQ") ||
        !bus.mapDevice(TIMER_BASE, timer.size(), timer, "Timer"))
        throw std::runtime_error(bus.getLastError());
    cpu.setSP(RAM_SIZE);
}

bool Machine::load(const char* path)
{
    lastError.clear();
    if(homebrewEntryEnabled && (testProtocolEnabled || !referenceHleEnabled))
    { lastError="Homebrew profile requires HLE and is incompatible with the test protocol";return false; }
    ProgramLoader::Image image;
    try
    {
        Memory staged(RAM_SIZE);
        Process stagedProcess;
        if (!ProgramLoader::loadImage(staged,path,image,lastError) ||
            !stagedProcess.initialize(staged,image,referenceHleEnabled,lastError,homebrewEntryEnabled,selectedGpuProfile)) return false;
        stagedProcess.setReadOnlyFiles(mountedFiles);
        ram = std::move(staged);
        processContext = std::move(stagedProcess);
    }
    catch (const std::bad_alloc&) { lastError = "Failed to allocate process context"; return false; }
    interrupts = InterruptController();
    device = TestDevice();
    timer.reset();
    bus.clearError();
    cpu.reset();
    cpu.setSP(image.stack);
    cpu.setPC(image.entry);
    auto context = cpu.captureContext();
    context.tpidrroEl0 = processContext.mainThread().localRegion;
    if(homebrewEntryEnabled) {
        context.registers[0]=Process::HomebrewEnvironment;
        context.registers[1]=UINT64_MAX;
        context.registers[30]=Process::HomebrewReturn;
    }
    cpu.restoreContext(context);
    processContext.checkpoint(cpu,Process::State::Ready);
    exited = false;
    quantumUsed = 0;
    exitCode = 0;
    return true;
}

bool Machine::mountReadOnlyRoot(const std::filesystem::path& path)
{
    if(!referenceHleEnabled) { lastError="File directory requires --hle-reference"; return false; }
    auto snapshot=ReadOnlyFiles::load(path,lastError);
    if(!snapshot) return false;
    mountedFiles=std::move(snapshot);
    return true; // Used by the next successful load, never replaces a live session's files.
}

Machine::Result Machine::run(uint64_t maxInstructions,bool stopOnFrame)
{
    if (processContext.state() == Process::State::Faulted) return {Stop::Error,0};
    lastError.clear();
    auto finish = [&](Stop stop,uint64_t count,uint64_t code = 0) -> Result {
        processContext.checkpoint(cpu,stop == Stop::Exited ? Process::State::Exited :
            stop == Stop::Error ? Process::State::Faulted :
            stop == Stop::Waiting ? Process::State::Waiting : Process::State::Paused);
        return {stop,count,code};
    };
    if (exited) return finish(Stop::Exited,0,exitCode);
    if (!processContext.schedule(cpu)) return finish(Stop::Waiting,0);
    processContext.checkpoint(cpu,Process::State::Running);
    uint64_t count = 0;
    while (count < maxInstructions)
    {
        const auto framesBefore = processContext.frameCount();
        // Report IRQ to host. Architectural exception entry is not implemented yet.
        if (interrupts.isIRQActive()) return finish(Stop::Interrupt,count);
        if(referenceHleEnabled) cpu.setSystemCounter(processContext.systemTick(),19200000);
        const auto step = cpu.stepDetailed();
        if (step.status == CPU::StepStatus::Error)
        {
            lastError = cpu.getLastError() ? cpu.getLastError() : "CPU error";
            return finish(Stop::Error,count);
        }
        ++count;
        timer.tick(1);
        if (step.status == CPU::StepStatus::SystemCall)
        {
            if(homebrewEntryEnabled && step.immediate==0xF001 && cpu.getPC()==Process::HomebrewReturn+4 &&
               processContext.activeThreadIndex()==0) {
                exited=true;exitCode=cpu.readRegister(0);
                return finish(Stop::Exited,count,exitCode);
            }
            if (testProtocolEnabled && step.immediate == 0xF000)
            {
                exited = true;
                exitCode = cpu.readRegister(0);
                return finish(Stop::Exited,count,exitCode);
            }
            const auto action = processContext.dispatch(step.immediate,cpu,bus);
            if (action == Process::Action::Wait || action == Process::Action::ThreadExit || action == Process::Action::Yield)
            {
                quantumUsed = 0;
                if (action == Process::Action::ThreadExit && processContext.allThreadsExited())
                {
                    exited = true; exitCode = 0;
                    return finish(Stop::Exited,count);
                }
                if (!processContext.schedule(cpu,true)) return finish(Stop::Waiting,count);
                if (interrupts.isIRQActive()) return finish(Stop::Interrupt,count);
                continue;
            }
            if (action == Process::Action::Unsupported)
            {
                lastError = (step.immediate == 0x18 && referenceHleEnabled ?
                    "WaitSynchronization: negative timeout other than -1 or virtual time overflow; SVC " :
                    "Unsupported SVC: ") + std::to_string(step.immediate) +
                    " at PC " + std::to_string(cpu.getPC()-4);
                return finish(Stop::Error,count);
            }
            if (action == Process::Action::Exit)
            {
                exited = true;
                exitCode = 0;
                return finish(Stop::Exited,count,0);
            }
        }
        if (processContext.appletSuspended()) return finish(Stop::Waiting,count);
        if (interrupts.isIRQActive()) return finish(Stop::Interrupt,count);
        if (++quantumUsed == Process::Quantum)
        {
            quantumUsed = 0;
            if (!processContext.schedule(cpu,true)) return finish(Stop::Waiting,count);
        }
        // Stop only after normal instruction/timer/quantum accounting, never replay the SVC.
        if(stopOnFrame && processContext.frameCount()!=framesBefore) return finish(Stop::Frame,count);
    }
    return finish(Stop::Budget,count);
}
