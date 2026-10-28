#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
struct Capture { Ipc::Buffer packet{};Ipc::NvIoctl ioctl{};CPU::Context context{};uint64_t borrowed=0; };
static bool seekInitialize(Machine& m,Capture& capture){
    const auto ipc=m.process().mainThread().localRegion;
    for(unsigned step=0;step<500000;++step){
        const auto op=m.ram.fetch32(m.cpu.getPC());if(op==0xD40002A1)capture.borrowed=m.cpu.readRegister(1);
        if(op==0xD4000421){Ipc::Buffer b{};for(size_t i=0;i<b.size();++i)b[i]=m.ram.read8(ipc+i);
            Ipc::NvIoctl plan;if(!Ipc::parseNvIoctl(b,m.bus,ipc,plan) && plan.request==0x40284109){capture.packet=b;capture.ioctl=plan;capture.context=m.cpu.captureContext();return true;}}
        if(m.run(1).stop!=Machine::Stop::Budget)return false;
    }return false;
}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;const bool baseline=argc==4 && std::string(argv[3])=="--baseline";if(argc==4 && !baseline)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original address-space client exits cleanly without rendering");
        for(unsigned iteration=0;iteration<2;++iteration){const auto base=symbols.at("gpu_as_report")+iteration*128;auto get=[&](unsigned i){return m.ram.read64(base+i*8);};
            check(!get(0) && get(1)==(baseline?Ipc::FileNotFound:0) && get(2)==(baseline?0:1) && get(3)==(baseline?UINT32_MAX:iteration+1) && get(4)==0x10000,"original Create reaches the expected Open boundary or initialized descriptor");
            check(!get(5) && get(6)==UINT32_MAX && !get(7) && !get(8),"original Close, repeated Close and NV service exit retire the lifecycle");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"address-space execution is independent of slicing");
    }
    if(!baseline){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;
        unsigned opens=0,initializes=0,closes=0;uint32_t root=0,clone=0;Ipc::Buffer initializePacket{};CPU::Context initializeContext{};std::array<uint8_t,40> initializeInput{};bool exited=false;
        for(unsigned step=0;step<500000;++step){
            bool cloning=false;
            if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);
                if(read(packet,4)==0x80000009 && read(packet,40)==3)root=uint32_t(m.cpu.readRegister(0));
                cloning=read(packet,0)==5 && read(packet,4)==9 && read(packet,24)==4;
                const auto context=m.cpu.captureContext();
                auto send=[&](const Ipc::Buffer& message,uint32_t endpoint=0){m.ram.writeBytes(ipc,message.data(),message.size());m.cpu.restoreContext(context);if(endpoint)m.cpu.writeRegister(0,endpoint);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                Ipc::Layout layout;
                if(!Ipc::parseBuffers(packet,&m.bus,ipc,layout) && layout.command==0 && layout.sourceSize==18){
                    ++opens;m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                    check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected address-space Open publishes no descriptor");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    std::array<uint32_t,Process::NvDescriptorLimit> fds{};for(auto& fd:fds){check(!send(packet),"address-space descriptors share the bounded NV quota");fd=m.ram.read32(ipc+32);}
                    if(opens==1)check(fds[0]==1,"failed Open consumes no descriptor identity");check(send(packet)==Process::OutOfResource,"address-space descriptor quota is enforced");
                    Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);
                    for(size_t i=0;i<fds.size();++i){put(close,32,fds[i]);check(!send(close,i%2?root:clone),"root and tagged clone share address-space descriptor retirement");}
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
                Ipc::NvIoctl ioctl;
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,ioctl) && ioctl.request==0x40284109){
                    ++initializes;initializePacket=packet;initializeContext=context;
                    check(ioctl.size==40 && !ioctl.destination && m.ram.read32(ioctl.source)==1 && m.ram.read32(ioctl.source+8)==0x10000,"actual original InitializeEx uses a 40-byte input and null output");
                    std::array<uint8_t,40> input{};for(size_t i=0;i<input.size();++i)input[i]=m.ram.read8(ioctl.source+i);
                    initializeInput=input;
                    for(size_t offset=0;offset<40;offset+=4){const auto value=m.ram.read32(ioctl.source+offset);m.ram.write32(ioctl.source+offset,value+1);check(send(packet)==Ipc::InvalidArguments,"InitializeEx rejects unsupported flags, page size and reserved fields");m.ram.writeBytes(ioctl.source,input.data(),input.size());}
                    auto bad=packet;put(bad,64,UINT32_MAX);check(send(bad)==Process::InvalidHandle,"foreign descriptor cannot initialize an address space");
                    bad=packet;put(bad,28,40);put(bad,32,uint32_t(ioctl.source));check(send(bad)==Ipc::InvalidArguments,"write-only ioctl requires an entirely null receive descriptor");
                    bad=packet;put(bad,32,uint32_t(ioctl.source));check(send(bad)==Ipc::InvalidArguments,"zero receive size cannot conceal a non-null output address");
                    bad=packet;put(bad,16,39);check(send(bad)==Ipc::InvalidArguments,"input descriptor size must match the ioctl encoding");
                    bad=packet;put(bad,20,uint32_t(ipc));check(send(bad)==Ipc::InvalidBuffer,"write-only input cannot overlap IPC reply memory");
                    const auto page=ioctl.source&~uint64_t(4095);const auto permissions=m.ram.permissionsAt(page);
                    m.ram.setPermissions(page,4096,Memory::None);check(send(packet)==Ipc::InvalidBuffer,"InitializeEx requires readable input");m.ram.setPermissions(page,4096,permissions);
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                    check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected InitializeEx reply cannot publish initialization");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    // Cross-device checks use an independently opened nvmap descriptor in the same service.
                    const auto scratch=Process::HeapBase+0x170000;const std::string_view path="/dev/nvmap";m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());
                    Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,uint32_t(path.size()));put(open,12,uint32_t(scratch));put(open,32,0x49434653);put(open,40,0);
                    check(!send(open),"nvmap can coexist with the address-space endpoint");const auto mapFd=m.ram.read32(ipc+32);
                    bad=packet;put(bad,64,mapFd);check(send(bad)==Ipc::UnknownCommand,"address-space initialization cannot run on a nvmap endpoint");
                    Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,mapFd);check(!send(close),"cross-device test descriptor retires");
                    Ipc::Buffer probe{};put(probe,0,0x01110004);put(probe,4,0xC0B);put(probe,16,8);put(probe,20,uint32_t(scratch));put(probe,28,8);put(probe,32,uint32_t(scratch));put(probe,48,0x49434653);put(probe,56,1);put(probe,64,ioctl.fd);put(probe,68,0xC0080101);m.ram.write64(scratch,0xBADF00D000001000ULL);
                    check(send(probe)==Ipc::NotInitialized,"other address-space operations cannot run before initialization");
                    auto allocation=probe;put(allocation,16,24);put(allocation,28,24);put(allocation,68,0xC0184102);
                    m.ram.write32(scratch,1);m.ram.write32(scratch+4,0x10000);m.ram.write64(scratch+8,0);m.ram.write64(scratch+16,0x10000);
                    std::array<uint8_t,24> allocationInput{};for(size_t i=0;i<allocationInput.size();++i)allocationInput[i]=m.ram.read8(scratch+i);
                    check(send(allocation)==Ipc::NotInitialized,"AS AllocSpace cannot run before InitializeEx");
                    // Legacy transport is validated using the captured original input, without initializing twice.
                    Ipc::Buffer legacy{};put(legacy,0,0x01100004);put(legacy,4,10);put(legacy,8,40);put(legacy,12,uint32_t(ioctl.source));put(legacy,32,0x49434653);put(legacy,40,1);put(legacy,48,ioctl.fd);put(legacy,52,0x40284109);
                    Ipc::NvIoctl parsed;check(!Ipc::parseNvIoctl(legacy,m.bus,ipc,parsed) && parsed.source==ioctl.source && !parsed.destination,"legacy write-only transport accepts null output too");
                    m.ram.setPermissions(page,4096,Memory::Read);check(!send(packet),"original InitializeEx needs no writable input or output buffer");m.ram.setPermissions(page,4096,permissions);
                    const auto after=m.cpu.captureContext();Ipc::Buffer response{};for(size_t i=0;i<response.size();++i)response[i]=m.ram.read8(ipc+i);
                    check(send(packet,clone)==Ipc::AlreadyInitialized,"tagged clone observes initialized address-space state");
                    check(!send(allocation) && m.ram.read64(scratch+16)==0x100000000ULL,"initialized AS AllocSpace publishes a separate GPU virtual address");
                    for(size_t i=0;i<16;++i)check(m.ram.read8(scratch+i)==allocationInput[i],"allocation preserves its non-output fields");
                    check(send(probe)==Ipc::UnknownCommand,"nvmap operations remain unsupported on an initialized address-space endpoint");
                    for(size_t i=0;i<input.size();++i)check(m.ram.read8(ioctl.source+i)==input[i],"write-only initialization leaves every input byte unchanged");
                    m.ram.writeBytes(ipc,response.data(),response.size());m.cpu.restoreContext(after);continue;
                }
                if(!Ipc::parseBuffers(packet,&m.bus,ipc,layout) && layout.command==2 && layout.payloadSize==4){
                    ++closes;m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                    check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected address-space Close cannot retire state");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    const auto source=uint64_t(read(initializePacket,20));std::array<uint8_t,40> current{};for(size_t i=0;i<current.size();++i)current[i]=m.ram.read8(source+i);
                    m.ram.writeBytes(source,initializeInput.data(),initializeInput.size());m.ram.writeBytes(ipc,initializePacket.data(),initializePacket.size());m.cpu.restoreContext(initializeContext);m.run(1);check(m.ram.read32(ipc+24)==Ipc::AlreadyInitialized,"failed Close preserves the live initialized state");m.ram.writeBytes(source,current.data(),current.size());
                    check(!send(packet),"original Close retires the address-space endpoint");const auto after=m.cpu.captureContext();Ipc::Buffer response{};for(size_t i=0;i<response.size();++i)response[i]=m.ram.read8(ipc+i);
                    m.ram.writeBytes(ipc,initializePacket.data(),initializePacket.size());m.cpu.restoreContext(initializeContext);m.cpu.writeRegister(0,clone);m.run(1);check(m.ram.read32(ipc+24)==Process::InvalidHandle,"retired descriptors cannot be initialized through another clone");
                    m.ram.writeBytes(ipc,response.data(),response.size());m.cpu.restoreContext(after);continue;
                }
            }
            const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
        }
        check(exited && opens==2 && initializes==2 && closes==2 && !m.process().frameCount(),"trace completes both real address-space lifetimes after negative probes");
        for(bool exitNormally:{false,true}){
            Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;Capture capture;const bool captured=seekInitialize(probe,capture);check(captured,"original initialization boundary is captured for lifetime checks");if(!captured)continue;
            const auto local=probe.process().mainThread().localRegion;probe.run(1);check(!probe.ram.read32(local+24),"captured address space initializes");
            const auto missing=std::string(argv[1])+".missing";check(!probe.load(missing.c_str()),"failed reload is reported");probe.ram.writeBytes(local,capture.packet.data(),capture.packet.size());probe.cpu.restoreContext(capture.context);probe.run(1);check(probe.ram.read32(local+24)==Ipc::AlreadyInitialized,"failed reload preserves address-space state");
            const auto scratch=Process::HeapBase+0x170000;probe.ram.setPermissions(scratch,4096,Memory::All);probe.ram.write32(scratch,exitNormally?0xD40000E1:0);probe.cpu.setPC(scratch);const auto stopped=probe.run(1);
            check(stopped.stop==(exitNormally?Machine::Stop::Exited:Machine::Stop::Error) && probe.ram.canAccess(capture.borrowed,0x10000,Memory::Read|Memory::Write),"exit/fault drops GPU descriptor state and restores borrowed NV communication memory");
            check(probe.load(argv[1]),"fresh reload succeeds after teardown");Capture fresh;if(seekInitialize(probe,fresh)){check(fresh.ioctl.fd==1,"fresh process starts a new descriptor namespace");probe.run(1);check(!probe.ram.read32(probe.process().mainThread().localRegion+24),"fresh address-space initialization inherits no stale state");}else check(false,"fresh original initialization is reachable");
        }
    }
    std::cout<<"Address-space instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
