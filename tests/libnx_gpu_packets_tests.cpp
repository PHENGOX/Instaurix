#include "machine.hpp"
#include "ipc_nv.hpp"
#include "gpu_packets.hpp"
#include <fstream>
#include <iostream>
#include <map>
#ifdef GPU_ALLOCATION_FAILURE_TESTS
#include <cstdlib>
#include <new>
namespace { long failAfter=-1;size_t rejectedBytes=0; }
void* operator new(std::size_t bytes){
    // MSVC's iterator-debug proxies can allocate in noexcept default container constructors.
    // Exempt their size: this test injects failures into production payload/state/reply allocations.
    bool debugProxy=false;
#if defined(_MSC_VER) && _ITERATOR_DEBUG_LEVEL != 0
    debugProxy=bytes==sizeof(std::_Container_proxy);
#endif
    if(!debugProxy){if(failAfter==0){rejectedBytes=bytes;throw std::bad_alloc();}if(failAfter>0)--failAfter;}
    if(void* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
#endif
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static Ipc::Buffer ioctlPacket(uint32_t fd,uint32_t request,uint64_t source){Ipc::Buffer b{};const auto size=(request>>16)&0x3FFF;put(b,0,0x01110004);put(b,4,0xC0B);put(b,16,size);put(b,20,uint32_t(source));if(request>>31){put(b,28,size);put(b,32,uint32_t(source));}put(b,48,0x49434653);put(b,56,1);put(b,64,fd);put(b,68,request);return b;}
int main(int argc,char** argv){
    if(argc!=3)return 2;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original mapped packet client exits without GPU rendering");
        for(unsigned i=0;i<2;++i){const auto report=symbols.at("gpu_as_report")+i*512;auto get=[&](unsigned j){return m.ram.read64(report+j*8);};
            check(!get(0) && !get(1) && !get(3) && !get(5) && get(32) && !get(33) && get(34) && !get(35) && get(36)==Gpu::AddressSpace::Base,"original channel and command mapping initialize");
            check(!get(26) && get(27)==(Gpu::AddressSpace::Base|(3ULL<<42)) && !get(29) && get(28)==1 && !get(30) && !get(31),"original Kickoff accepts the supported reference stream and clears the client queue");
            check(get(8)==i+1 && !get(9) && !get(37) && !get(38) && get(39)==0x20010014 && get(40)==0x12345678 && !get(41),"reference execution preserves packet bytes and allocation fence while releasing mapping lifetimes");
            check(get(15)==Process::TimedOut && !get(16) && get(17)==UINT32_MAX && !get(18) && !get(19) && get(21)==UINT32_MAX && get(23)==UINT32_MAX && !get(24),"original cleanup completes without a fake completion or error signal");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"packet decoding is independent of execution slicing");
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000;unsigned kicks=0;uint32_t clone=0;bool exited=false;
    for(unsigned step=0;step<500000;++step){bool cloning=false;
        if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;Ipc::NvIoctl plan;
            if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && plan.command==11 && plan.request==0xC018481B){
                ++kicks;const auto report=symbols.at("gpu_as_report")+(kicks-1)*512;const auto as=uint32_t(m.ram.read64(report+16));const auto backing=m.ram.read64(report+256),va=m.ram.read64(report+288);
                std::array<uint8_t,24> parameters{};for(size_t i=0;i<parameters.size();++i)parameters[i]=m.ram.read8(plan.source+i);std::array<uint8_t,8> entries{};for(size_t i=0;i<entries.size();++i)entries[i]=m.ram.read8(plan.extraSource+i);
                Gpu::SubmissionSnapshot snapshot;const Gpu::CommandReader reader=[&](Gpu::VirtualAddress addr,std::span<uint8_t> bytes){return m.process().readGpuMemory(m.bus,uint32_t(context.registers[0]),as,addr,bytes);};Gpu::DecodedPushbuffer decoded;
                check(Gpu::prepareKickoff(parameters,entries,reader,snapshot)==Gpu::PreflightResult::Success && Gpu::decodePushbuffer(snapshot,decoded)==Gpu::DecodeResult::Success && decoded.methods.size()==1 && decoded.methods[0].method==0x50 && decoded.methods[0].value==0x12345678 && decoded.methods[0].subchannel==0 && decoded.dmaNops==1,"actual Nvmap-backed reference decodes to SET_REFERENCE and DMA_NOP");
                m.ram.write32(backing+4,0xA5A5A5A5);check(decoded.methods[0].value==0x12345678,"published trace does not alias live command memory");Gpu::SubmissionSnapshot changed;Gpu::DecodedPushbuffer changedTrace;check(Gpu::prepareKickoff(parameters,entries,reader,changed)==Gpu::PreflightResult::Success && Gpu::decodePushbuffer(changed,changedTrace)==Gpu::DecodeResult::Success && changedTrace.methods[0].value==0xA5A5A5A5,"fresh snapshot decodes current payload rather than hard-coded reference data");m.ram.write32(backing+4,0x12345678);
                const auto endpoint=uint32_t(context.registers[0]);
                auto view=[&](){return m.process().gpuChannelExecution(endpoint,plan.fd);};
                check(view()==std::optional<Process::GpuChannelExecution>{{0,false,0}},"each newly allocated channel begins with independent unwritten reference state");
                check(!m.process().gpuChannelExecution(endpoint,as) && !m.process().gpuChannelExecution(0,plan.fd),"execution diagnostics require a live typed owned channel");
                auto send=[&](const Ipc::Buffer& p,uint32_t ep=0){const auto before=view();const auto bindings=m.process().gpuChannelState(endpoint,plan.fd);m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(ep)m.cpu.writeRegister(0,ep);m.run(1);const auto code=m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);if(code)check(view()==before && m.process().gpuChannelState(endpoint,plan.fd)==bindings,"every failed submission preserves reference, bindings and committed count");return code;};
                auto probe=packet;put(probe,28,uint32_t(scratch));put(probe,52,uint32_t(scratch));put(probe,40,uint32_t(scratch+512));for(size_t i=0;i<parameters.size();++i)m.ram.write8(scratch+i,parameters[i]);
                auto reset=[&](){m.ram.write32(backing,0x20010014);m.ram.write32(backing+4,0x12345678);m.ram.write32(backing+8,0);m.ram.write32(scratch+8,1);put(probe,36,8);m.ram.write64(scratch+512,va|(3ULL<<42));};
                reset();check(!send(probe) && !send(probe,clone),"recognized packet streams execute through root and cloned service sessions");
                check(view()==std::optional<Process::GpuChannelExecution>{{0x12345678,true,2}} && m.process().gpuChannelExecution(clone,plan.fd)==view(),"root and clone share the channel register and exact committed submission count");
                check(!m.ram.read32(scratch+16) && !m.ram.read32(scratch+20),"no-increment submission returns its zero input fence unchanged");
                if(kicks==1)for(unsigned cycle=0;cycle<2;++cycle){
                    const auto mainState=view();const auto data=scratch+1024;
                    const std::string_view path="/dev/nvhost-gpu";m.ram.writeBytes(scratch+2048,reinterpret_cast<const uint8_t*>(path.data()),path.size());
                    Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch+2048));put(open,32,0x49434653);
                    check(!send(open),"independent execution channel opens");const auto peer=m.ram.read32(ipc+32);
                    m.ram.write32(data,uint32_t(m.ram.read64(report+32)));check(!send(ioctlPacket(peer,0x40044801,data)),"peer associates the same Nvmap owner");
                    m.ram.write32(data,peer);check(!send(ioctlPacket(as,0x40044101,data)),"peer binds the live address space");
                    for(unsigned i=0;i<32;++i)m.ram.write8(data+i,0);m.ram.write32(data,Process::GpfifoEntries);m.ram.write32(data+4,1);
                    check(!send(ioctlPacket(peer,0xC020481A,data)),"peer allocates an independent FIFO");
                    for(unsigned i=0;i<16;++i)m.ram.write8(data+i,0);m.ram.write32(data,0xB197);
                    check(!send(ioctlPacket(peer,0xC0104809,data)),"peer allocates its reference graphics context");
                    check(m.process().gpuChannelExecution(endpoint,peer)==std::optional<Process::GpuChannelExecution>{{0,false,0}},"reused descriptor slots start with fresh channel execution state");
                    reset();m.ram.write32(backing+4,0xBEEF0000+cycle);auto peerKick=probe;put(peerKick,80,peer);
                    check(!send(peerKick,clone) && m.process().gpuChannelExecution(endpoint,peer)==std::optional<Process::GpuChannelExecution>{{0xBEEF0000+cycle,true,1}} && view()==mainState,"peer submission updates only its own register and count");
                    Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,peer);
                    check(!send(close,clone) && !m.process().gpuChannelExecution(endpoint,peer) && !m.process().gpuChannelExecution(clone,peer) && view()==mainState,"closing through a clone retires only the selected channel execution state");
                    check(send(peerKick)==Process::InvalidHandle,"closed channel cannot accept further reference work");
                }
                reset();m.ram.write32(backing+4,0xA5A5A5A5);check(!send(probe) && view()==std::optional<Process::GpuChannelExecution>{{0xA5A5A5A5,true,3}},"execution consumes the actual current payload rather than a hard-coded reference value");
                reset();m.ram.write32(backing+8,0x80000015);check(send(probe)==Ipc::UnknownCommand,"late unknown method rolls back earlier supported reference writes");
                reset();m.ram.write32(backing,0x20012014);check(send(probe)==Ipc::UnknownCommand,"nonzero subchannel remains outside the bounded execution profile");
                for(const auto flags:{0U,5U,6U,0x104U}){reset();m.ram.write32(scratch+12,flags);check(send(probe)==Ipc::InvalidArguments,"wait and increment submission policies remain explicitly unsupported");}m.ram.write32(scratch+12,4);
                reset();m.ram.write32(scratch+16,1);check(send(probe)==Ipc::InvalidArguments,"nonzero input synchronization identity is rejected");m.ram.write32(scratch+16,0);
                for(const auto header:{0x20030014U,0x20011014U}){reset();m.ram.write32(backing,header);check(send(probe)==Ipc::InvalidArguments,"truncated payload and reserved header bit are rejected by driver decoding");}
                reset();m.ram.write64(scratch+512,va|(1ULL<<42));check(send(probe)==Ipc::InvalidArguments,"packet cannot read payload beyond its declared GPFIFO range");
                reset();m.ram.write32(backing+8,0x20010014);check(send(probe)==Ipc::InvalidArguments,"late malformed packet rejects the entire stream after a valid reference packet");
                reset();m.ram.write32(backing,0xC0010014);check(send(probe)==Ipc::UnknownCommand,"unimplemented packet opcode remains an explicit unsupported result");
                reset();m.ram.write32(backing,0);m.ram.write64(scratch+512,va|(1ULL<<42));check(!send(probe) && view()==std::optional<Process::GpuChannelExecution>{{0xA5A5A5A5,true,4}},"DMA_NOP submission completes synchronously without changing the reference register");
                reset();auto split=probe;put(split,52,uint32_t(scratch+4096));
                for(unsigned i=0;i<24;++i)m.ram.write8(scratch+4096+i,0xCC);
                m.ram.setPermissions(scratch,4096,Memory::Read);
                check(!send(split) && view()==std::optional<Process::GpuChannelExecution>{{0x12345678,true,5}},"read-only input with separate writable output supports synchronous submission");
                for(unsigned i=0;i<24;++i)check(m.ram.read8(scratch+i)==m.ram.read8(scratch+4096+i),"separate ioctl output receives the unchanged no-increment parameters");
                m.ram.setPermissions(scratch,4096,Memory::Read|Memory::Write);
                reset();m.ram.write64(scratch+512,va|(3ULL<<42)|(1ULL<<63));check(send(probe)==Ipc::InvalidArguments,"GPFIFO sync-wait bit is not mistaken for command length");
                reset();m.ram.write32(scratch+8,2);put(probe,36,16);m.ram.write64(scratch+512,va|(1ULL<<42));m.ram.write64(scratch+520,(va+4)|(1ULL<<42));check(send(probe)==Ipc::InvalidArguments,"bounded decoder does not merge an incomplete packet across GPFIFO ranges");
                reset();m.ram.setPermissions(scratch,4096,Memory::Read);check(send(probe)==Ipc::InvalidBuffer,"protected ioctl output cannot publish register or submission progress");m.ram.setPermissions(scratch,4096,Memory::Read|Memory::Write);
#ifdef GPU_MEMORY_RELEASE_TESTS
                const auto target=backing+0x2000,gpuTarget=va+0x2000;
                auto release=[&](uint64_t destination,uint32_t payload){
                    reset();const std::array<uint32_t,7> words{0x20040004,uint32_t(destination>>32),uint32_t(destination),payload,0x01000002,0x20010014,0x12345678};
                    for(size_t i=0;i<words.size();++i)m.ram.write32(backing+i*4,words[i]);
                    m.ram.write64(scratch+512,va|(uint64_t(words.size())<<42));
                };
                m.ram.write32(target-4,0x11223344);m.ram.write32(target,0);m.ram.write32(target+4,0xAABBCCDD);
                release(gpuTarget,0x78563412);const auto beforeRelease=view();const auto oldVersion=m.ram.reservationVersion(target);
                check(!send(probe) && m.ram.read32(target)==0x78563412 && m.ram.read8(target)==0x12 && m.ram.read8(target+3)==0x78 && m.ram.reservationVersion(target)!=oldVersion && view()->submissions==beforeRelease->submissions+1,"mapped semaphore release writes exact little-endian RAM bytes and invalidates CPU reservations");
                check(m.ram.read32(target-4)==0x11223344 && m.ram.read32(target+4)==0xAABBCCDD,"four-byte release preserves both adjacent guard words");
                release(gpuTarget,0);check(!send(probe) && !m.ram.read32(target),"zero payload performs a real memory store");
                release(gpuTarget,0xDEADBEEF);check(!send(probe,clone) && m.ram.read32(target)==0xDEADBEEF,"clone executes a changed payload through the same mapped channel");
                release(gpuTarget,1);const std::array<uint32_t,5> repeated{0x20040004,uint32_t(gpuTarget>>32),uint32_t(gpuTarget),0xDEADBEEF,0x01000002};
                for(size_t i=0;i<repeated.size();++i)m.ram.write32(backing+28+i*4,repeated[i]);m.ram.write64(scratch+512,va|(12ULL<<42));
                const auto beforeRepeated=view();check(!send(probe) && m.ram.read32(target)==0xDEADBEEF && view()->submissions==beforeRepeated->submissions+1,"ordered duplicate GPU stores publish the last value in exactly one submission");
                const auto preserved=m.ram.read32(target);
                release(gpuTarget,0x123);m.ram.write32(backing+28,0xE0000000);m.ram.write64(scratch+512,va|(8ULL<<42));
                check(send(probe)==Ipc::UnknownCommand && m.ram.read32(target)==preserved,"late unsupported control rolls back an earlier prepared memory release");
                release(gpuTarget,0x123);const std::array<uint32_t,5> badRelease{0x20040004,uint32_t(va>>32),uint32_t(va+0x20000),0x456,0x01000002};
                for(size_t i=0;i<badRelease.size();++i)m.ram.write32(backing+28+i*4,badRelease[i]);m.ram.write64(scratch+512,va|(12ULL<<42));
                check(send(probe)==Process::InvalidAddress && m.ram.read32(target)==preserved,"late unmapped destination rolls back all earlier writes and channel progress");
                release(gpuTarget,0x123);m.ram.setPermissions(target,4096,Memory::Read);const auto readOnlyVersion=m.ram.reservationVersion(target);
                check(send(probe)==Process::InvalidAddress && m.ram.read32(target)==preserved && m.ram.reservationVersion(target)==readOnlyVersion,"read-only destination rejects release without altering data or reservation state");m.ram.setPermissions(target,4096,Memory::Read|Memory::Write);
                release(gpuTarget,0x123);auto collision=probe;put(collision,52,uint32_t(target));
                check(send(collision)==Ipc::InvalidBuffer && m.ram.read32(target)==preserved,"GPU write cannot overlap the ioctl reply buffer");
                release(va+4,0xAABBCCDD);check(!send(probe) && m.ram.read32(backing+4)==0xAABBCCDD,"self-modifying command backing executes from its owned pre-write snapshot");
                const std::string_view foreignPath="/dev/nvmap";m.ram.writeBytes(scratch+1024,reinterpret_cast<const uint8_t*>(foreignPath.data()),foreignPath.size());
                Ipc::Buffer foreignOpen{};put(foreignOpen,0,0x00100004);put(foreignOpen,4,8);put(foreignOpen,8,uint32_t(foreignPath.size()));put(foreignOpen,12,uint32_t(scratch+1024));put(foreignOpen,32,0x49434653);
                check(!send(foreignOpen),"foreign write-ownership probe opens another Nvmap device");const auto foreign=m.ram.read32(ipc+32);const auto data=scratch+2048;
                auto foreignIo=[&](uint32_t descriptor,uint32_t request){return send(ioctlPacket(descriptor,request,data));};
                m.ram.write32(data,0x10000);m.ram.write32(data+4,0);check(!foreignIo(foreign,0xC0080101),"foreign write-ownership object creates");const auto object=m.ram.read32(data+4);
                for(unsigned i=0;i<32;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,object);m.ram.write32(data+8,1);m.ram.write32(data+12,0x10000);m.ram.write64(data+24,Process::HeapBase+0x1A0000);
                check(!foreignIo(foreign,0xC0200104),"foreign write-ownership backing binds");
                for(unsigned i=0;i<40;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,4);m.ram.write32(data+8,object);m.ram.write32(data+12,0x10000);
                check(!foreignIo(as,0xC0284106),"foreign object maps into the channel address space");const auto foreignVa=m.ram.read64(data+32);m.ram.write32(Process::HeapBase+0x1A0000,0xCAFE);
                release(foreignVa,0xBAD);check(send(probe)==Process::InvalidAddress && m.ram.read32(Process::HeapBase+0x1A0000)==0xCAFE,"channel cannot write another Nvmap owner's mapped object");
                m.ram.write64(data,foreignVa);check(!foreignIo(as,0xC0084105),"foreign write-ownership mapping releases");
                for(unsigned i=0;i<24;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,object);check(!foreignIo(foreign,0xC0180105),"rejected release retains no foreign mapping resource");
                Ipc::Buffer foreignClose{};put(foreignClose,0,4);put(foreignClose,4,9);put(foreignClose,16,0x49434653);put(foreignClose,24,2);put(foreignClose,32,foreign);check(!send(foreignClose),"foreign Nvmap write probe closes");
#endif
#ifdef GPU_LINEAR_COPY_TESTS
                const auto copySource=backing+0x4000,copyDestination=backing+0x6000;
                auto copyCommands=[&](uint64_t source,uint64_t destination,uint32_t bytes=64){
                    reset();const std::array<uint32_t,12> words{0x60018000,0xB0B5,0x20048100,uint32_t(source>>32),uint32_t(source),uint32_t(destination>>32),uint32_t(destination),0x20018106,bytes,0x818680C0,0x20010014,0x12345678};
                    for(size_t i=0;i<words.size();++i)m.ram.write32(backing+i*4,words[i]);m.ram.write64(scratch+512,va|(uint64_t(words.size())<<42));
                };
                auto copyMatches=[&](uint64_t source,uint64_t destination,size_t size){bool equal=true;for(size_t i=0;i<size;++i)equal=equal && m.ram.read8(source+i)==m.ram.read8(destination+i);return equal;};
                auto rectangleCommands=[&](uint64_t source,uint64_t destination,uint32_t width,uint32_t rows,uint32_t sourcePitch,uint32_t destinationPitch){
                    reset();const std::array<uint32_t,14> words{0x60018000,0xB0B5,0x20088100,uint32_t(source>>32),uint32_t(source),uint32_t(destination>>32),uint32_t(destination),sourcePitch,destinationPitch,width,rows,0x838680C0,0x20010014,0x12345678};
                    for(size_t i=0;i<words.size();++i)m.ram.write32(backing+i*4,words[i]);m.ram.write64(scratch+512,va|(uint64_t(words.size())<<42));
                };
                auto blockCommands=[&](uint64_t source,uint64_t destination,uint32_t width=128,uint32_t rows=16,uint32_t destinationPitch=144){
                    reset();const std::array<uint32_t,21> words{0x60018000,0xB0B5,0x20088100,uint32_t(source>>32),uint32_t(source),uint32_t(destination>>32),uint32_t(destination),0,destinationPitch,width,rows,0x200681CA,0x1000,width,rows,1,0,0,0x830680C0,0x20010014,0x12345678};
                    for(size_t i=0;i<words.size();++i)m.ram.write32(backing+i*4,words[i]);m.ram.write64(scratch+512,va|(uint64_t(words.size())<<42));
                };
                auto imageByte=[](unsigned x,unsigned y){return uint8_t((x*17+y*29+(x/64)*97+(y/8)*43)^0xA5);};
                // Independent physical-sector traversal, based on pinned libnx's inverse 16Bx2 ordering.
                std::array<uint8_t,2048> tiled{};
                for(unsigned gy=0;gy<2;++gy)for(unsigned gx=0;gx<2;++gx)for(unsigned sector=0;sector<32;++sector)for(unsigned lane=0;lane<16;++lane){
                    const auto x=gx*64+(sector/16)*32+((sector/2)%2)*16+lane;
                    const auto y=gy*8+((sector/4)%4)*2+sector%2;
                    tiled[(gy*2+gx)*512+sector*16+lane]=imageByte(x,y);
                }
                m.ram.writeBytes(copySource,tiled.data(),tiled.size());for(unsigned i=0;i<2304;++i)m.ram.write8(copyDestination+i,0xCC);
                m.ram.write8(copyDestination-1,0xE1);m.ram.write8(copyDestination+2304,0xD2);
                const auto blockSourceVersion=m.ram.reservationVersion(copySource);
                blockCommands(va+0x4000,va+0x6000);check(!send(probe),"block-linear source converts across horizontal and vertical GOB boundaries");
                auto blockMatches=[&](){bool same=true;for(unsigned i=0;i<2304;++i)same=same && m.ram.read8(copyDestination+i)==(i%144<128?imageByte(i%144,i/144):0xCC);return same;};
                check(blockMatches() && m.ram.read8(copyDestination-1)==0xE1 && m.ram.read8(copyDestination+2304)==0xD2,"every converted byte, row padding and both guards match independent sector data");
                for(unsigned i=0;i<tiled.size();++i)check(m.ram.read8(copySource+i)==tiled[i],"conversion preserves every physical source byte");
                check(m.ram.reservationVersion(copySource)==blockSourceVersion,"conversion does not invalidate source reservations");
                for(unsigned i=0;i<2304;++i)m.ram.write8(copyDestination+i,0xCC);
                reset();m.ram.write32(backing,0x830680C0);m.ram.write64(scratch+512,va|(1ULL<<42));
                check(!send(probe,clone) && blockMatches(),"clone reuses committed block geometry without host-side layout aliasing");
                blockCommands(va+0xFC00,va+0x6000);const auto blockDestinationVersion=m.ram.reservationVersion(copyDestination);
                check(send(probe)==Process::InvalidAddress && m.ram.reservationVersion(copyDestination)==blockDestinationVersion,"whole tiled source must fit its current mapping");
                blockCommands(va+0x4000,va+0x6000);auto blockCollision=probe;put(blockCollision,52,uint32_t(copyDestination+15*144));
                check(send(blockCollision)==Ipc::InvalidBuffer && m.ram.reservationVersion(copyDestination)==blockDestinationVersion,"reply collision in the last converted row prevents earlier writes");
                for(unsigned i=0;i<128;++i)m.ram.write8(copyDestination+i,0xCC);
                blockCommands(va+0x4000,va+0x6000,128,8,4096);m.ram.setPermissions(backing+0xD000,4096,Memory::Read);
                const auto beforeLastBlockRow=m.ram.reservationVersion(copyDestination);
                check(send(probe)==Process::InvalidAddress && m.ram.reservationVersion(copyDestination)==beforeLastBlockRow,"inaccessible final converted row prevents every earlier row");
                for(unsigned i=0;i<128;++i)check(m.ram.read8(copyDestination+i)==0xCC,"refused conversion leaves earlier destination bytes untouched");
                m.ram.setPermissions(backing+0xD000,4096,Memory::Read|Memory::Write);
                blockCommands(va+0x4000,va+0x6000);m.ram.write32(backing+21*4,0xE0000000);m.ram.write64(scratch+512,va|(22ULL<<42));
                check(send(probe)==Ipc::UnknownCommand && m.ram.reservationVersion(copyDestination)==beforeLastBlockRow,"late unknown control prevents an otherwise valid conversion");
                blockCommands(va+0x4000,va+0x6000);m.ram.setPermissions(copySource,4096,Memory::None);
                check(send(probe)==Process::InvalidAddress && m.ram.reservationVersion(copyDestination)==beforeLastBlockRow,"unreadable tiled source cannot publish converted rows");m.ram.setPermissions(copySource,4096,Memory::Read|Memory::Write);
                for(unsigned i=0;i<4096;++i){m.ram.write8(copySource+i,uint8_t(i*17+9));m.ram.write8(copyDestination+i,0xCC);}
                rectangleCommands(va+0x4000,va+0x6000,7,Gpu::CopyRowLimit,13,64);
                check(!send(probe),"transaction accepts the maximum supported row count alongside both replies");
                for(unsigned i=0;i<4096;++i)check(m.ram.read8(copyDestination+i)==(i%64<7?m.ram.read8(copySource+(i/64)*13+i%64):0xCC),"maximum-row transaction copies each row and preserves every padding byte");
                for(unsigned i=0;i<256;++i){m.ram.write8(copySource+i,uint8_t(i*17+9));m.ram.write8(copyDestination+i,0xCC);}
                rectangleCommands(va+0x4000,va+0x6000,7,3,13,64);
                const auto paddingVersion=m.ram.reservationVersion(copyDestination+32);
                check(!send(probe),"multiline copy accepts independent source and destination pitches");
                for(unsigned i=0;i<256;++i)check(m.ram.read8(copyDestination+i)==(i/64<3 && i%64<7?m.ram.read8(copySource+(i/64)*13+i%64):0xCC),"2D copy changes only requested rows and preserves all padding and guards");
                check(m.ram.reservationVersion(copyDestination+32)==paddingVersion,"untouched padding granules do not receive spurious writes");
                for(unsigned row=0;row<3;++row)for(unsigned i=0;i<7;++i)m.ram.write8(copyDestination+row*4096+i,0xDD);
                rectangleCommands(va+0x4000,va+0x6000,7,3,13,4096);m.ram.setPermissions(copyDestination+8192,4096,Memory::Read);
                check(send(probe)==Process::InvalidAddress,"read-only final row refuses the whole 2D copy");
                for(unsigned row=0;row<3;++row)for(unsigned i=0;i<7;++i)check(m.ram.read8(copyDestination+row*4096+i)==0xDD,"late row validation cannot partially modify earlier rows");
                m.ram.setPermissions(copyDestination+8192,4096,Memory::Read|Memory::Write);
                rectangleCommands(va+0x4000,va+0x6000,7,3,13,64);const auto before2d=m.process().gpuChannelState(endpoint,plan.fd);
                m.ram.writeBytes(ipc,probe.data(),probe.size());m.cpu.restoreContext(context);m.ram.setPermissions(ipc,4096,Memory::Read);const auto before2dVersion=m.ram.reservationVersion(copyDestination);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer && m.process().gpuChannelState(endpoint,plan.fd)==before2d && m.ram.reservationVersion(copyDestination)==before2dVersion,"protected reply prevents every row and geometry publication");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                rectangleCommands(va+0x4000,va+0xFFF0,7,3,13,64);const auto endVersion=m.ram.reservationVersion(backing+0xFFF0);
                check(send(probe)==Process::InvalidAddress && m.ram.reservationVersion(backing+0xFFF0)==endVersion,"final row beyond mapping cannot write an earlier valid row");
                rectangleCommands(va+0x4000,va+0x6000,7,3,13,64);auto rowCollision=probe;put(rowCollision,52,uint32_t(copyDestination+128));
                check(send(rowCollision)==Ipc::InvalidBuffer && m.ram.reservationVersion(copyDestination)==before2dVersion,"reply collision in the final row prevents the complete copy");
                for(unsigned i=0;i<64;++i){m.ram.write8(copySource+i,uint8_t(i^0xA5));m.ram.write8(copyDestination+i,0xCC);}m.ram.write8(copyDestination-1,0xEE);m.ram.write8(copyDestination+64,0xDD);
                copyCommands(va+0x4000,va+0x6000);const auto sourceVersion=m.ram.reservationVersion(copySource),destinationVersion=m.ram.reservationVersion(copyDestination);const auto beforeCopy=view();
                check(!send(probe) && copyMatches(copySource,copyDestination,64) && view()->submissions==beforeCopy->submissions+1,"B0B5 copies all 64 bytes through the original ioctl2 transport");
                check(m.ram.read8(copyDestination-1)==0xEE && m.ram.read8(copyDestination+64)==0xDD && m.ram.reservationVersion(copySource)==sourceVersion && m.ram.reservationVersion(copyDestination)!=destinationVersion,"copy preserves neighboring bytes and source reservations while invalidating destination reservations");
                for(unsigned i=0;i<64;++i)m.ram.write8(copyDestination+i,0xCC);reset();m.ram.write32(backing,0x818680C0);m.ram.write64(scratch+512,va|(1ULL<<42));
                check(!send(probe,clone) && copyMatches(copySource,copyDestination,64),"clone launches using the previously committed copy register bank");
                copyCommands(va+0x4001,va+0x6001,63);check(!send(probe) && copyMatches(copySource+1,copyDestination+1,63) && m.ram.read8(copyDestination+64)==0xDD,"unaligned byte addresses and odd lengths preserve the exact requested range");
                auto preservedDestination=std::array<uint8_t,64>{};for(unsigned i=0;i<64;++i)preservedDestination[i]=m.ram.read8(copyDestination+i);
                auto unchangedDestination=[&](){bool same=true;for(unsigned i=0;i<64;++i)same=same && m.ram.read8(copyDestination+i)==preservedDestination[i];return same;};
                for(const auto bad:std::array<std::array<uint64_t,2>,5>{{{va+0x20000,va+0x6000},{va+0x4000,va+0x20000},{va+0xFFF0,va+0x6000},{va+0x4000,va+0xFFF0},{copySource,va+0x6000}}}){copyCommands(bad[0],bad[1]);check(send(probe)==Process::InvalidAddress && unchangedDestination(),"absent mappings, crossing spans and CPU pointers reject the whole copy");}
                copyCommands(va+0x4000,va+0x4001);check(send(probe)==Ipc::InvalidArguments && unchangedDestination(),"overlapping virtual ranges are refused explicitly");
                copyCommands(va+0x4000,va+0x6000);m.ram.setPermissions(copySource,4096,Memory::Read);check(!send(probe) && copyMatches(copySource,copyDestination,64),"copy source requires read permission rather than write permission");m.ram.setPermissions(copySource,4096,Memory::None);check(send(probe)==Process::InvalidAddress && unchangedDestination(),"inaccessible source cannot produce a partial transfer");m.ram.setPermissions(copySource,4096,Memory::Read|Memory::Write);
                m.ram.setPermissions(copyDestination,4096,Memory::Read);check(send(probe)==Process::InvalidAddress && unchangedDestination(),"read-only destination cannot be copied into");m.ram.setPermissions(copyDestination,4096,Memory::Read|Memory::Write);
                copyCommands(va+0x4000,va+0x6000);const auto beforeSourceCollision=m.ram.reservationVersion(copySource);auto collision=probe;put(collision,52,uint32_t(copyDestination));check(send(collision)==Ipc::InvalidBuffer && unchangedDestination(),"copy cannot overwrite the ioctl reply");put(collision,52,uint32_t(copySource));check(send(collision)==Ipc::InvalidBuffer && m.ram.reservationVersion(copySource)==beforeSourceCollision,"copy source cannot alias an ioctl reply that would change it");
                copyCommands(va+0x4000,va+0x6000);m.ram.write32(backing+48,0xE0000000);m.ram.write64(scratch+512,va|(13ULL<<42));check(send(probe)==Ipc::UnknownCommand && unchangedDestination(),"late unsupported control prevents the prepared DMA transfer");
                copyCommands(va+0x4000,va+0x6000);m.ram.write32(backing+48,0x818680C0);m.ram.write64(scratch+512,va|(13ULL<<42));check(send(probe)==Ipc::TransferLimit && unchangedDestination(),"two launches cannot bypass the one-copy transaction limit");
                copyCommands(va+0x4000,va+0x6000);const auto beforeProtected=m.process().gpuChannelState(endpoint,plan.fd);m.ram.writeBytes(ipc,probe.data(),probe.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer && unchangedDestination() && m.process().gpuChannelState(endpoint,plan.fd)==beforeProtected,"protected IPC reply prevents copying and register publication");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                const auto data=scratch+2048;for(unsigned i=0;i<40;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,4);m.ram.write32(data+8,uint32_t(m.ram.read64(report+34*8)));m.ram.write32(data+12,0x10000);
                check(!send(ioctlPacket(as,0xC0284106,data)),"same object can be mapped a second time for physical-overlap validation");const auto alias=m.ram.read64(data+32);
                copyCommands(va+0x4000,alias+0x4000);check(send(probe)==Ipc::InvalidArguments && unchangedDestination(),"distinct GPU addresses cannot hide overlapping CPU backing");
                m.ram.write64(data,alias);check(!send(ioctlPacket(as,0xC0084105,data)),"alias releases after rejected DMA");copyCommands(va+0x4000,alias+0x6000);check(send(probe)==Process::InvalidAddress && unchangedDestination(),"unmapped alias is unavailable to a later copy");
                const std::string_view foreignPath="/dev/nvmap";m.ram.writeBytes(scratch+1024,reinterpret_cast<const uint8_t*>(foreignPath.data()),foreignPath.size());Ipc::Buffer openForeign{};
                put(openForeign,0,0x00100004);put(openForeign,4,8);put(openForeign,8,uint32_t(foreignPath.size()));put(openForeign,12,uint32_t(scratch+1024));put(openForeign,32,0x49434653);check(!send(openForeign),"foreign DMA ownership probe opens");const auto foreign=m.ram.read32(ipc+32);
                auto foreignIo=[&](uint32_t descriptor,uint32_t request){return send(ioctlPacket(descriptor,request,data));};
                m.ram.write32(data,0x10000);m.ram.write32(data+4,0);check(!foreignIo(foreign,0xC0080101),"foreign DMA object creates");const auto object=m.ram.read32(data+4);
                for(unsigned i=0;i<32;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,object);m.ram.write32(data+8,1);m.ram.write32(data+12,0x10000);m.ram.write64(data+24,Process::HeapBase+0x1A0000);check(!foreignIo(foreign,0xC0200104),"foreign DMA backing binds");
                for(unsigned i=0;i<40;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,4);m.ram.write32(data+8,object);m.ram.write32(data+12,0x10000);check(!foreignIo(as,0xC0284106),"foreign DMA object maps in the same address space");const auto foreignVa=m.ram.read64(data+32);
                m.ram.write32(Process::HeapBase+0x1A0000,0xCAFEBABE);copyCommands(foreignVa,va+0x6000);check(send(probe)==Process::InvalidAddress && unchangedDestination(),"DMA source must belong to the channel's Nvmap owner");
                copyCommands(va+0x4000,foreignVa);check(send(probe)==Process::InvalidAddress && m.ram.read32(Process::HeapBase+0x1A0000)==0xCAFEBABE,"DMA destination must belong to the channel's Nvmap owner");
                m.ram.write64(data,foreignVa);check(!foreignIo(as,0xC0084105),"foreign DMA mapping releases");for(unsigned i=0;i<24;i+=4)m.ram.write32(data+i,0);m.ram.write32(data,object);check(!foreignIo(foreign,0xC0180105),"failed DMA retains no foreign object");
                Ipc::Buffer closeForeign{};put(closeForeign,0,4);put(closeForeign,4,9);put(closeForeign,16,0x49434653);put(closeForeign,24,2);put(closeForeign,32,foreign);check(!send(closeForeign),"foreign DMA probe closes");
#endif
#ifdef GPU_MODULAR_CHANNEL_TESTS
                auto bindings=[&](){return m.process().gpuChannelState(endpoint,plan.fd);};
                check(bindings() && bindings()->profile==Gpu::ProfileId::MaxwellReference && bindings()->bindings==std::array<uint32_t,8>{},"fresh channel inherits its hardware profile without stale engine objects");
                auto commands=[&](std::initializer_list<uint32_t> words){reset();size_t i=0;for(const auto word:words)m.ram.write32(backing+4*i++,word);m.ram.write64(scratch+512,va|(uint64_t(words.size())<<42));};
                commands({0x60010000,0xB197,0x80000040});check(!send(probe,clone) && bindings()->bindings[0]==0xB197,"SET_OBJECT and limited 3D noop commit through cloned NV session");
                commands({0x80000040});check(!send(probe) && bindings()->bindings[0]==0xB197,"engine dispatch uses the class selected by the preceding submission");
                for(const auto id:{0xC6B5U,0xC7B5U,0xC797U,0xDEADU,0xB06FU}){commands({0x60010000,id});check(send(probe)==Ipc::UnknownCommand && bindings()->bindings[0]==0xB197,"unsupported class cannot replace the existing binding or execute through another implementation");}
                commands({0x60014000,0xB197,0x80004041});check(send(probe)==Ipc::UnknownCommand && !bindings()->bindings[2],"late unsupported engine method rolls back a newly staged binding");
                commands({0x60014000,0xB197,0x80004040});const auto beforeBindings=bindings();m.ram.writeBytes(ipc,probe.data(),probe.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer && bindings()==beforeBindings,"protected IPC reply cannot publish SET_OBJECT");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
#endif
#ifdef GPU_ALLOCATION_FAILURE_TESTS
                unsigned failedAllocations=0,metadataFailures=0,backingFailures=0;bool committed=false;
                const auto beforeAllocations=view();
                const auto beforeAllocationBindings=m.process().gpuChannelState(endpoint,plan.fd);
                for(long boundary=0;boundary<32;++boundary){
#ifdef GPU_MEMORY_RELEASE_TESTS
                    const auto faultTarget=target+4096;
                    release(gpuTarget+4096,0x0BADF00D);m.ram.discardPages(faultTarget,4096);m.ram.setPermissions(faultTarget,4096,Memory::Read|Memory::Write);
                    const auto beforeTarget=m.ram.read32(faultTarget);const auto beforeTargetVersion=m.ram.reservationVersion(faultTarget);
#elif defined(GPU_LINEAR_COPY_TESTS)
                    const auto faultTarget=copyDestination+4096;copyCommands(va+0x4000,va+0x7000);
                    m.ram.discardPages(faultTarget,4096);m.ram.setPermissions(faultTarget,4096,Memory::Read|Memory::Write);
                    const auto beforeTargetVersion=m.ram.reservationVersion(faultTarget);
#elif defined(GPU_MODULAR_CHANNEL_TESTS)
                    commands({0x60014000,0xB197,0x80004040,0x20010014,0x12345678});
#else
                    reset();
#endif
                    m.ram.writeBytes(ipc,probe.data(),probe.size());
                    // Free tracking rows without changing bytes, forcing late allocation in both reply ranges.
                    m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);m.ram.setPermissions(scratch,4096,Memory::Read|Memory::Write);
                    std::array<uint8_t,24> beforeOutput{};for(size_t i=0;i<beforeOutput.size();++i)beforeOutput[i]=m.ram.read8(scratch+i);
                    m.cpu.restoreContext(context);failAfter=boundary;rejectedBytes=0;
                    try{m.run(1);}catch(...){failAfter=-1;check(false,"submission allocation failures must not escape execution");}
                    failAfter=-1;
                    const auto code=m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);
                    if(!code){committed=true;check(view()==std::optional<Process::GpuChannelExecution>{{0x12345678,true,beforeAllocations->submissions+1}},"first fully allocated submission commits exactly once");
#ifdef GPU_MODULAR_CHANNEL_TESTS
                        check(bindings()->bindings[0]==0xB197 && bindings()->bindings[2]==0xB197,"first complete reply atomically publishes the new binding alongside retained classes");
#endif
#ifdef GPU_MEMORY_RELEASE_TESTS
                        check(m.ram.read32(faultTarget)==0x0BADF00D,"first complete memory submission publishes its exact payload");
#endif
#ifdef GPU_LINEAR_COPY_TESTS
                        check(copyMatches(copySource,faultTarget,64),"first complete copy publishes the full source pattern exactly once");
#endif
                        break;}
                    ++failedAllocations;if(rejectedBytes==2048)++metadataFailures;if(rejectedBytes>=4096 && rejectedBytes<8192)++backingFailures;
                    check(code==Process::OutOfResource && view()==beforeAllocations && m.process().gpuChannelState(endpoint,plan.fd)==beforeAllocationBindings,"failure at every allocation boundary preserves reference, bindings and committed count");
#ifdef GPU_MEMORY_RELEASE_TESTS
                    check(m.ram.read32(faultTarget)==beforeTarget && m.ram.reservationVersion(faultTarget)==beforeTargetVersion,"allocation failure preserves GPU destination bytes and CPU exclusive reservations");
#endif
#ifdef GPU_LINEAR_COPY_TESTS
                    bool zero=true;for(unsigned i=0;i<64;++i)zero=zero && !m.ram.read8(faultTarget+i);
                    check(zero && m.ram.reservationVersion(faultTarget)==beforeTargetVersion,"allocation failure preserves every copy byte and destination reservation");
#endif
                    for(size_t i=0;i<probe.size();++i)check(m.ram.read8(ipc+i)==probe[i],"failed reply allocation leaves the entire IPC request unchanged");
                    for(size_t i=0;i<beforeOutput.size();++i)check(m.ram.read8(scratch+i)==beforeOutput[i],"late allocation failure leaves ioctl output unchanged");
                }
                check(committed && failedAllocations>=8 && metadataFailures>=2,"injection reaches preflight, interpreter, staged channel and both atomic reply allocations");
#if defined(GPU_MEMORY_RELEASE_TESTS) || defined(GPU_LINEAR_COPY_TESTS)
                check(backingFailures>0,"failure injection reaches cold GPU destination backing allocation");
#endif
                std::cout<<"Submission allocation failures="<<failedAllocations<<" reply metadata failures="<<metadataFailures<<" backing failures="<<backingFailures<<'\n';
#ifdef GPU_LINEAR_COPY_TESTS
                for(unsigned layout=0;layout<2;++layout){
                if(layout)m.ram.writeBytes(copySource,tiled.data(),tiled.size());
                const unsigned rowCount=layout?8:3,rowWidth=layout?128:7;
                bool rowsCommitted=false;unsigned rowFailures=0;
                for(long boundary=0;boundary<64;++boundary){
                    const auto destination=backing+(layout?0x6000:0x7000);
                    if(layout)blockCommands(va+0x4000,va+0x6000,128,8,4096);else rectangleCommands(va+0x4000,va+0x7000,7,3,13,4096);
                    m.ram.discardPages(destination,rowCount*4096);m.ram.setPermissions(destination,rowCount*4096,Memory::Read|Memory::Write);
                    std::array<uint64_t,8> versions{};for(unsigned row=0;row<rowCount;++row)versions[row]=m.ram.reservationVersion(destination+row*4096);
                    const auto beforeRows=view();const auto beforeGeometry=m.process().gpuChannelState(endpoint,plan.fd);
                    m.ram.writeBytes(ipc,probe.data(),probe.size());
                    // Force reply tracking allocation too, as in the single-line failure sweep.
                    m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);m.ram.setPermissions(scratch,4096,Memory::Read|Memory::Write);
                    std::array<uint8_t,24> rowOutput{};for(size_t i=0;i<rowOutput.size();++i)rowOutput[i]=m.ram.read8(scratch+i);
                    m.cpu.restoreContext(context);failAfter=boundary;rejectedBytes=0;
                    try{m.run(1);}catch(...){failAfter=-1;check(false,"2D allocation failures do not escape execution");}failAfter=-1;
                    const auto code=m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);
                    if(!code){rowsCommitted=true;for(unsigned row=0;row<rowCount;++row)for(unsigned x=0;x<rowWidth;++x)check(m.ram.read8(destination+row*4096+x)==(layout?imageByte(x,row):m.ram.read8(copySource+row*13+x)),"complete conversion or pitched transaction publishes every cold row");check(view()->submissions==beforeRows->submissions+1,"2D transaction publishes one submission");break;}
                    ++rowFailures;check(code==Process::OutOfResource && view()==beforeRows && m.process().gpuChannelState(endpoint,plan.fd)==beforeGeometry,"failed 2D transaction preserves geometry and submission state");
                    for(unsigned row=0;row<rowCount;++row){check(m.ram.reservationVersion(destination+row*4096)==versions[row],"late allocation failure preserves every row reservation");for(unsigned i=0;i<rowWidth;++i)check(!m.ram.read8(destination+row*4096+i),"late allocation failure preserves every row byte");}
                    for(size_t i=0;i<probe.size();++i)check(m.ram.read8(ipc+i)==probe[i],"failed 2D transaction leaves IPC request unchanged");
                    for(size_t i=0;i<rowOutput.size();++i)check(m.ram.read8(scratch+i)==rowOutput[i],"failed 2D transaction leaves ioctl output unchanged");
                }
                check(rowsCommitted && rowFailures>=8,"2D fault injection reaches staged state and multiple cold row allocations");
                std::cout<<"Cold row layout="<<layout<<" allocation failures="<<rowFailures<<'\n';
                }
#endif
#endif
                reset();const auto beforeReplyFailure=view();m.ram.writeBytes(ipc,probe.data(),probe.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer && view()==beforeReplyFailure,"failed IPC reply cannot execute or acknowledge a stream");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                check(m.ram.read32(backing)==0x20010014 && m.ram.read32(backing+4)==0x12345678 && !m.ram.read32(backing+8),"execution probes preserve the complete original reference stream");for(size_t i=0;i<parameters.size();++i)check(m.ram.read8(plan.source+i)==parameters[i],"original ioctl/fence bytes are unchanged");
                m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
            }
        }
        const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
    }
    check(exited && kicks==2,"both original packet submissions execute and clean up after transactional probes");
    for(unsigned teardown=0;teardown<3;++teardown){
        Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;bool captured=false;
        auto reachKickoff=[&](uint32_t& endpoint,uint32_t& descriptor){
            const auto local=probe.process().mainThread().localRegion;
            for(unsigned step=0;step<500000;++step){
                if(probe.ram.fetch32(probe.cpu.getPC())==0xD4000421){
                    Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::NvIoctl request;
                    if(!Ipc::parseNvIoctl(p,probe.bus,local,request) && request.command==11 && request.request==0xC018481B){endpoint=uint32_t(probe.cpu.readRegister(0));descriptor=request.fd;return true;}
                }
                if(probe.run(1).stop!=Machine::Stop::Budget)break;
            }
            return false;
        };
        uint32_t root=0,fd=0;captured=reachKickoff(root,fd);check(captured,"original Kickoff is reached for channel teardown tests");if(!captured)continue;
#if defined(GPU_MODULAR_CHANNEL_TESTS) || defined(GPU_LINEAR_COPY_TESTS)
        const auto local=probe.process().mainThread().localRegion;Ipc::Buffer p{};for(size_t i=0;i<p.size();++i)p[i]=probe.ram.read8(local+i);Ipc::NvIoctl request;
        if(Ipc::parseNvIoctl(p,probe.bus,local,request))return 2;
        const auto backing=probe.ram.read64(symbols.at("gpu_as_report")+256);const auto descriptor=probe.ram.read64(request.extraSource);
        const auto gpuBase=descriptor&((1ULL<<40)-1);
#ifdef GPU_LINEAR_COPY_TESTS
        for(unsigned i=0;i<64;++i){probe.ram.write8(backing+0x4000+i,uint8_t(i));probe.ram.write8(backing+0x6000+i,0xCC);}
        const std::array<uint32_t,12> bindingWords{0x60018000,0xB0B5,0x20048100,uint32_t(gpuBase>>32),uint32_t(gpuBase+0x4000),uint32_t(gpuBase>>32),uint32_t(gpuBase+0x6000),0x20018106,64,0x818680C0,0x20010014,0x12345678};
#else
        const std::array<uint32_t,5> bindingWords{0x60010000,0xB197,0x80000040,0x20010014,0x12345678};
#endif
        for(size_t i=0;i<bindingWords.size();++i)probe.ram.write32(backing+4*i,bindingWords[i]);probe.ram.write64(request.extraSource,gpuBase|(uint64_t(bindingWords.size())<<42));
#endif
        probe.run(1);const auto committed=probe.process().gpuChannelExecution(root,fd);
        const auto committedEngines=probe.process().gpuChannelState(root,fd);
        check(committed==std::optional<Process::GpuChannelExecution>{{0x12345678,true,1}},"original driver call commits one real reference execution before teardown");
        const auto missing=std::string(argv[1])+".missing";
        check(!probe.load(missing.c_str()) && probe.process().gpuChannelExecution(root,fd)==committed,"failed reload preserves committed register state and submission count");
        check(probe.process().gpuChannelState(root,fd)==committedEngines,"failed reload preserves the complete engine register state");
#ifdef GPU_LINEAR_COPY_TESTS
        check(committedEngines && committedEngines->bindings[4]==0xB0B5 && committedEngines->copy[4].configured==31 && committedEngines->copy[4].length==64,"DMA register bank is committed before lifecycle checks");
        for(unsigned i=0;i<64;++i)check(probe.ram.read8(backing+0x6000+i)==i,"lifecycle probe performs its copy before teardown");
#endif
#ifdef GPU_MODULAR_CHANNEL_TESTS
        check(probe.process().gpuChannelState(root,fd) && probe.process().gpuChannelState(root,fd)->bindings[0]==0xB197,"failed reload preserves a committed engine binding");
#endif
        if(teardown){
            const auto code=Process::HeapBase+0x180000;probe.ram.setPermissions(code,4096,Memory::All);probe.ram.write32(code,teardown==1?0xD40000E1:0);probe.cpu.setPC(code);
            const auto stopped=probe.run(1);
            check(stopped.stop==(teardown==1?Machine::Stop::Exited:Machine::Stop::Error) && !probe.process().gpuChannelExecution(root,fd),"exit/fault retires channel execution state");
        }
        check(probe.load(argv[1]) && !probe.process().gpuChannelExecution(root,fd),"successful reload retires the old channel state");
        check(reachKickoff(root,fd) && probe.process().gpuChannelExecution(root,fd)==std::optional<Process::GpuChannelExecution>{{0,false,0}},"fresh channel after reload has no stale reference or submission count");
#ifdef GPU_MODULAR_CHANNEL_TESTS
        check(probe.process().gpuChannelState(root,fd) && probe.process().gpuChannelState(root,fd)->bindings==std::array<uint32_t,8>{},"reload, exit and fault cannot retain old subchannel bindings");
#endif
#ifdef GPU_LINEAR_COPY_TESTS
        check(probe.process().gpuChannelState(root,fd) && probe.process().gpuChannelState(root,fd)->copy==std::array<Gpu::MaxwellCopyState,8>{} && probe.process().gpuChannelState(root,fd)->bindings==std::array<uint32_t,8>{},"reload, exit and fault cannot retain a stale DMA register bank");
#endif
        const auto finished=probe.run(500000);check(finished.stop==Machine::Stop::Exited && !finished.exitCode && !probe.process().gpuChannelExecution(root,fd),"reloaded original reference executes and cleans up normally");
    }
    std::cout<<"GPU packet reference instructions="<<instructions<<" failures="<<failures<<'\n';return failures?1:0;
}
