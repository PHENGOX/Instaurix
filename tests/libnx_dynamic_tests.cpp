#include "machine.hpp"
#include <iostream>

int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* message) {
        if(!ok) { ++failures; std::cerr<<"FAIL: "<<message<<'\n'; }
    };
    for(unsigned mode=0;mode<3;++mode) {
        Machine whole(true), sliced(true);
        if(!whole.load(argv[1]) || !sliced.load(argv[1])) {
            std::cerr<<whole.error()<<' '<<sliced.error()<<'\n'; return 1;
        }
        whole.cpu.writeRegister(0,mode); sliced.cpu.writeRegister(0,mode);
        const auto result=whole.run(100000);
        const auto expectedCode=mode==2 ? 857U : 0U; // MAKERESULT(345,1), libnx BadReloc.
        check(result.stop==Machine::Stop::Exited && result.exitCode==expectedCode,
            "upstream runtime terminates with expected result");
        check(whole.ram.read64(0x15000)==(mode==2 ? 2U : 1U),"guest reached checked result or upstream abort");
        if(mode==0) {
            const auto base=whole.ram.read64(0x15010);
            check(whole.ram.read64(0x15018)==base+400 && whole.ram.read64(0x15020)==base+40,
                "RELA and RELR results independently visible to host");
        }
        if(mode==2) check(whole.ram.read64(0x15008)==857,"unsupported relocation reports actual libnx error");
        uint64_t instructions=0;
        Machine::Result part{Machine::Stop::Budget,0};
        for(unsigned i=0;i<15000 && part.stop==Machine::Stop::Budget;++i) {
            part=sliced.run(7); instructions+=part.instructions;
        }
        check(part.stop==result.stop && part.exitCode==result.exitCode && instructions==result.instructions,
            "upstream code deterministic across seven-instruction budgets");
        for(unsigned i=0;i<8;++i) check(whole.ram.read64(0x15000+8*i)==sliced.ram.read64(0x15000+8*i),
            "whole and sliced reports match");
        std::cout<<"libnx dynamic mode="<<mode<<" instructions="<<result.instructions<<" exit="<<result.exitCode<<'\n';
    }
    return failures ? 1 : 0;
}
