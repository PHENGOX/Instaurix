#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream input(argv[2]);std::string name;uint64_t value;
    while(input>>name>>std::hex>>value)symbols[name]=value;
    for(const char* required:{"__tls_start","__tls_end","__tdata_lma","__tdata_lma_end","referenceTlsReadSeed","referenceTlsReadZero","referenceTlsWrite","fsOpenSdCardFileSystem"})
        if(!symbols.contains(required))return 2;
    Machine m(false,true,true);if(!m.mountReadOnlyRoot(std::filesystem::path(argv[1]).parent_path().parent_path()/"hle_files/root") || !m.load(argv[1]))return 2;
    bool ready=false;
    for(unsigned i=0;i<300000;++i){
        if(m.ram.fetch32(m.cpu.getPC())==(0xD2800000U|(0xE017U<<5))){ready=true;break;}
        if(m.run(1).stop!=Machine::Stop::Budget)break;
    }
    check(ready,"original startup reaches SD dependency with real TLS sections");if(!ready)return 1;
    const auto original=m.cpu.captureContext();const auto local=m.process().mainThread().localRegion;
    const auto start=symbols.at("__tls_start"),end=symbols.at("__tls_end");
    const auto load=symbols.at("__tdata_lma"),loadEnd=symbols.at("__tdata_lma_end");
    check(end>start && end-start>=loadEnd-load && start%16==0,"TLS allocation includes initialized and zero-filled sections");
    check(m.ram.read64(local+0x1F8)==start-16,"original newlibSetup installs the software TLS pointer");
    const uint64_t otherLocal=Process::WorkerTlsPage,otherStart=Process::WorkerStackBase+0x1000,ret=Process::WorkerStackBase;
    for(uint64_t i=0;i<end-start;++i)m.ram.write8(otherStart+i,0);
    for(uint64_t i=0;i<loadEnd-load;++i)m.ram.write8(otherStart+i,m.ram.read8(load+i));
    m.ram.write64(otherLocal+0x1F8,otherStart-16);m.ram.write32(otherLocal+0x1E4,2);
    auto call=[&](uint64_t region,const char* function,uint64_t a=0,uint64_t b=0){
        auto context=original;context.tpidrroEl0=region;context.tpidrEl0=0xBAD000;
        m.cpu.restoreContext(context);m.cpu.setPC(symbols.at(function));m.cpu.writeRegister(0,a);m.cpu.writeRegister(1,b);m.cpu.writeRegister(30,ret);
        for(unsigned i=0;i<300000;++i){
            if(m.cpu.getPC()==ret){
                const auto after=m.cpu.captureContext();check(after.sp==context.sp && after.tpidrEl0==context.tpidrEl0,"TLS lowering preserves stack and architectural TPIDR_EL0");
                for(unsigned r=19;r<30;++r)check(after.registers[r]==context.registers[r],"TLS calls preserve callee-saved registers");
                return m.cpu.readRegister(0);
            }
            if(m.run(1).stop!=Machine::Stop::Budget)throw std::runtime_error(m.error());
        }
        throw std::runtime_error("TLS call instruction budget exceeded");
    };
    try {
        for(auto region:{local,otherLocal}){
            check(call(region,"referenceTlsReadSeed")==0x1122334455667788ULL,"initialized TLS value is loaded from template");
            check(call(region,"referenceTlsReadZero")==0,"uninitialized TLS starts at zero");
        }
        call(local,"referenceTlsWrite",0x123456789ABCDEF0ULL,77);
        check(call(otherLocal,"referenceTlsReadSeed")==0x1122334455667788ULL && call(otherLocal,"referenceTlsReadZero")==0,"main TLS writes do not affect alternate logical thread context");
        call(otherLocal,"referenceTlsWrite",0xFEDCBA9876543210ULL,99);
        check(call(local,"referenceTlsReadSeed")==0x123456789ABCDEF0ULL && call(local,"referenceTlsReadZero")==77,"alternate TLS writes do not affect main context");
        check(call(otherLocal,"referenceTlsReadSeed")==0xFEDCBA9876543210ULL && call(otherLocal,"referenceTlsReadZero")==99,"TLS values survive repeated context switches");
        const auto out=Process::WorkerStackBase+0x2000;
        check(call(local,"fsOpenSdCardFileSystem",out)==0,"original filesystem client opens configured SD root through real TLS path");
        check(m.ram.read32(out)!=0 && m.ram.read32(out+4)==0 && m.ram.read32(out+8)==2,"libnx decodes returned SD object as borrowed session plus domain ID");
        check(m.ram.read32(symbols.at("g_fsSessionMgr")+68)==7,"original session manager releases client slot after successful SD opening");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    check(m.ram.read64(load)==0x1122334455667788ULL,"TLS writes preserve load template");
    std::cout<<"libnx TLS failures="<<failures<<'\n';return failures?1:0;
}
