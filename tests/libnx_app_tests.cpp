#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
static uint64_t get(const Ipc::Buffer& b,size_t at,size_t n=4)
{ uint64_t v=0;for(size_t i=0;i<n;++i) v|=uint64_t(b[at+i])<<(8*i);return v; }
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4)
{ for(size_t i=0;i<n;++i) b[at+i]=uint8_t(v>>(8*i)); }
static Ipc::Buffer snapshot(const Memory& ram,uint64_t at)
{ Ipc::Buffer b{};for(size_t i=0;i<b.size();++i) b[i]=ram.read8(at+i);return b; }
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    unsigned failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    constexpr uint64_t missingMount=0xE017;
    Machine whole(false,true,true),sliced(false,true,true);
    if(!whole.load(argv[1]) || !sliced.load(argv[1])) { std::cerr<<whole.error()<<'\n';return 1; }
    const uint64_t report=0x10000+whole.ram.read32(0x10000+0x30)+0x2000;
    const auto result=whole.run(100000);
    check(whole.ram.read64(report-0x2000)==report && whole.ram.permissionsAt(report-0x2000)==Memory::Read,
        "automatically placed RELRO relocation points at the actual report and remains read-only");
    auto value=[&](unsigned slot) { return whole.ram.read64(report+slot*8); };
    check(result.stop==Machine::Stop::Exited && result.exitCode==missingMount && value(15)==missingMount,
        "original app initialization completes fsInitialize before explicit missing fsdevMountSdmc dependency");
    check(value(0)==0 && value(14)==0 && value(16)==0 && value(17)==0 && value(18)==0 &&
        value(19)==0 && value(20)==0 && value(21)==0,
        "main and constructors not reached; dependency bridge does not fabricate SDK diagnostic report");
    check(value(22)>=Process::HeapBase && value(22)<Process::HeapBase+Process::HeapAlignment &&
        whole.ram.read8(whole.ram.read64(value(22)))=='T' && whole.ram.read64(value(22)+8)==0,
        "diagnostic exposes real allocated environment with one TZ entry");
    uint64_t count=0;Machine::Result part{Machine::Stop::Budget,0};
    for(unsigned i=0;i<20000 && part.stop==Machine::Stop::Budget;++i) { part=sliced.run(7);count+=part.instructions; }
    check(part.stop==result.stop && part.exitCode==result.exitCode && count==result.instructions,"default failure path deterministic across small budgets");
    for(unsigned i=0;i<32;++i) check(sliced.ram.read64(report+8*i)==value(i),"default init diagnostic reports match");
    struct Exchange { uint32_t handle=0;uint64_t transport=0;Ipc::Buffer request{},response{}; };
    std::vector<Exchange> calls;std::vector<uint32_t> closed;
    Machine traced(false,true,true);check(traced.load(argv[1]),"trace fixture");const auto tls=traced.process().mainThread().localRegion;
    Machine::Result step{Machine::Stop::Budget,0};
    for(unsigned i=0;i<100000 && step.stop==Machine::Stop::Budget;++i) {
        const auto op=traced.ram.fetch32(traced.cpu.getPC());const bool svc=(op&0xFFE0001F)==0xD4000001;
        const auto id=(op>>5)&0xFFFF;Exchange e;
        if(svc && id==0x21) { e.handle=uint32_t(traced.cpu.readRegister(0));e.request=snapshot(traced.ram,tls); }
        if(svc && id==0x16) closed.push_back(uint32_t(traced.cpu.readRegister(0)));
        step=traced.run(1);
        if(svc && id==0x21) { e.transport=traced.cpu.readRegister(0);e.response=snapshot(traced.ram,tls);calls.push_back(e); }
    }
    check(step.stop==Machine::Stop::Exited && step.exitCode==missingMount,"trace reaches the named unlinked SD mounting layer");
    std::vector<std::string> requested;Ipc::Buffer apmLookup{},openRequest{},registration{};uint32_t apm=0,child=0,applet=0,messageEvent=0,managerHandle=0;
    for(const auto& e:calls) {
        if(e.transport==0 && get(e.response,8)==2 && get(e.response,32)==0x4F434653 && get(e.response,40)==0)
            messageEvent=uint32_t(get(e.response,12));
        if(get(e.request,0)!=4) continue;
        const bool pid=get(e.request,4)&0x80000000U;const size_t cmif=pid?32:16;
        if(get(e.request,cmif)!=0x49434653) continue;
        const auto command=get(e.request,cmif+8);
        if(pid && command==0 && !managerHandle) { registration=e.request;managerHandle=e.handle; }
        if(command==1 && e.handle==managerHandle) {
            std::string name;
            for(unsigned i=0;i<8 && e.request[cmif+16+i];++i) name+=char(e.request[cmif+16+i]);
            requested.push_back(name);
            if(name=="apm") {
                apmLookup=e.request;apm=uint32_t(get(e.response,12));
                check(e.transport==0 && get(e.response,24)==0 && (get(e.response,4)&0x80000000U) && apm,
                    "native APM lookup returns a real moved session");
            } else if(name=="appletOE") {
                applet=uint32_t(get(e.response,12));
                check(e.transport==0 && get(e.response,24)==0 && (get(e.response,4)&0x80000000U) && applet,
                    "native applet lookup returns a real moved session");
            } else if(name=="hid") {
                check(e.transport==0 && get(e.response,24)==0 && get(e.response,8)==32 && get(e.response,12)!=0,"native HID lookup returns a moved typed session");
            } else if(name=="time:u") {
                check(e.transport==0 && get(e.response,24)==0 && get(e.response,8)==32 && get(e.response,12)!=0,"native time lookup returns a moved typed session");
            } else if(name=="fsp-srv") {
                check(e.transport==0 && get(e.response,24)==0 && get(e.response,8)==32 && get(e.response,12)!=0,"native filesystem lookup returns a moved typed session");
            } else check(e.transport==0 && get(e.response,24)==Ipc::PortNotFound && !(get(e.response,4)&0x80000000U),
                "missing native services remain explicit errors without handles");
        }
        if(apm && e.handle==apm && command==0) {
            openRequest=e.request;child=uint32_t(get(e.response,12));
            check(e.transport==0 && get(e.response,24)==0 && (get(e.response,4)&0x80000000U) && child && child!=apm,
                "original apmInitialize opens a distinct typed child session");
        }
    }
    check(requested==std::vector<std::string>{"set:sys","apm","appletOE","hid","time:u","fsp-srv"},"real native service order includes original filesystem client");
    check(apm && child && applet && messageEvent && closed.empty(),
        "missing time dependency returns directly without fabricated applet/APM cleanup");
    check(get(openRequest,0)==4 && get(openRequest,4)==8 && get(openRequest,24)==0,"capture authentic no-payload APM OpenSession");
    {
        std::ofstream out(std::filesystem::path(argv[2])/"libnx-app-ipc.csv",std::ios::trunc);out<<"handle,transport,request_hex,response_hex\n";
        auto hex=[&](const Ipc::Buffer& b) { out<<std::hex<<std::setfill('0');for(auto byte:b) out<<std::setw(2)<<unsigned(byte);out<<std::dec; };
        for(const auto& e:calls) { out<<e.handle<<','<<e.transport<<',';hex(e.request);out<<',';hex(e.response);out<<'\n'; }
        check(bool(out),"persist default SDK service trace");
    }
    if(!apm || !child) return 1;
    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;std::string error;Process p;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error,true),"native APM replay fixture");
    Bus bus(ram);CPU cpu(bus);const auto ipc=p.mainThread().localRegion;
    const std::array<uint8_t,4> name{'s','m',':',0};ram.writeBytes(report+0x300,name.data(),name.size());
    cpu.writeRegister(1,report+0x300);p.dispatch(0x1F,cpu,bus);const auto manager=uint32_t(cpu.readRegister(1));
    auto send=[&](const Ipc::Buffer& request,uint32_t handle) {
        ram.writeBytes(ipc,request.data(),request.size());cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);return cpu.readRegister(0);
    };
    check(send(registration,manager)==0 && ram.read32(ipc+24)==0,"register replay client");
    check(send(apmLookup,manager)==0 && ram.read32(ipc+24)==0,"open APM root replay");const auto root=ram.read32(ipc+12);
    ram.writeBytes(ipc,openRequest.data(),openRequest.size());ram.setPermissions(ipc,4096,Memory::Read);
    cpu.writeRegister(0,root);p.dispatch(0x21,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidPointer && snapshot(ram,ipc)==openRequest,"failed OpenSession reply publishes no handle");
    ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
    for(auto [at,v]:std::vector<std::pair<size_t,uint32_t>>{{0,6},{4,7},{16,0},{20,1},{28,1},{0,0x10004}}) {
        auto bad=openRequest;put(bad,at,v);const auto status=send(bad,root);
        check(status!=0 && snapshot(ram,ipc)==bad,"malformed APM requests rejected before reply or handle allocation");
    }
    auto extra=openRequest;put(extra,4,9);
    check(send(extra,root)==0 && ram.read32(ipc+24)==Ipc::InvalidArguments && !(ram.read32(ipc+4)&0x80000000U),"OpenSession payload rejected without moved handle");
    auto perf=openRequest;put(perf,24,1);
    check(send(perf,root)==0 && ram.read32(ipc+24)==Ipc::UnknownCommand,"performance mode is not fabricated");
    check(send(openRequest,root)==0 && ram.read32(ipc+24)==0,"typed APM child creation succeeds");const auto session=ram.read32(ipc+12);
    check(session==root+1,"failed APM calls did not consume handle IDs");
    check(send(openRequest,session)==0 && ram.read32(ipc+24)==Ipc::UnknownCommand,"child cannot recursively open sessions");
    auto configure=openRequest;put(configure,4,10);
    check(send(configure,session)==0 && ram.read32(ipc+24)==Ipc::UnknownCommand,"unsupported power configuration has no fake success");
    Ipc::Buffer close{};put(close,0,2);check(send(close,root)==0,"close APM parent endpoint");
    cpu.writeRegister(0,root);p.dispatch(0x16,cpu,bus);
    check(send(openRequest,session)==0 && ram.read32(ipc+24)==Ipc::UnknownCommand,"child remains independent after parent closure");
    check(send(openRequest,root)==Process::InvalidHandle,"closed APM parent ID cannot be used");
    check(send(close,session)==0,"close child endpoint");cpu.writeRegister(0,session);p.dispatch(0x16,cpu,bus);
    check(send(openRequest,session)==Process::InvalidHandle,"closed APM child ID cannot be used");
    // Fill the real shared handle table with children; failed creation must not leak a slot or ID.
    check(send(apmLookup,manager)==0,"reopen root for resource limits");const auto limitedRoot=ram.read32(ipc+12);
    std::vector<uint32_t> children;
    for(unsigned i=0;i<125;++i) { check(send(openRequest,limitedRoot)==0 && ram.read32(ipc+24)==0,"APM children within shared handle quota");children.push_back(ram.read32(ipc+12)); }
    check(send(openRequest,limitedRoot)==0 && ram.read32(ipc+24)==Process::OutOfHandles && !(ram.read32(ipc+4)&0x80000000U),"APM quota failure has no moved handle");
    cpu.writeRegister(0,children.front());p.dispatch(0x16,cpu,bus);
    check(send(openRequest,limitedRoot)==0 && ram.read32(ipc+12)==children.back()+1,"freed APM slot reused without reusing a stale ID");
    Memory legacyRam(Machine::RAM_SIZE);ProgramLoader::Image legacyImage;Process legacy;
    const auto legacyFile=std::filesystem::path(argv[1]).parent_path().parent_path()/"libnx_init"/"program.elf";
    check(ProgramLoader::loadImage(legacyRam,legacyFile.string().c_str(),legacyImage,error) &&
        legacy.initialize(legacyRam,legacyImage,true,error),"legacy reference profile fixture");
    Bus legacyBus(legacyRam);CPU legacyCpu(legacyBus);const auto legacyIpc=legacy.mainThread().localRegion;
    legacyRam.writeBytes(0x23000+0x300,name.data(),name.size());legacyCpu.writeRegister(1,0x23000+0x300);
    legacy.dispatch(0x1F,legacyCpu,legacyBus);const auto legacyManager=uint32_t(legacyCpu.readRegister(1));
    auto legacySend=[&](const Ipc::Buffer& request) {
        legacyRam.writeBytes(legacyIpc,request.data(),request.size());legacyCpu.writeRegister(0,legacyManager);
        legacy.dispatch(0x21,legacyCpu,legacyBus);
    };
    legacySend(registration);check(legacyCpu.readRegister(0)==0 && legacyRam.read32(legacyIpc+24)==0,"legacy manager registration");
    legacySend(apmLookup);
    check(legacyCpu.readRegister(0)==0 && legacyRam.read32(legacyIpc+24)==Ipc::PortNotFound &&
        !(legacyRam.read32(legacyIpc+4)&0x80000000U),"native APM support requires the explicit homebrew profile");
    std::cout<<"Default libnx app instructions="<<result.instructions<<" diagnostic="<<result.exitCode<<" IPC="<<calls.size()<<" failures="<<failures<<'\n';
    return failures?1:0;
}
