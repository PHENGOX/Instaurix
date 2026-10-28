#include "machine.hpp"
#include "ipc.hpp"
#include <iostream>
#include <tuple>
static uint64_t get(const Ipc::Buffer& b,size_t at,size_t n=4) { uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[at+i])<<(8*i);return v; }
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4) { for(size_t i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i)); }
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* message) { if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';} };
    using Mode=Process::OperationMode;
    Ipc::Buffer registration{},lookup{},convert{},proxy{},common{},self{},operation{},performance{},event{},opNotifications{},perfNotifications{};
    for(auto mode:{Mode::Handheld,Mode::Docked}) {
        Machine sdk(false,true,true);if(!sdk.load(argv[1]))return 2;
        check(sdk.setAppletOperationMode(mode),"select SDK operation profile");const auto tls=sdk.process().mainThread().localRegion;
        bool opSeen=false,perfSeen=false,opEnabled=false,perfEnabled=false;Machine::Result step{Machine::Stop::Budget,0};
        for(unsigned i=0;i<100000 && step.stop==Machine::Stop::Budget;++i) {
            const auto instruction=sdk.ram.fetch32(sdk.cpu.getPC());const bool ipc=(instruction&0xFFE0001F)==0xD4000001 && ((instruction>>5)&0xFFFF)==0x21;
            Ipc::Buffer b{};if(ipc)for(size_t j=0;j<b.size();++j)b[j]=sdk.ram.read8(tls+j);
            step=sdk.run(1);if(!ipc)continue;
            if(get(b,0)==5 && get(b,24)==0)convert=b;
            if(get(b,0)!=4)continue;
            const bool pid=get(b,4)&0x80000000U;const auto cmif=pid?32:16;
            if(get(b,cmif)==0x49434653) {
                if(pid && get(b,cmif+8)==0 && get(registration,0)==0)registration=b;
                if(get(b,cmif+8)==1 && std::string(reinterpret_cast<const char*>(b.data()+cmif+16),8)=="appletOE")lookup=b;
                continue;
            }
            Ipc::DomainRequest d;if(Ipc::parseDomain(b,d)||d.close)continue;
            if(d.object==1 && d.copyHandlePresent)proxy=b;
            if(d.object==2 && d.command==0)common=b;
            if(d.object==2 && d.command==1)self=b;
            if(d.object==5 && d.command==0)event=b;
            if(d.object==5 && d.command==5){operation=b;opSeen=true;check(sdk.ram.read32(tls+40)==0 && sdk.ram.read8(tls+48)==uint8_t(mode) && sdk.ram.read32(tls+4)==13,"SDK receives actual selected operation mode as u8");}
            if(d.object==5 && d.command==6){performance=b;perfSeen=true;check(sdk.ram.read32(tls+40)==0 && sdk.ram.read32(tls+48)==uint8_t(mode) && sdk.ram.read32(tls+4)==13,"SDK receives paired performance mode as u32");}
            if(d.object==6 && d.command==11){opNotifications=b;opEnabled=sdk.ram.read32(tls+40)==0;}
            if(d.object==6 && d.command==12){perfNotifications=b;perfEnabled=sdk.ram.read32(tls+40)==0;}
        }
        check(opSeen && perfSeen && opEnabled && perfEnabled,"original SDK reads modes and enables both notifications");
        check(step.stop==Machine::Stop::Exited && step.exitCode==0xE017,"SDK initializes filesystem and reaches explicit missing SD mounting dependency");
    }
    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;std::string error;Process p;
    if(!ProgramLoader::loadImage(ram,argv[1],image,error)||!p.initialize(ram,image,true,error,true))return 2;
    Bus bus(ram);CPU cpu(bus);const auto ipc=p.mainThread().localRegion;
    const uint8_t sm[]{'s','m',':',0};ram.writeBytes(0x200300,sm,sizeof sm);cpu.writeRegister(1,0x200300);p.dispatch(0x1F,cpu,bus);const auto manager=uint32_t(cpu.readRegister(1));
    auto send=[&](Ipc::Buffer b,uint32_t handle){ram.writeBytes(ipc,b.data(),b.size());cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);return cpu.readRegister(0);};
    check(send(registration,manager)==0,"register replay manager");
    auto connect=[&](){send(lookup,manager);const auto h=ram.read32(ipc+12);send(convert,h);send(proxy,h);send(common,h);const auto c=ram.read32(ipc+48);send(self,h);return std::tuple{h,c,ram.read32(ipc+48)};};
    const auto [root,commonId,selfId]=connect();put(operation,20,commonId);put(performance,20,commonId);put(event,20,commonId);put(opNotifications,20,selfId);put(perfNotifications,20,selfId);
    auto receive=operation;put(receive,40,1);send(event,root);const auto eventHandle=ram.read32(ipc+12);
    auto poll=[&](){ram.write32(0x200320,eventHandle);cpu.writeRegister(1,0x200320);cpu.writeRegister(2,1);cpu.writeRegister(3,0);p.dispatch(0x18,cpu,bus);return cpu.readRegister(0);};
    check(send(operation,root)==0 && ram.read8(ipc+48)==0 && send(performance,root)==0 && ram.read32(ipc+48)==0,"default profile is handheld/normal");
    check(p.setAppletOperationMode(Mode::Docked) && poll()==Process::TimedOut,"mode changes are silent before opt-in");
    check(send(operation,root)==0 && ram.read8(ipc+48)==1 && send(performance,root)==0 && ram.read32(ipc+48)==1,"both queries reflect same docked profile");
    auto bad=operation;put(bad,18,17,2);put(bad,4,13);check(send(bad,root)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments,"getter rejects extra payload");
    auto invalid=perfNotifications;invalid[48]=2;check(send(invalid,root)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments,"notification flag must be boolean");
    ram.writeBytes(ipc,opNotifications.data(),opNotifications.size());ram.setPermissions(ipc,4096,Memory::Read);cpu.writeRegister(0,root);p.dispatch(0x21,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidPointer,"notification opt-in reply must be writable");ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
    check(p.setAppletOperationMode(Mode::Handheld) && poll()==Process::TimedOut,"failed opt-in published no subscription");
    check(send(opNotifications,root)==0 && send(perfNotifications,root)==0,"enable both notifications");
    check(poll()==Process::TimedOut && !p.setAppletOperationMode(static_cast<Mode>(2)),"opting in emits no synthetic message and invalid profile is rejected");
    auto clone=convert;put(clone,24,2);send(clone,root);const auto cloned=ram.read32(ipc+12);
    ram.write32(0x200320,eventHandle);cpu.writeRegister(1,0x200320);cpu.writeRegister(2,1);cpu.writeRegister(3,UINT64_MAX);
    check(p.dispatch(0x18,cpu,bus)==Process::Action::Wait,"guest blocks on mode notification event");
    check(p.setAppletOperationMode(Mode::Docked),"profile transition signals the shared message event");
    check(p.resumeWait(cpu) && cpu.readRegister(0)==0 && poll()==0,"mode transition completes the blocked kernel wait");
    check(send(receive,cloned)==0 && ram.read32(ipc+48)==30 && poll()==0,"clone consumes OperationModeChanged first, leaving event signaled");
    check(send(receive,root)==0 && ram.read32(ipc+48)==31 && poll()==Process::TimedOut,"PerformanceModeChanged follows and clears last signal");
    check(p.setAppletOperationMode(Mode::Docked) && poll()==Process::TimedOut,"unchanged profile emits no duplicates");
    auto disablePerf=perfNotifications;disablePerf[48]=0;send(disablePerf,root);
    check(p.setAppletOperationMode(Mode::Handheld) && send(receive,root)==0 && ram.read32(ipc+48)==30 && poll()==Process::TimedOut,"subscriptions are independent");
    send(perfNotifications,root);
    bool focused=true;for(size_t i=0;i<Process::AppletMessageLimit-1;++i){focused=!focused;check(p.setAppletFocus(focused),"fill queue with focus notifications");}
    check(!p.setAppletOperationMode(Mode::Docked) && p.appletOperationMode()==Mode::Handheld && p.appletPerformanceMode()==0,"insufficient room for two notifications preserves both modes");
    for(size_t i=0;i<Process::AppletMessageLimit-1;++i)check(send(receive,root)==0 && ram.read32(ipc+48)==15,"failed transition leaves no partial mode notification");
    check(poll()==Process::TimedOut && p.setAppletOperationMode(Mode::Docked),"transition succeeds after queue capacity is available");
    auto disableOp=opNotifications;disableOp[48]=0;send(disableOp,root);send(disablePerf,root);
    check(send(receive,root)==0 && ram.read32(ipc+48)==30 && send(receive,root)==0 && ram.read32(ipc+48)==31,"disabling notifications preserves already queued messages");
    const auto [other,otherCommon,otherSelf]=connect();auto query=operation;put(query,20,otherCommon);
    check(send(query,other)==0 && ram.read8(ipc+48)==1 && p.setAppletOperationMode(Mode::Handheld) && poll()==Process::TimedOut,"independent connections share profile and disabled subscriptions");
    Ipc::Buffer close{};put(close,0,2);send(close,root);send(close,cloned);send(close,other);
    check(p.appletOperationMode()==Mode::Handheld,"closing sessions preserves host profile");
    p.checkpoint(cpu,Process::State::Exited);check(!p.setAppletOperationMode(Mode::Docked),"retired process rejects profile changes");
    Machine reload(false,true,true);check(reload.load(argv[1]) && reload.setAppletOperationMode(Mode::Docked) && reload.load(argv[1]) && reload.process().appletOperationMode()==Mode::Handheld,"reload resets local profile");
    std::cout<<"Operation profile failures="<<failures<<'\n';return failures?1:0;
}
