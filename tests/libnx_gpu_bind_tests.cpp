#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static Ipc::Buffer ioctlPacket(uint32_t fd,uint32_t request,uint64_t source){
    Ipc::Buffer b{};const auto size=(request>>16)&0x3FFF;put(b,0,0x01110004);put(b,4,0xC0B);put(b,16,size);put(b,20,uint32_t(source));if(request>>31){put(b,28,size);put(b,32,uint32_t(source));}put(b,48,0x49434653);put(b,56,1);put(b,64,fd);put(b,68,request);return b;
}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;const bool baseline=argc==4 && std::string(argv[3])=="--baseline";if(argc==4 && !baseline)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original binding client exits normally without rendering");
        for(unsigned iteration=0;iteration<2;++iteration){const auto base=symbols.at("gpu_as_report")+iteration*256;auto get=[&](unsigned i){return m.ram.read64(base+i*8);};
            check(!get(0) && !get(1) && get(2) && !get(3) && get(4) && !get(5) && get(6),"original address-space, Nvmap and channel creation succeeds");
            check(get(7)==(baseline?Ipc::UnknownCommand:0) && !get(8),"BindChannel and the later bounded GPFIFO allocation succeed");
            check(get(9)==(baseline?0:iteration+1) && !get(10),"later GPFIFO allocation publishes only a fresh local fence identity with value zero");
            check(get(11)==UINT32_MAX && !get(12) && get(13)==UINT32_MAX && !get(14) && get(15)==UINT32_MAX && !get(16),"original teardown closes channel before address space and Nvmap");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"binding behavior is independent of execution slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000;
        unsigned binds=0;uint32_t clone=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;
                auto send=[&](const Ipc::Buffer& p,uint32_t endpoint=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(endpoint)m.cpu.writeRegister(0,endpoint);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                Ipc::NvIoctl plan;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && plan.request==0x40044101){
                    ++binds;const auto channel=m.ram.read32(plan.source);const auto mapFd=uint32_t(m.ram.read64(symbols.at("gpu_as_report")+(binds-1)*256+32));
                    check(plan.size==4 && !plan.destination,"original BindChannel carries one write-only channel FD");
                    auto open=[&](std::string_view path){m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());Ipc::Buffer b{};put(b,0,0x00100004);put(b,4,8);put(b,8,uint32_t(path.size()));put(b,12,uint32_t(scratch));put(b,32,0x49434653);check(!send(b),"probe endpoint opens");return m.ram.read32(ipc+32);};
                    auto close=[&](uint32_t fd,uint32_t endpoint=0){Ipc::Buffer b{};put(b,0,4);put(b,4,9);put(b,16,0x49434653);put(b,24,2);put(b,32,fd);return send(b,endpoint);};
                    auto bind=[&](uint32_t as,uint32_t ch,uint32_t ep=0){m.ram.write32(scratch+256,ch);return send(ioctlPacket(as,0x40044101,scratch+256),ep);};
                    auto init=[&](uint32_t as){for(size_t i=0;i<40;i+=4)m.ram.write32(scratch+512+i,0);m.ram.write32(scratch+512,1);m.ram.write32(scratch+520,0x10000);return send(ioctlPacket(as,0x40284109,scratch+512));};
                    for(const auto fd:{0U,UINT32_MAX,plan.fd,mapFd})check(bind(plan.fd,fd)==Process::InvalidHandle,"binding rejects missing and wrong-type channel descriptors");
                    check(bind(mapFd,channel)==Ipc::UnknownCommand && bind(channel,channel)==Ipc::UnknownCommand,"binding cannot execute on Nvmap or channel endpoints");
                    const auto otherAs=open("/dev/nvhost-as-gpu"),otherChannel=open("/dev/nvhost-gpu");
                    check(bind(otherAs,channel)==Ipc::NotInitialized,"address space must initialize before binding");check(!init(otherAs),"second address space initializes");
                    check(bind(plan.fd,otherChannel)==Process::InvalidState,"channel requires its Nvmap association before AS binding");
                    m.ram.write32(scratch+256,mapFd);check(!send(ioctlPacket(otherChannel,0x40044801,scratch+256)),"second channel associates Nvmap");
                    auto binding=ioctlPacket(otherAs,0x40044101,scratch+256);m.ram.write32(scratch+256,otherChannel);
                    auto bad=binding;put(bad,16,3);check(send(bad)==Ipc::InvalidArguments,"BindChannel rejects incorrect descriptor size");
                    bad=binding;put(bad,28,4);put(bad,32,uint32_t(scratch+256));check(send(bad)==Ipc::InvalidArguments,"BindChannel requires a null receive descriptor");
                    m.ram.writeBytes(ipc,binding.data(),binding.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed reply cannot publish AS binding");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(!send(binding,clone),"clone can bind after protected-reply rollback");
                    check(bind(plan.fd,otherChannel)==Ipc::AlreadyInitialized && bind(otherAs,otherChannel)==Ipc::AlreadyInitialized,"one channel cannot bind twice or migrate to another AS");
                    const auto peer=open("/dev/nvhost-gpu");m.ram.write32(scratch+256,mapFd);check(!send(ioctlPacket(peer,0x40044801,scratch+256)) && !bind(otherAs,peer,clone),"independent channels can share one initialized address space");
                    check(close(otherAs)==Process::InvalidState,"a bound channel retains its address space");
                    Ipc::Buffer closing{};put(closing,0,4);put(closing,4,9);put(closing,16,0x49434653);put(closing,24,2);put(closing,32,otherChannel);
                    m.ram.writeBytes(ipc,closing.data(),closing.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed channel Close retains AS binding");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(close(otherAs)==Process::InvalidState && !close(otherChannel,clone),"successful channel Close releases its AS association");
                    check(close(otherAs)==Process::InvalidState && !close(peer),"closing one channel preserves retention by another channel");
                    check(bind(otherAs,otherChannel)==Process::InvalidHandle && !close(otherAs),"closed channel cannot be rebound and independent AS retires");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && binds==2,"both original binding lifetimes finish after injected probes");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;bool captured=false;uint64_t borrowed=0;
            for(unsigned step=0;step<500000;++step){
                const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);
                if(op==0xD4000421){
                    Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::NvIoctl plan;
                    if(!Ipc::parseNvIoctl(p,probe.bus,local,plan) && plan.request==0x40044101){
                        captured=true;const auto context=probe.cpu.captureContext();probe.run(1);check(!probe.ram.read32(local+24),"original channel binds before teardown probe");
                        const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()),"failed reload preserves live AS binding");
                        Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,plan.fd);
                        probe.ram.writeBytes(local,close.data(),close.size());probe.cpu.restoreContext(context);probe.run(1);check(probe.ram.read32(local+24)==Process::InvalidState,"failed reload preserves channel retention of its address space");
                        const auto code=Process::HeapBase+0x180000;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);
                        check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault drops bindings and restores borrowed communication memory");
                        check(probe.load(argv[1]),"fresh process loads after bound teardown");const auto result=probe.run(500000);
                        check(result.stop==Machine::Stop::Exited && !result.exitCode && probe.ram.read64(symbols.at("gpu_as_report")+16)==1 && !probe.ram.read64(symbols.at("gpu_as_report")+56),"fresh process has reset AS identities and no stale channel binding");break;
                    }
                }
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }
            check(captured,"original BindChannel boundary is reached for teardown checks");
        }
    }
    std::cout<<"GPU binding instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
