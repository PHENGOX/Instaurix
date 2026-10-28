#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

static std::string stringAt(const Memory& ram,uint64_t address)
{
    std::string text;for(size_t i=0;i<8192 && ram.canAccess(address+i,1,Memory::Read);++i){const auto c=ram.read8(address+i);if(!c)break;text+=char(c);}return text;
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream input(argv[2]);std::string name;uint64_t address;
    while(input>>name>>std::hex>>address)symbols[name]=address;
    for(const char* symbol:{"setenv","getenv","unsetenv","malloc","free","realloc","_sbrk_r","environ","fake_heap_start","fake_heap_end","_impure_ptr","__env_recursive_mutex","__malloc_recursive_mutex"})
        if(!symbols.contains(symbol)){std::cerr<<"Missing symbol: "<<symbol<<'\n';return 2;}
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;
    unsigned envDepth=0,mallocDepth=0;bool realOwner=false,ready=false;
    auto sample=[&](){envDepth=std::max(envDepth,m.ram.read32(symbols.at("__env_recursive_mutex")+4));mallocDepth=std::max(mallocDepth,m.ram.read32(symbols.at("__malloc_recursive_mutex")+4));realOwner|=m.ram.read32(symbols.at("__env_recursive_mutex"))==m.process().mainHandle();};
    for(unsigned i=0;i<300000;++i){
        if(m.ram.fetch32(m.cpu.getPC())==(0xD2800000U|(0xE017U<<5))){ready=true;break;}
        const auto result=m.run(1);sample();if(result.stop!=Machine::Stop::Budget){std::cerr<<m.error()<<'\n';break;}
    }
    check(ready && m.process().appletActive(),"original startup finishes filesystem initialization before unlinked SD mounting layer");if(!ready)return 1;
    check(envDepth>=2 && mallocDepth>=1 && realOwner,"original recursive environment/malloc locks use actual libnx thread ownership");
    const auto context=m.cpu.captureContext();const auto heapBegin=m.ram.read64(symbols.at("fake_heap_start")),heapEnd=m.ram.read64(symbols.at("fake_heap_end"));
    constexpr uint64_t returnPC=Process::WorkerStackBase,nameAt=Process::WorkerStackBase+0x1000,valueAt=Process::WorkerStackBase+0x2000;
    auto writeString=[&](uint64_t at,const std::string& text){check(m.ram.writeBytes(at,reinterpret_cast<const uint8_t*>(text.c_str()),text.size()+1),"write guest call arguments");};
    auto call=[&](const char* function,uint64_t a=0,uint64_t b=0,uint64_t c=0)->uint64_t{
        m.cpu.restoreContext(context);m.cpu.setPC(symbols.at(function));m.cpu.writeRegister(0,a);m.cpu.writeRegister(1,b);m.cpu.writeRegister(2,c);m.cpu.writeRegister(30,returnPC);
        for(unsigned i=0;i<300000;++i){
            if(m.cpu.getPC()==returnPC){const auto after=m.cpu.captureContext();check(after.sp==context.sp,"libc call restores stack");for(unsigned reg=19;reg<30;++reg)check(after.registers[reg]==context.registers[reg],"libc call preserves callee-saved registers");return m.cpu.readRegister(0);}
            const auto result=m.run(1);sample();if(result.stop!=Machine::Stop::Budget)throw std::runtime_error(std::string(function)+": "+m.error());
        }
        throw std::runtime_error(std::string(function)+": guest instruction limit");
    };
    try {
        auto getenvValue=[&](const std::string& key){writeString(nameAt,key);const auto ptr=call("getenv",nameAt);return ptr ? stringAt(m.ram,ptr) : std::string();};
        auto set=[&](const std::string& key,const std::string& value,unsigned rewrite=1){writeString(nameAt,key);writeString(valueAt,value);return static_cast<int32_t>(call("setenv",nameAt,valueAt,rewrite));};
        check(getenvValue("TZ")=="<UTC>-00:00:00","original newlib stored TZ during real startup");
        const auto initialTable=m.ram.read64(symbols.at("environ"));check(initialTable>=heapBegin && initialTable<heapEnd && stringAt(m.ram,m.ram.read64(initialTable))=="TZ=<UTC>-00:00:00" && m.ram.read64(initialTable+8)==0,"environment table and copied TZ entry live inside guest heap");
        const auto reent=m.ram.read64(symbols.at("_impure_ptr"));const auto initialBreak=call("_sbrk_r",reent,0);
        check(set("TZ","IGNORED",0)==0 && getenvValue("TZ")=="<UTC>-00:00:00" && call("_sbrk_r",reent,0)==initialBreak,"overwrite=0 preserves value without growing heap");
        check(set("EMU_TEST","first")==0 && getenvValue("EMU_TEST")=="first","new variable is stored and readable");
        m.ram.write8(valueAt,'X');check(getenvValue("EMU_TEST")=="first","setenv owns a copy rather than borrowing argument memory");
        check(set("EMU_TEST","a")==0 && getenvValue("EMU_TEST")=="a","shorter overwrite works");
        const std::string longValue(4096,'Q');check(set("EMU_TEST",longValue)==0 && getenvValue("EMU_TEST")==longValue,"growing overwrite uses actual allocator");
        for(unsigned i=0;i<24;++i){const auto key="ENV_KEY_"+std::to_string(i),value="value_"+std::to_string(i);check(set(key,value)==0 && getenvValue(key)==value,"environment pointer array grows through original realloc");}
        check(getenvValue("TZ")=="<UTC>-00:00:00" && getenvValue("EMU_TEST")==longValue,"array growth preserves earlier environment entries");
        check(set("BAD=NAME","value")==-1 && m.ram.read32(reent)==22,"invalid environment name returns EINVAL in guest reent");
        writeString(nameAt,"EMU_TEST");check(static_cast<int32_t>(call("unsetenv",nameAt))==0 && getenvValue("EMU_TEST").empty(),"unsetenv removes stored variable");
        writeString(nameAt,"ABSENT");check(call("getenv",nameAt)==0,"missing variable returns NULL");
        const auto block=call("malloc",64);check(block>=heapBegin && block+64<=heapEnd && block%16==0,"original malloc returns aligned guest-owned storage");
        for(unsigned i=0;i<64;++i)m.ram.write8(block+i,uint8_t(i^0xA5));
        const auto grown=call("realloc",block,8192);check(grown>=heapBegin && grown+8192<=heapEnd,"original realloc grows block within heap");
        for(unsigned i=0;i<64;++i)check(m.ram.read8(grown+i)==uint8_t(i^0xA5),"realloc preserves previous data");
        call("free",grown);call("free",0);
        check(call("malloc",heapEnd-heapBegin)==0 && m.ram.read32(reent)==12,"allocator rejects exhaustion at real heap end with ENOMEM");
        const auto small=call("malloc",32);check(small>=heapBegin && small+32<=heapEnd && getenvValue("TZ")=="<UTC>-00:00:00","heap remains usable after failed allocation");call("free",small);
        check(m.ram.read32(symbols.at("__env_recursive_mutex"))==0 && m.ram.read32(symbols.at("__env_recursive_mutex")+4)==0 && m.ram.read32(symbols.at("__malloc_recursive_mutex"))==0 && m.ram.read32(symbols.at("__malloc_recursive_mutex")+4)==0,"all real recursive locks are released after success and errors");
        check(call("_sbrk_r",reent,0)<=heapEnd,"allocator break remains bounded");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<"Newlib environment failures="<<failures<<" envDepth="<<envDepth<<" mallocDepth="<<mallocDepth<<'\n';return failures?1:0;
}
