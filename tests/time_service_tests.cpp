#include "machine.hpp"
#include "ipc.hpp"
#include <iostream>
static uint64_t get(const Ipc::Buffer& b,size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4){for(size_t i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i));}
static std::string stringAt(const Memory& ram,uint64_t address){std::string text;for(size_t i=0;i<64 && ram.canAccess(address+i,1,Memory::Read);++i){const auto c=ram.read8(address+i);if(!c)break;text+=char(c);}return text;}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Ipc::Buffer registration{},lookup{},open[5]{},readClock{},calendar{};uint32_t rootHandle=0;std::array<uint32_t,5> children{};
    Machine sdk(false,true,true);if(!sdk.load(argv[1]))return 2;check(sdk.advanceTime(1234567890),"advance reference time before SDK startup");
    const auto tls=sdk.process().mainThread().localRegion;bool readSeen=false,calendarSeen=false,tzSeen=false;
    Machine::Result step{Machine::Stop::Budget,0};
    for(unsigned i=0;i<300000 && step.stop==Machine::Stop::Budget;++i){
        const auto instruction=sdk.ram.fetch32(sdk.cpu.getPC());
        const bool ipc=(instruction&0xFFE0001F)==0xD4000001 && ((instruction>>5)&0xFFFF)==0x21;
        Ipc::Buffer b{};const auto handle=uint32_t(sdk.cpu.readRegister(0));if(ipc)for(size_t j=0;j<b.size();++j)b[j]=sdk.ram.read8(tls+j);
        step=sdk.run(1);if(!ipc || get(b,0)!=4)continue;
        const bool pid=get(b,4)&0x80000000U;const size_t cmif=pid?32:16;if(get(b,cmif)!=0x49434653)continue;
        const auto command=get(b,cmif+8);
        if(pid && command==0 && !get(registration,0))registration=b;
        if(command==1 && std::string(reinterpret_cast<const char*>(b.data()+cmif+16),8)==std::string("time:u\0\0",8)){
            lookup=b;rootHandle=sdk.ram.read32(tls+12);check(sdk.ram.read32(tls+24)==0 && sdk.ram.read32(tls+8)==32,"SM creates native time:u session");
        }else if(handle==rootHandle && command<5){open[command]=b;children[command]=sdk.ram.read32(tls+12);check(sdk.ram.read32(tls+24)==0 && sdk.ram.read32(tls+8)==32,"time root opens typed clock session");}
        else if(handle==children[0] && command==0){readClock=b;readSeen=true;check(sdk.ram.read32(tls+24)==0 && sdk.ram.read64(tls+32)==Process::ReferenceUnixEpoch+1,"original client gets epoch plus virtual seconds");}
        else if(handle==children[3] && command==101){calendar=b;calendarSeen=true;check(sdk.ram.read32(tls+24)==0 && sdk.ram.read16(tls+32)==2000 && sdk.ram.read8(tls+38)==1,"original client receives UTC calendar output");}
    }
    const uint64_t report=0x10000+sdk.ram.read32(0x10000+0x30)+0x2000;
    const auto environment=sdk.ram.read64(report+22*8);
    tzSeen=environment && stringAt(sdk.ram,sdk.ram.read64(environment))=="TZ=<UTC>-00:00:00";
    check(step.stop==Machine::Stop::Exited && step.exitCode==0xE017 && readSeen && calendarSeen && tzSeen,"original time initialization stores real TZ before named SD mounting dependency");
    for(auto child:children)check(child!=0,"all five original time interfaces created");
    if(!get(calendar,0))return 1;

    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;std::string error;Process p;
    if(!ProgramLoader::loadImage(ram,argv[1],image,error)||!p.initialize(ram,image,true,error,true))return 2;
    Bus bus(ram);CPU cpu(bus);const auto ipc=p.mainThread().localRegion;
    const uint8_t sm[]{'s','m',':',0};ram.writeBytes(0x200300,sm,sizeof sm);cpu.writeRegister(1,0x200300);p.dispatch(0x1F,cpu,bus);const auto manager=uint32_t(cpu.readRegister(1));
    auto send=[&](const Ipc::Buffer& b,uint32_t handle){ram.writeBytes(ipc,b.data(),b.size());cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);return cpu.readRegister(0);};
    send(registration,manager);send(lookup,manager);const auto root=ram.read32(ipc+12);
    auto failedReply=[&](const Ipc::Buffer& b,uint32_t handle){ram.writeBytes(ipc,b.data(),b.size());ram.setPermissions(ipc,4096,Memory::Read);cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);check(cpu.readRegister(0)==Process::InvalidPointer,"time reply requires writable IPC memory");ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);};
    failedReply(open[0],root);
    std::array<uint32_t,5> clocks{};
    for(size_t i=0;i<clocks.size();++i){check(send(open[i],root)==0 && ram.read32(ipc+24)==0,"replay opens each clock interface");clocks[i]=ram.read32(ipc+12);}
    check(clocks[0]==root+1,"failed clock publication consumed no handle ID");
    for(auto i:{0,1,4})check(send(readClock,clocks[size_t(i)])==0 && ram.read64(ipc+32)==Process::ReferenceUnixEpoch,"system clock roles share explicit reference epoch");
    check(p.advanceTime(999999999) && send(readClock,clocks[0])==0 && ram.read64(ipc+32)==Process::ReferenceUnixEpoch,"wall clock rounds down fractional virtual seconds");
    check(p.advanceTime(1) && send(readClock,clocks[4])==0 && ram.read64(ipc+32)==Process::ReferenceUnixEpoch+1,"wall clock crosses exact second with virtual time");
    check(send(readClock,clocks[2])==0 && ram.read32(ipc+4)==14 && ram.read64(ipc+32)==1 && ram.read64(ipc+40)!=0,"steady clock returns elapsed seconds and nonzero source ID");const auto source=ram.read64(ipc+40);
    check(p.advanceTime(2000000000ULL) && send(readClock,clocks[2])==0 && ram.read64(ipc+32)==3 && ram.read64(ipc+40)==source,"steady clock source survives time progression");
    struct Date{uint64_t timestamp;unsigned year,month,day,wday,yday;};
    // Independent UTC reference vectors generated with Python datetime, not the C++ implementation.
    constexpr Date dates[]={{0,1970,1,1,4,0},{946684800,2000,1,1,6,0},{951782400,2000,2,29,2,59},{4107542400,2100,3,1,1,59},{1735603200,2024,12,31,2,365},{253402214400ULL,9999,12,31,5,364}};
    for(const auto& date:dates){auto b=calendar;put(b,32,date.timestamp,8);check(send(b,clocks[3])==0 && ram.read32(ipc+24)==0 && ram.read32(ipc+4)==16 && ram.read16(ipc+32)==date.year && ram.read8(ipc+34)==date.month && ram.read8(ipc+35)==date.day && ram.read32(ipc+40)==date.wday && ram.read32(ipc+44)==date.yday,"UTC conversion matches independent calendar vector");check(stringAt(ram,ipc+48)=="UTC" && ram.read32(ipc+56)==0 && ram.read32(ipc+60)==0,"calendar reports UTC without DST or offset");}
    auto last=calendar;put(last,32,253402300799ULL,8);check(send(last,clocks[3])==0 && ram.read8(ipc+36)==23 && ram.read8(ipc+37)==59 && ram.read8(ipc+38)==59,"calendar accepts final supported second");
    put(last,32,253402300800ULL,8);check(send(last,clocks[3])==0 && ram.read32(ipc+24)==Ipc::InvalidArguments && ram.read32(ipc+4)==8 && ram.read64(ipc+32)==0,"calendar rejects overflow without output data");
    auto extra=readClock;put(extra,4,9);check(send(extra,clocks[0])==0 && ram.read32(ipc+24)==Ipc::InvalidArguments,"clock getter rejects extra input");
    auto setter=calendar;put(setter,24,1);check(send(setter,clocks[0])==0 && ram.read32(ipc+24)==Ipc::UnknownCommand,"clock writes remain unsupported");
    auto memory=open[0];put(memory,24,20);check(send(memory,root)==0 && ram.read32(ipc+24)==Ipc::UnknownCommand,"modern time shared memory is not fabricated");
    auto bad=open[0];put(bad,20,1);check(send(bad,root)==Ipc::UnsupportedFormat,"unsupported CMIF version rejected structurally");
    auto copy=open[0];put(copy,0,5);put(copy,24,2);send(copy,clocks[4]);const auto cloned=ram.read32(ipc+12);
    check(send(readClock,cloned)==0 && ram.read64(ipc+32)==Process::ReferenceUnixEpoch+3,"clone retains local clock identity");
    failedReply(readClock,clocks[0]);check(send(readClock,clocks[0])==0 && ram.read64(ipc+32)==Process::ReferenceUnixEpoch+3,"failed read response does not advance virtual time");
    Ipc::Buffer close{};put(close,0,2);send(close,root);check(send(readClock,clocks[0])==0 && ram.read32(ipc+24)==0,"child clock survives closed root endpoint");
    send(lookup,manager);const auto limited=ram.read32(ipc+12);uint32_t lastHandle=0;unsigned count=0;
    for(unsigned i=0;i<128;++i){send(open[0],limited);if(ram.read32(ipc+24)==Process::OutOfHandles)break;check(ram.read32(ipc+24)==0,"clock handle within quota");lastHandle=ram.read32(ipc+12);++count;}
    check(count && ram.read32(ipc+24)==Process::OutOfHandles && !(ram.read32(ipc+4)&0x80000000U),"clock sessions obey common handle quota");
    cpu.writeRegister(0,cloned);p.dispatch(0x16,cpu,bus);send(open[0],limited);check(ram.read32(ipc+12)==lastHandle+1,"clock quota failure consumed no ID");
    p.checkpoint(cpu,Process::State::Exited);check(send(readClock,clocks[0])==0 && ram.read32(ipc+24)==Process::InvalidState,"retired process cannot read clock service");
    Ipc::Buffer response{};Ipc::TimeRequest plan;check(Ipc::dispatchTime(readClock,response,Ipc::TimeRole::User,1000000000ULL,UINT64_MAX,plan)==0 && plan.result==Ipc::InvalidArguments,"clock arithmetic overflow is rejected");
    std::cout<<"Time service failures="<<failures<<'\n';return failures?1:0;
}
