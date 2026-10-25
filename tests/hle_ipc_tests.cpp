#include "machine.hpp"
#include "ipc.hpp"
#include <iostream>
#include <vector>

static void put(Ipc::Buffer& b,size_t offset,uint64_t value,size_t n=4)
{ for(size_t i=0;i<n;++i) b[offset+i]=uint8_t(value>>(8*i)); }
static uint64_t get(const Ipc::Buffer& b,size_t offset,size_t n=4)
{ uint64_t value=0; for(size_t i=0;i<n;++i) value|=uint64_t(b[offset+i])<<(8*i); return value; }
static Ipc::Buffer addRequest(uint64_t a=20,uint64_t b=35)
{
    Ipc::Buffer message{};
    put(message,0,4); put(message,4,12); put(message,16,0x49434653);
    put(message,32,a,8); put(message,40,b,8);
    return message;
}

int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } };
    Machine m(false,true), sliced(false,true);
    check(m.load(argv[1]) && sliced.load(argv[1]),"load independent IPC ELF");
    m.advanceTime(12345); sliced.advanceTime(12345);
    auto whole=m.run(1000);
    check(whole.stop==Machine::Stop::Exited && m.ram.read32(0x12000)==55 && m.ram.read32(0x12004)==99 &&
        m.ram.read64(0x12010)==12345 && m.ram.read32(0x12018)==1,"two threads share session and receive independent replies");
    uint64_t count=0;
    for(unsigned i=0;i<1000;++i) {
        auto part=sliced.run(1); count+=part.instructions;
        if(part.stop==Machine::Stop::Exited) break;
        check(part.stop==Machine::Stop::Budget,"IPC does not stall ready thread");
    }
    check(count==whole.instructions && sliced.ram.read32(0x12018)==1,"IPC integration invariant under sliced budgets");
    check(m.ram.read64(Process::WorkerTlsPage+0x220)==99 && m.ram.read64(m.process().mainThread().localRegion+32)==12345,
        "worker and main IPC regions distinct");

    Ipc::Buffer response{};
    auto request=addRequest(UINT64_MAX,2);
    check(Ipc::dispatchDemo(request,response,0)==0 && get(response,0)==0 && get(response,4)==10 &&
        get(response,16)==0x4F434653 && get(response,24)==0 && get(response,32,8)==1,"HIPC framing SFCO and modulo-64 sum");
    request=addRequest(); put(request,4,8); put(request,24,1);
    check(Ipc::dispatchDemo(request,response,12345)==0 && get(response,32,8)==12345,"virtual time command");
    put(request,24,999);
    check(Ipc::dispatchDemo(request,response,0)==0 && get(response,24)==Ipc::UnknownCommand && get(response,4)==8,"unknown command is a service error in successful transport");
    put(request,24,0);
    check(Ipc::dispatchDemo(request,response,0)==0 && get(response,24)==Ipc::InvalidArguments,"wrong service payload length");
    for(auto [offset,value] : std::vector<std::pair<size_t,uint32_t>>{
        {0,5},{0,0x10004},{4,0x8000000C},{4,0x40C},{4,1023},{4,0},{4,7},{16,0},{20,1},{28,1}})
    {
        auto malformed=addRequest(); put(malformed,offset,value);
        Ipc::Buffer sentinel; sentinel.fill(0xA5); auto unchanged=sentinel;
        check(Ipc::dispatchDemo(malformed,sentinel,0)!=0 && sentinel==unchanged,"invalid or unsupported frame leaves output untouched");
    }

    Memory ram(Machine::RAM_SIZE); ProgramLoader::Image image; Process p; std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"dispatcher fixture");
    Bus bus(ram); CPU cpu(bus);
    const auto tls=p.mainThread().localRegion;
    auto connect=[&](uint64_t address=0x11000) {
        cpu.writeRegister(1,address); p.dispatch(0x1F,cpu,bus);
        return uint32_t(cpu.readRegister(1));
    };
    auto session=connect(); check(cpu.readRegister(0)==0,"connect local named port");
    auto writeRequest=[&](const Ipc::Buffer& bytes) { return ram.writeBytes(tls,bytes.data(),bytes.size()); };
    auto readBuffer=[&]() { Ipc::Buffer b{}; for(size_t i=0;i<b.size();++i) b[i]=ram.read8(tls+i); return b; };
    auto send=[&](uint32_t handle) { cpu.writeRegister(0,handle); p.dispatch(0x21,cpu,bus); return uint32_t(cpu.readRegister(0)); };
    request=addRequest(); check(writeRequest(request) && send(session)==0 && get(readBuffer(),32,8)==55,"session dispatch writes reply into canonical TLS");
    auto original=cpu.captureContext(); original.vectors[31]={0xABC,0xDEF}; original.fpcr=0x400000; original.fpsr=0x8000010;
    original.pc=0x10000; original.sp=Machine::RAM_SIZE; original.tpidrEl0=0x12345; original.pstate=0xA0000000;
    original.registers[1]=0x1234; cpu.restoreContext(original);
    writeRequest(request); send(session);
    auto after=cpu.captureContext();
    check(after.vectors==original.vectors && after.fpcr==original.fpcr && after.fpsr==original.fpsr &&
        after.pc==original.pc && after.sp==original.sp && after.tpidrEl0==original.tpidrEl0 && after.pstate==original.pstate &&
        after.registers[1]==original.registers[1],"synchronous reply preserves context beyond W0");
    ram.write64(tls+0x100,0xFEEDBEEF);
    writeRequest(request); send(session); check(ram.read64(tls+0x100)==0xFEEDBEEF,"IPC stays inside 256-byte command buffer");
    put(request,4,1023); writeRequest(request);
    check(send(session)==Ipc::InvalidMessage && readBuffer()==request,"malformed request unchanged in guest RAM");
    request=addRequest(); writeRequest(request);
    const auto page=tls & ~uint64_t(Memory::PAGE_SIZE-1);
    ram.setPermissions(page,Memory::PAGE_SIZE,Memory::Read);
    check(send(session)==Process::InvalidPointer && readBuffer()==request,"read-only reply buffer rejected before modification");
    ram.setPermissions(page,Memory::PAGE_SIZE,Memory::Write);
    check(send(session)==Process::InvalidPointer,"write-only request buffer rejected");
    ram.setPermissions(page,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
    check(send(session)==0,"session remains usable after local transport error");
    for(auto svc : {0x11,0x12,0x17,0x09}) {
        cpu.writeRegister(0,session); p.dispatch(uint16_t(svc),cpu,bus);
        check(cpu.readRegister(0)==Process::InvalidHandle,"session cannot be used as event or thread");
    }
    ram.write32(0x12000,session); cpu.writeRegister(1,0x12000); cpu.writeRegister(2,1); cpu.writeRegister(3,0);
    p.dispatch(0x18,cpu,bus); check(cpu.readRegister(0)==Process::InvalidHandle,"waiting on local synchronous session explicitly rejected");
    p.dispatch(0x45,cpu,bus); auto event=uint32_t(cpu.readRegister(1));
    check(send(event)==Process::InvalidHandle,"event cannot be used as session");
    connect(Machine::DEVICE_BASE); check(cpu.readRegister(0)==Process::InvalidPointer,"MMIO port name rejected");
    connect(UINT64_MAX); check(cpu.readRegister(0)==Process::InvalidPointer,"overflow port name rejected");
    const uint8_t unknown[]={'x',0}; ram.writeBytes(0x12020,unknown,sizeof unknown);
    check(connect(0x12020)==0x12020 && cpu.readRegister(0)==Ipc::PortNotFound,"unknown port does not return fake session");
    for(size_t i=0;i<12;++i) ram.write8(0x12020+i,'x');
    connect(0x12020); check(cpu.readRegister(0)==Process::InvalidSize,"unterminated name bounded to 12 bytes");
    ram.write32(0x12FFC,0x3A756D65); connect(0x12FFC);
    check(cpu.readRegister(0)==Process::InvalidPointer,"name crossing unreadable page rejected");
    cpu.writeRegister(0,session); p.dispatch(0x16,cpu,bus);
    check(send(session)==Process::InvalidHandle,"closed session invalid");
    auto replacement=connect(); check(cpu.readRegister(0)==0 && replacement!=session && send(session)==Process::InvalidHandle,"closed ID never aliases new session");
    check(p.initialize(ram,image,true,error) && send(replacement)==Process::InvalidHandle,"reload invalidates session table");
    std::vector<uint32_t> sessions;
    for(unsigned i=0;i<127;++i) { auto h=connect(); check(cpu.readRegister(0)==0,"session allocation within quota"); sessions.push_back(h); }
    cpu.writeRegister(1,0xAB); cpu.writeRegister(2,0xCD); p.dispatch(0x45,cpu,bus);
    check(cpu.readRegister(0)==Process::OutOfHandles && cpu.readRegister(1)==0xAB && cpu.readRegister(2)==0xCD,"session slots count toward atomic event pair allocation");
    auto last=connect(); check(cpu.readRegister(0)==0,"last session slot");
    connect(); check(cpu.readRegister(0)==Process::OutOfHandles,"full shared handle table rejected");
    cpu.writeRegister(0,last); p.dispatch(0x16,cpu,bus);
    cpu.writeRegister(0,sessions.back()); p.dispatch(0x16,cpu,bus);
    p.dispatch(0x45,cpu,bus); check(cpu.readRegister(0)==0,"event pair reuses released session slots");
    std::cout<<"IPC integration instructions="<<whole.instructions<<" failures="<<failures<<'\n';
    return failures?1:0;
}
