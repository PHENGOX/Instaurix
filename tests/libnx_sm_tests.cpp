#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

static uint64_t get(const Ipc::Buffer& bytes,size_t at,size_t size=4)
{ uint64_t value=0;for(size_t i=0;i<size;++i) value|=uint64_t(bytes[at+i])<<(8*i);return value; }
static void put(Ipc::Buffer& bytes,size_t at,uint64_t value,size_t size=4)
{ for(size_t i=0;i<size;++i) bytes[at+i]=uint8_t(value>>(8*i)); }
static Ipc::Buffer snapshot(const Memory& ram,uint64_t address)
{ Ipc::Buffer b{};for(size_t i=0;i<b.size();++i) b[i]=ram.read8(address+i);return b; }
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    unsigned failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    constexpr uint64_t report=0x23000;
    Machine whole(false,true,true), sliced(false,true,true);
    if(!whole.load(argv[1]) || !sliced.load(argv[1])) { std::cerr<<whole.error()<<'\n';return 1; }
    const auto result=whole.run(100000);
    auto value=[&](unsigned index) { return whole.ram.read64(report+index*8); };
    check(result.stop==Machine::Stop::Exited && result.exitCode==0 && value(0)==15 && value(14)==77 && value(15)==0,
        "original sm client completes main and cleanup without dependency traps");
    check(value(16)==0 && value(17)==0 && value(18)!=0 && value(19)==0,"real smInitialize and repeated guarded initialize succeed");
    check(value(20)==Ipc::PortNotFound && value(21)==0 && value(22)==0 && value(23)!=value(18) && value(23)!=0 &&
        value(24)==Ipc::PortNotFound && value(25)==value(18) && value(26)==Ipc::PortNotFound && value(27)==0 &&
        value(28)==0 && value(29)==0,"native unavailable service error, clone independence and balanced reference cleanup");
    const auto tls=whole.process().mainThread().localRegion;
    check(whole.ram.read32(tls+0x1E0)==0x21545624 && whole.ram.read32(tls+0x1E4)==whole.process().mainHandle(),
        "SDK IPC buffer usage preserves ThreadVars");
    uint64_t instructions=0;Machine::Result part{Machine::Stop::Budget,0};
    for(unsigned i=0;i<20000 && part.stop==Machine::Stop::Budget;++i) { part=sliced.run(7);instructions+=part.instructions; }
    check(part.stop==result.stop && part.exitCode==result.exitCode && instructions==result.instructions,"SM client deterministic with seven-instruction budgets");
    for(unsigned i=0;i<32;++i) check(sliced.ram.read64(report+i*8)==value(i),"SM reports match across budgets");

    struct Exchange { uint32_t handle=0;Ipc::Buffer request{},response{};uint64_t transport=0; };
    std::vector<Exchange> exchanges;std::vector<uint32_t> closed;unsigned connects=0;
    Machine traced(false,true,true);check(traced.load(argv[1]),"trace fixture");
    Machine::Result step{Machine::Stop::Budget,0};
    for(unsigned i=0;i<100000 && step.stop==Machine::Stop::Budget;++i) {
        const auto pc=traced.cpu.getPC();const auto opcode=traced.ram.fetch32(pc);
        const bool svc=(opcode&0xFFE0001FU)==0xD4000001U;
        const auto id=(opcode>>5)&0xFFFF;
        Exchange exchange;
        if(svc && id==0x21) { exchange.handle=uint32_t(traced.cpu.readRegister(0));exchange.request=snapshot(traced.ram,tls); }
        if(svc && id==0x1F) ++connects;
        if(svc && id==0x16) closed.push_back(uint32_t(traced.cpu.readRegister(0)));
        step=traced.run(1);
        if(svc && id==0x21) { exchange.transport=traced.cpu.readRegister(0);exchange.response=snapshot(traced.ram,tls);exchanges.push_back(exchange); }
    }
    check(step.stop==Machine::Stop::Exited && connects==1 && exchanges.size()==8 && closed.size()==2,
        "SDK guard produces one connection, eight IPC exchanges and exactly two handle closures");
    if(exchanges.size()!=8 || closed.size()!=2) return 1;
    const auto manager=uint32_t(value(18)), clone=uint32_t(value(23));
    const std::array<unsigned,8> types{5,4,4,5,4,2,4,2};
    for(size_t i=0;i<exchanges.size();++i) {
        const auto& e=exchanges[i];
        check(e.transport==0 && get(e.request,0)==types[i],"authentic HIPC types accepted with successful transport");
        if(types[i]!=2) check(get(e.response,16)==0x4F434653,"SDK receives an SFCO response");
        else check(e.request==e.response,"HIPC Close has no fabricated response body");
    }
    check(get(exchanges[0].request,24)==3 && get(exchanges[0].response,32,2)==0 &&
        get(exchanges[0].response,4)==9,"SDK pointer-buffer query receives a zero u16 with correct raw size");
    check(get(exchanges[1].request,4)==0x8000000A && get(exchanges[1].request,8)==1 &&
        get(exchanges[1].request,32)==0x49434653 && get(exchanges[1].request,40)==0 &&
        get(exchanges[1].response,24)==0,"actual RegisterClient CMIF includes SendPid and gets success");
    check(get(exchanges[3].request,24)==2 && get(exchanges[3].response,4)==0x80000008 &&
        get(exchanges[3].response,8)==32 && get(exchanges[3].response,12)==clone,
        "actual clone control receives one moved handle");
    for(size_t i:{size_t(2),size_t(4),size_t(6)}) {
        const auto& e=exchanges[i];
        check(get(e.request,24)==1 && get(e.response,24)==Ipc::PortNotFound && !(get(e.response,4)&0x80000000),
            "unavailable native GetService returns a service error without a moved handle");
        constexpr char name[8]={'s','e','t',':','s','y','s',0};
        for(size_t j=0;j<8;++j) check(e.request[32+j]==uint8_t(name[j]),"GetService uses the native service name");
    }
    check(exchanges[4].handle==clone && exchanges[5].handle==clone && exchanges[6].handle==manager &&
        exchanges[7].handle==manager && closed[0]==clone && closed[1]==manager,"real clone closes independently and original closes at final smExit");
    {
        std::ofstream out(std::filesystem::path(argv[2])/"libnx-sm-ipc.csv",std::ios::trunc);
        out<<"exchange,handle,transport,request_hex,response_hex\n";
        auto hex=[&](const Ipc::Buffer& b) { out<<std::hex<<std::setfill('0');for(auto byte:b) out<<std::setw(2)<<unsigned(byte);out<<std::dec; };
        for(size_t i=0;i<exchanges.size();++i) { const auto& e=exchanges[i];out<<i<<','<<e.handle<<','<<e.transport<<',';hex(e.request);out<<',';hex(e.response);out<<'\n'; }
        check(bool(out),"save observed original SDK request and response bytes");
    }

    // Replay the actual SDK bytes to exercise malformed requests and failed reply atomicity.
    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;Process p;std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error,true),"replay process fixture");
    Bus bus(ram);CPU cpu(bus);const auto ipc=p.mainThread().localRegion;
    const std::array<uint8_t,4> port{'s','m',':',0};ram.writeBytes(report+0x300,port.data(),port.size());
    cpu.writeRegister(1,report+0x300);p.dispatch(0x1F,cpu,bus);const auto handle=uint32_t(cpu.readRegister(1));
    auto sendTo=[&](const Ipc::Buffer& b,uint32_t session) { ram.writeBytes(ipc,b.data(),b.size());cpu.writeRegister(0,session);p.dispatch(0x21,cpu,bus);return cpu.readRegister(0); };
    auto send=[&](const Ipc::Buffer& b) { return sendTo(b,handle); };
    auto requested=exchanges[2].request;
    check(send(requested)==0 && ram.read32(ipc+24)==Ipc::NotInitialized,"SDK GetService cannot precede RegisterClient");
    const auto registration=exchanges[1].request;
    ram.writeBytes(ipc,registration.data(),registration.size());ram.setPermissions(ipc,4096,Memory::Read);
    cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidPointer && snapshot(ram,ipc)==registration,"failed SDK reply preserves request bytes");
    ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
    check(send(requested)==0 && ram.read32(ipc+24)==Ipc::NotInitialized,"failed SDK registration did not change state");
    for(auto [offset,value]:std::vector<std::pair<size_t,uint32_t>>{{4,0x80000009},{8,3},{32,0},{36,1},{44,1}}) {
        auto bad=registration;put(bad,offset,value);
        const auto transport=send(bad);
        check(transport!=0 || ram.read32(ipc+24)!=0,"malformed SDK registration never succeeds");
        check(send(requested)==0 && ram.read32(ipc+24)==Ipc::NotInitialized,"malformed SDK registration cannot publish registration state");
    }
    check(send(registration)==0 && ram.read32(ipc+24)==0,"actual SDK RegisterClient replay succeeds");
    check(send(registration)==0 && ram.read32(ipc+24)==Ipc::AlreadyInitialized,"duplicate registration has an explicit service error");
    check(send(requested)==0 && ram.read32(ipc+24)==Ipc::PortNotFound,"native unknown service error survives real SDK replay");
    auto forgedPid=registration;put(forgedPid,12,UINT64_MAX,8);
    Ipc::ManagerRequest parsed;
    check(Ipc::parseManager(forgedPid,false,parsed)==0 && parsed.initialize,"guest PID bytes are not trusted as a process identity");
    cpu.writeRegister(1,report+0x300);p.dispatch(0x1F,cpu,bus);const auto second=uint32_t(cpu.readRegister(1));
    check(second!=handle && sendTo(requested,second)==0 && ram.read32(ipc+24)==Ipc::NotInitialized,
        "separate SM connections do not share registration state");
    check(sendTo(registration,second)==0 && ram.read32(ipc+24)==0,"second client registers independently using SDK bytes");
    check(sendTo(exchanges[7].request,second)==0,"SDK close request closes the independent session");
    cpu.writeRegister(0,second);p.dispatch(0x16,cpu,bus);
    check(cpu.readRegister(0)==0 && sendTo(requested,second)==Process::InvalidHandle && snapshot(ram,ipc)==requested,
        "closed SDK session ID is rejected without modifying the request");
    check(send(requested)==0 && ram.read32(ipc+24)==Ipc::PortNotFound,"first client remains live after independent client closure");
    std::cout<<"Original libnx SM instructions="<<result.instructions<<" exchanges="<<exchanges.size()<<" failures="<<failures<<'\n';
    return failures?1:0;
}
