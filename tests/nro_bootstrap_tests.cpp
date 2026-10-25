#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <tuple>

static void put(std::vector<uint8_t>& b,size_t at,uint64_t value,size_t size=8)
{ for(size_t i=0;i<size;++i) b[at+i]=uint8_t(value>>(8*i)); }
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    std::ifstream in(argv[1],std::ios::binary);
    std::vector<uint8_t> original((std::istreambuf_iterator<char>(in)),{});
    if(original.size()!=24576) return 2;
    const auto path=std::filesystem::path(argv[2])/"bootstrap-validation.nro";
    auto save=[&](const auto& bytes) {
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());return bool(out);
    };
    Machine m(false,true,true), sliced(false,true,true), old(false,true), incompatible(true,true,true);
    check(!old.load(argv[1]) && !incompatible.load(argv[1]),"MOD0 requires an explicit compatible entry profile");
    if(!m.load(argv[1]) || !sliced.load(argv[1])) { std::cerr<<m.error()<<'\n';return 1; }
    check(m.cpu.readRegister(0)==Process::HomebrewEnvironment && m.cpu.readRegister(1)==UINT64_MAX &&
        m.cpu.readRegister(30)==Process::HomebrewReturn && !(m.cpu.getSP()&15),"NRO entry registers and aligned stack");
    check(m.process().heapSize()==Process::HeapAlignment && m.process().mainHandle()!=0,"real heap and main handle allocated");
    check(m.ram.read64(0x15040)==0,"host validates without pre-applying the guest relocation");
    check(m.ram.permissionsAt(Process::HomebrewEnvironment)==Memory::Read &&
        m.ram.permissionsAt(Process::HomebrewReturn)==(Memory::Read|Memory::Execute),"environment RO NX and return RX");
    check(m.ram.read32(Process::HomebrewEnvironment)==1 && m.ram.read64(Process::HomebrewEnvironment+8)==m.process().mainHandle() &&
        m.ram.read32(Process::HomebrewEnvironment+24)==3 && m.ram.read32(Process::HomebrewEnvironment+48)==0,
        "bounded ConfigEntry layout with main thread, heap and terminator");
    // Dirty BSS after the loader: clearing must now be performed by actual libnx code.
    m.ram.write64(0x16000,UINT64_MAX);sliced.ram.write64(0x16000,UINT64_MAX);
    const auto result=m.run(100000);
    check(result.stop==Machine::Stop::Exited && result.exitCode==42 && m.ram.read64(0x15000)==1,
        "guest libnx relocation reaches main and returns status through the loader trampoline");
    check(m.ram.read64(0x15040)==0x15000 && m.ram.read64(0x16000)==0 &&
        m.ram.read64(Process::HeapBase)==0x12345678 &&
        m.ram.read64(Process::HeapBase+Process::HeapAlignment-8)==0x87654321,"guest relocations BSS and heap writes");
    uint64_t count=0;Machine::Result part{Machine::Stop::Budget,0};
    for(unsigned i=0;i<20000 && part.stop==Machine::Stop::Budget;++i) { part=sliced.run(7);count+=part.instructions; }
    check(part.stop==Machine::Stop::Exited && part.exitCode==42 && count==result.instructions,"bootstrap deterministic across small budgets");
    const auto previousPc=m.cpu.getPC();const auto previousReport=m.ram.read64(0x15000);
    auto reject=[&](const auto& bytes,const char* label) {
        check(save(bytes),"write malformed bootstrap fixture");
        check(!m.load(path.string().c_str()) && !m.error().empty() && m.cpu.getPC()==previousPc &&
            m.ram.read64(0x15000)==previousReport && m.run(1).stop==Machine::Stop::Exited,label);
    };
    // Each individual malformed image must leave the previous completed process intact.
    for(auto [at,value,size]:std::vector<std::tuple<size_t,uint64_t,size_t>>{
        {0x4004,0x7FFFFFFF,4},{0x4004,0x80000000,4},{0x4004,0x41,4},
        {0x4008,0x1000,4},{0x400C,0x2000,4},{0x4018,1,4},{0x401C,0x30594E4C,4},
        {0x4028,0x31594E4C,4},{0x4040,1,8},{0x4050,7,8},{0x4058,25,8},{0x4068,8,8},
        {0x4080,0x4000,8},{0x4080,0x5041,8},{0x4080,UINT64_MAX,8},
        {0x4088,257,8},{0x4088,(uint64_t(1)<<32)|1027,8},{0x4090,UINT64_MAX-0x10000,8},
        {0x4048,0x6000,8},{0x4048,0x5000,8}}) {
        auto bad=original;put(bad,at,value,size);reject(bad,"invalid MOD0 metadata or relocation rejected transactionally");
    }
    auto unterminated=original;
    for(size_t at=0x4040;at+16<=0x5000;at+=16) { put(unterminated,at,7);put(unterminated,at+8,0); }
    reject(unterminated,"unterminated dynamic table rejected");
    auto duplicate=original;put(duplicate,0x4058,48);put(duplicate,0x4098,0x5040);put(duplicate,0x40A0,1027);put(duplicate,0x40A8,0x5000);
    reject(duplicate,"duplicate relocation targets rejected");
    auto relr=original;
    put(relr,0x4040,36);put(relr,0x4048,0x4080);put(relr,0x4050,35);put(relr,0x4058,16);
    put(relr,0x4060,37);put(relr,0x4068,8);put(relr,0x4080,0x5040);put(relr,0x4088,129);put(relr,0x5040,0x5000);
    auto badRelr=relr;put(badRelr,0x4080,3);reject(badRelr,"RELR bitmap before a direct address rejected");
    badRelr=relr;put(badRelr,0x4080,0x4000);reject(badRelr,"RELR writes to read-only data rejected");
    badRelr=relr;put(badRelr,0x5040,UINT64_MAX);reject(badRelr,"RELR overflow rejected");
    check(save(relr) && m.load(path.string().c_str()),"validated RELR bootstrap loads");
    const auto relrResult=m.run(100000);
    check(relrResult.stop==Machine::Stop::Exited && relrResult.exitCode==42 && m.ram.read64(0x15000)==1 && m.ram.read64(0x15078)==0x10000,
        "upstream libnx applies validated RELR direct and bitmap relocations");

    // Real handle semantics without making an arbitrary integer look valid.
    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;Process process;std::string error;
    check(ProgramLoader::loadNro(ram,original,image,error) && process.initialize(ram,image,true,error,true),"direct process fixture");
    Bus bus(ram);CPU cpu(bus);const auto handle=process.mainHandle();
    cpu.writeRegister(1,handle);process.dispatch(0x25,cpu,bus);
    check(cpu.readRegister(0)==0 && cpu.readRegister(1)==1,"GetThreadId resolves the main handle");
    cpu.writeRegister(0,handle);process.dispatch(0x09,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidState,"main thread cannot be started again");
    ram.write32(0x15080,handle);cpu.writeRegister(1,0x15080);cpu.writeRegister(2,1);cpu.writeRegister(3,0);
    process.dispatch(0x18,cpu,bus);check(cpu.readRegister(0)==Process::TimedOut,"live main handle is not signaled");
    check(process.dispatch(0x0A,cpu,bus)==Process::Action::ThreadExit,"main thread exits explicitly");
    cpu.writeRegister(1,0x15080);cpu.writeRegister(2,1);cpu.writeRegister(3,0);process.dispatch(0x18,cpu,bus);
    check(cpu.readRegister(0)==0 && cpu.readRegister(1)==0,"main completion handle becomes signaled on exit");
    cpu.writeRegister(0,handle);process.dispatch(0x16,cpu,bus);cpu.writeRegister(1,handle);process.dispatch(0x25,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidHandle,"closed main handle cannot be reused");
    auto conflict=image;conflict.regions.push_back({Process::HomebrewEnvironment,Process::HomebrewEnvironment+4096,Memory::Read});
    check(!process.initialize(ram,conflict,true,error,true),"image cannot overlap the environment reservation");
    Machine wrongFormat(false,true,true);
    auto noModule=original;put(noModule,4,0,4);check(save(noModule) && !wrongFormat.load(path.string().c_str()),
        "bootstrap profile does not silently accept static NRO without MOD0");
    Machine forged(false,true,true);check(forged.load(argv[1]),"forged return fixture");
    forged.ram.setPermissions(0x10000,4096,Memory::All);forged.ram.write32(0x10000,0xD41E0021);
    check(forged.run(1).stop==Machine::Stop::Error,"loader private SVC refused outside the return trampoline");
    std::filesystem::remove(path);
    std::cout<<"NRO bootstrap instructions="<<result.instructions<<" exit="<<result.exitCode<<" failures="<<failures<<'\n';
    return failures?1:0;
}
