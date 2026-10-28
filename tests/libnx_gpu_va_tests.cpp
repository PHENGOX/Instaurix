#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include "gpu_va.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(8*i);return v;}
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
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original GPU VA client exits cleanly without rendering");
        for(unsigned iteration=0;iteration<2;++iteration){const auto base=symbols.at("gpu_as_report")+iteration*256;auto get=[&](unsigned i){return m.ram.read64(base+i*8);};
            check(!get(0) && !get(1) && get(2)==1 && get(3)==iteration+1 && get(4)==0x10000 && !get(5) && get(6)==UINT32_MAX && !get(7) && !get(8),"Create/Close retires both original address-space lifetimes");
            if(baseline)check(get(9)==Ipc::UnknownCommand && get(10)==UINT64_MAX,"original allocation reaches the explicitly unsupported baseline boundary");
            else{
                check(!get(9) && get(10)==Gpu::AddressSpace::Base && !get(11) && get(12)==Gpu::AddressSpace::Base+0x20000,"original allocation rounds a partial page and reserves distinct ranges");
                check(!get(13) && get(14)==Process::InvalidAddress && !get(15) && get(16)==get(10) && !get(17) && !get(18),"original Free rejects duplicates and freed holes are reused");
                check(!get(19) && get(20)==Process::InvalidState && !get(21) && get(22)==Ipc::InvalidArguments && get(23)==Process::InvalidAddress,"fixed overlap, zero-size and unknown-free errors are explicit");
                check(!get(24) && get(25)==Gpu::AddressSpace::Base && !m.bus.isRamRange(get(25),0x10000),"live reservations are GPU addresses, not CPU backing, and are discarded on Close");
            }
        }
        if(budget==500000)instructions=count;else check(count==instructions,"GPU VA execution is independent of slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;const auto scratch=Process::HeapBase+0x170000;
        unsigned allocations=0,frees=0;uint32_t clone=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);
                cloning=read(packet,0)==5 && read(packet,24)==4;
                Ipc::NvIoctl plan;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && (plan.request==0xC0184102 || plan.request==0xC0104103)){
                    const auto context=m.cpu.captureContext();
                    auto send=[&](const Ipc::Buffer& message,uint32_t endpoint=0){m.ram.writeBytes(ipc,message.data(),message.size());m.cpu.restoreContext(context);if(endpoint)m.cpu.writeRegister(0,endpoint);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                    auto reserve=[&](uint32_t fd,uint32_t pages,uint32_t flags,uint64_t align,uint32_t page=0x10000,uint32_t reserved=0,uint32_t endpoint=0){
                        m.ram.write32(scratch,pages);m.ram.write32(scratch+4,page);m.ram.write32(scratch+8,flags);m.ram.write32(scratch+12,reserved);m.ram.write64(scratch+16,align);
                        const auto code=send(ioctlPacket(fd,0xC0184102,scratch),endpoint);if(code)check(m.ram.read64(scratch+16)==align,"failed reservation does not write an output address");return code;
                    };
                    auto release=[&](uint64_t value,uint32_t pages,uint32_t endpoint=0){m.ram.write64(scratch,value);m.ram.write32(scratch+8,pages);m.ram.write32(scratch+12,0x10000);return send(ioctlPacket(plan.fd,0xC0104103,scratch),endpoint);};
                    if(plan.request==0xC0184102){
                        ++allocations;
                        if(allocations==1){
                            std::array<uint8_t,24> before{};for(size_t i=0;i<before.size();++i)before[i]=m.ram.read8(plan.source+i);
                            m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected IPC reply cannot commit a virtual reservation");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                            for(size_t i=0;i<before.size();++i)check(m.ram.read8(plan.source+i)==before[i],"late reply failure leaves original output bytes untouched");
                            for(const auto pages:{0U,UINT32_MAX})check(reserve(plan.fd,pages,0,0x10000)==Ipc::InvalidArguments,"zero or excessive page counts are rejected before arithmetic publication");
                            check(reserve(plan.fd,1,0,0x10000,4096)==Ipc::InvalidArguments && reserve(plan.fd,1,0,0x10000,0x10000,1)==Ipc::InvalidArguments,"page size and reserved padding are validated");
                            for(const auto flags:{2U,3U,UINT32_MAX})check(reserve(plan.fd,1,flags,0x10000)==Ipc::InvalidArguments,"sparse and unknown flags remain unsupported");
                            for(const auto align:{uint64_t(0),uint64_t(1),uint64_t(0x18000),UINT64_MAX})check(reserve(plan.fd,1,0,align)==Ipc::InvalidArguments,"invalid automatic alignments cannot reserve addresses");
                            for(const auto value:{uint64_t(0x200000),Gpu::AddressSpace::Base-Gpu::AddressSpace::PageSize,Gpu::AddressSpace::Base+Gpu::AddressSpace::Capacity,UINT64_MAX})check(reserve(plan.fd,1,1,value)==Ipc::InvalidArguments,"fixed requests cannot use CPU, out-of-range or overflowing addresses");
                            check(reserve(UINT32_MAX,1,0,0x10000)==Process::InvalidHandle,"foreign address-space descriptor is rejected");
                            reserve(plan.fd,0,0,0x10000);m.ram.write32(scratch,1);const auto raw=ioctlPacket(plan.fd,0xC0184102,scratch);m.ram.setPermissions(scratch,4096,Memory::Read);check(send(raw)==Ipc::InvalidBuffer,"unwritable allocation output cannot reserve a range");m.ram.setPermissions(scratch,4096,Memory::Read|Memory::Write);
                            std::array<uint64_t,Gpu::AddressSpace::Limit> ranges{};
                            for(size_t i=0;i<ranges.size();++i){check(!reserve(plan.fd,1,0,0x10000,0x10000,0,i%2?clone:0),"root and clone fill the same virtual reservation registry");ranges[i]=m.ram.read64(scratch+16);check(ranges[i]==Gpu::AddressSpace::Base+i*0x10000,"failed requests consume no virtual space");}
                            check(reserve(plan.fd,1,0,0x10000)==Process::OutOfResource,"per-address-space record quota is enforced");
                            m.ram.write64(scratch,ranges[0]);m.ram.write32(scratch+8,1);m.ram.write32(scratch+12,0x10000);const auto freePacket=ioctlPacket(plan.fd,0xC0104103,scratch);
                            m.ram.writeBytes(ipc,freePacket.data(),freePacket.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected Free reply cannot discard a reservation");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                            check(reserve(plan.fd,1,0,0x10000)==Process::OutOfResource,"failed Free preserves the occupied record quota");
                            for(size_t i=0;i<ranges.size();++i)check(!release(ranges[i],1,i%2?clone:0),"both endpoints release the original exact reservations");
                            check(!reserve(plan.fd,uint32_t(Gpu::AddressSpace::Capacity/0x10000),0,0x10000),"the entire local GPU VA region can be reserved without CPU backing");check(reserve(plan.fd,1,0,0x10000)==Process::OutOfResource,"virtual byte capacity is enforced");check(!release(Gpu::AddressSpace::Base,uint32_t(Gpu::AddressSpace::Capacity/0x10000)),"full-region release reclaims its capacity");
                            // Two descriptors may own the same GPU number in independent namespaces.
                            const std::string_view path="/dev/nvhost-as-gpu";m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch));put(open,32,0x49434653);
                            check(!send(open),"a second address-space descriptor opens");const auto other=m.ram.read32(ipc+32);
                            for(size_t i=0;i<40;i+=4)m.ram.write32(scratch+i,0);m.ram.write32(scratch,1);m.ram.write32(scratch+8,0x10000);check(!send(ioctlPacket(other,0x40284109,scratch)),"independent address space initializes");
                            check(!reserve(other,1,0,0x10000) && m.ram.read64(scratch+16)==Gpu::AddressSpace::Base && release(Gpu::AddressSpace::Base,1)==Process::InvalidAddress,"one descriptor cannot free another descriptor's identical GPU number");
                            Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,other);check(!send(close,clone),"closing a descriptor discards its unmapped reservations");
                            m.ram.writeBytes(plan.source,before.data(),before.size());
                        }
                    }else{
                        ++frees;if(frees==1){
                            const auto value=m.ram.read64(plan.source);const auto pages=m.ram.read32(plan.source+8);
                            m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"original Free rolls back when its IPC reply is protected");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                            check(reserve(plan.fd,pages,1,value)==Process::InvalidState,"failed original Free leaves its complete range occupied");
                        }
                    }
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && allocations==14 && frees==12 && !m.process().frameCount(),"instrumented original client completes all allocation/free lifetimes");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;uint64_t borrowed=0;bool captured=false;
            for(unsigned step=0;step<500000;++step){const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);
                if(op==0xD4000421){Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=probe.ram.read8(local+i);Ipc::NvIoctl plan;
                    if(!Ipc::parseNvIoctl(packet,probe.bus,local,plan) && plan.request==0xC0184102){captured=true;const auto context=probe.cpu.captureContext();probe.run(1);check(probe.ram.read64(plan.source+16)==Gpu::AddressSpace::Base,"live reservation is captured before teardown");
                        const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()),"failed reload is reported");probe.ram.write32(plan.source+8,1);probe.ram.write64(plan.source+16,Gpu::AddressSpace::Base);probe.ram.writeBytes(local,packet.data(),packet.size());probe.cpu.restoreContext(context);probe.run(1);check(probe.ram.read32(local+24)==Process::InvalidState,"failed load preserves occupied GPU VA ranges");
                        const auto code=Process::HeapBase+0x170000;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault retires live reservations and restores communication memory");
                        check(probe.load(argv[1]),"fresh reload succeeds after live-range teardown");auto result=probe.run(500000);check(result.stop==Machine::Stop::Exited && !result.exitCode && probe.ram.read64(symbols.at("gpu_as_report")+80)==Gpu::AddressSpace::Base,"fresh process inherits no GPU VA reservations");break;}}
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }check(captured,"original allocation boundary is reached for lifetime probes");
        }
    }
    std::cout<<"GPU VA instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
