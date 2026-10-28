#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include "gpu_va.hpp"
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
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original mapping client exits normally without rendering");
        for(unsigned iteration=0;iteration<2;++iteration){const auto base=symbols.at("gpu_as_report")+iteration*512;auto get=[&](unsigned i){return m.ram.read64(base+i*8);};
            check(!get(0) && !get(1) && get(2) && !get(3) && !get(4) && get(5) && get(6),"original address-space and Nvmap initialization bind CPU backing");
            if(baseline)check(get(7)==Ipc::UnknownCommand && get(8)==UINT64_MAX,"first original MapBufferEx reaches the unsupported baseline boundary");
            else{
                check(!get(7) && get(8)==Gpu::AddressSpace::Base && get(9)==Process::InvalidState && !get(10) && get(11)==get(8)+0x20000,"automatic aliases have separate addresses and pin their Nvmap object");
                check(!get(12) && get(13)==Process::InvalidAddress && get(14)==Process::InvalidState && !get(15),"one unmap preserves other aliases and duplicate unmap fails");
                check(!get(16) && get(17)==Gpu::AddressSpace::Base && !get(18) && get(19)==Process::InvalidState && !get(20) && !get(21) && get(22)==Process::InvalidAddress,"fixed map requires a reservation, blocks Free and leaves the range after Unmap");
                check(!get(23) && get(24)==Gpu::AddressSpace::Base && !get(25) && !get(26) && !get(27),"original explicit subrange maps the second backing page");
                check(!get(28) && get(29)==Gpu::AddressSpace::Base,"a live automatic mapping is left for Close teardown");
            }
            check(get(30)==0x1122334455667788ULL && get(31)==0x8877665544332211ULL && get(32)==0x0123456789abcdefULL,"mapping operations leave original CPU backing bytes unchanged");
            check(get(33)==UINT32_MAX && !get(34) && get(35)==UINT32_MAX && !get(36) && !get(37) && !get(38),"original AS Close releases mappings before Nvmap and service cleanup");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"mapping execution is independent of slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;const auto scratch=Process::HeapBase+0x170000;
        uint64_t backing=0;uint32_t mapFd=0,asFd=0,object=0,clone=0;unsigned maps=0,unmaps=0,nvFrees=0,asCloses=0;bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false,mapping=false,freeing=false,closing=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();
                cloning=read(packet,0)==5 && read(packet,24)==4;
                auto send=[&](const Ipc::Buffer& message,uint32_t endpoint=0){m.ram.writeBytes(ipc,message.data(),message.size());m.cpu.restoreContext(context);if(endpoint)m.cpu.writeRegister(0,endpoint);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                auto freeObject=[&](uint32_t id){for(size_t i=0;i<24;i+=4)m.ram.write32(scratch+i,0);m.ram.write32(scratch,id);return send(ioctlPacket(mapFd,0xC0180105,scratch));};
                Ipc::NvIoctl plan;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan)){
                    if(plan.request==0xC0200104){backing=m.ram.read64(plan.source+24);mapFd=plan.fd;object=m.ram.read32(plan.source);}
                    if(plan.request==0xC0180105){++nvFrees;freeing=true;}
                    if(plan.request==0xC0284106){
                        ++maps;mapping=true;asFd=plan.fd;
                        if(maps==1){
                            auto map=[&](uint32_t id,uint32_t flags=4,uint64_t offset=0,uint64_t bytes=0,uint64_t va=0,uint32_t page=0x10000,uint32_t kind=0,uint32_t endpoint=0){
                                m.ram.write32(scratch,flags);m.ram.write32(scratch+4,kind);m.ram.write32(scratch+8,id);m.ram.write32(scratch+12,page);m.ram.write64(scratch+16,offset);m.ram.write64(scratch+24,bytes);m.ram.write64(scratch+32,va);
                                const auto code=send(ioctlPacket(asFd,0xC0284106,scratch),endpoint);if(code)check(m.ram.read64(scratch+32)==va,"failed MapBufferEx does not publish an output address");return code;
                            };
                            auto unmap=[&](uint64_t va,uint32_t endpoint=0){m.ram.write64(scratch,va);return send(ioctlPacket(asFd,0xC0084105,scratch),endpoint);};
                            std::array<uint8_t,40> original{};for(size_t i=0;i<original.size();++i)original[i]=m.ram.read8(plan.source+i);
                            m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected original Map reply cannot publish mapping or reservation");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                            for(size_t i=0;i<original.size();++i)check(m.ram.read8(plan.source+i)==original[i],"late Map reply failure preserves every input/output byte");
                            for(const auto id:{0U,UINT32_MAX})check(map(id)==Process::InvalidHandle,"unknown Nvmap identities cannot be mapped");
                            for(const auto flags:{2U,8U,UINT32_MAX})check(map(object,flags)==Ipc::InvalidArguments,"modify and unsupported mapping flags are rejected");
                            check(map(object,4,0,0,0,4096)==Ipc::InvalidArguments && map(object,4,0,0,0,0x10000,1)==Ipc::InvalidArguments,"page size and unsupported kind are rejected");
                            for(const auto offset:{uint64_t(1),uint64_t(0x20000),UINT64_MAX})check(map(object,4,offset)==Ipc::InvalidArguments,"misaligned, empty and overflowing backing subranges fail");
                            check(map(object,4,0,0x30000)==Ipc::InvalidArguments && map(object,4,0,UINT64_MAX)==Ipc::InvalidArguments && map(object,4,0,1)==Ipc::InvalidArguments,"mapping size must fit backing and page units");
                            check(map(object,4,0,0,Gpu::AddressSpace::Base)==Ipc::InvalidArguments && map(object,5,0,0,Gpu::AddressSpace::Base)==Process::InvalidAddress,"automatic offset fields and unreserved fixed addresses are rejected");
                            m.ram.setPermissions(backing,0x10000,Memory::None);check(map(object)==Ipc::InvalidBuffer,"inaccessible backing cannot be mapped");
                            check(!map(object,4,0x10000,0x10000),"a readable subrange does not require unrelated backing pages to be readable");const auto partial=m.ram.read64(scratch+32);
                            std::array<uint8_t,8> subrange{};
                            check(m.process().readGpuMemory(m.bus,uint32_t(context.registers[0]),asFd,Gpu::VirtualAddress{partial},subrange) && subrange[0]==0x11 && subrange[7]==0x88,"partial mapping reads the object-relative second page while the first page is inaccessible");
                            check(!unmap(partial),"subrange probe unmaps");m.ram.setPermissions(backing,0x10000,Memory::Read|Memory::Write);
                            m.ram.setPermissions(backing+0x10000,0x10000,Memory::Read);check(map(object)==Ipc::InvalidBuffer,"the writable mapping profile rejects read-only backing");m.ram.setPermissions(backing+0x10000,0x10000,Memory::Read|Memory::Write);
                            // A separate object proves failed Map never pins backing.
                            m.ram.write32(scratch,0x10000);m.ram.write32(scratch+4,0);check(!send(ioctlPacket(mapFd,0xC0080101,scratch)),"probe Nvmap object is created");const auto extra=m.ram.read32(scratch+4);
                            for(size_t i=0;i<32;i+=4)m.ram.write32(scratch+i,0);m.ram.write32(scratch,extra);m.ram.write32(scratch+8,1);m.ram.write32(scratch+12,0x10000);m.ram.write64(scratch+24,Process::HeapBase+0x190000);check(!send(ioctlPacket(mapFd,0xC0200104,scratch)),"probe Nvmap object binds independent CPU backing");
                            map(extra,2);m.ram.write32(scratch,4);const auto extraMap=ioctlPacket(asFd,0xC0284106,scratch);m.ram.writeBytes(ipc,extraMap.data(),extraMap.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected probe Map fails after planning");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                            check(!freeObject(extra),"a failed Map does not retain its Nvmap object");
                            std::array<uint64_t,Gpu::AddressSpace::Limit> addresses{};for(size_t i=0;i<addresses.size();++i){check(!map(object,4,0,0,0,0x10000,0,i%2?clone:0),"root and clone share mapping ownership");addresses[i]=m.ram.read64(scratch+32);check(addresses[i]==Gpu::AddressSpace::Base+i*0x20000,"failed mappings consume no GPU virtual ranges");}
                            check(map(object)==Process::OutOfResource && freeObject(object)==Process::InvalidState,"mapping quota and backing retention are enforced");
                            const auto endpoint=uint32_t(context.registers[0]);
                            std::array<uint8_t,8> data{},sentinel{};sentinel.fill(0xA5);
                            auto readGpu=[&](uint64_t va,uint32_t ep,uint32_t fd){return m.process().readGpuMemory(m.bus,ep,fd,Gpu::VirtualAddress{va},data);};
                            auto writeGpu=[&](uint64_t va){return m.process().writeGpuMemory(m.bus,endpoint,asFd,Gpu::VirtualAddress{va},data);};
                            check(readGpu(addresses[0],endpoint,asFd) && data[0]==0x88 && data[7]==0x11,"GPU reads resolve original CPU backing markers");
                            check(readGpu(addresses[1]+0x10000,clone,asFd) && data[0]==0x11 && data[7]==0x88,"cloned endpoint resolves alias and second-page offsets");
                            check(readGpu(addresses[0]+0x1FFF8,endpoint,asFd) && data[0]==0xEF && data[7]==0x01,"GPU reads reach the last complete backing word");
                            const auto marker=m.ram.read64(backing+123);data.fill(0x5A);
                            check(writeGpu(addresses[0]+123) && m.ram.read64(backing+123)==0x5A5A5A5A5A5A5A5AULL,"unaligned GPU write changes CPU-visible backing");
                            data.fill(0);check(readGpu(addresses[1]+123,clone,asFd) && data[0]==0x5A && data[7]==0x5A,"alias immediately observes synchronous GPU writes");
                            m.ram.write64(backing+123,marker);
                            for(const auto va:{backing,UINT64_MAX,addresses.back()+0x20000,addresses[0]+0x1FFFC}){
                                data=sentinel;check(!readGpu(va,endpoint,asFd) && data==sentinel,"invalid and cross-mapping reads preserve the entire host output");
                                const auto last=m.ram.read64(backing+0x1FFF8);check(!writeGpu(va) && m.ram.read64(backing+0x1FFF8)==last,"invalid GPU writes preserve backing bytes");
                            }
                            data=sentinel;check(!readGpu(addresses[0],0,asFd) && !readGpu(addresses[0],endpoint,mapFd) && !readGpu(addresses[0],endpoint,UINT32_MAX) && data==sentinel,"GPU access requires the live owning NV endpoint and address-space descriptor");
                            Memory foreignRam(m.ram.size());Bus foreignBus(foreignRam);
                            check(!m.process().readGpuMemory(foreignBus,endpoint,asFd,Gpu::VirtualAddress{addresses[0]},data) && !m.process().writeGpuMemory(foreignBus,endpoint,asFd,Gpu::VirtualAddress{addresses[0]},data),"a GPU mapping cannot be used with another machine's RAM");
                            check(!m.process().readGpuMemory(m.bus,endpoint,asFd,Gpu::VirtualAddress{addresses[0]},std::span<uint8_t>{}) && !m.process().writeGpuMemory(m.bus,endpoint,asFd,Gpu::VirtualAddress{addresses[0]},std::span<const uint8_t>{}),"zero-byte GPU access is rejected");
                            m.ram.setPermissions(backing+0x1000,0x1000,Memory::Read);
                            data=sentinel;const auto before=m.ram.read64(backing+0xFFC);
                            check(readGpu(addresses[0]+0xFFC,endpoint,asFd) && !writeGpu(addresses[0]+0xFFC) && m.ram.read64(backing+0xFFC)==before,"write preflight rejects a protected second CPU page without a partial write");
                            m.ram.setPermissions(backing+0x1000,0x1000,Memory::None);data=sentinel;
                            check(!readGpu(addresses[0]+0xFFC,endpoint,asFd) && data==sentinel,"read preflight rejects inaccessible second page without partial output");
                            m.ram.setPermissions(backing+0x1000,0x1000,Memory::Read|Memory::Write);
                            Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,mapFd);check(send(close)==Process::InvalidState,"Nvmap device cannot close while objects are GPU-mapped");
                            for(size_t i=0;i<addresses.size();++i)check(!unmap(addresses[i],i%2?clone:0),"all aliases unmap across shared endpoints");
                            data=sentinel;check(!readGpu(addresses[0],endpoint,asFd) && data==sentinel && !writeGpu(addresses[0]),"unmapping immediately revokes synchronous GPU memory access");
                            m.ram.writeBytes(plan.source,original.data(),original.size());
                        }
                    }else if(plan.request==0xC0084105){
                        ++unmaps;if((unmaps-1)%5!=1){
                            const auto va=m.ram.read64(plan.source);m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected Unmap reply cannot drop mapping or reservation");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                            m.ram.write64(scratch,va);m.ram.write32(scratch+8,unmaps%5==0?1:2);m.ram.write32(scratch+12,0x10000);check(send(ioctlPacket(plan.fd,0xC0104103,scratch))==Process::InvalidState,"failed Unmap leaves its reservation mapped and unfreeable");
                        }
                    }
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
                Ipc::Layout layout;
                if(asFd && !Ipc::parseBuffers(packet,&m.bus,ipc,layout) && layout.command==2 && layout.payloadSize==4 && read(packet,size_t(layout.payload))==asFd){
                    closing=true;++asCloses;m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected AS Close cannot release live mappings");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    check(freeObject(object)==Process::InvalidState,"failed AS Close preserves Nvmap retention");m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(mapping)check(!m.ram.read32(ipc+24),"each original mapping succeeds after host failure probes");if(freeing)check(m.ram.read32(ipc+24)==(nvFrees%3?Process::InvalidState:0),"original Nvmap frees are blocked until AS teardown releases all references");
            if(closing){std::array<uint8_t,8> bytes{};check(!m.process().readGpuMemory(m.bus,clone,asFd,Gpu::VirtualAddress{Gpu::AddressSpace::Base},bytes) && !m.process().writeGpuMemory(m.bus,clone,asFd,Gpu::VirtualAddress{Gpu::AddressSpace::Base},bytes),"address-space Close revokes access through cloned endpoints");}
            if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && maps==10 && unmaps==10 && nvFrees==6 && asCloses==2,"actual original mapping/unmap and lifetime requests complete");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto local=probe.process().mainThread().localRegion;
            uint64_t borrowed=0;uint32_t cloneHandle=0,mapDevice=0;bool captured=false;
            for(unsigned step=0;step<500000;++step){
                const auto op=probe.ram.fetch32(probe.cpu.getPC());if(op==0xD40002A1)borrowed=probe.cpu.readRegister(1);bool cloningNow=false;
                if(op==0xD4000421){
                    Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=probe.ram.read8(local+i);cloningNow=read(packet,0)==5 && read(packet,24)==4;Ipc::NvIoctl plan;
                    if(!Ipc::parseNvIoctl(packet,probe.bus,local,plan)){
                        if(plan.request==0xC0200104)mapDevice=plan.fd;
                        if(plan.request==0xC0284106){
                            captured=true;const auto context=probe.cpu.captureContext();const auto root=uint32_t(probe.cpu.readRegister(0));const auto id=probe.ram.read32(plan.source+8);
                            probe.run(1);check(!probe.ram.read32(local+24) && probe.ram.read64(plan.source+32)==Gpu::AddressSpace::Base,"live mapping is captured before teardown");
                            const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()),"failed reload is reported with a live mapping");
                            const auto data=Process::HeapBase+0x170000,code=data+4096;for(size_t i=0;i<24;i+=4)probe.ram.write32(data+i,0);probe.ram.write32(data,id);const auto freePacket=ioctlPacket(mapDevice,0xC0180105,data);
                            auto pinned=[&](){probe.ram.writeBytes(local,freePacket.data(),freePacket.size());probe.cpu.restoreContext(context);probe.run(1);return probe.ram.read32(local+24)==Process::InvalidState;};
                            check(pinned(),"failed reload preserves backing retention");
                            probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,0xD40002C1);probe.cpu.restoreContext(context);probe.cpu.setPC(code);probe.cpu.writeRegister(0,cloneHandle);probe.run(1);check(!probe.cpu.readRegister(0),"one cloned kernel endpoint can close while the owning driver remains live");
                            probe.cpu.restoreContext(context);probe.cpu.setPC(code);probe.cpu.writeRegister(0,root);probe.run(1);check(probe.cpu.readRegister(0)==Process::InvalidState && pinned(),"last driver endpoint cannot discard GPU-mapped Nvmap backing");
                            probe.ram.write32(code,exitNormally?0xD40000E1:0);probe.cpu.setPC(code);const auto stopped=probe.run(1);
                            check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && probe.ram.canAccess(borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault releases mapped driver state and borrowed communication memory");
                            std::array<uint8_t,8> retired{};retired.fill(0xA5);const auto unchanged=retired;
                            check(!probe.process().readGpuMemory(probe.bus,root,plan.fd,Gpu::VirtualAddress{Gpu::AddressSpace::Base},retired) && retired==unchanged && !probe.process().writeGpuMemory(probe.bus,root,plan.fd,Gpu::VirtualAddress{Gpu::AddressSpace::Base},retired),"exit/fault revokes GPU access without mutating host output");
                            check(probe.load(argv[1]),"fresh load succeeds after mapped teardown");const auto result=probe.run(500000);check(result.stop==Machine::Stop::Exited && !result.exitCode && probe.ram.read64(symbols.at("gpu_as_report")+64)==Gpu::AddressSpace::Base,"fresh process inherits no mappings or backing references");break;
                        }
                    }
                }
                const auto result=probe.run(1);if(cloningNow)cloneHandle=probe.ram.read32(local+12);if(result.stop!=Machine::Stop::Budget)break;
            }
            check(captured,"mapping boundary is reached for lifetime checks");
        }
    }
    std::cout<<"GPU mapping instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
