#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static Ipc::Buffer queryPacket(uint32_t fd,uint32_t event=3){Ipc::Buffer b{};put(b,0,4);put(b,4,10);put(b,16,0x49434653);put(b,24,4);put(b,32,fd);put(b,36,event);return b;}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;const bool baseline=argc==4 && std::string(argv[3])=="--baseline";if(argc==4 && !baseline)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original error-event client exits normally without GPU rendering");
        for(unsigned i=0;i<2;++i){const auto base=symbols.at("gpu_as_report")+i*256;auto get=[&](unsigned j){return m.ram.read64(base+j*8);};
            check(!get(0) && !get(1) && !get(3) && !get(5) && !get(7) && !get(8) && !get(17),"original channel/context prerequisites succeed");
            check(get(19)==(baseline?Ipc::UnknownCommand:0) && get(20)==(baseline?0:1) && (baseline?!get(21):bool(get(21))),"original QueryEvent reaches its expected boundary or copied read handle");
            check(!get(22) && !get(23),"later notifier enable and cleanup-disable complete successfully");
            check(get(24)==(baseline?0:Process::TimedOut) && !get(25) && !get(26),"original zero-time event wait times out and eventClose clears its handles");
            check(get(11)==UINT32_MAX && !get(12) && get(13)==UINT32_MAX && !get(14) && get(15)==UINT32_MAX && !get(16),"original channel/AS/Nvmap cleanup succeeds");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"event lifecycle is independent of execution slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000,code=scratch+4096;
        m.ram.setPermissions(code,4096,Memory::All);unsigned queries=0;uint32_t clone=0,saved=0,channel=0,as=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;
                auto send=[&](const Ipc::Buffer& p,uint32_t ep=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(ep)m.cpu.writeRegister(0,ep);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                auto svc=[&](uint16_t id,uint64_t a,uint64_t b=0,uint64_t c=0,uint64_t d=0){m.ram.write32(code,0xD4000001U|(uint32_t(id)<<5));m.cpu.restoreContext(context);m.cpu.setPC(code);m.cpu.writeRegister(0,a);m.cpu.writeRegister(1,b);m.cpu.writeRegister(2,c);m.cpu.writeRegister(3,d);m.run(1);return uint32_t(m.cpu.readRegister(0));};
                auto poll=[&](uint32_t h){m.ram.write32(scratch+512,h);return svc(0x18,0,scratch+512,1,0);};
                Ipc::Layout layout;
                if(!Ipc::parseBuffers(packet,&m.bus,ipc,layout) && layout.command==4 && layout.payloadSize==8){
                    ++queries;channel=read(packet,size_t(layout.payload));const auto report=symbols.at("gpu_as_report")+(queries-1)*256;as=uint32_t(m.ram.read64(report+16));const auto map=uint32_t(m.ram.read64(report+32));
                    for(const auto fd:{0U,UINT32_MAX,as,map})check(send(queryPacket(fd))==Process::InvalidHandle,"error-event query rejects absent and wrong-type descriptors");
                    for(const auto event:{0U,2U,4U,UINT32_MAX})check(send(queryPacket(channel,event))==Ipc::InvalidArguments,"unsupported event identities fail explicitly");
                    auto bad=packet;put(bad,4,9);check(send(bad)==Ipc::InvalidArguments,"QueryEvent requires exactly eight payload bytes");
                    const std::string_view path="/dev/nvhost-gpu";m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch));put(open,32,0x49434653);check(!send(open),"uninitialized event probe channel opens");const auto uninitialized=m.ram.read32(ipc+32);
                    check(send(queryPacket(uninitialized))==Process::InvalidState,"error-event query requires an allocated graphics context");Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,uninitialized);check(!send(close),"uninitialized event probe channel closes");
                    auto get=[&](uint32_t ep=0){check(!send(queryPacket(channel),ep),"error-event query succeeds");check(m.ram.read32(ipc+8)==2 && m.ram.read32(ipc+32)==0,"reply copies one readable handle and returns zero NvError");return m.ram.read32(ipc+12);};
                    const auto first=get(),second=get(clone);check(first!=second,"repeated root/clone queries publish distinct handle identities");
                    check(poll(first)==Process::TimedOut && poll(second)==Process::TimedOut,"new error-event copies remain unsignaled");
                    check(svc(0x11,second)==Process::InvalidHandle && svc(0x12,second)==Process::InvalidState && svc(0x17,second)==Process::InvalidState,"guest cannot signal or clear a driver-managed error event");
                    check(!svc(0x16,first) && poll(first)==Process::InvalidHandle && poll(second)==Process::TimedOut && !svc(0x16,second),"closing one copy preserves another and closed handles stay invalid");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected QueryEvent reply cannot publish a handle");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    saved=get();check(saved==second+1 && !svc(0x16,saved),"failed replies consume no kernel handle identity");saved=0;
                    auto fill=[&](){std::vector<uint32_t> handles;uint32_t result=0;while(handles.size()<128 && !(result=send(queryPacket(channel))))handles.push_back(m.ram.read32(ipc+12));check(result==Process::OutOfHandles,"copied error handles share the kernel handle quota");return handles;};
                    auto handles=fill();for(auto h:handles)check(!svc(0x16,h),"quota handle copy closes independently");const auto count=handles.size();
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed reply cannot leak a handle quota slot");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    handles=fill();check(handles.size()==count && count>0,"protected reply preserves complete handle capacity");for(auto h:handles)check(!svc(0x16,h),"refilled quota copies close");saved=get(clone);
                }
                if(saved && !Ipc::parseBuffers(packet,&m.bus,ipc,layout) && layout.command==2 && layout.payloadSize==4 && read(packet,size_t(layout.payload))==channel){
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed channel Close retains its error-event owner");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(!send(queryPacket(channel)) && poll(saved)==Process::TimedOut,"queries and existing copies remain valid after failed channel Close");const auto extra=m.ram.read32(ipc+12);check(!svc(0x16,extra),"post-failure query copy closes");
                }
                if(saved && !Ipc::parseBuffers(packet,&m.bus,ipc,layout) && layout.command==2 && layout.payloadSize==4 && read(packet,size_t(layout.payload))==as){
                    check(send(queryPacket(channel))==Process::InvalidHandle && poll(saved)==Process::TimedOut,"channel Close retires queries while previously copied event handles survive unsignaled");check(!svc(0x16,saved),"copied event closes after its channel");saved=0;
                }
                m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && queries==2 && !saved,"both original query lifetimes finish after handle probes");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;bool captured=false;uint64_t borrowed=0;
            for(unsigned step=0;step<500000;++step){
                const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);
                if(op==0xD4000421){
                    Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::Layout layout;
                    if(!Ipc::parseBuffers(p,&probe.bus,local,layout) && layout.command==4 && layout.payloadSize==8){
                        captured=true;probe.run(1);const auto h=probe.ram.read32(local+12);check(!probe.ram.read32(local+24) && h,"original error query publishes a handle before teardown");
                        const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()),"failed reload preserves GPU event handles");
                        const auto array=Process::HeapBase+0x180000,code=array+4096;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,0xD4000301);probe.ram.write32(array,h);probe.cpu.setPC(code);probe.cpu.writeRegister(0,0);probe.cpu.writeRegister(1,array);probe.cpu.writeRegister(2,1);probe.cpu.writeRegister(3,0);probe.run(1);check(probe.cpu.readRegister(0)==Process::TimedOut,"copied error handle remains valid after failed reload");
                        probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);
                        check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault tears down GPU events and borrowed communication memory");
                        check(probe.load(argv[1]),"fresh process loads after GPU event teardown");const auto result=probe.run(500000);check(result.stop==Machine::Stop::Exited && !result.exitCode && probe.ram.read64(symbols.at("gpu_as_report")+168)==h,"fresh process has no stale event handles and restarts handle identities");break;
                    }
                }
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }
            check(captured,"original error-event query is reached for lifetime probes");
        }
    }
    std::cout<<"GPU event instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
