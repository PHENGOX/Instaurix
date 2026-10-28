#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iostream>
#include <map>
int main(int argc,char** argv){
    if(argc!=3)return 2;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    for(const auto profile:{Gpu::ProfileId::MaxwellReference,Gpu::ProfileId::AmpereResearch}){
        Machine machine(false,true,true,profile);check(machine.load(argv[1]) && machine.process().gpuProfile()==profile,"selected profile reaches the staged process loader");
        const auto missing=std::string(argv[1])+".missing";check(!machine.load(missing.c_str()) && machine.process().gpuProfile()==profile,"failed reload retains the configured live process profile");
        const auto result=machine.run(500000);check(result.stop==Machine::Stop::Exited && !result.exitCode && !machine.process().frameCount(),"both explicit profiles terminate the reference with no rendering claim");
        for(unsigned i=0;i<2;++i){const auto report=symbols.at("gpu_as_report")+i*512;
            if(profile==Gpu::ProfileId::MaxwellReference)check(!machine.ram.read64(report+5*8) && !machine.ram.read64(report+29*8) && !machine.ram.read64(report+30*8),"Maxwell default preserves the original successful submission path");
            else check(machine.ram.read64(report+5*8)==Ipc::UnknownCommand && !machine.ram.read64(report+6*8) && machine.ram.read64(report+7*8)==UINT32_MAX && !machine.ram.read64(report+8*8),"Ampere research rejects FIFO initialization and lets original failure cleanup run without publishing a fence");
        }
        check(machine.load(argv[1]) && machine.process().gpuProfile()==profile,"successful reload keeps profile selection but replaces process resources");
    }
    Machine invalid(false,true,true,static_cast<Gpu::ProfileId>(99));check(!invalid.load(argv[1]) && invalid.error()=="Unknown GPU profile","invalid host-side profile cannot silently select Maxwell");
    std::cout<<"GPU profile lifecycle failures="<<failures<<'\n';return failures?1:0;
}
