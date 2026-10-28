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
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original allocation client exits normally without rendering");
        for(unsigned iteration=0;iteration<2;++iteration){const auto base=symbols.at("gpu_as_report")+iteration*256;auto get=[&](unsigned i){return m.ram.read64(base+i*8);};
            check(!get(0) && !get(1) && get(2) && !get(3) && get(4) && !get(5) && get(6) && !get(7),"original NV, AS, Nvmap, channel and binding succeed");
            check(get(8)==(baseline?Ipc::UnknownCommand:0) && get(9)==(baseline?0:iteration+1) && !get(10),"original GPFIFO allocation returns a fresh local fence identity with value zero");
            check(!get(17) && get(18)==(baseline?0:iteration+1),"later bounded graphics-context allocation returns its own local object identity");
            check(get(11)==UINT32_MAX && !get(12) && get(13)==UINT32_MAX && !get(14) && get(15)==UINT32_MAX && !get(16),"original teardown releases allocation and retained descriptors");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"allocation behavior is independent of slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000;
        unsigned allocations=0;uint32_t clone=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;
                auto send=[&](const Ipc::Buffer& p,uint32_t endpoint=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(endpoint)m.cpu.writeRegister(0,endpoint);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                Ipc::NvIoctl plan;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && plan.request==0xC020481A){
                    ++allocations;const auto report=symbols.at("gpu_as_report")+(allocations-1)*256;const auto as=uint32_t(m.ram.read64(report+16)),map=uint32_t(m.ram.read64(report+32));
                    check(plan.size==32 && plan.destination==plan.source && m.ram.read32(plan.source)==0x800 && m.ram.read32(plan.source+4)==1,"original allocation uses the audited 32-byte request");
                    auto close=[&](uint32_t fd){Ipc::Buffer b{};put(b,0,4);put(b,4,9);put(b,16,0x49434653);put(b,24,2);put(b,32,fd);return send(b);};
                    auto open=[&](){const std::string_view path="/dev/nvhost-gpu";m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());Ipc::Buffer b{};put(b,0,0x00100004);put(b,4,8);put(b,8,uint32_t(path.size()));put(b,12,uint32_t(scratch));put(b,32,0x49434653);check(!send(b),"probe channel opens");return m.ram.read32(ipc+32);};
                    auto allocate=[&](uint32_t fd,uint32_t ep=0){for(size_t i=0;i<32;i+=4)m.ram.write32(scratch+512+i,0);m.ram.write32(scratch+512,0x800);m.ram.write32(scratch+516,1);return send(ioctlPacket(fd,0xC020481A,scratch+512),ep);};
                    check(allocate(0)==Process::InvalidHandle && allocate(UINT32_MAX)==Process::InvalidHandle,"invalid allocation descriptors fail");
                    check(allocate(as)==Ipc::UnknownCommand && allocate(map)==Ipc::UnknownCommand,"GPFIFO cannot allocate on AS or Nvmap endpoints");
                    const auto unbound=open();check(allocate(unbound)==Process::InvalidState,"allocation requires channel associations");m.ram.write32(scratch+256,map);check(!send(ioctlPacket(unbound,0x40044801,scratch+256)),"probe channel binds Nvmap");check(allocate(unbound)==Process::InvalidState,"Nvmap alone cannot allocate GPFIFO");check(!close(unbound),"unallocated probe closes");
                    std::array<uint32_t,8> channels{};
                    uint32_t lastFence=allocations==1?0:uint32_t(m.ram.read64(report-256+72));
                    for(size_t i=0;i<channels.size();++i){
                        channels[i]=open();m.ram.write32(scratch+256,map);check(!send(ioctlPacket(channels[i],0x40044801,scratch+256)),"quota channel associates Nvmap");m.ram.write32(scratch+256,channels[i]);check(!send(ioctlPacket(as,0x40044101,scratch+256)),"quota channel binds AS");
                        if(!i){
                            for(size_t at=0;at<32;at+=4)m.ram.write32(scratch+512+at,0);m.ram.write32(scratch+512,0x800);m.ram.write32(scratch+516,1);auto p=ioctlPacket(channels[i],0xC020481A,scratch+512);
                            for(const auto offset:{0U,4U,8U,20U,24U,28U}){const auto old=m.ram.read32(scratch+512+offset);m.ram.write32(scratch+512+offset,old+1);check(send(p)==Ipc::InvalidArguments,"unsupported allocation inputs are rejected");m.ram.write32(scratch+512+offset,old);}
                            m.ram.write32(scratch+524,0xA5A5A5A5);m.ram.write32(scratch+528,0x5A5A5A5A);
                            auto bad=p;put(bad,28,31);check(send(bad)==Ipc::InvalidArguments,"allocation rejects a mismatched receive size");
                            bad=p;put(bad,32,uint32_t(scratch+4096));m.ram.setPermissions(scratch+4096,4096,Memory::Read);check(send(bad)==Ipc::InvalidBuffer,"allocation requires writable destination memory");m.ram.setPermissions(scratch+4096,4096,Memory::Read|Memory::Write);
                            m.ram.writeBytes(ipc,p.data(),p.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"late reply failure cannot publish queue or fence");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                            check(m.ram.read32(scratch+524)==0xA5A5A5A5 && m.ram.read32(scratch+528)==0x5A5A5A5A,"failed allocation preserves both output words");
                        }
                        check(!allocate(channels[i],i%2?clone:0),"root and clone share bounded queue allocation");
                        check(m.ram.read32(scratch+512)==0x800 && m.ram.read32(scratch+516)==1 && !m.ram.read32(scratch+520) && !m.ram.read64(scratch+532) && !m.ram.read32(scratch+540),"successful allocation changes only the two fence output words");
                        check(m.ram.read32(scratch+524)==lastFence+1 && !m.ram.read32(scratch+528),"failed allocations consume no fence identities and successful allocation has value zero");lastFence=m.ram.read32(scratch+524);
                        check(allocate(channels[i])==Ipc::AlreadyInitialized,"channel cannot allocate twice");
                    }
                    check(allocate(plan.fd)==Process::OutOfResource,"eight live queues enforce the service quota");
                    Ipc::Buffer closing{};put(closing,0,4);put(closing,4,9);put(closing,16,0x49434653);put(closing,24,2);put(closing,32,channels[0]);
                    m.ram.writeBytes(ipc,closing.data(),closing.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"failed channel Close cannot release allocated queue storage");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(allocate(plan.fd)==Process::OutOfResource && close(as)==Process::InvalidState && close(map)==Process::InvalidState,"failed Close preserves queue quota and descriptor retention");
                    for(auto fd:channels)check(!close(fd),"channel Close releases queue quota and associations");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && allocations==2,"both original allocations succeed after quota cleanup");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;bool captured=false;uint64_t borrowed=0;
            for(unsigned step=0;step<500000;++step){
                const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);
                if(op==0xD4000421){
                    Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::NvIoctl plan;
                    if(!Ipc::parseNvIoctl(p,probe.bus,local,plan) && plan.request==0xC020481A){
                        captured=true;const auto context=probe.cpu.captureContext();probe.run(1);check(!probe.ram.read32(local+24),"original allocation succeeds before lifetime probe");
                        const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()),"failed reload leaves the live queue intact");
                        probe.ram.writeBytes(local,p.data(),p.size());probe.cpu.restoreContext(context);probe.run(1);check(probe.ram.read32(local+24)==Ipc::AlreadyInitialized,"failed reload preserves the allocated channel state");
                        const auto code=Process::HeapBase+0x180000;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);
                        check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault releases queues and borrowed communication memory");
                        check(probe.load(argv[1]),"fresh process loads after queue teardown");const auto result=probe.run(500000);
                        check(result.stop==Machine::Stop::Exited && !result.exitCode && probe.ram.read64(symbols.at("gpu_as_report")+72)==1 && probe.ram.read64(symbols.at("gpu_as_report")+256+72)==2,"fresh process has no stale quota and restarts the local fence identity namespace");break;
                    }
                }
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }
            check(captured,"original GPFIFO allocation boundary is reached for teardown checks");
        }
    }
    std::cout<<"GPU FIFO instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
