#include "machine.hpp"
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    int failures = 0;
    auto check = [&](bool ok,const char* text) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
    };
    auto validate = [&](Machine& m) {
        check(m.ram.read32(0x1200C) == 55,"shared calculation completed");
        check(m.ram.read64(0x12010) == 0x1111 && m.ram.read64(0x12018) == 0x2222,"integer contexts isolated");
        check(m.ram.read64(0x12020) == 0x1111 && m.ram.read64(0x12028) == 0x1111 &&
            m.ram.read64(0x12030) == 0x2222 && m.ram.read64(0x12038) == 0x2222,"128-bit vector contexts isolated");
        check(m.ram.read64(0x12040) == 0x1234 && m.ram.read64(0x12048) == 0x5678,"TPIDR_EL0 isolated");
        check(m.ram.read64(0x12050) == m.process().mainThread().localRegion &&
            m.ram.read64(0x12058) == m.process().workerThread().localRegion &&
            m.ram.read64(Process::WorkerTlsPage+0x380) == 0x2222,"separate TLS addresses and content");
        check(m.process().mainThread().state == Process::State::Exited && m.process().workerThread().state == Process::State::Exited,"both threads exited");
        check(m.process().workerThread().context.sp == Process::WorkerStackTop,"worker stack restored");
    };
    Machine continuous(false,true), sliced(false,true), preempted(false,true), idle(false,true);
    check(continuous.load(argv[1]) && sliced.load(argv[1]) && preempted.load(argv[1]) && idle.load(argv[1]),"load thread ELF");
    auto original = continuous.cpu.captureContext(); original.fpcr=0x400000; original.fpsr=0x8000010;
    continuous.cpu.restoreContext(original);
    auto result = continuous.run(2000);
    check(result.stop == Machine::Stop::Exited,"two-thread event and join ELF exits");
    validate(continuous);
    check(continuous.process().mainThread().context.fpcr == original.fpcr &&
        continuous.process().mainThread().context.fpsr == original.fpsr &&
        continuous.process().workerThread().context.fpcr == 0 && continuous.process().workerThread().context.fpsr == 0,"FP control and status independent");
    uint64_t count = 0; bool observedWorker=false, observedJoin=false;
    for (unsigned i=0;i<2000;++i)
    {
        auto part=sliced.run(1); count+=part.instructions;
        observedWorker |= sliced.process().activeThreadIndex() == 1;
        observedJoin |= sliced.process().mainThread().state == Process::State::Waiting && sliced.ram.read32(0x1200C) == 42;
        if (part.stop == Machine::Stop::Exited) break;
        check(part.stop == Machine::Stop::Budget,"runnable peer prevents host waiting");
    }
    check(count == result.instructions && observedWorker && observedJoin,"budget slicing preserves schedule and join wait");
    validate(sliced);
    preempted.ram.write32(0x12060,1);
    auto preemptResult=preempted.run(4000);
    check(preemptResult.stop == Machine::Stop::Exited,"busy main loop permits worker execution without yield");
    validate(preempted);
    check(sliced.load(argv[1]),"reload for preemption slicing");
    sliced.ram.write32(0x12060,1); count=0;
    Machine::Stop finalStop=Machine::Stop::Budget;
    for (unsigned i=0;i<4000 && finalStop==Machine::Stop::Budget;++i)
    { auto part=sliced.run(1); count+=part.instructions; finalStop=part.stop; }
    check(finalStop==Machine::Stop::Exited && count==preemptResult.instructions,"preemption quantum persists across host run calls");
    validate(sliced);
    check(!preempted.ram.canAccess(Process::WorkerStackBase-1,1,Memory::Read) &&
        !preempted.ram.canAccess(Process::WorkerStackTop,1,Memory::Read) &&
        !preempted.ram.canAccess(Process::WorkerStackBase,4,Memory::Execute),"stack guards and NX");
    idle.ram.write32(0x12064,1);
    check(idle.run(1000).stop == Machine::Stop::Waiting && idle.process().mainThread().state == Process::State::Waiting &&
        idle.process().workerThread().state == Process::State::Waiting,"host waiting only when both threads blocked");
    auto idlePc=idle.cpu.getPC();
    check(idle.run(1000).instructions == 0 && idle.cpu.getPC() == idlePc,"idle consumes zero instructions");
    check(!idle.load("missing-thread-test.elf") && idle.cpu.getPC() == idlePc,"failed load preserves scheduler");
    check(idle.signalEvent(idle.ram.read32(0x12000)) == 0 && idle.run(2000).stop == Machine::Stop::Exited,"host signal wakes two waiters");
    validate(idle);
    check(idle.load(argv[1]) && idle.process().activeThreadIndex() == 0 && idle.process().workerThread().context.pc == 0,"reload clears worker and schedule");

    Process p; Memory ram(Machine::RAM_SIZE); ProgramLoader::Image image; std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"dispatcher fixture");
    Bus bus(ram); CPU cpu(bus);
    auto create = [&](uint64_t entry,uint64_t stack,uint32_t priority=32,uint32_t core=0) {
        cpu.writeRegister(1,entry); cpu.writeRegister(2,0xABC); cpu.writeRegister(3,stack);
        cpu.writeRegister(4,priority); cpu.writeRegister(5,core);
        return p.dispatch(8,cpu,bus);
    };
    create(0x10001,Process::WorkerStackTop); check(cpu.readRegister(0)==Process::InvalidAddress,"misaligned entry rejected");
    create(0x12000,Process::WorkerStackTop); check(cpu.readRegister(0)==Process::InvalidAddress,"NX entry rejected");
    create(0x10000,Process::WorkerStackTop-16); check(cpu.readRegister(0)==Process::InvalidAddress,"unsupported stack rejected");
    check(create(0x10000,Process::WorkerStackTop,31)==Process::Action::Unsupported,"priority policy explicit");
    check(create(0x10000,Process::WorkerStackTop,32,1)==Process::Action::Unsupported,"single logical core explicit");
    create(0x10000,Process::WorkerStackTop);
    const auto handle=cpu.readRegister(1);
    check(cpu.readRegister(0)==0 && p.workerThread().context.registers[0]==0xABC,"creation publishes argument in X0");
    cpu.setPC(0x10100); p.checkpoint(cpu,Process::State::Running); p.schedule(cpu,true);
    check(p.activeThreadIndex()==0,"unstarted worker cannot run");
    cpu.writeRegister(0,handle); p.dispatch(9,cpu,bus); check(cpu.readRegister(0)==0,"start worker");
    cpu.writeRegister(0,handle); p.dispatch(9,cpu,bus); check(cpu.readRegister(0)==Process::InvalidState,"double start rejected");
    cpu.writeRegister(0,handle); p.dispatch(0x11,cpu,bus); check(cpu.readRegister(0)==Process::InvalidHandle,"thread handle cannot signal");
    cpu.writeRegister(0,handle); p.dispatch(0x12,cpu,bus); check(cpu.readRegister(0)==Process::InvalidHandle,"thread handle cannot clear");
    create(0x10000,Process::WorkerStackTop); check(cpu.readRegister(0)==Process::OutOfResource,"third thread rejected");
    cpu.writeRegister(0,handle); p.dispatch(0x16,cpu,bus);
    check(p.schedule(cpu,true) && p.activeThreadIndex()==1 && cpu.readRegister(0)==0xABC,"closing handle does not terminate started worker");
    p.dispatch(0xA,cpu,bus); check(p.schedule(cpu,true) && p.activeThreadIndex()==0,"ExitThread restores runnable main");
    cpu.writeRegister(0,0); check(p.dispatch(0xB,cpu,bus)==Process::Action::Yield,"zero sleep yields");
    cpu.writeRegister(0,1); check(p.dispatch(0xB,cpu,bus)==Process::Action::Wait,"finite sleep registered");
    std::cout << "Thread integration instructions=" << result.instructions << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
