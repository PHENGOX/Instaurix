#include "machine.hpp"
#include "ipc.hpp"
#include <iostream>
static uint64_t get(const Ipc::Buffer& b,size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4){for(size_t i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i));}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Ipc::Buffer registration{},lookup{},create{},getMemory{};uint32_t rootHandle=0,resourceHandle=0,sharedHandle=0;
    Machine sdk(false,true,true);if(!sdk.load(argv[1]))return 2;const auto tls=sdk.process().mainThread().localRegion;
    uint64_t mapped=0,instructions=0;unsigned entropy=0;Machine::Result step{Machine::Stop::Budget,0};
    for(unsigned i=0;i<300000 && step.stop==Machine::Stop::Budget;++i){
        const auto op=sdk.ram.fetch32(sdk.cpu.getPC());const bool svc=(op&0xFFE0001F)==0xD4000001;const auto id=(op>>5)&0xFFFF;
        Ipc::Buffer b{};uint32_t handle=0;
        if(svc && id==0x21){handle=uint32_t(sdk.cpu.readRegister(0));for(size_t j=0;j<b.size();++j)b[j]=sdk.ram.read8(tls+j);}
        const auto address=sdk.cpu.readRegister(1);const bool seed=svc && id==0x29 && address==11;
        step=sdk.run(1);instructions+=step.instructions;
        if(seed){check(sdk.cpu.readRegister(0)==0,"original libnx PRNG obtains each reference seed word");++entropy;}
        if(svc && id==0x13){mapped=address;check(sdk.cpu.readRegister(0)==0,"original shmemMap maps HID memory");}
        if(!svc || id!=0x21 || get(b,0)!=4)continue;
        const bool pid=get(b,4)&0x80000000U;const size_t cmif=pid?32:16;
        if(get(b,cmif)!=0x49434653)continue;
        if(pid && get(b,cmif+8)==0 && !get(registration,0))registration=b;
        if(get(b,cmif+8)==1 && std::string(reinterpret_cast<const char*>(b.data()+cmif+16),8)==std::string("hid\0\0\0\0\0",8)){
            lookup=b;rootHandle=sdk.ram.read32(tls+12);check(sdk.ram.read32(tls+24)==0,"native SM opens HID session");
        }else if(handle==rootHandle){create=b;resourceHandle=sdk.ram.read32(tls+12);check(sdk.ram.read32(tls+8)==32 && sdk.ram.read32(tls+24)==0,"HID resource is a moved session");}
        else if(handle==resourceHandle){getMemory=b;sharedHandle=sdk.ram.read32(tls+12);check(sdk.ram.read32(tls+8)==2 && sdk.ram.read32(tls+24)==0,"HID memory is a copied typed handle");}
    }
    if(step.stop==Machine::Stop::Error)std::cerr<<sdk.error()<<'\n';
    check(step.stop==Machine::Stop::Exited && step.exitCode==0xE017,"original HID/time/environment/filesystem initialization completes before unlinked SD mounting layer");
    check(rootHandle && resourceHandle && sharedHandle && mapped && entropy==4,"capture native HID resource, mapping and original PRNG path");
    if(!mapped || !get(create,0))return 1;
    check(sdk.ram.canAccess(mapped,Process::HidSharedSize,Memory::Read) && !sdk.ram.canAccess(mapped,1,Memory::Write) && !sdk.ram.canAccess(mapped,4,Memory::Execute),"SDK mapping is R and never W/X");
    for(size_t i=0;i<Process::HidSharedSize;i+=4096)check(sdk.ram.read64(mapped+i)==0,"neutral HID backing starts zeroed");
    const auto oldId=sdk.process().appletResourceId(),oldPC=sdk.cpu.getPC();const auto missing=std::string(argv[1])+".missing";
    check(!sdk.load(missing.c_str()) && sdk.cpu.getPC()==oldPC && sdk.process().appletResourceId()==oldId && sdk.ram.canAccess(mapped,4096,Memory::Read),"failed load preserves existing HID mapping and process");
    check(sdk.load(argv[1]) && sdk.process().appletResourceId()!=oldId && sdk.ram.permissionsAt(mapped)==Memory::None,"successful reload releases old HID mapping");

    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;std::string error;Process p;
    if(!ProgramLoader::loadImage(ram,argv[1],image,error)||!p.initialize(ram,image,true,error,true))return 2;
    Bus bus(ram);CPU cpu(bus);const auto ipc=p.mainThread().localRegion;
    const uint8_t sm[]{'s','m',':',0};ram.writeBytes(0x200300,sm,sizeof sm);cpu.writeRegister(1,0x200300);p.dispatch(0x1F,cpu,bus);const auto manager=uint32_t(cpu.readRegister(1));
    auto send=[&](const Ipc::Buffer& b,uint32_t h){ram.writeBytes(ipc,b.data(),b.size());cpu.writeRegister(0,h);p.dispatch(0x21,cpu,bus);return cpu.readRegister(0);};
    for(uint64_t index=0;index<4;++index){cpu.writeRegister(1,11);cpu.writeRegister(2,0);cpu.writeRegister(3,index);p.dispatch(0x29,cpu,bus);const auto seed=cpu.readRegister(1);check(cpu.readRegister(0)==0 && seed!=0,"reference entropy word is available");cpu.writeRegister(1,11);p.dispatch(0x29,cpu,bus);check(cpu.readRegister(1)==seed,"reference entropy is reproducible");}
    cpu.writeRegister(1,11);cpu.writeRegister(2,0);cpu.writeRegister(3,4);p.dispatch(0x29,cpu,bus);check(cpu.readRegister(0)==Process::InvalidSize,"entropy index is bounded");
    cpu.writeRegister(1,11);cpu.writeRegister(2,p.mainHandle());cpu.writeRegister(3,0);p.dispatch(0x29,cpu,bus);check(cpu.readRegister(0)==Process::InvalidHandle,"entropy ABI requires INVALID_HANDLE");
    send(registration,manager);send(lookup,manager);const auto root=ram.read32(ipc+12);
    put(create,48,p.appletResourceId(),8);
    auto wrong=create;put(wrong,48,p.appletResourceId()+1,8);check(send(wrong,root)==0 && ram.read32(ipc+24)==Ipc::InvalidArguments && !(ram.read32(ipc+4)&0x80000000U),"foreign application resource identity cannot allocate HID resource");
    auto malformed=create;put(malformed,8,3);check(send(malformed,root)==Ipc::UnsupportedFormat,"reject transferred handles in HID creation");
    auto failedReply=[&](const Ipc::Buffer& b,uint32_t h){ram.writeBytes(ipc,b.data(),b.size());ram.setPermissions(ipc,4096,Memory::Read);cpu.writeRegister(0,h);p.dispatch(0x21,cpu,bus);check(cpu.readRegister(0)==Process::InvalidPointer,"failed HID reply is atomic");ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);};
    failedReply(create,root);check(send(create,root)==0 && ram.read32(ipc+24)==0,"retry resource creation succeeds");const auto child=ram.read32(ipc+12);
    check(child==root+1,"rejected resource creations consume no handle IDs");
    failedReply(getMemory,child);send(getMemory,child);const auto memory=ram.read32(ipc+12);check(memory==child+1 && ram.read32(ipc+8)==2,"memory publication failure consumes no handle ID");
    send(getMemory,child);const auto copy=ram.read32(ipc+12);
    auto map=[&](uint32_t h,uint64_t address,uint64_t size=Process::HidSharedSize,uint64_t perm=Memory::Read){cpu.writeRegister(0,h);cpu.writeRegister(1,address);cpu.writeRegister(2,size);cpu.writeRegister(3,perm);p.dispatch(0x13,cpu,bus);return cpu.readRegister(0);};
    auto unmap=[&](uint32_t h,uint64_t address,uint64_t size=Process::HidSharedSize){cpu.writeRegister(0,h);cpu.writeRegister(1,address);cpu.writeRegister(2,size);p.dispatch(0x14,cpu,bus);return cpu.readRegister(0);};
    constexpr uint64_t at=0x80000,other=0x100000;
    check(map(child,at)==Process::InvalidHandle,"session handle cannot map shared memory");
    check(map(memory,at+1)==Process::InvalidAddress && map(memory,at,4096)==Process::InvalidSize,"mapping checks alignment and full object size");
    check(map(memory,at,Process::HidSharedSize,Memory::Read|Memory::Write)==Process::InvalidState,"guest cannot request write access");
    check(map(memory,Process::HeapBase)==Process::InvalidAddress && map(memory,Process::HomebrewEnvironment)==Process::InvalidAddress,"mapping cannot overlap reserved runtime regions");
    check(map(memory,0x10000)==Process::InvalidAddress && map(memory,UINT64_MAX-4095)==Process::InvalidAddress,"mapping cannot overwrite executable or overflow RAM");
    check(map(memory,at)==0,"valid free-range HID mapping");
    cpu.writeRegister(0,0x200340);cpu.writeRegister(2,at);p.dispatch(0x06,cpu,bus);
    check(ram.read32(0x200340+16)==6 && ram.read32(0x200340+24)==Memory::Read,"QueryMemory exposes shared memory type and R protection");
    ram.write32(at,0x11223344);check(ram.hasError() && ram.read32(at)==0,"guest write to HID memory is denied");ram.clearError();
    check(map(copy,other)==Process::InvalidState,"same object has one bounded mapping even through copied handle");
    send(lookup,manager);const auto independentRoot=ram.read32(ipc+12);send(create,independentRoot);const auto independentChild=ram.read32(ipc+12);
    send(getMemory,independentChild);const auto independentMemory=ram.read32(ipc+12);
    check(map(independentMemory,other)==Process::InvalidState,"independent HID resources share the process-owned memory object");
    cpu.writeRegister(0,memory);p.dispatch(0x16,cpu,bus);check(ram.canAccess(at,4096,Memory::Read) && ram.read32(at)==0,"closing one handle does not revoke mapped pages");
    check(map(memory,other)==Process::InvalidHandle && unmap(copy,at+4096)==Process::InvalidState && ram.canAccess(at,4096,Memory::Read),"stale handle or wrong unmap leaves mapping intact");
    check(unmap(copy,at,4096)==Process::InvalidSize && unmap(copy,at)==0 && ram.permissionsAt(at)==Memory::None,"valid unmap removes pages and permissions");
    check(unmap(copy,at)==Process::InvalidState && map(independentMemory,other)==0,"unmapped object can be mapped again through an independent resource");
    cpu.writeRegister(0,0x200340);cpu.writeRegister(2,at);p.dispatch(0x06,cpu,bus);check(ram.read32(0x200340+16)==0,"unmapped region becomes free");
    check(unmap(copy,other)==0,"release final test mapping");
    cpu.writeRegister(1,0x200320);ram.write32(0x200320,copy);cpu.writeRegister(2,1);cpu.writeRegister(3,0);p.dispatch(0x18,cpu,bus);check(cpu.readRegister(0)==Process::InvalidHandle,"memory handles are not waitable events");
    Ipc::Buffer close{};put(close,0,2);send(close,root);check(send(getMemory,child)==0 && ram.read32(ipc+24)==0,"HID resource survives parent endpoint closure");
    auto clone=registration;clone={};put(clone,0,5);put(clone,4,8);put(clone,16,0x49434653);put(clone,24,2);send(clone,child);const auto cloned=ram.read32(ipc+12);
    check(send(getMemory,cloned)==0 && ram.read32(ipc+8)==2,"cloned resource retains shared-memory object");
    // Exercise the common handle quota with copied memory handles, preserving monotonic IDs.
    uint32_t last=0;unsigned allocated=0;
    for(unsigned i=0;i<128;++i){send(getMemory,child);if(ram.read32(ipc+24)==Process::OutOfHandles)break;check(ram.read32(ipc+24)==0,"memory handle within quota");last=ram.read32(ipc+12);++allocated;}
    check(allocated && ram.read32(ipc+24)==Process::OutOfHandles && !(ram.read32(ipc+4)&0x80000000U),"memory handles share finite kernel quota");
    cpu.writeRegister(0,copy);p.dispatch(0x16,cpu,bus);send(getMemory,child);check(ram.read32(ipc+12)==last+1,"quota failure consumes no ID and closed slot is reusable");
    p.checkpoint(cpu,Process::State::Exited);check(send(getMemory,child)==0 && ram.read32(ipc+24)==Process::InvalidState,"retired applet cannot publish HID memory handles");

    // Shared pages alias the actual backing, rather than copying an initial snapshot.
    Memory alias(3*4096);alias.reset(Memory::None);Memory::SharedPages pages{std::make_shared<uint8_t[]>(4096)};
    check(alias.mapReadOnlySharedPages(0,pages) && alias.mapReadOnlySharedPages(4096,pages),"shared backing supports read-only aliases");
    pages[0][7]=0xA5;check(alias.read8(7)==0xA5 && alias.read8(4096+7)==0xA5,"host backing update is visible through both mappings");
    check(alias.discardPages(0,4096) && alias.read8(4096+7)==0xA5,"unmapping one alias preserves other references");
    std::cout<<"HID initialization instructions="<<instructions<<" mapping=0x"<<std::hex<<mapped<<std::dec<<" failures="<<failures<<'\n';return failures?1:0;
}
