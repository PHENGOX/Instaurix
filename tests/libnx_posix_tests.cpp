#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <map>

int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream in(argv[2]);std::string name;uint64_t address;
    while(in>>name>>std::hex>>address)symbols[name]=address;
    if(!symbols.contains("report") || !symbols.contains("posix_report"))return 2;
    for(bool mounted:{false,true}){
        uint64_t referenceCount=0;std::array<uint64_t,32> referenceReport{};
        for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
            Machine m(false,true,true);
            if(mounted && !m.mountReadOnlyRoot(std::filesystem::path(argv[1]).parent_path().parent_path()/"hle_files/root"))return 2;
            if(!m.load(argv[1])){std::cerr<<m.error();return 2;}
            Machine::Result result{Machine::Stop::Budget,0};uint64_t total=0;
            while(total<500000 && result.stop==Machine::Stop::Budget){result=m.run(budget);total+=result.instructions;}
            if(result.stop!=Machine::Stop::Exited)std::cerr<<m.error()<<" PC="<<std::hex<<m.cpu.getPC()<<std::dec<<'\n';
            check(result.stop==Machine::Stop::Exited && result.exitCode==0,"guest POSIX application returns through original cleanup");
            const auto main=symbols.at("report"),report=symbols.at("posix_report");auto value=[&](size_t i){return m.ram.read64(report+i*8);};
            check(m.ram.read64(main)==6 && m.ram.read64(main+14*8)==77 && m.ram.read64(main+15*8)==0,"original startup/main checks and constructor succeed without dependency traps");
            if(mounted){
                check(value(0)==1 && value(1)>=3 && value(3)>=3 && value(1)!=value(3),"guest libc opens independent descriptors through original devoptab");
                check(value(4)==7 && value(5)==23 && value(6)==0 && value(7)==30,"guest sequential reads track independent positions and EOF");
                check(value(8)==1 && value(9)==1,"guest verifies exact fixture bytes from both descriptors");
                check(value(10)==0 && value(11)==0,"zero read and close succeed");
                check(value(12)==UINT64_MAX && value(13)==9,"closed descriptor read returns guest EBADF");
                check(value(14)==value(1) && value(15)==3 && value(16)==1,"descriptor slot is reused with a fresh file offset");
                check(value(17)==0 && value(18)==0,"both live descriptors close normally");
                check(value(19)==UINT64_MAX && value(20)==9,"repeated close returns guest EBADF");
                check(value(21)==UINT64_MAX && value(22)==5,"unmapped local missing-file result follows original fsdev EIO fallback");
                check(value(23)==UINT64_MAX && value(24)==5,"write-only open is rejected through original fsdev error translation");
                check(value(25)==UINT64_MAX && value(26)==22,"read-only append is rejected by original fsdev with EINVAL");
            }else check(value(0)==2 && value(1)==UINT64_MAX && value(2)==88,"no SD device yields original libc ENOSYS without fabricated file success");
            if(budget==500000){referenceCount=total;for(size_t i=0;i<referenceReport.size();++i)referenceReport[i]=value(i);}
            else {check(total==referenceCount,"instruction slicing preserves POSIX execution count");for(size_t i=0;i<referenceReport.size();++i)check(value(i)==referenceReport[i],"instruction slicing preserves complete guest POSIX report");}
        }
        std::cout<<"POSIX mounted="<<mounted<<" instructions="<<referenceCount<<'\n';
    }
    std::cout<<"POSIX failures="<<failures<<'\n';return failures?1:0;
}
