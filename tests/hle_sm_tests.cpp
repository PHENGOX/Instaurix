#include "machine.hpp"
#include "ipc.hpp"
#include <iostream>
#include <vector>

static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t count=4)
{ for(size_t i=0;i<count;++i) b[at+i]=uint8_t(v>>(8*i)); }
static Ipc::Buffer initializeRequest()
{
    Ipc::Buffer b{}; put(b,0,4); put(b,4,0x8000000A); put(b,8,1);
    put(b,12,0xDEADBEEF,8); put(b,32,0x49434653); return b;
}
static Ipc::Buffer serviceRequest()
{
    Ipc::Buffer b{}; put(b,0,4); put(b,4,10); put(b,16,0x49434653); put(b,24,1);
    const char* name="emu:demo"; for(size_t i=0;i<8;++i) b[32+i]=uint8_t(name[i]); return b;
}
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } };
    Machine m(false,true), sliced(false,true);
    check(m.load(argv[1]) && sliced.load(argv[1]),"load independent service-manager ELF");
    const auto whole=m.run(1000);
    check(whole.stop==Machine::Stop::Exited && m.ram.read64(0x12008)==55 && m.ram.read32(0x12010)==1 &&
        m.ram.read32(0x12000)!=m.ram.read32(0x12004),"guest obtains moved session closes manager and uses child service");
    uint64_t count=0;
    for(unsigned i=0;i<1000;++i) {
        auto part=sliced.run(1); count+=part.instructions;
        if(part.stop==Machine::Stop::Exited) break;
        check(part.stop==Machine::Stop::Budget,"service exchange remains runnable");
    }
    check(count==whole.instructions && sliced.ram.read32(0x12010)==1,"service manager independent of budget slicing");

    Memory ram(Machine::RAM_SIZE); ProgramLoader::Image image; Process p; std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"manager dispatcher fixture");
    Bus bus(ram); CPU cpu(bus); const auto tls=p.mainThread().localRegion;
    auto connect=[&]() {
        cpu.writeRegister(1,0x11000); p.dispatch(0x1F,cpu,bus);
        check(cpu.readRegister(0)==0,"connect sm port"); return uint32_t(cpu.readRegister(1));
    };
    auto manager=connect();
    auto call=[&](uint32_t handle) { cpu.writeRegister(0,handle); p.dispatch(0x21,cpu,bus); return uint32_t(cpu.readRegister(0)); };
    auto send=[&](const Ipc::Buffer& b,uint32_t handle) { ram.writeBytes(tls,b.data(),b.size()); return call(handle); };
    auto unchanged=[&](const Ipc::Buffer& b) { bool same=true; for(size_t i=0;i<b.size();++i) same &= ram.read8(tls+i)==b[i]; return same; };
    check(send(serviceRequest(),manager)==0 && ram.read32(tls+24)==Ipc::NotInitialized && ram.read32(tls+4)==8,"GetService requires initialized manager without leaking handle");
    auto init=initializeRequest(); ram.writeBytes(tls,init.data(),init.size());
    const auto page=tls&~uint64_t(4095); ram.setPermissions(page,4096,Memory::Read);
    check(call(manager)==Process::InvalidPointer && unchanged(init),"failed initialize reply leaves request intact");
    ram.setPermissions(page,4096,Memory::Read|Memory::Write);
    check(send(serviceRequest(),manager)==0 && ram.read32(tls+24)==Ipc::NotInitialized,"failed initialize does not mutate registration state");
    check(send(init,manager)==0 && ram.read32(tls+24)==0 && ram.read32(tls+16)==0x4F434653,"initialize accepts PID marker without trusting guest PID");
    check(send(init,manager)==0 && ram.read32(tls+24)==Ipc::AlreadyInitialized,"duplicate initialize explicit service error");
    auto uninitialized=connect();
    check(send(serviceRequest(),uninitialized)==0 && ram.read32(tls+24)==Ipc::NotInitialized,"registration scoped to manager session");
    cpu.writeRegister(0,uninitialized); p.dispatch(0x16,cpu,bus);
    auto unknown=serviceRequest(); unknown[32]='x';
    check(send(unknown,manager)==0 && ram.read32(tls+24)==Ipc::PortNotFound && ram.read32(tls+4)==8,"unregistered service receives no handle");
    unknown=serviceRequest(); put(unknown,24,2);
    check(send(unknown,manager)==0 && ram.read32(tls+24)==Ipc::UnknownCommand,"dynamic registration not silently accepted");
    for(auto [at,value] : std::vector<std::pair<size_t,uint32_t>>{{8,3},{8,33},{4,0x800003FF},{36,1},{44,1},{0,0x10004}})
    {
        auto bad=init; put(bad,at,value);
        check(send(bad,manager)!=0 && unchanged(bad),"malformed manager envelope does not change guest memory");
    }
    auto get=serviceRequest(); ram.writeBytes(tls,get.data(),get.size());
    ram.setPermissions(page,4096,Memory::Read);
    check(call(manager)==Process::InvalidPointer && unchanged(get),"failed handle reply publishes no session");
    ram.setPermissions(page,4096,Memory::Read|Memory::Write);
    check(send(get,manager)==0 && ram.read32(tls+24)==0 && ram.read32(tls+4)==0x80000008 && ram.read32(tls+8)==32,
        "reply encodes one moved handle in HIPC special header");
    const auto child=ram.read32(tls+12);
    check(child==3,"failed lookup and failed reply do not consume IDs");
    check(send(get,manager)==0 && ram.read32(tls+12)!=child,"repeated lookup creates independent session");
    const auto second=ram.read32(tls+12);
    cpu.writeRegister(0,manager); p.dispatch(0x16,cpu,bus);
    Ipc::Buffer add{}; put(add,0,4); put(add,4,12); put(add,16,0x49434653); put(add,32,20,8); put(add,40,35,8);
    check(send(add,child)==0 && ram.read64(tls+32)==55,"moved session survives manager closure");
    check(call(manager)==Process::InvalidHandle,"closed manager invalid");
    cpu.writeRegister(0,child); p.dispatch(0x16,cpu,bus);
    check(send(add,child)==Process::InvalidHandle && send(add,second)==0,"closing one service session preserves another");
    check(p.initialize(ram,image,true,error) && call(second)==Process::InvalidHandle,"reload clears returned service handles");
    manager=connect(); send(init,manager);
    std::vector<uint32_t> children;
    for(unsigned i=0;i<127;++i) {
        check(send(get,manager)==0 && ram.read32(tls+24)==0,"service handle within quota"); children.push_back(ram.read32(tls+12));
    }
    check(send(get,manager)==0 && ram.read32(tls+24)==Process::OutOfHandles && ram.read32(tls+4)==8,"full table gives SFCO resource error without special handle");
    cpu.writeRegister(0,children.front()); p.dispatch(0x16,cpu,bus);
    check(send(get,manager)==0 && ram.read32(tls+24)==0 && ram.read32(tls+12)==129,"resource failure rolls back ID and closed slot is reusable");
    check(send(add,children.front())==Process::InvalidHandle,"recycled slot does not revive stale service handle");
    std::cout<<"Service manager instructions="<<whole.instructions<<" failures="<<failures<<'\n';
    return failures?1:0;
}
