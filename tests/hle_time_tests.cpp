#include "machine.hpp"
#include <iostream>

int main(int argc,char** argv)
{
    if (argc!=2) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } };
    auto drive=[&](Machine& m,uint64_t budget) {
        uint64_t count=0;
        for (unsigned i=0;i<2000;++i)
        {
            auto part=m.run(budget); count+=part.instructions;
            if(part.stop==Machine::Stop::Exited) return count;
            if(part.stop==Machine::Stop::Waiting)
            {
                auto next=m.process().nextWakeDelay();
                if(!next || !m.advanceTime(*next)) break;
            }
            else if(part.stop!=Machine::Stop::Budget) break;
        }
        check(false,"virtual time driver terminates"); return count;
    };
    Machine normal(false,true), sliced(false,true), timeout(false,true);
    check(normal.load(argv[1]) && sliced.load(argv[1]) && timeout.load(argv[1]),"load timed ELF");
    auto first=normal.run(1000);
    check(first.stop==Machine::Stop::Waiting && normal.process().virtualTime()==0 && normal.process().nextWakeDelay()==1000,"event and sleep deadlines coexist");
    check(normal.run(1000).instructions==0 && normal.process().virtualTime()==0,"execution alone never advances nanoseconds");
    check(normal.advanceTime(999) && normal.process().nextWakeDelay()==1 && normal.run(100).instructions==0,"sleep cannot wake early");
    check(!normal.load("missing-time-test.elf") && normal.process().virtualTime()==999 && normal.process().nextWakeDelay()==1,"failed load preserves clock and deadlines");
    auto count=first.instructions+drive(normal,1000);
    check(normal.process().virtualTime()==1500 && normal.ram.read32(0x1200C)==55 && normal.ram.read32(0x12010)==0 &&
        normal.ram.read64(0x12018)==0 && normal.ram.read64(0x12030)==19 && normal.ram.read64(0x12038)==28,"worker signals before timeout then join completes");
    auto slicedCount=drive(sliced,1);
    check(slicedCount==count && sliced.process().virtualTime()==normal.process().virtualTime(),"clock and schedule invariant under single instruction budgets");
    timeout.ram.write64(0x12020,500);
    drive(timeout,1000);
    check(timeout.ram.read32(0x12010)==Process::TimedOut && timeout.ram.read64(0x12018)==0x12004 &&
        timeout.ram.read64(0x12030)==9 && timeout.process().virtualTime()==1500 && timeout.ram.read32(0x1200C)==55,"timeout preserves index and does not prevent later join");
    Machine busy(false,true);
    check(busy.load(argv[1]) && busy.run(1000).stop==Machine::Stop::Waiting,"busy peer fixture blocked");
    check(busy.advanceTime(1000) && busy.run(1).stop==Machine::Stop::Budget,"worker runnable after sleep");
    check(busy.advanceTime(1000),"host advances time while peer runnable");
    drive(busy,1000);
    check(busy.ram.read32(0x12010)==Process::TimedOut && busy.process().virtualTime()==2500 &&
        busy.ram.read64(0x12030)==38,"deadline expires before runnable peer's later signal");
    check(!normal.advanceTime(1),"terminal machine rejects clock advance");
    check(normal.load(argv[1]) && normal.process().virtualTime()==0 && !normal.process().nextWakeDelay(),"reload resets clock and pending deadlines");

    Memory ram(Machine::RAM_SIZE); ProgramLoader::Image image; Process p; std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"dispatcher fixture");
    Bus bus(ram); CPU cpu(bus);
    p.dispatch(0x45,cpu,bus);
    const auto writer=uint32_t(cpu.readRegister(1)), reader=uint32_t(cpu.readRegister(2));
    ram.write32(0x12000,reader);
    auto wait=[&](uint64_t duration,uint32_t count=1) {
        cpu.writeRegister(0,0x777); cpu.writeRegister(1,0x12000); cpu.writeRegister(2,count); cpu.writeRegister(3,duration);
        return p.dispatch(0x18,cpu,bus);
    };
    check(wait(100)==Process::Action::Wait && p.nextWakeDelay()==100,"finite event wait accepted");
    auto saved=cpu.captureContext(); saved.vectors[31]={0xABC,0xDEF}; saved.fpcr=0x400000; saved.fpsr=0x8000010;
    saved.pstate=0xA0000000; saved.tpidrEl0=0x1234; cpu.restoreContext(saved);
    p.advanceTime(99); check(!p.resumeWait(cpu),"timeout boundary minus one");
    p.advanceTime(1); check(p.resumeWait(cpu) && cpu.readRegister(0)==Process::TimedOut && cpu.readRegister(1)==0x12000,"timeout boundary exact");
    auto awake=cpu.captureContext();
    check(awake.vectors==saved.vectors && awake.fpcr==saved.fpcr && awake.fpsr==saved.fpsr && awake.pstate==saved.pstate &&
        awake.sp==saved.sp && awake.pc==saved.pc && awake.tpidrEl0==saved.tpidrEl0,"timeout preserves all other context");
    wait(100); p.signalEvent(writer); p.advanceTime(100);
    cpu.writeRegister(0,writer); p.dispatch(0x12,cpu,bus);
    check(p.resumeWait(cpu) && cpu.readRegister(0)==0 && cpu.readRegister(1)==0,"signal before deadline stays successful after clear and time advance");
    wait(100); p.advanceTime(100); p.signalEvent(writer);
    check(p.resumeWait(cpu) && cpu.readRegister(0)==Process::TimedOut && cpu.readRegister(1)==0x12000,"signal after deadline cannot overwrite latched timeout");
    cpu.writeRegister(0,writer); p.dispatch(0x12,cpu,bus);
    wait(UINT64_MAX); check(!p.nextWakeDelay(),"infinite wait has no deadline");
    p.advanceTime(1000); check(!p.resumeWait(cpu),"infinite wait never expires");
    p.signalEvent(writer); check(p.resumeWait(cpu),"infinite wait still responds to signal");
    cpu.writeRegister(0,writer); p.dispatch(0x12,cpu,bus);
    wait(1,0); p.advanceTime(1);
    check(p.resumeWait(cpu) && cpu.readRegister(0)==Process::TimedOut,"empty wait with finite timeout");
    cpu.writeRegister(0,123); cpu.writeRegister(1,0x987);
    check(p.dispatch(0xB,cpu,bus)==Process::Action::Wait && p.nextWakeDelay()==123,"finite sleep deadline");
    p.advanceTime(122); check(!p.resumeWait(cpu),"sleep boundary minus one");
    p.advanceTime(1); check(p.resumeWait(cpu) && cpu.readRegister(0)==123 && cpu.readRegister(1)==0x987,"sleep completion preserves input registers");
    cpu.writeRegister(0,UINT64_MAX); check(p.dispatch(0xB,cpu,bus)==Process::Action::Unsupported,"unsupported negative yield explicit");
    check(p.advanceTime(UINT64_MAX-p.virtualTime()) && p.virtualTime()==UINT64_MAX,"clock reaches maximum without wrap");
    p.dispatch(0x1E,cpu,bus);
    check(cpu.readRegister(0)==354177486215223391ULL,"large virtual tick conversion avoids intermediate overflow");
    check(!p.advanceTime(1) && p.virtualTime()==UINT64_MAX,"clock overflow rejected transactionally");
    check(wait(1)==Process::Action::Unsupported,"deadline overflow explicitly rejected");
    check(wait(UINT64_MAX-1)==Process::Action::Unsupported,"negative timeout other than -1 remains explicit");
    std::cout<<"Timed integration instructions="<<count<<" failures="<<failures<<'\n';
    return failures?1:0;
}
