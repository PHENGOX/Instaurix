#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static Ipc::Buffer ioctlPacket(uint32_t fd,uint32_t request,uint64_t source){Ipc::Buffer b{};const auto size=(request>>16)&0x3FFF;put(b,0,0x01110004);put(b,4,0xC0B);put(b,16,size);put(b,20,uint32_t(source));if(request>>31){put(b,28,size);put(b,32,uint32_t(source));}put(b,48,0x49434653);put(b,56,1);put(b,64,fd);put(b,68,request);return b;}
static Ipc::Buffer queryPacket(uint32_t fd){Ipc::Buffer b{};put(b,0,4);put(b,4,10);put(b,16,0x49434653);put(b,24,4);put(b,32,fd);put(b,36,3);return b;}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;const bool baseline=argc==4 && std::string(argv[3])=="--baseline";if(argc==4 && !baseline)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original notifier client exits without GPU rendering");
        for(unsigned i=0;i<2;++i){const auto base=symbols.at("gpu_as_report")+i*256;auto get=[&](unsigned j){return m.ram.read64(base+j*8);};
            check(!get(0) && !get(1) && !get(3) && !get(5) && !get(7) && !get(8) && !get(17) && !get(19) && get(20) && get(21),"original GPU context and error-event prerequisites succeed");
            check(get(22)==(baseline?Ipc::UnknownCommand:0) && get(23)==(baseline?Ipc::UnknownCommand:0),"original notifier enable and cleanup-disable reach expected results");
            check(!get(27),"later priority configuration completes the bounded original notifier harness");
            check(get(24)==Process::TimedOut && !get(25) && !get(26),"notifier state does not invent an error signal");
            check(get(11)==UINT32_MAX && !get(12) && get(13)==UINT32_MAX && !get(14) && get(15)==UINT32_MAX && !get(16),"original cleanup releases channel and retained resources");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"notifier execution is independent of slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000,code=scratch+4096;m.ram.setPermissions(code,4096,Memory::All);
        unsigned calls=0;uint32_t clone=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false,notifying=false;uint32_t fd=0,root=0;bool expected=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;root=uint32_t(context.registers[0]);
                auto send=[&](const Ipc::Buffer& p,uint32_t ep=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(ep)m.cpu.writeRegister(0,ep);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                Ipc::NvIoctl plan;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && plan.request==0xC018480C){
                    notifying=true;++calls;fd=plan.fd;expected=bool(m.ram.read32(plan.source+16));const auto view=[&](uint32_t ep,uint32_t descriptor){return m.process().gpuErrorNotifierEnabled(ep,descriptor);};
                    check(plan.size==24 && !m.ram.read64(plan.source) && !m.ram.read64(plan.source+8) && !m.ram.read32(plan.source+20),"original notifier uses the audited 24-byte structure");
                    std::array<uint8_t,24> original{};for(size_t i=0;i<original.size();++i)original[i]=m.ram.read8(plan.source+i);
                    const bool before=calls%2==0;check(view(root,fd)==std::optional<bool>{before} && view(clone,fd)==std::optional<bool>{before},"original enable/disable ordering observes shared channel state");
                    auto set=[&](uint32_t target,uint32_t value,uint32_t ep=0){for(size_t i=0;i<24;i+=4)m.ram.write32(scratch+256+i,0);m.ram.write32(scratch+272,value);return send(ioctlPacket(target,0xC018480C,scratch+256),ep);};
                    check(set(0,1)==Process::InvalidHandle && set(UINT32_MAX,1)==Process::InvalidHandle && !view(0,fd) && !view(root,0),"notifier and diagnostic reject absent identities");
                    const auto report=symbols.at("gpu_as_report")+((calls-1)/2)*256;const auto as=uint32_t(m.ram.read64(report+16)),map=uint32_t(m.ram.read64(report+32));
                    check(set(as,1)==Ipc::UnknownCommand && set(map,1)==Ipc::UnknownCommand && !view(root,as) && !view(root,map),"notifier methods require channel descriptors");
                    for(const auto value:{2U,UINT32_MAX})check(set(fd,value)==Ipc::InvalidArguments,"notifier state is a strict boolean");
                    set(fd,2);m.ram.write32(scratch+272,expected);auto p=ioctlPacket(fd,0xC018480C,scratch+256);
                    for(const auto offset:{0U,4U,8U,12U,20U}){m.ram.write32(scratch+256+offset,1);check(send(p)==Ipc::InvalidArguments,"nonzero offset, size and padding words fail");m.ram.write32(scratch+256+offset,0);}
                    auto bad=p;put(bad,16,23);check(send(bad)==Ipc::InvalidArguments,"notifier input extent must match encoding");bad=p;put(bad,28,23);check(send(bad)==Ipc::InvalidArguments,"notifier output extent must match encoding");
                    bad=p;put(bad,32,uint32_t(scratch+8192));m.ram.setPermissions(scratch+8192,4096,Memory::Read);check(send(bad)==Ipc::InvalidBuffer,"notifier output must be writable");m.ram.setPermissions(scratch+8192,4096,Memory::Read|Memory::Write);
                    m.ram.writeBytes(ipc,p.data(),p.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected notifier reply cannot publish new state");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(view(root,fd)==std::optional<bool>{before},"invalid requests and late reply failure preserve the previous enabled state");
                    check(!set(fd,expected,clone) && view(root,fd)==std::optional<bool>{expected} && !set(fd,expected),"root and clone share successful idempotent notifier updates");
                    auto disjoint=p;put(disjoint,32,uint32_t(scratch+8192));for(size_t i=0;i<24;++i)m.ram.write8(scratch+8192+i,0xA5);check(!send(disjoint),"notifier supports a separate validated receive buffer");for(size_t i=0;i<24;++i)check(m.ram.read8(scratch+8192+i)==m.ram.read8(scratch+256+i),"successful notifier reply preserves and copies every request byte");
                    check(!send(queryPacket(fd)),"notifier changes preserve the channel error event");const auto copy=m.ram.read32(ipc+12);m.ram.write32(scratch+512,copy);m.ram.write32(code,0xD4000301);m.cpu.restoreContext(context);m.cpu.setPC(code);m.cpu.writeRegister(0,0);m.cpu.writeRegister(1,scratch+512);m.cpu.writeRegister(2,1);m.cpu.writeRegister(3,0);m.run(1);check(m.cpu.readRegister(0)==Process::TimedOut,"enable or disable never signals the error event");
                    m.ram.write32(code,0xD40002C1);m.cpu.restoreContext(context);m.cpu.setPC(code);m.cpu.writeRegister(0,copy);m.run(1);check(!m.cpu.readRegister(0),"extra diagnostic event copy closes");
                    check(!set(fd,before) && view(root,fd)==std::optional<bool>{before},"host probes restore state before the original call");
                    if(calls%2){
                        for(unsigned cycle=0;cycle<2;++cycle){
                            const std::string_view path="/dev/nvhost-gpu";m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch));put(open,32,0x49434653);check(!send(open),"independent notifier channel opens");const auto peer=m.ram.read32(ipc+32);
                            check(set(peer,1)==Process::InvalidState && !view(root,peer),"notifier needs initialized context and queried event");
                            auto io=[&](uint32_t target,uint32_t request){return send(ioctlPacket(target,request,scratch+1024));};
                            m.ram.write32(scratch+1024,map);check(!io(peer,0x40044801),"peer associates Nvmap");m.ram.write32(scratch+1024,peer);check(!io(as,0x40044101),"peer binds AS");
                            for(size_t i=0;i<32;i+=4)m.ram.write32(scratch+1024+i,0);m.ram.write32(scratch+1024,0x800);m.ram.write32(scratch+1028,1);check(!io(peer,0xC020481A),"peer allocates GPFIFO");m.ram.write32(scratch+1024,0xB197);m.ram.write32(scratch+1028,0);m.ram.write64(scratch+1032,0);check(!io(peer,0xC0104809),"peer allocates context");
                            check(set(peer,1)==Process::InvalidState && !view(root,peer),"context alone does not configure a notifier before QueryEvent");check(!send(queryPacket(peer)),"peer queries its error event");const auto peerEvent=m.ram.read32(ipc+12);
                            check(view(root,peer)==std::optional<bool>{false} && !set(peer,1,clone) && view(root,peer)==std::optional<bool>{true} && view(root,fd)==std::optional<bool>{before},"reused descriptor slots start disabled and channel notifier states are independent");
                            Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,peer);m.ram.writeBytes(ipc,close.data(),close.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed Close cannot drop an enabled notifier");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                            check(view(root,peer)==std::optional<bool>{true} && !send(close) && !view(root,peer) && set(peer,1)==Process::InvalidHandle,"successful enabled-channel Close drops notifier state and retires its descriptor");
                            m.ram.write32(code,0xD40002C1);m.cpu.restoreContext(context);m.cpu.setPC(code);m.cpu.writeRegister(0,peerEvent);m.run(1);check(!m.cpu.readRegister(0),"peer event copy closes after its channel");
                        }
                    }
                    for(size_t i=0;i<24;++i)check(m.ram.read8(plan.source+i)==original[i],"injected probes preserve the original SDK request bytes");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(notifying)check(!m.ram.read32(ipc+24) && m.process().gpuErrorNotifierEnabled(root,fd)==std::optional<bool>{expected},"each original notifier call publishes its requested state");if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && calls==4,"both original notifier lifetimes complete after probes");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;bool captured=false;uint64_t borrowed=0;
            for(unsigned step=0;step<500000;++step){
                const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);
                if(op==0xD4000421){Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::NvIoctl plan;
                    if(!Ipc::parseNvIoctl(p,probe.bus,local,plan) && plan.request==0xC018480C && probe.ram.read32(plan.source+16)==1){
                        captured=true;const auto root=uint32_t(probe.cpu.readRegister(0));probe.run(1);check(!probe.ram.read32(local+24) && probe.process().gpuErrorNotifierEnabled(root,plan.fd)==std::optional<bool>{true},"original notifier enables before lifetime probe");
                        const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()) && probe.process().gpuErrorNotifierEnabled(root,plan.fd)==std::optional<bool>{true},"failed reload preserves enabled notifier state");
                        const auto code=Process::HeapBase+0x180000;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);
                        check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && !probe.process().gpuErrorNotifierEnabled(root,plan.fd) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault drops notifier state and restores borrowed memory");
                        check(probe.load(argv[1]),"fresh process loads after enabled notifier teardown");bool fresh=false;
                        for(unsigned next=0;next<500000;++next){if(probe.ram.fetch32(probe.cpu.getPC())==0xD4000421){Ipc::Buffer q{};for(size_t i=0;i<q.size();++i)q[i]=probe.ram.read8(local+i);Ipc::NvIoctl request;
                            if(!Ipc::parseNvIoctl(q,probe.bus,local,request) && request.request==0xC018480C){fresh=true;check(probe.process().gpuErrorNotifierEnabled(uint32_t(probe.cpu.readRegister(0)),request.fd)==std::optional<bool>{false},"fresh process has no stale enabled notifier");break;}}
                            if(probe.run(1).stop!=Machine::Stop::Budget)break;
                        }
                        check(fresh,"fresh process reaches notifier initialization");const auto result=probe.run(500000);check(result.stop==Machine::Stop::Exited && !result.exitCode,"fresh notifier lifecycle completes normally");break;
                    }
                }
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }
            check(captured,"original enable call is reached for teardown checks");
        }
    }
    std::cout<<"GPU notifier instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
