#include "machine.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    unsigned failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    Machine m(false,true,true),sliced(false,true,true);
    if(!m.load(argv[1]) || !sliced.load(argv[1])) { std::cerr<<m.error()<<'\n';return 1; }
    const auto tls=m.cpu.captureContext().tpidrroEl0;
    check(tls==Machine::RAM_SIZE-Process::StackSize-2*Memory::PAGE_SIZE && tls==m.process().mainThread().localRegion,
        "homebrew TPIDRRO_EL0 is the actual TLR base shared with IPC");
    const auto result=m.run(100000);
    constexpr uint64_t report=0x23000;
    auto value=[&](unsigned slot) { return m.ram.read64(report+slot*8); };
    check(result.stop==Machine::Stop::Exited && result.exitCode==0 && value(0)==15 && value(14)==77 && value(15)==0,
        "original libnx init reaches main, runs the newlib constructor, and performs app cleanup without dependency traps");
    check(value(1)==tls && value(2)==0x21545624 && value(3)==m.process().mainHandle() && value(4)!=0 && value(5)!=0,
        "actual newlibSetup and thread init populate ThreadVars");
    check(m.ram.read32(tls+0x1E0)==0x21545624 && m.ram.read32(tls+0x1E4)==value(3) &&
        m.ram.read64(tls+0x1E8)==value(5) && m.ram.read64(tls+0x1F0)==value(4) && m.ram.read64(tls+0x1F8)==value(6),
        "libnx ThreadVars occupy the correct offsets at the end of the TLR");
    check(value(7)==Process::HeapBase && value(8)==Process::HeapBase+Process::HeapAlignment &&
        value(9)==0x1122334455667788ULL && value(10)==0 && value(11)!=0 && value(12)==0 && value(13)==Process::StackSize,
        "real heap override, TLS data copy, empty argv and stack description");
    check(m.ram.read64(0x21000)==report && m.ram.permissionsAt(0x21000)==Memory::Read &&
        m.ram.permissionsAt(0x22000)==Memory::Read,"relocation and RELRO precede init");
    uint64_t total=0;Machine::Result part{Machine::Stop::Budget,0};
    for(unsigned i=0;i<20000 && part.stop==Machine::Stop::Budget;++i) { part=sliced.run(7);total+=part.instructions; }
    check(part.stop==result.stop && part.exitCode==result.exitCode && total==result.instructions,"original init is deterministic under small budgets");
    for(unsigned i=0;i<16;++i) check(sliced.ram.read64(report+i*8)==value(i),"sliced init report matches");

    Machine traced(false,true,true);check(traced.load(argv[1]),"trace load");
    std::vector<std::array<uint64_t,6>> calls;Machine::Result step{Machine::Stop::Budget,0};
    for(unsigned i=0;i<100000 && step.stop==Machine::Stop::Budget;++i) {
        const auto pc=traced.cpu.getPC();const auto opcode=traced.ram.fetch32(pc);
        if((opcode&0xFFE0001FU)==0xD4000001U) calls.push_back({(opcode>>5)&0xFFFF,pc,traced.cpu.readRegister(0),
            traced.cpu.readRegister(1),traced.cpu.readRegister(2),traced.cpu.readRegister(3)});
        step=traced.run(1);
    }
    const std::array<unsigned,9> infoIds{2,3,28,4,5,12,13,14,15};size_t infoIndex=0;unsigned queries=0;
    for(const auto& call:calls) {
        if(call[0]==0x29) { check(infoIndex<infoIds.size() && call[3]==infoIds[infoIndex],"GetInfo calls follow the original virtmemSetup sequence");++infoIndex; }
        if(call[0]==6) ++queries;
    }
    check(step.stop==Machine::Stop::Exited && infoIndex==infoIds.size() && queries==1 && calls.size()==13 &&
        calls.front()[0]==2 && calls[1][0]==3 && calls.back()[0]==0xF001,"guest trace covers RELRO, virtual regions, stack query and loader return");
    std::ofstream trace(std::filesystem::path(argv[2])/"libnx-init-trace.csv",std::ios::trunc);trace<<"svc,pc,x0,x1,x2,x3\n";
    for(const auto& call:calls) { for(size_t i=0;i<call.size();++i) { if(i) trace<<',';trace<<call[i]; } trace<<'\n'; }
    check(bool(trace),"save actual startup trace");

    // GetInfo describes concrete reserved intervals and preserves X1 on failure.
    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;std::string error;Process p;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error,true),"GetInfo fixture");
    Bus bus(ram);CPU cpu(bus);
    for(auto [id,expected]:std::vector<std::pair<uint64_t,uint64_t>>{{2,Process::AliasBase},{3,Process::AliasSize},
        {4,Process::HeapBase},{5,Process::HeapCapacity},{12,0x10000},{13,Machine::RAM_SIZE-0x10000},
        {14,Machine::RAM_SIZE-Process::StackSize},{15,Process::StackSize},{28,0}}) {
        cpu.writeRegister(1,id);cpu.writeRegister(2,0xFFFF8001);cpu.writeRegister(3,0);
        check(p.dispatch(0x29,cpu,bus)==Process::Action::Continue && cpu.readRegister(0)==0 && cpu.readRegister(1)==expected,
            "GetInfo returns actual local reservation values");
    }
    cpu.writeRegister(1,2);cpu.writeRegister(2,p.mainHandle());p.dispatch(0x29,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidHandle && cpu.readRegister(1)==2,"GetInfo rejects a thread handle without publishing a value");
    cpu.writeRegister(2,0xFFFF8001);cpu.writeRegister(3,1);p.dispatch(0x29,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidSize && cpu.readRegister(1)==2,"unsupported GetInfo sub-ID rejected");
    cpu.writeRegister(3,0);cpu.writeRegister(1,99);check(p.dispatch(0x29,cpu,bus)==Process::Action::Unsupported,"unknown GetInfo ID remains unsupported");
    auto conflict=image;conflict.regions.push_back({Process::AliasBase,Process::AliasBase+4096,Memory::Read});
    check(!p.initialize(ram,conflict,true,error,true),"image cannot overlap the advertised alias reservation");
    std::cout<<"Original libnx initialization instructions="<<result.instructions<<" flags="<<value(0)<<" main_result="<<value(14)<<" failures="<<failures<<'\n';
    return failures?1:0;
}
