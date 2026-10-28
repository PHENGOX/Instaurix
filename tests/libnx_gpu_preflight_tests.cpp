#include "machine.hpp"
#include "ipc_nv.hpp"
#include "gpu_submission.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static Ipc::Buffer ioctlPacket(uint32_t fd,uint32_t request,uint64_t source){Ipc::Buffer b{};const auto size=(request>>16)&0x3FFF;put(b,0,0x01110004);put(b,4,0xC0B);put(b,16,size);put(b,20,uint32_t(source));if(request>>31){put(b,28,size);put(b,32,uint32_t(source));}put(b,48,0x49434653);put(b,56,1);put(b,64,fd);put(b,68,request);return b;}
int main(int argc,char** argv){
    if(argc!=3)return 2;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original mapped preflight client exits without rendering");
        for(unsigned i=0;i<2;++i){const auto report=symbols.at("gpu_as_report")+i*512;auto get=[&](unsigned j){return m.ram.read64(report+j*8);};
            check(!get(0) && !get(1) && !get(3) && !get(5) && get(32) && !get(33) && get(34) && !get(35) && get(36)==Gpu::AddressSpace::Base,"original Nvmap object binds real CPU memory to the channel address space");
            check(!get(26) && get(27)==(Gpu::AddressSpace::Base|(2ULL<<42)) && get(28)==1 && get(29)==Ipc::UnknownCommand && get(30)==1 && !get(31),"valid mapped preflight remains unsupported work and preserves the client queue");
            check(get(8)==i+1 && !get(9) && get(10)==i+1,"preflight advances no fence and creates no additional graphics object");
            check(!get(37) && !get(38) && get(39)==0x11223344 && get(40)==0x55667788,"no admitted job pins the rejected command mapping or modifies its bytes");
            check(get(15)==Process::TimedOut && !get(16) && get(17)==UINT32_MAX && !get(18) && !get(19) && get(21)==UINT32_MAX && get(23)==UINT32_MAX && !get(24),"original event and channel cleanup complete without fake error or completion signals");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"mapped preflight is independent of execution slicing");
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000;unsigned kicks=0;uint32_t clone=0;bool exited=false;
    for(unsigned step=0;step<500000;++step){bool cloning=false;
        if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;Ipc::NvIoctl plan;
            if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && plan.command==11 && plan.request==0xC018481B){
                ++kicks;const auto report=symbols.at("gpu_as_report")+(kicks-1)*512;const auto as=uint32_t(m.ram.read64(report+16)),map=uint32_t(m.ram.read64(report+32));const auto backing=m.ram.read64(report+256),va=m.ram.read64(report+288);
                std::array<uint8_t,24> original{};for(size_t i=0;i<original.size();++i)original[i]=m.ram.read8(plan.source+i);std::array<uint8_t,8> entries{};for(size_t i=0;i<entries.size();++i)entries[i]=m.ram.read8(plan.extraSource+i);
                Gpu::SubmissionSnapshot snapshot;check(Gpu::prepareKickoff(original,entries,[&](Gpu::VirtualAddress addr,std::span<uint8_t> bytes){return m.process().readGpuMemory(m.bus,uint32_t(context.registers[0]),as,addr,bytes);},snapshot)==Gpu::PreflightResult::Success && snapshot.buffers.size()==1 && snapshot.buffers[0].bytes==std::vector<uint8_t>({0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55}),"snapshot reads the original Nvmap-backed words through GPU translation");
                m.ram.write32(backing,0xA5A5A5A5);check(snapshot.buffers[0].bytes[0]==0x44,"owned snapshot does not change with later CPU writes");m.ram.write32(backing,0x11223344);
                auto send=[&](const Ipc::Buffer& p,uint32_t ep=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(ep)m.cpu.writeRegister(0,ep);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                auto probe=packet;put(probe,28,uint32_t(scratch));put(probe,52,uint32_t(scratch));put(probe,40,uint32_t(scratch+512));
                auto reset=[&](){m.ram.write64(scratch,UINT64_MAX);m.ram.write32(scratch+8,1);m.ram.write32(scratch+12,4);m.ram.write64(scratch+16,0);m.ram.write64(scratch+512,va|(2ULL<<42));put(probe,36,8);};
                reset();check(send(probe)==Ipc::UnknownCommand && send(probe,clone)==Ipc::UnknownCommand,"valid mapped snapshot still never acknowledges a job through root or clone");
                for(const auto count:{0U,2U,2049U}){reset();m.ram.write32(scratch+8,count);check(send(probe)==Ipc::InvalidArguments,"entry count must be nonzero, bounded and match the extra array");}
                for(const auto flags:{0U,5U,260U}){reset();m.ram.write32(scratch+12,flags);check(send(probe)==Ipc::InvalidArguments,"unsupported synchronization flags are rejected");}
                for(const auto offset:{16U,20U}){reset();m.ram.write32(scratch+offset,1);check(send(probe)==Ipc::InvalidArguments,"unsupported input fences cannot be silently satisfied");}
                for(const uint64_t descriptor:std::array<uint64_t,3>{va,va|(1ULL<<40)|(1ULL<<42),(va+1)|(1ULL<<42)}){reset();m.ram.write64(scratch+512,descriptor);check(send(probe)==Ipc::InvalidArguments,"empty, reserved-control and unaligned descriptors fail");}
                for(const uint64_t address:std::array<uint64_t,3>{backing,va+0x10000,va+0xFFFC}){reset();m.ram.write64(scratch+512,address|(2ULL<<42));check(send(probe)==Process::InvalidAddress,"CPU addresses, unmapped ranges and cross-mapping command spans fail");}
                reset();m.ram.write64(scratch+512,va|(16385ULL<<42));check(send(probe)==Ipc::TransferLimit,"command snapshot bytes have a bounded aggregate limit");
                reset();m.ram.write32(scratch+8,2);put(probe,36,16);m.ram.write64(scratch+520,(va+0x10000)|(1ULL<<42));check(send(probe)==Process::InvalidAddress,"a late invalid command range rejects the complete preflight");m.ram.write64(scratch+520,va|(1ULL<<42));check(send(probe)==Ipc::UnknownCommand,"independent entries may read the same mapped buffer without queuing work");
                reset();m.ram.setPermissions(backing,4096,Memory::Read);check(send(probe)==Ipc::UnknownCommand,"command preflight requires read permission only");m.ram.setPermissions(backing,4096,Memory::None);check(send(probe)==Process::InvalidAddress,"inaccessible backing is rejected at read time");m.ram.setPermissions(backing,4096,Memory::Read|Memory::Write);
                reset();m.ram.writeBytes(ipc,probe.data(),probe.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected reply cannot admit a prepared snapshot");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                check(m.ram.read32(backing)==0x11223344 && m.ram.read32(backing+4)==0x55667788,"all rejected preflights leave command words unchanged");for(size_t i=0;i<original.size();++i)check(m.ram.read8(plan.source+i)==original[i],"original parameter and fence bytes remain unchanged");
                // The channel is tied to its own Nvmap device, not every object mapped into the AS.
                const std::string_view path="/dev/nvmap";m.ram.writeBytes(scratch+1024,reinterpret_cast<const uint8_t*>(path.data()),path.size());Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch+1024));put(open,32,0x49434653);check(!send(open),"independent Nvmap device opens");const auto foreign=m.ram.read32(ipc+32);
                const auto data=scratch+2048;auto io=[&](uint32_t fd,uint32_t request){return send(ioctlPacket(fd,request,data));};m.ram.write32(data,0x10000);m.ram.write32(data+4,0);check(!io(foreign,0xC0080101),"independent Nvmap object creates");const auto object=m.ram.read32(data+4);for(size_t i=0;i<32;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,object);m.ram.write32(data+8,1);m.ram.write32(data+12,0x10000);m.ram.write64(data+24,Process::HeapBase+0x1A0000);check(!io(foreign,0xC0200104),"independent Nvmap backing binds");
                for(size_t i=0;i<40;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,4);m.ram.write32(data+8,object);m.ram.write32(data+12,0x10000);check(!io(as,0xC0284106),"independent Nvmap object maps into the same AS");const auto foreignVa=m.ram.read64(data+32);reset();m.ram.write64(scratch+512,foreignVa|(1ULL<<42));check(send(probe)==Process::InvalidAddress,"channel cannot consume a different Nvmap device's object");
                m.ram.write64(data,foreignVa);check(!io(as,0xC0084105),"independent probe unmaps");for(size_t i=0;i<24;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,object);check(!io(foreign,0xC0180105),"rejected preflight does not retain the independent object");Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,foreign);check(!send(close),"independent Nvmap probe closes");
                (void)map;m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
            }
        }
        const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
    }
    check(exited && kicks==2,"both original mapped submission attempts complete after negative probes");std::cout<<"GPU mapped preflight instructions="<<instructions<<" failures="<<failures<<'\n';return failures?1:0;
}
