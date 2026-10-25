#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

static void put(std::vector<uint8_t>& b,size_t at,uint32_t value)
{ for(size_t i=0;i<4;++i) b[at+i]=uint8_t(value>>(8*i)); }
static uint32_t read(const std::vector<uint8_t>& b,size_t at)
{ uint32_t v=0;for(size_t i=0;i<4;++i) v|=uint32_t(b[at+i])<<(8*i);return v; }
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    std::ifstream in(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
    if(bytes.size()!=32768) return 2;
    const auto module=read(bytes,4);
    check(module+64<bytes.size() && std::string(bytes.begin()+8,bytes.begin()+16)=="HOMEBREW","original crt0 entry header preserved");
    Machine m(false,true,true), sliced(false,true,true);
    if(!m.load(argv[1]) || !sliced.load(argv[1])) { std::cerr<<m.error()<<'\n';return 1; }
    for(auto* machine:{&m,&sliced}) {
        machine->ram.write64(0x18000,UINT64_MAX);machine->ram.write64(0x18FF8,UINT64_MAX);
    }
    const auto result=m.run(100000);
    check(result.stop==Machine::Stop::Exited && result.exitCode==0xC001 && m.ram.read64(0x17000)==1,
        "unchanged crt0 and dynamic reach the explicit init boundary trap");
    check(m.ram.read64(0x17008)==Process::HomebrewEnvironment && m.ram.read64(0x17010)==UINT64_MAX &&
        m.ram.read64(0x17018)==Process::HomebrewReturn && m.ram.read64(0x17020)==Machine::RAM_SIZE,
        "crt0 preserves and passes the real homebrew entry arguments to init");
    check(m.ram.read64(0x18000)==Machine::RAM_SIZE && m.ram.read64(0x18FF8)==0 && m.cpu.getSP()==Machine::RAM_SIZE,
        "dynamic clears BSS then crt0 saves and restores the initial stack");
    check(m.ram.read64(0x15000)==0x17000 && m.ram.permissionsAt(0x15000)==Memory::Read &&
        m.ram.permissionsAt(0x16000)==Memory::Read && m.ram.permissionsAt(0x17000)==(Memory::Read|Memory::Write),
        "guest relocates then protects both RELRO pages without changing data permissions");
    uint64_t instructions=0;Machine::Result part{Machine::Stop::Budget,0};
    for(unsigned i=0;i<15000 && part.stop==Machine::Stop::Budget;++i) { part=sliced.run(7);instructions+=part.instructions; }
    check(part.stop==result.stop && part.exitCode==result.exitCode && instructions==result.instructions,
        "original startup deterministic across seven-instruction budgets");
    for(unsigned i=0;i<8;++i) check(m.ram.read64(0x17000+i*8)==sliced.ram.read64(0x17000+i*8),"startup reports match");

    // Capture the actual guest calls, without changing any upstream instruction.
    Machine traced(false,true,true);
    check(traced.load(argv[1]),"trace fixture");
    std::vector<std::array<uint64_t,6>> calls;
    Machine::Result traceResult{Machine::Stop::Budget,0};
    for(unsigned i=0;i<100000 && traceResult.stop==Machine::Stop::Budget;++i) {
        const auto pc=traced.cpu.getPC();
        const auto instruction=traced.ram.fetch32(pc);
        if((instruction&0xFFE0001FU)==0xD4000001U)
            calls.push_back({(instruction>>5)&0xFFFF,pc,traced.cpu.readRegister(0),traced.cpu.readRegister(1),
                traced.cpu.readRegister(2),traced.cpu.readRegister(3)});
        traceResult=traced.run(1);
    }
    check(calls.size()==3 && calls[0][0]==2 && calls[0][2]==0x15000 && calls[0][3]==8192 && calls[0][4]==1 &&
        calls[1][0]==3 && calls[1][2]==0x15000 && calls[1][3]==8192 && calls[1][4]==16 && calls[1][5]==16 &&
        calls[2][0]==0xF001 && calls[2][1]==Process::HomebrewReturn && calls[2][2]==0xC001 &&
        traceResult.stop==Machine::Stop::Exited,"actual guest SVC trace is protect, lock, explicit boundary return");
    {
        std::ofstream out(std::filesystem::path(argv[2])/"libnx-crt0-trace.csv",std::ios::trunc);
        out<<"svc,pc,x0,x1,x2,x3\n";
        for(const auto& call:calls) {
            for(size_t i=0;i<call.size();++i) { if(i) out<<',';out<<call[i]; } out<<'\n';
        }
        check(bool(out),"persist the observed guest trace");
    }

    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;Process p;std::string error;
    check(ProgramLoader::loadNro(ram,bytes,image,error) && p.initialize(ram,image,true,error,true),"RELRO process fixture");
    Bus bus(ram);CPU cpu(bus);
    auto svc=[&](unsigned id,uint64_t address,uint64_t size,uint64_t value,uint64_t flags=0) {
        cpu.writeRegister(0,address);cpu.writeRegister(1,size);cpu.writeRegister(2,value);cpu.writeRegister(3,flags);
        const auto action=p.dispatch(uint16_t(id),cpu,bus);
        check(action==Process::Action::Continue,"RELRO SVC returns an explicit result");return cpu.readRegister(0);
    };
    auto query=[&](uint64_t address) {
        cpu.writeRegister(0,0x17080);cpu.writeRegister(2,address);p.dispatch(6,cpu,bus);
        check(cpu.readRegister(0)==0,"QueryMemory succeeds for RELRO");
    };
    check(svc(2,0x15001,4096,1)==Process::InvalidAddress && svc(2,0x15000,0,1)==Process::InvalidSize &&
        svc(2,0x15000,4097,1)==Process::InvalidSize,"permission address and size validation");
    check(svc(2,0x14000,4096,1)==Process::InvalidAddress && svc(2,0x16000,8192,1)==Process::InvalidAddress &&
        svc(2,Process::HeapBase,4096,1)==Process::InvalidAddress,"only the validated RELRO interval is mutable");
    check(svc(2,0x15000,8192,5)==Process::InvalidState && svc(3,0x15000,8192,16,16)==Process::InvalidState,
        "execute permission and locking writable pages rejected");
    check(svc(2,0x15000,8192,0)==0 && !ram.canAccess(0x15000,8192,Memory::Read),"temporary no-access permission applied");
    check(svc(2,0x15000,8192,3)==0 && svc(2,0x15000,4096,1)==0 && svc(3,0x15000,4096,16,16)==0,
        "first RELRO page made read-only then locked");
    check(svc(2,0x15000,8192,3)==Process::InvalidState && ram.permissionsAt(0x16000)==3,
        "permission change preflight prevents partial changes across locked and unlocked pages");
    check(svc(3,0x15000,8192,16,16)==Process::InvalidState,"locking mixed permissions rejected atomically");
    query(0x16000);check(ram.read32(0x17094)==0,"failed multi-page lock leaves second page attributes intact");
    query(0x15000);check(ram.read32(0x17094)==16 && ram.read64(0x17088)==4096,
        "QueryMemory splits regions at permission or attribute boundaries");
    check(svc(2,0x16000,4096,1)==0 && svc(3,0x15000,8192,16,16)==0,"all RELRO pages locked after complete protection");
    query(0x15000);check(ram.read64(0x17080)==0x15000 && ram.read64(0x17088)==8192 && ram.read32(0x17094)==16 &&
        ram.read32(0x17098)==1,"QueryMemory reports the coalesced locked read-only region");
    check(svc(2,0x15000,8192,3)==Process::InvalidState && svc(3,0x15000,8192,16,0)==Process::InvalidState &&
        svc(3,0x15000,8192,8,8)==Process::InvalidState,"locked RELRO cannot be reopened or have its lock cleared");
    check(!ram.writeBytes(0x15000,bytes.data(),1),"actual RAM enforces read-only RELRO");ram.clearError();

    const auto path=std::filesystem::path(argv[2])/"crt0-invalid.nro";
    for(auto [offset,value]:std::vector<std::pair<size_t,uint32_t>>{
        {28,0},{40,0},{52,0},{56,2},{60,1},{44,1},{48,1},{32,1},{36,1},
        {44,uint32_t(0x14000-(0x10000+module))},{48,uint32_t(0x19000-(0x10000+module))}}) {
        auto bad=bytes;put(bad,module+offset,value);
        { std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bad.data()),bad.size()); }
        const auto pc=m.cpu.getPC();
        check(!m.load(path.string().c_str()) && m.cpu.getPC()==pc && m.ram.read64(0x17000)==1 && m.run(1).exitCode==0xC001,
            "malformed LNY/GOT/RELRO rejected without replacing the completed session");
    }
    std::filesystem::remove(path);
    std::cout<<"Original libnx crt0 boundary instructions="<<result.instructions<<" marker="<<result.exitCode<<" failures="<<failures<<'\n';
    return failures?1:0;
}
