#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static Ipc::Buffer ioctlPacket(uint32_t fd,uint32_t request,uint64_t source){Ipc::Buffer b{};const auto size=(request>>16)&0x3FFF;put(b,0,0x01110004);put(b,4,0xC0B);put(b,16,size);put(b,20,uint32_t(source));if(request>>31){put(b,28,size);put(b,32,uint32_t(source));}put(b,48,0x49434653);put(b,56,1);put(b,64,fd);put(b,68,request);return b;}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;const bool baseline=argc==4 && std::string(argv[3])=="--baseline";if(argc==4 && !baseline)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original full channel-initialization helper exits without GPU rendering");
        for(unsigned i=0;i<2;++i){const auto base=symbols.at("gpu_as_report")+i*256;auto get=[&](unsigned j){return m.ram.read64(base+j*8);};
            check(!get(0) && !get(1) && get(2) && !get(3) && get(4),"original NV, AS and Nvmap initialization succeed");
            check(get(5)==(baseline?Ipc::UnknownCommand:0) && get(6)==(baseline?0:1) && (baseline?get(7)==UINT32_MAX:bool(get(7))),"original nvGpuChannelCreate reaches its expected boundary or completes");
            check(get(8)==i+1 && !get(9) && get(10)==i+1 && !get(13) && !get(14),"original helper initializes local fence/context identities and an empty client queue");
            check(get(11)==(baseline?0:1) && (baseline?!get(12):bool(get(12))) && get(15)==(baseline?0:Process::TimedOut),"successful helper returns an unsignaled error-event handle");
            check(!get(16) && get(17)==UINT32_MAX && !get(18) && !get(19) && !get(20) && get(21)==UINT32_MAX && !get(22) && get(23)==UINT32_MAX && !get(24),"original nvGpuChannelClose, repeated Close and driver cleanup retire all resources");
            check(get(25)==(i?150:100),"original helper receives the intended priority");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"full channel initialization is independent of slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000;unsigned priorities=0;uint32_t clone=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false,setting=false;uint32_t fd=0,root=0,requested=0;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;root=uint32_t(context.registers[0]);
                auto send=[&](const Ipc::Buffer& p,uint32_t ep=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(ep)m.cpu.writeRegister(0,ep);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                Ipc::NvIoctl plan;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && plan.request==0x4004480D){
                    setting=true;++priorities;fd=plan.fd;requested=m.ram.read32(plan.source);const auto view=[&](uint32_t ep,uint32_t d){return m.process().gpuChannelPriority(ep,d);};
                    const auto report=symbols.at("gpu_as_report")+(priorities-1)*256;const auto map=uint32_t(m.ram.read64(report+32)),as=uint32_t(m.ram.read64(report+16));
                    check(plan.size==4 && !plan.destination && view(root,fd)==std::optional<uint32_t>{0} && m.process().gpuErrorNotifierEnabled(root,fd)==std::optional<bool>{true},"original priority follows complete context/event/notifier initialization");
                    auto set=[&](uint32_t d,uint32_t value,uint32_t ep=0){m.ram.write32(scratch+256,value);return send(ioctlPacket(d,0x4004480D,scratch+256),ep);};
                    check(set(0,100)==Process::InvalidHandle && set(UINT32_MAX,100)==Process::InvalidHandle && !view(0,fd),"priority rejects absent descriptors and diagnostic endpoints");
                    check(set(as,100)==Ipc::UnknownCommand && set(map,100)==Ipc::UnknownCommand && !view(root,as),"priority requires a typed channel");
                    for(const auto value:{0U,1U,49U,51U,99U,101U,149U,151U,UINT32_MAX})check(set(fd,value)==Ipc::InvalidArguments,"only the three audited SDK priorities are supported");
                    auto bad=packet;put(bad,16,3);check(send(bad)==Ipc::InvalidArguments,"priority input extent must match encoding");bad=packet;put(bad,32,uint32_t(plan.source));check(send(bad)==Ipc::InvalidArguments,"write-only priority requires a null receive descriptor");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed reply cannot configure priority");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);check(view(root,fd)==std::optional<uint32_t>{0},"invalid requests and failed reply retain unconfigured priority");
                    for(unsigned cycle=0;cycle<2;++cycle){
                        const std::string_view path="/dev/nvhost-gpu";m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch));put(open,32,0x49434653);check(!send(open),"independent priority channel opens");const auto peer=m.ram.read32(ipc+32);
                        check(set(peer,100)==Process::InvalidState && !view(root,peer),"priority requires the local Nvmap association");m.ram.write32(scratch+512,map);check(!send(ioctlPacket(peer,0x40044801,scratch+512)),"peer associates Nvmap");check(view(root,peer)==std::optional<uint32_t>{0},"reused channel slots start with no configured priority");
                        for(const auto value:{50U,100U,150U})check(!set(peer,value,clone) && view(root,peer)==std::optional<uint32_t>{value} && !set(peer,value),"root/clone share valid idempotent low, medium and high priorities");
                        m.ram.write32(scratch+256,50);auto p=ioctlPacket(peer,0x4004480D,scratch+256);m.ram.setPermissions(scratch,4096,Memory::Read);check(!send(p) && m.ram.read32(scratch+256)==50,"write-only priority accepts readable immutable input");m.ram.setPermissions(scratch,4096,Memory::Read|Memory::Write);
                        m.ram.write32(scratch+256,100);m.ram.writeBytes(ipc,p.data(),p.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed update cannot replace a configured priority");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);check(view(root,peer)==std::optional<uint32_t>{50} && view(root,fd)==std::optional<uint32_t>{0},"peer priorities remain independent and reply rollback preserves the old value");
                        Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,peer);m.ram.writeBytes(ipc,close.data(),close.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed Close retains priority state");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);check(view(root,peer)==std::optional<uint32_t>{50} && !send(close) && !view(root,peer) && set(peer,100)==Process::InvalidHandle,"successful Close drops priority and retires its descriptor");
                    }
                    check(m.ram.read32(plan.source)==requested,"host probes preserve the original priority input");m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(setting)check(!m.ram.read32(ipc+24) && m.process().gpuChannelPriority(root,fd)==std::optional<uint32_t>{requested} && m.process().gpuErrorNotifierEnabled(root,fd)==std::optional<bool>{true},"original final priority call publishes state without changing notifier configuration");if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && priorities==2,"both full original channel helpers finish after injected priority probes");
        {
            Machine failed(false,true,true);if(!failed.load(argv[1]))return 2;const auto local=failed.process().mainThread().localRegion;unsigned prioritiesSeen=0,disables=0;uint32_t retired=0,endpoint=0;bool closed=false,finished=false;
            for(unsigned step=0;step<500000;++step){bool closing=false;
                if(failed.ram.fetch32(failed.cpu.getPC())==0xD4000421){Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=failed.ram.read8(local+i);Ipc::NvIoctl request;
                    if(!Ipc::parseNvIoctl(p,failed.bus,local,request)){
                        if(request.request==0x4004480D && ++prioritiesSeen==1){retired=request.fd;endpoint=uint32_t(failed.cpu.readRegister(0));failed.ram.write32(request.source,0);}
                        if(request.request==0xC018480C && !failed.ram.read32(request.source+16)){++disables;if(request.fd==retired)check(failed.process().gpuChannelPriority(endpoint,retired)==std::optional<uint32_t>{0} && failed.process().gpuErrorNotifierEnabled(endpoint,retired)==std::optional<bool>{true},"failed final priority preserves prior state until original helper unwinds");}
                    }
                    Ipc::Layout layout;if(retired && !Ipc::parseBuffers(p,&failed.bus,local,layout) && layout.command==2 && layout.payloadSize==4 && read(p,size_t(layout.payload))==retired){closing=true;check(failed.process().gpuErrorNotifierEnabled(endpoint,retired)==std::optional<bool>{false},"original failed-create cleanup disables the notifier before channel Close");}
                }
                const auto result=failed.run(1);if(closing){closed=true;check(!failed.process().gpuChannelPriority(endpoint,retired) && !failed.process().gpuErrorNotifierEnabled(endpoint,retired),"original failed-create cleanup retires the partially initialized channel");}
                if(result.stop!=Machine::Stop::Budget){finished=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
            }
            const auto report=symbols.at("gpu_as_report");check(finished && closed && prioritiesSeen==2 && disables==2,"original helper unwinds a final-call failure and a later lifetime still succeeds");
            check(failed.ram.read64(report+40)==Ipc::InvalidArguments && !failed.ram.read64(report+48) && failed.ram.read64(report+56)==UINT32_MAX && !failed.ram.read64(report+88) && !failed.ram.read64(report+96),"failed helper returns the priority error with its client channel/event closed");
            check(!failed.ram.read64(report+256+40) && failed.ram.read64(report+256+48)==1 && failed.ram.read64(report+256+120)==Process::TimedOut,"next original helper initializes completely after failure cleanup");
        }
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;bool captured=false;uint64_t borrowed=0;
            for(unsigned step=0;step<500000;++step){
                const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);
                if(op==0xD4000421){Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::NvIoctl request;
                    if(!Ipc::parseNvIoctl(p,probe.bus,local,request) && request.request==0x4004480D){
                        captured=true;const auto root=uint32_t(probe.cpu.readRegister(0));probe.run(1);check(!probe.ram.read32(local+24) && probe.process().gpuChannelPriority(root,request.fd)==std::optional<uint32_t>{100},"original helper configures priority before lifetime probe");
                        const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()) && probe.process().gpuChannelPriority(root,request.fd)==std::optional<uint32_t>{100},"failed reload preserves configured priority");
                        const auto code=Process::HeapBase+0x180000;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);
                        check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && !probe.process().gpuChannelPriority(root,request.fd) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault drops channel priority and releases borrowed memory");
                        check(probe.load(argv[1]),"fresh process loads after channel priority teardown");bool fresh=false;
                        for(unsigned next=0;next<500000;++next){if(probe.ram.fetch32(probe.cpu.getPC())==0xD4000421){Ipc::Buffer q{};for(size_t i=0;i<q.size();++i)q[i]=probe.ram.read8(local+i);Ipc::NvIoctl nextRequest;
                            if(!Ipc::parseNvIoctl(q,probe.bus,local,nextRequest) && nextRequest.request==0x4004480D){fresh=true;check(probe.process().gpuChannelPriority(uint32_t(probe.cpu.readRegister(0)),nextRequest.fd)==std::optional<uint32_t>{0},"fresh process has no stale configured priority");break;}}
                            if(probe.run(1).stop!=Machine::Stop::Budget)break;
                        }
                        check(fresh,"fresh process reaches original final priority call");const auto result=probe.run(500000);check(result.stop==Machine::Stop::Exited && !result.exitCode,"fresh full channel helper and cleanup complete normally");break;
                    }
                }
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }
            check(captured,"original priority call is reached for teardown checks");
        }
    }
    std::cout<<"GPU channel initialization instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
