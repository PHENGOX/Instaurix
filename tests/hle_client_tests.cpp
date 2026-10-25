#include "machine.hpp"
#include "ipc.hpp"
#include <iostream>
#include <vector>

static void put(Ipc::Buffer& b,size_t at,uint64_t value,size_t count=4)
{ for(size_t i=0;i<count;++i) b[at+i]=uint8_t(value>>(8*i)); }
static Ipc::Buffer control(uint32_t command)
{
    Ipc::Buffer b{}; put(b,0,5); put(b,4,8); put(b,16,0x49434653); put(b,24,command); return b;
}
static Ipc::Buffer add()
{
    auto b=control(0); put(b,0,4); put(b,4,12); put(b,32,20,8); put(b,40,35,8); return b;
}
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } };
    Machine m(false,true), sliced(false,true);
    check(m.load(argv[1]) && sliced.load(argv[1]),"load Clang-compiled C client");
    m.advanceTime(12345); sliced.advanceTime(12345);
    auto original=m.cpu.captureContext(); original.vectors[31]={0xABC,0xDEF}; original.fpcr=0x400000; original.fpsr=0x8000010;
    m.cpu.restoreContext(original);
    auto whole=m.run(50000);
    uint32_t expected=2166136261U;
    for(unsigned i=0;i<512;++i) expected=(expected^uint8_t(i*17+3))*16777619U;
    check(whole.stop==Machine::Stop::Exited && m.ram.read64(0x12000)==1 && m.ram.read64(0x12008)==55 &&
        m.ram.read64(0x12010)==512 && m.ram.read64(0x12018)==expected && m.ram.read64(0x12020)==0 &&
        m.ram.read64(0x12028)==12345 && m.ram.read64(0x12030)==7,"C client validates clone shared state add copy hash time and closure");
    const auto final=m.cpu.captureContext();
    check(final.sp==original.sp && final.vectors==original.vectors && final.fpcr==original.fpcr && final.fpsr==original.fpsr &&
        final.tpidrroEl0==original.tpidrroEl0,"C call stack and FP TLS state preserved");
    uint64_t count=0;
    for(unsigned i=0;i<20000;++i) {
        auto part=sliced.run(7); count+=part.instructions;
        if(part.stop==Machine::Stop::Exited) break;
        check(part.stop==Machine::Stop::Budget,"compiled client stays runnable");
    }
    check(count==whole.instructions && sliced.ram.read64(0x12000)==1 && sliced.ram.read64(0x12018)==expected,
        "compiled client deterministic under seven-instruction slices");

    Memory ram(Machine::RAM_SIZE); ProgramLoader::Image image; Process p; std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"session fixture");
    Bus bus(ram); CPU cpu(bus); const auto tls=p.mainThread().localRegion;
    constexpr uint8_t name[]{'e','m','u',':','d','e','m','o',0};
    ram.writeBytes(0x12800,name,sizeof name);
    auto connect=[&]() {
        cpu.writeRegister(1,0x12800); p.dispatch(0x1F,cpu,bus);
        check(cpu.readRegister(0)==0,"direct demo connection"); return uint32_t(cpu.readRegister(1));
    };
    auto send=[&](const Ipc::Buffer& b,uint32_t handle) {
        ram.writeBytes(tls,b.data(),b.size()); cpu.writeRegister(0,handle); p.dispatch(0x21,cpu,bus);
        return uint32_t(cpu.readRegister(0));
    };
    auto unchanged=[&](const Ipc::Buffer& b) { bool same=true; for(size_t i=0;i<b.size();++i) same &= ram.read8(tls+i)==b[i]; return same; };
    auto originalHandle=connect();
    check(send(control(3),originalHandle)==0 && ram.read32(tls+24)==0 && ram.read32(tls+4)==9 && ram.read16(tls+32)==0,
        "pointer buffer query returns u16 zero with correct raw length");
    auto cloneRequest=control(2); ram.writeBytes(tls,cloneRequest.data(),cloneRequest.size());
    const auto page=tls&~uint64_t(4095);
    ram.setPermissions(page,4096,Memory::Read);
    cpu.writeRegister(0,originalHandle); p.dispatch(0x21,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidPointer && unchanged(cloneRequest),"failed clone reply leaves no visible mutation");
    ram.setPermissions(page,4096,Memory::Read|Memory::Write);
    check(send(cloneRequest,originalHandle)==0 && ram.read32(tls+4)==0x80000008 && ram.read32(tls+8)==32 && ram.read32(tls+12)==2,
        "clone publishes exactly one new moved handle without consuming failed ID");
    const auto clone=ram.read32(tls+12);
    for(auto [at,value] : std::vector<std::pair<size_t,uint32_t>>{{0,0x10005},{4,1023},{4,0},{4,0x80000008},{16,0},{20,1},{28,1}})
    {
        auto bad=control(2); put(bad,at,value);
        check(send(bad,originalHandle)!=0 && unchanged(bad),"malformed control rejected before reply");
    }
    auto extra=control(2); put(extra,4,9);
    check(send(extra,originalHandle)==0 && ram.read32(tls+24)==Ipc::InvalidArguments && ram.read32(tls+4)==8,"clone payload rejected without moved handle");
    check(send(control(0),originalHandle)==0 && ram.read32(tls+24)==Ipc::UnknownCommand,"domain conversion is not falsely accepted");
    Ipc::Buffer close{}; put(close,0,2);
    auto malformedClose=close; put(malformedClose,4,1);
    check(send(malformedClose,originalHandle)==Ipc::InvalidMessage && unchanged(malformedClose),"invalid close cannot terminate session");
    check(send(add(),originalHandle)==0 && ram.read64(tls+32)==55,"session remains live after malformed close");
    check(send(close,originalHandle)==0 && unchanged(close),"valid close changes endpoint state without writing SFCO reply");
    check(send(add(),originalHandle)==Ipc::SessionClosed && unchanged(add()),"closed endpoint rejects requests and preserves request bytes");
    check(send(add(),clone)==0 && ram.read64(tls+32)==55,"clone stays live after original CMIF close");
    cpu.writeRegister(0,originalHandle); p.dispatch(0x16,cpu,bus);
    check(cpu.readRegister(0)==0 && send(add(),originalHandle)==Process::InvalidHandle,"CloseHandle releases closed endpoint");
    check(p.initialize(ram,image,true,error),"reset session fixture");
    originalHandle=connect();
    std::vector<uint32_t> clones;
    for(unsigned i=0;i<127;++i) {
        check(send(control(2),originalHandle)==0 && ram.read32(tls+24)==0,"clone within handle quota"); clones.push_back(ram.read32(tls+12));
    }
    check(send(control(2),originalHandle)==0 && ram.read32(tls+24)==Process::OutOfHandles && ram.read32(tls+4)==8,"clone table exhaustion is service error without extra handle");
    check(send(close,clones.front())==0,"close endpoint in full table");
    check(send(control(2),originalHandle)==0 && ram.read32(tls+24)==Process::OutOfHandles,"CMIF close alone retains handle slot");
    cpu.writeRegister(0,clones.front()); p.dispatch(0x16,cpu,bus);
    check(send(control(2),originalHandle)==0 && ram.read32(tls+12)==129,"CloseHandle frees slot and failed clone does not consume ID");
    check(send(add(),clones.front())==Process::InvalidHandle,"old closed ID cannot reach replacement clone");
    std::cout<<"Compiled C client instructions="<<whole.instructions<<" hash="<<expected<<" failures="<<failures<<'\n';
    return failures?1:0;
}
