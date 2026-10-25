#include "machine.hpp"
#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    int failures = 0;
    auto check = [&](bool ok, const char* label) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
    };
    Machine machine(false,true);
    check(machine.load(argv[1]),"load independent assembled ELF");
    auto context = machine.cpu.captureContext();
    context.vectors[31] = {0x123456789ABCDEF0ULL,0xFEDCBA9876543210ULL};
    context.fpcr = 0x400000; context.fpsr = 0x8000010; context.tpidrEl0 = 0x1234;
    machine.cpu.restoreContext(context);
    auto run = machine.run(100);
    check(run.stop == Machine::Stop::Waiting && machine.process().state() == Process::State::Waiting,"guest suspends on unsignaled event");
    const auto blocked = machine.cpu.captureContext();
    const uint32_t writer = machine.ram.read32(0x12000), reader = machine.ram.read32(0x12004);
    check(writer && reader && writer != reader,"distinct typed handles");
    check(machine.run(100).instructions == 0 && machine.cpu.getPC() == blocked.pc,"waiting consumes no instructions");
    check(!machine.load("missing-event-test.elf") && machine.process().state() == Process::State::Waiting,"failed load preserves pending wait");
    check(machine.signalEvent(reader) == Process::InvalidHandle,"read endpoint cannot signal");
    check(machine.signalEvent(writer) == 0,"host signals write endpoint");
    check(machine.run(0).stop == Machine::Stop::Budget && machine.cpu.readRegister(0) == 0 && machine.cpu.readRegister(1) == 0,"wake completes SVC before next instruction");
    auto awake = machine.cpu.captureContext();
    check(awake.pc == blocked.pc && awake.sp == blocked.sp && awake.vectors == context.vectors &&
        awake.fpcr == context.fpcr && awake.fpsr == context.fpsr && awake.tpidrEl0 == context.tpidrEl0 &&
        awake.tpidrroEl0 == context.tpidrroEl0,"wake preserves vector FP TLS and stack context");
    run = machine.run(100);
    check(run.stop == Machine::Stop::Exited && machine.ram.read32(0x12008) == 55,"guest resumes resets closes and writes result");
    check(machine.signalEvent(writer) == Process::InvalidHandle,"closed endpoint invalid");

    // Direct dispatcher calls isolate argument validation and resource lifetime.
    check(machine.load(argv[1]),"reload reference process");
    Process process;
    ProgramLoader::Image image; std::string error;
    Memory ram(Machine::RAM_SIZE);
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && process.initialize(ram,image,true,error),"independent reference process");
    Bus bus(ram); CPU cpu(bus);
    auto call = [&](uint16_t svc) { return process.dispatch(svc,cpu,bus); };
    auto create = [&]() {
        call(0x45); check(cpu.readRegister(0) == 0,"create event");
        return std::pair<uint32_t,uint32_t>(uint32_t(cpu.readRegister(1)),uint32_t(cpu.readRegister(2)));
    };
    auto [w,r] = create(); auto [w2,r2] = create();
    auto wait = [&](uint64_t address,uint32_t count,uint64_t timeout) {
        cpu.writeRegister(1,address); cpu.writeRegister(2,count); cpu.writeRegister(3,timeout);
        return call(0x18);
    };
    ram.write32(0x12000,r); ram.write32(0x12004,r2);
    wait(0x12000,2,0); check(cpu.readRegister(0) == Process::TimedOut && cpu.readRegister(1) == 0x12000,"poll timeout preserves index");
    cpu.writeRegister(0,w2); call(0x11);
    check(cpu.readRegister(0) == 0,"guest SignalEvent SVC");
    wait(0x12000,2,0);
    check(cpu.readRegister(0) == 0 && cpu.readRegister(1) == 1,"wait-any returns signaled index");
    wait(0x12000,2,0); check(cpu.readRegister(0) == 0,"event remains signaled after wait");
    ram.write32(0x12000,0xFFFFFFFF);
    wait(0x12000,2,0); check(cpu.readRegister(0) == Process::InvalidHandle,"validates all handles before success");
    ram.write32(0x12000,w); wait(0x12000,1,0);
    check(cpu.readRegister(0) == Process::InvalidHandle,"write endpoint cannot wait");
    wait(Machine::DEVICE_BASE,1,0); check(cpu.readRegister(0) == Process::InvalidPointer,"MMIO wait array rejected");
    wait(0x12FFE,1,0); check(cpu.readRegister(0) == Process::InvalidPointer,"cross-page array rejected");
    wait(UINT64_MAX,1,0); check(cpu.readRegister(0) == Process::InvalidPointer,"overflow pointer rejected");
    wait(0,65,0); check(cpu.readRegister(0) == Process::TooManyHandles,"oversized wait list");
    wait(0,UINT32_MAX,0); check(cpu.readRegister(0) == Process::TooManyHandles,"negative handle count rejected");
    wait(UINT64_MAX,0,0); check(cpu.readRegister(0) == Process::TimedOut,"empty poll ignores pointer");
    cpu.writeRegister(0,r); call(0x17); check(cpu.readRegister(0) == Process::InvalidState,"reset unsignaled readable event");
    cpu.writeRegister(0,w2); call(0x12); check(cpu.readRegister(0) == 0,"clear writable endpoint");
    cpu.writeRegister(0,r2); call(0x12); check(cpu.readRegister(0) == 0,"clear readable endpoint idempotent");
    cpu.writeRegister(0,w2); call(0x17); check(cpu.readRegister(0) == Process::InvalidHandle,"reset rejects writer");
    process.signalEvent(w); cpu.writeRegister(0,w); call(0x16);
    ram.write32(0x12000,r); wait(0x12000,1,0);
    check(cpu.readRegister(0) == 0,"closing writer preserves readable event");
    cpu.writeRegister(0,r); call(0x17);
    check(wait(0x12000,1,1) == Process::Action::Wait,"finite timeout registered");
    check(wait(0x12000,1,UINT64_MAX) == Process::Action::Wait,"indefinite wait registered");
    cpu.writeRegister(0,r); call(0x16);
    check(!process.resumeWait(cpu),"pending wait retains object after endpoint closes");
    // Release the orphaned pending wait through reinitialization, as on load.
    check(process.initialize(ram,image,true,error) && process.resumeWait(cpu),"reload clears pending wait");
    auto initial = create(); w = initial.first; r = initial.second;
    auto second = create(); w2 = second.first; r2 = second.second;
    cpu.writeRegister(0,w); call(0x16);
    auto replacement = create(); check(replacement.first != w && process.signalEvent(w) == Process::InvalidHandle,"stale handle never aliases recycled slot");
    std::vector<uint32_t> remaining{r,w2,r2,replacement.first,replacement.second};
    for (int i=0;i<61;++i) { auto pair=create(); remaining.push_back(pair.first); remaining.push_back(pair.second); }
    cpu.writeRegister(1,0xAB); cpu.writeRegister(2,0xCD); call(0x45);
    check(cpu.readRegister(0) == Process::OutOfHandles && cpu.readRegister(1) == 0xAB && cpu.readRegister(2) == 0xCD,"pair allocation fails atomically with one free slot");
    cpu.writeRegister(0,remaining.back()); call(0x16); call(0x45);
    check(cpu.readRegister(0) == 0,"closed slots reusable");
    std::cout << "Event integration failures=" << failures << '\n';
    return failures ? 1 : 0;
}
