#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;const bool baseline=argc==4 && std::string(argv[3])=="--baseline";if(argc==4 && !baseline)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original channel client exits cleanly without rendering");
        for(unsigned iteration=0;iteration<2;++iteration){const auto base=symbols.at("gpu_as_report")+iteration*128;auto get=[&](unsigned i){return m.ram.read64(base+i*8);};
            check(!get(0) && !get(1) && get(2),"original NV and Nvmap initialization succeeds");
            check(get(3)==(baseline?Ipc::FileNotFound:0) && get(4)==(baseline?0:1) && get(5)==(baseline?UINT32_MAX:get(2)+1),"original channel creation reaches the expected boundary");
            check(!get(6),"later priority configuration succeeds after channel creation");
            check(!get(7) && get(8)==UINT32_MAX && !get(9) && get(10)==UINT32_MAX && !get(11),"original close, repeated close and driver exit retire state");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"channel execution is independent of slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000;
        unsigned binds=0;uint32_t clone=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();
                cloning=read(packet,0)==5 && read(packet,24)==4;
                auto send=[&](const Ipc::Buffer& p,uint32_t endpoint=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(endpoint)m.cpu.writeRegister(0,endpoint);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                Ipc::NvIoctl ioctl;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,ioctl) && ioctl.request==0x40044801){
                    ++binds;const auto mapFd=m.ram.read32(ioctl.source);
                    check(ioctl.size==4 && !ioctl.destination,"original SetNvmapFd sends one input descriptor and no output");
                    auto close=[&](uint32_t fd,uint32_t ep=0){Ipc::Buffer b{};put(b,0,4);put(b,4,9);put(b,16,0x49434653);put(b,24,2);put(b,32,fd);return send(b,ep);};
                    const std::string_view path="/dev/nvhost-gpu";m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());
                    Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch));put(open,32,0x49434653);
                    m.ram.writeBytes(ipc,open.data(),open.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                    check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected channel Open publishes no descriptor");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(!send(open),"probe channel opens");const auto channel=m.ram.read32(ipc+32);
                    auto bind=packet;put(bind,64,channel);put(bind,20,uint32_t(scratch+256));m.ram.write32(scratch+256,mapFd);
                    for(const auto fd:{0U,UINT32_MAX,ioctl.fd,channel}){m.ram.write32(scratch+256,fd);check(send(bind)==Process::InvalidHandle,"binding rejects absent and non-Nvmap descriptors");}
                    m.ram.write32(scratch+256,mapFd);auto wrong=bind;put(wrong,64,mapFd);check(send(wrong)==Ipc::UnknownCommand,"SetNvmapFd cannot run on a Nvmap device");
                    wrong=bind;put(wrong,28,4);put(wrong,32,uint32_t(scratch+256));check(send(wrong)==Ipc::InvalidArguments,"write-only binding rejects a receive descriptor");
                    wrong=bind;put(wrong,16,3);check(send(wrong)==Ipc::InvalidArguments,"binding rejects an incorrect input extent");
                    m.ram.writeBytes(ipc,bind.data(),bind.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                    check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected SetNvmapFd reply cannot publish a binding");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(!send(bind,clone),"binding succeeds through a clone after a protected reply failure");
                    check(send(bind)==Ipc::AlreadyInitialized && close(mapFd)==Process::InvalidState,"a bound channel cannot be rebound and retains Nvmap");
                    Ipc::Buffer closing{};put(closing,0,4);put(closing,4,9);put(closing,16,0x49434653);put(closing,24,2);put(closing,32,channel);
                    m.ram.writeBytes(ipc,closing.data(),closing.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                    check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected channel Close does not release the binding");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(close(mapFd)==Process::InvalidState && !close(channel,clone),"successful channel Close releases the retained descriptor");
                    check(send(bind)==Process::InvalidHandle && close(channel)==Process::InvalidHandle,"closed channel identities cannot bind or close twice");
                    std::array<uint32_t,Process::NvDescriptorLimit-2> quota{};
                    for(auto& fd:quota){check(!send(open),"channel shares the bounded NV descriptor quota");fd=m.ram.read32(ipc+32);}
                    check(send(open)==Process::OutOfResource,"channel Open enforces the descriptor quota");for(auto fd:quota)check(!close(fd),"unbound channels close without retaining Nvmap");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);
            if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && binds==2,"both original channel lifetimes finish after failure and quota probes");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;bool captured=false;uint64_t borrowed=0;
            for(unsigned step=0;step<500000;++step){
                const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);
                if(op==0xD4000421){
                    Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::NvIoctl plan;
                    if(!Ipc::parseNvIoctl(p,probe.bus,local,plan) && plan.request==0x40044801){
                        captured=true;const auto context=probe.cpu.captureContext();const auto fd=probe.ram.read32(plan.source);probe.run(1);check(!probe.ram.read32(local+24),"live channel binds before teardown probe");
                        const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()),"failed reload preserves the current process");
                        Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,fd);
                        probe.ram.writeBytes(local,close.data(),close.size());probe.cpu.restoreContext(context);probe.run(1);check(probe.ram.read32(local+24)==Process::InvalidState,"failed reload preserves the channel's Nvmap retention");
                        const auto code=Process::HeapBase+0x180000;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);
                        check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"channel teardown restores borrowed communication memory on exit/fault");
                        check(probe.load(argv[1]),"fresh process loads after channel teardown");const auto result=probe.run(500000);
                        check(result.stop==Machine::Stop::Exited && !result.exitCode && probe.ram.read64(symbols.at("gpu_as_report")+40)==2,"fresh process starts with no retained channel and reset descriptor namespace");break;
                    }
                }
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }
            check(captured,"original binding boundary is reached for teardown checks");
        }
    }
    std::cout<<"GPU channel instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
