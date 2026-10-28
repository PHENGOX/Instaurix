#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream input(argv[2]);std::string name;uint64_t value;
    while(input>>name>>std::hex>>value)symbols[name]=value;
    for(const char* key:{"main","FindDevice","GetDeviceOpTab","fsdevGetDeviceFileSystem","fsdevMountSdmc","fsdevUnmountDevice","report"})if(!symbols.contains(key))return 2;
    for(bool mounted:{false,true}){
        Machine m(false,true,true);
        if(mounted && !m.mountReadOnlyRoot(std::filesystem::path(argv[1]).parent_path().parent_path()/"hle_files/root"))return 2;
        if(!m.load(argv[1])){std::cerr<<m.error();return 2;}
        uint64_t instructions=0;bool reached=false;
        for(unsigned i=0;i<500000;++i){
            if(m.cpu.getPC()==symbols.at("main")){reached=true;break;}
            const auto step=m.run(1);instructions+=step.instructions;
            if(step.stop!=Machine::Stop::Budget){std::cerr<<"Before main: "<<m.error()<<" exit="<<step.exitCode<<'\n';break;}
        }
        check(reached,"original default app initialization and constructors reach main");if(!reached)return 1;
        const auto context=m.cpu.captureContext();const auto report=symbols.at("report");
        check(m.ram.read64(report)==2 && m.ram.read64(report+15*8)==0,"constructor ran without diagnostic or dependency trap");
        const uint64_t text=Process::WorkerStackBase+0x2000,ret=Process::WorkerStackBase;
        auto call=[&](const char* function,const char* argument=nullptr){
            if(argument)m.ram.writeBytes(text,reinterpret_cast<const uint8_t*>(argument),std::char_traits<char>::length(argument)+1);
            m.cpu.restoreContext(context);m.cpu.setPC(symbols.at(function));m.cpu.writeRegister(0,argument?text:0);m.cpu.writeRegister(30,ret);
            for(unsigned i=0;i<500000;++i){
                if(m.cpu.getPC()==ret){check(m.cpu.captureContext().sp==context.sp,"original device function restores stack");return m.cpu.readRegister(0);}
                if(m.run(1).stop!=Machine::Stop::Budget)throw std::runtime_error(m.error());
            }
            throw std::runtime_error("Device call instruction budget exceeded");
        };
        try{
            const auto device=static_cast<int32_t>(call("FindDevice","sdmc:"));
            check(mounted ? device>2 : device==-1,"original device table matches explicit SD configuration");
            const auto fs=call("fsdevGetDeviceFileSystem","sdmc");
            if(mounted){
                check(fs && m.ram.read32(fs)!=0 && m.ram.read32(fs+8)==2,"mounted fsdev retains real SD domain object");
                check(call("GetDeviceOpTab","")!=0,"original runtime installs a default device");
                check(call("fsdevMountSdmc")!=0 && call("fsdevGetDeviceFileSystem","sdmc")==fs,"duplicate mount fails without replacing existing registration");
                check(static_cast<int32_t>(call("fsdevUnmountDevice","sdmc"))==0 && static_cast<int32_t>(call("FindDevice","sdmc:"))==-1,"original unmount removes registered device");
                check(static_cast<int32_t>(call("fsdevUnmountDevice","sdmc"))==-1,"repeated unmount reports missing device");
                check(call("fsdevMountSdmc")==0 && call("fsdevGetDeviceFileSystem","sdmc")!=0,"original SD mount succeeds again after unmount");
            }else check(fs==0,"disabled SD leaves no fabricated filesystem device");
        }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
        m.cpu.restoreContext(context);
        auto step=m.run(500000);instructions+=step.instructions;
        check(step.stop==Machine::Stop::Exited && step.exitCode==0,"original default application cleanup returns to loader");
        check(m.ram.read64(report)==6 && m.ram.read64(report+14*8)==77 && m.ram.read64(report+15*8)==0,"main returns 77 through real cleanup without dependency marker");
        check(!m.process().appletActive(),"process exit retires application state");
        std::cout<<"FSDEV mounted="<<mounted<<" startup/exit instructions="<<instructions<<'\n';
        uint64_t cleanInstructions=0;
        std::array<uint64_t,32> cleanReport{};
        for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
            Machine clean(false,true,true);
            if(mounted && !clean.mountReadOnlyRoot(std::filesystem::path(argv[1]).parent_path().parent_path()/"hle_files/root"))return 2;
            if(!clean.load(argv[1]))return 2;
            uint64_t total=0;Machine::Result result{Machine::Stop::Budget,0};
            while(total<500000 && result.stop==Machine::Stop::Budget){result=clean.run(budget);total+=result.instructions;}
            check(result.stop==Machine::Stop::Exited && result.exitCode==0,"unmodified startup/main/cleanup exits normally");
            check(clean.ram.read64(report)==6 && clean.ram.read64(report+14*8)==77 && clean.ram.read64(report+15*8)==0,"unmodified run reaches constructor/main without dependency traps");
            if(budget==500000){cleanInstructions=total;for(size_t i=0;i<cleanReport.size();++i)cleanReport[i]=clean.ram.read64(report+i*8);}
            else {check(total==cleanInstructions,"budget slicing preserves full application instruction count");for(size_t i=0;i<cleanReport.size();++i)check(cleanReport[i]==clean.ram.read64(report+i*8),"budget slicing preserves application report");}
        }
        std::cout<<"FSDEV clean mounted="<<mounted<<" instructions="<<cleanInstructions<<'\n';
    }
    std::cout<<"FSDEV failures="<<failures<<'\n';return failures?1:0;
}
