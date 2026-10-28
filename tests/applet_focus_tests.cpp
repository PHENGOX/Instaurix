#include "machine.hpp"
#include "ipc.hpp"
#include <iostream>

int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    unsigned failures=0;
    auto check=[&](bool ok,const char* message) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<message<<'\n'; } };
    using Focus=Process::AppletFocus;
    Machine machine(false,true,true);
    if(!machine.load(argv[1])) return 2;
    const auto tls=machine.process().mainThread().localRegion;
    Ipc::Buffer original{};Ipc::DomainRequest request;bool found=false;
    for(unsigned i=0;i<100000;++i) {
        const auto op=machine.ram.fetch32(machine.cpu.getPC());
        if((op&0xFFE0001F)==0xD4000001 && ((op>>5)&0xFFFF)==0x21) {
            for(size_t j=0;j<original.size();++j) original[j]=machine.ram.read8(tls+j);
            if(!Ipc::parseDomain(original,request) && request.object==6 && request.command==13) { found=true;break; }
        }
        if(machine.run(1).stop!=Machine::Stop::Budget) break;
    }
    check(found,"reach original SDK focus-policy request before execution");if(!found) return 1;
    check(request.payloadSize==3 && original[request.payload]==0 && original[request.payload+1]==0 && original[request.payload+2]==1,
        "original SDK requests SuspendHomeSleep as three booleans");
    const auto before=machine.cpu.captureContext();const auto svcPC=machine.cpu.getPC();
    auto invoke=[&](uint8_t a,uint8_t b,uint8_t c,unsigned size=3,bool writable=true) {
        auto packet=original;packet[request.payload]=a;packet[request.payload+1]=b;packet[request.payload+2]=c;
        packet[18]=uint8_t(16+size);
        machine.ram.writeBytes(tls,packet.data(),packet.size());
        if(!writable) machine.ram.setPermissions(tls,4096,Memory::Read);
        machine.cpu.restoreContext(before);const auto result=machine.run(1);
        machine.ram.setPermissions(tls,4096,Memory::Read|Memory::Write);
        return result;
    };
    check(!machine.process().appletSuspendsInBackground(),"no policy installed before SDK command");
    invoke(0,0,1,3,false);
    check(machine.cpu.readRegister(0)==Process::InvalidPointer && !machine.process().appletSuspendsInBackground(),"failed reply installs no suspension policy");
    invoke(0,0,1,2);
    check(machine.ram.read32(tls+40)==Ipc::InvalidArguments && !machine.process().appletSuspendsInBackground(),"reject short policy payload");
    invoke(2,0,1);
    check(machine.ram.read32(tls+40)==Ipc::InvalidArguments && !machine.process().appletSuspendsInBackground(),"reject non-boolean flag");
    invoke(1,0,1);
    check(machine.ram.read32(tls+40)==Ipc::UnknownCommand && !machine.process().appletSuspendsInBackground(),"unsupported notification mode stays explicit");
    check(invoke(0,0,1).stop==Machine::Stop::Budget && machine.ram.read32(tls+40)==0 && machine.process().appletSuspendsInBackground(),"install SDK background-suspension mode");
    check(machine.setAppletFocus(false) && !machine.process().appletSuspended(),"ordinary focus loss does not suspend SuspendHomeSleep");
    check(machine.setAppletFocusState(Focus::Background) && machine.process().appletSuspended(),"HOME/sleep background state suspends the process");
    const auto pc=machine.cpu.getPC();const auto regs=machine.cpu.captureContext();
    const auto paused=machine.run(1000);
    check(paused.stop==Machine::Stop::Waiting && paused.instructions==0 && machine.cpu.getPC()==pc,
        "suspended process executes no instructions");
    check(machine.advanceTime(1000) && !machine.process().nextWakeDelay() && machine.run(1000).instructions==0,
        "virtual time does not resume a background-suspended process");
    check(!machine.setAppletFocusState(static_cast<Focus>(0)) && machine.process().appletFocusState()==Focus::Background,
        "invalid host focus leaves suspended state intact");
    check(machine.setAppletFocusState(Focus::OutOfFocus) && !machine.process().appletSuspended(),"leaving background resumes even without foreground focus");
    check(machine.cpu.getPC()==pc && machine.cpu.readRegister(0)==regs.registers[0],"focus transition preserves guest continuation");
    check(invoke(1,1,0).stop==Machine::Stop::Budget && !machine.process().appletSuspendsInBackground(),"NoSuspend clears installed policy");
    check(machine.setAppletFocusState(Focus::Background) && !machine.process().appletSuspended(),"NoSuspend permits background execution");
    check(invoke(0,0,1).stop==Machine::Stop::Waiting && machine.cpu.getPC()==svcPC+4,
        "enabling suspension while background stops immediately after completed SVC");
    check(machine.run(1).instructions==0,"suspended policy SVC is not replayed");
    check(machine.setAppletFocus(true) && machine.run(1).instructions==1,"focus restoration resumes next guest instruction");

    // Keep a genuine timed kernel wait pending while applet scheduling is suspended.
    constexpr uint64_t code=0xC00000;
    machine.ram.setPermissions(code,4096,Memory::Read|Memory::Write|Memory::Execute);
    machine.ram.write32(code,0xD4000161); // svc #0x0b: SleepThread
    machine.ram.write32(code+4,0xD503201F); // nop
    machine.cpu.setPC(code);machine.cpu.writeRegister(0,2000);
    const auto sleep=machine.run(1);
    check(sleep.stop==Machine::Stop::Waiting,"guest timed wait blocks");
    check(machine.setAppletFocusState(Focus::Background) && machine.advanceTime(2000),"deadline expires while applet suspended");
    check(machine.run(100).instructions==0 && machine.cpu.getPC()==code+4 && !machine.process().nextWakeDelay(),"expired wait cannot bypass applet suspension");
    check(machine.setAppletFocus(true) && machine.run(1).instructions==1 && machine.cpu.getPC()==code+8,"expired wait resumes once after host focus restoration");
    machine.ram.write32(code+16,0xD4000101); // CreateThread
    machine.ram.write32(code+20,0xD4000121); // StartThread
    machine.ram.write32(code+24,0xD4000161); // SleepThread(0), yield
    machine.ram.write32(code+128,0x9100054A); // add x10,x10,#1
    machine.ram.write32(code+132,0x17FFFFFF); // b previous instruction
    machine.cpu.setPC(code+16);machine.cpu.writeRegister(1,code+128);machine.cpu.writeRegister(2,0);
    machine.cpu.writeRegister(3,Process::WorkerStackTop);machine.cpu.writeRegister(4,32);machine.cpu.writeRegister(5,0);
    check(machine.run(1).stop==Machine::Stop::Budget && machine.cpu.readRegister(0)==0,"create runnable-worker suspension fixture");
    const auto worker=machine.cpu.readRegister(1);machine.cpu.writeRegister(0,worker);
    check(machine.run(1).stop==Machine::Stop::Budget,"start second logical thread");
    check(machine.setAppletFocusState(Focus::Background) && machine.run(1000).instructions==0 && machine.process().workerThread().context.registers[10]==0,
        "suspension blocks both logical threads even when worker runnable");
    check(machine.setAppletFocus(true),"resume both threads");
    if(machine.process().activeThreadIndex()==0) { machine.cpu.writeRegister(0,0);machine.run(1); }
    check(machine.process().activeThreadIndex()==1 && machine.run(1).instructions==1 && machine.cpu.readRegister(10)==1,"worker executes after process resumes");
    check(machine.setAppletFocusState(Focus::Background) && machine.run(1000).instructions==0 && machine.cpu.readRegister(10)==1,
        "suspension also blocks the currently selected worker without rotating to main");
    check(machine.load(argv[1]) && !machine.process().appletSuspendsInBackground(),"reload resets focus policy");
    std::cout<<"Applet suspension failures="<<failures<<'\n';return failures?1:0;
}
