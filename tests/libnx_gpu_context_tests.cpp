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
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original graphics-context client exits without rendering");
        for(unsigned iteration=0;iteration<2;++iteration){const auto base=symbols.at("gpu_as_report")+iteration*256;auto get=[&](unsigned i){return m.ram.read64(base+i*8);};
            check(!get(0) && !get(1) && get(2) && !get(3) && get(4) && !get(5) && get(6) && !get(7) && !get(8),"original NV, AS, Nvmap, channel, binding and queue allocation succeed");
            check(get(9)==iteration+1 && !get(10),"GPFIFO allocation returns only a local fence identity");
            check(get(17)==(baseline?Ipc::UnknownCommand:0) && get(18)==(baseline?0:iteration+1),"original context allocation returns a fresh local object identity");
            check(!get(19) && get(20)==(baseline?0:1) && (baseline?!get(21):bool(get(21))),"later GPU error-event query publishes a readable event before original cleanup");
            check(get(11)==UINT32_MAX && !get(12) && get(13)==UINT32_MAX && !get(14) && get(15)==UINT32_MAX && !get(16),"original cleanup retires context, queue and retained descriptors");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"context behavior is independent of execution slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000;
        unsigned contexts=0;uint32_t clone=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;
                auto send=[&](const Ipc::Buffer& p,uint32_t endpoint=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(endpoint)m.cpu.writeRegister(0,endpoint);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                Ipc::NvIoctl plan;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && plan.request==0xC0104809){
                    ++contexts;const auto report=symbols.at("gpu_as_report")+(contexts-1)*256;const auto as=uint32_t(m.ram.read64(report+16)),map=uint32_t(m.ram.read64(report+32));
                    check(plan.size==16 && m.ram.read32(plan.source)==0xB197 && !m.ram.read32(plan.source+4) && m.ram.read64(plan.source+8)==0xDEADBEEF,"original context request uses the historical class and ignores its output initializer");
                    auto allocate=[&](uint32_t fd,uint32_t cls=0xB197,uint32_t flags=0,uint32_t ep=0){m.ram.write32(scratch+256,cls);m.ram.write32(scratch+260,flags);m.ram.write64(scratch+264,0xA5A5A5A55A5A5A5AULL);const auto code=send(ioctlPacket(fd,0xC0104809,scratch+256),ep);if(code)check(m.ram.read64(scratch+264)==0xA5A5A5A55A5A5A5AULL,"failed allocation preserves its complete output field");return code;};
                    auto close=[&](uint32_t fd){Ipc::Buffer b{};put(b,0,4);put(b,4,9);put(b,16,0x49434653);put(b,24,2);put(b,32,fd);return send(b);};
                    check(allocate(0)==Process::InvalidHandle && allocate(UINT32_MAX)==Process::InvalidHandle,"absent context descriptors fail");
                    check(allocate(as)==Ipc::UnknownCommand && allocate(map)==Ipc::UnknownCommand,"context allocation cannot execute on AS or Nvmap devices");
                    for(const auto cls:{0U,0xC797U,UINT32_MAX})check(allocate(plan.fd,cls)==Ipc::InvalidArguments,"unvalidated graphics classes are rejected");
                    check(allocate(plan.fd,0xB197,1)==Ipc::InvalidArguments,"unsupported graphics-context flags are rejected");
                    const std::string_view path="/dev/nvhost-gpu";m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch));put(open,32,0x49434653);check(!send(open),"probe channel opens");const auto fd=m.ram.read32(ipc+32);
                    check(allocate(fd)==Process::InvalidState,"context allocation requires a queue");m.ram.write32(scratch+512,map);check(!send(ioctlPacket(fd,0x40044801,scratch+512)),"probe channel associates Nvmap");m.ram.write32(scratch+512,fd);check(!send(ioctlPacket(as,0x40044101,scratch+512)),"probe channel binds AS");check(allocate(fd)==Process::InvalidState,"AS binding alone cannot allocate a context");
                    for(size_t i=0;i<32;i+=4)m.ram.write32(scratch+512+i,0);m.ram.write32(scratch+512,0x800);m.ram.write32(scratch+516,1);check(!send(ioctlPacket(fd,0xC020481A,scratch+512)),"probe channel allocates GPFIFO first");
                    m.ram.write32(scratch+256,0xB197);m.ram.write32(scratch+260,0);m.ram.write64(scratch+264,UINT64_MAX);auto p=ioctlPacket(fd,0xC0104809,scratch+256);
                    auto bad=p;put(bad,16,15);check(send(bad)==Ipc::InvalidArguments,"context request extent must match encoding");bad=p;put(bad,28,15);check(send(bad)==Ipc::InvalidArguments,"context receive extent must match encoding");
                    bad=p;put(bad,32,uint32_t(scratch+4096));m.ram.setPermissions(scratch+4096,4096,Memory::Read);check(send(bad)==Ipc::InvalidBuffer,"context allocation requires writable output");m.ram.setPermissions(scratch+4096,4096,Memory::Read|Memory::Write);
                    m.ram.writeBytes(ipc,p.data(),p.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected reply cannot publish a graphics context");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);check(m.ram.read64(scratch+264)==UINT64_MAX,"failed reply leaves output bytes unchanged");
                    const auto previous=contexts==1?0:m.ram.read64(report-256+144);check(!allocate(fd,0xB197,0,clone) && m.ram.read64(scratch+264)==previous+1,"successful allocation through clone consumes exactly one object identity");
                    check(m.ram.read32(scratch+256)==0xB197 && !m.ram.read32(scratch+260),"context reply preserves all input fields");check(allocate(fd)==Ipc::AlreadyInitialized,"one context is permitted per channel");
                    check(!send(open),"independent peer channel opens");const auto peer=m.ram.read32(ipc+32);m.ram.write32(scratch+512,map);check(!send(ioctlPacket(peer,0x40044801,scratch+512)),"peer channel associates Nvmap");m.ram.write32(scratch+512,peer);check(!send(ioctlPacket(as,0x40044101,scratch+512)),"peer channel shares address space");
                    for(size_t i=0;i<32;i+=4)m.ram.write32(scratch+512+i,0);m.ram.write32(scratch+512,0x800);m.ram.write32(scratch+516,1);check(!send(ioctlPacket(peer,0xC020481A,scratch+512)) && !allocate(peer) && m.ram.read64(scratch+264)==previous+2,"simultaneous channels own distinct graphics contexts");
                    check(!close(peer) && allocate(fd)==Ipc::AlreadyInitialized,"closing a peer preserves the other channel's graphics context");
                    Ipc::Buffer closing{};put(closing,0,4);put(closing,4,9);put(closing,16,0x49434653);put(closing,24,2);put(closing,32,fd);m.ram.writeBytes(ipc,closing.data(),closing.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed channel Close preserves context state");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(allocate(fd)==Ipc::AlreadyInitialized && !close(fd) && allocate(fd)==Process::InvalidHandle,"successful Close releases context and stale descriptors cannot allocate");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && contexts==2,"both original context lifetimes finish after injected probes");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;bool captured=false;uint64_t borrowed=0;
            for(unsigned step=0;step<500000;++step){
                const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);
                if(op==0xD4000421){
                    Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::NvIoctl plan;
                    if(!Ipc::parseNvIoctl(p,probe.bus,local,plan) && plan.request==0xC0104809){
                        captured=true;const auto context=probe.cpu.captureContext();probe.run(1);check(!probe.ram.read32(local+24),"original context allocates before lifetime probe");
                        const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()),"failed reload preserves live context");
                        probe.ram.writeBytes(local,p.data(),p.size());probe.cpu.restoreContext(context);probe.run(1);check(probe.ram.read32(local+24)==Ipc::AlreadyInitialized,"failed reload retains allocated graphics-context state");
                        const auto code=Process::HeapBase+0x180000;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);
                        check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault releases context and borrowed communication memory");
                        check(probe.load(argv[1]),"fresh process loads after graphics-context teardown");const auto result=probe.run(500000);
                        check(result.stop==Machine::Stop::Exited && !result.exitCode && probe.ram.read64(symbols.at("gpu_as_report")+144)==1 && probe.ram.read64(symbols.at("gpu_as_report")+256+144)==2,"fresh process has no old context and resets object identities");break;
                    }
                }
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }
            check(captured,"original context boundary is reached for teardown checks");
        }
    }
    std::cout<<"GPU context instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
