#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iostream>
#include <vector>
static uint64_t get(const Ipc::Buffer& b,size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4){for(size_t i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i));}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Ipc::Buffer registration{},lookup{},convert{},attach{},clone{};uint32_t fsp=0;std::vector<uint32_t> clones;
    Machine sdk(false,true,true);if(!sdk.load(argv[1]))return 2;const auto tls=sdk.process().mainThread().localRegion;
    Machine::Result step{Machine::Stop::Budget,0};bool attached=false,converted=false;
    for(unsigned i=0;i<300000 && step.stop==Machine::Stop::Budget;++i){
        const auto instruction=sdk.ram.fetch32(sdk.cpu.getPC());const bool ipc=(instruction&0xFFE0001F)==0xD4000001 && ((instruction>>5)&0xFFFF)==0x21;
        Ipc::Buffer b{};const auto handle=uint32_t(sdk.cpu.readRegister(0));if(ipc)for(size_t j=0;j<b.size();++j)b[j]=sdk.ram.read8(tls+j);
        step=sdk.run(1);if(!ipc)continue;
        if(handle==fsp && get(b,0)==5){
            if(get(b,24)==0){convert=b;converted=sdk.ram.read32(tls+24)==0 && sdk.ram.read32(tls+32)==1;}
            if(get(b,24)==2){clone=b;clones.push_back(sdk.ram.read32(tls+12));check(sdk.ram.read32(tls+24)==0 && sdk.ram.read32(tls+8)==32,"original sessionmgr obtains moved clone handles");}
        }
        if(get(b,0)!=4)continue;const bool pid=get(b,4)&0x80000000U;const size_t cmif=pid?32:16;
        if(get(b,cmif)==0x49434653){
            if(pid && get(b,cmif+8)==0 && !get(registration,0))registration=b;
            if(get(b,cmif+8)==1 && std::string(reinterpret_cast<const char*>(b.data()+cmif+16),8)==std::string("fsp-srv\0",8)){lookup=b;fsp=sdk.ram.read32(tls+12);check(sdk.ram.read32(tls+24)==0,"native SM opens fsp-srv");}
        }else if(handle==fsp){Ipc::DomainRequest d;check(!Ipc::parseDomain(b,d) && d.object==1 && d.command==1 && d.sendPid && !d.copyHandlePresent && d.payloadSize==8,"SDK sends domain SetCurrentProcess with PID only");attach=b;attached=sdk.ram.read32(tls+40)==0;}
    }
    check(step.stop==Machine::Stop::Exited && step.exitCode==0xE017 && converted && attached && clones.size()==2,"original fsInitialize completes before missing fsdevMountSdmc");
    std::ifstream symbols(argv[2]);std::string symbol;uint64_t value,managerAddress=0;while(symbols>>symbol>>std::hex>>value)if(symbol=="g_fsSessionMgr")managerAddress=value;
    check(managerAddress && clones.size()==2 && sdk.ram.read32(managerAddress)==fsp && sdk.ram.read32(managerAddress+4)==clones[0] && sdk.ram.read32(managerAddress+8)==clones[1] && sdk.ram.read32(managerAddress+64)==3 && sdk.ram.read32(managerAddress+68)==7,
        "original SessionMgr contains root and two distinct free clients");
    if(!attached || clones.size()!=2)return 1;
    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;std::string error;Process p;
    if(!ProgramLoader::loadImage(ram,argv[1],image,error)||!p.initialize(ram,image,true,error,true))return 2;
    Bus bus(ram);CPU cpu(bus);const auto ipc=p.mainThread().localRegion;
    const uint8_t sm[]{'s','m',':',0};ram.writeBytes(0x200300,sm,sizeof sm);cpu.writeRegister(1,0x200300);p.dispatch(0x1F,cpu,bus);const auto manager=uint32_t(cpu.readRegister(1));
    auto send=[&](const Ipc::Buffer& b,uint32_t handle){ram.writeBytes(ipc,b.data(),b.size());cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);return cpu.readRegister(0);};
    auto failedReply=[&](const Ipc::Buffer& b,uint32_t handle){ram.writeBytes(ipc,b.data(),b.size());ram.setPermissions(ipc,4096,Memory::Read);cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);check(cpu.readRegister(0)==Process::InvalidPointer,"FSP failed reply publishes no state");ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);};
    send(registration,manager);send(lookup,manager);const auto root=ram.read32(ipc+12);
    check(send(attach,root)==Ipc::UnsupportedFormat,"FSP requires domain conversion before attachment");
    failedReply(convert,root);check(send(attach,root)==Ipc::UnsupportedFormat,"failed conversion preserves plain session");
    check(send(convert,root)==0 && ram.read32(ipc+32)==1,"convert FSP session to root domain object");
    check(send(convert,root)==0 && ram.read32(ipc+24)==Process::InvalidState,"duplicate conversion preserves domain");
    Ipc::Buffer query{};put(query,0,4);put(query,4,12);put(query,16,1,1);put(query,18,16,2);put(query,20,1);put(query,32,0x49434653);put(query,40,19);
    check(send(query,root)==0 && ram.read32(ipc+40)==Ipc::NotInitialized,"file operations require attached process");
    failedReply(attach,root);check(send(query,root)==0 && ram.read32(ipc+40)==Ipc::NotInitialized,"failed attachment reply does not initialize service");
    auto bad=attach;put(bad,64,1,8);check(send(bad,root)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments,"nonzero PID placeholder rejected");
    bad=attach;put(bad,36,2);check(send(bad,root)==0 && ram.read32(ipc+40)==Ipc::InvalidObject,"wrong domain object rejected");
    bad=attach;put(bad,8,3);put(bad,20,0xFFFF8001);check(send(bad,root)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments,"FSP refuses copied process-handle form used by appletOE");
    for(auto [at,v]:std::vector<std::pair<size_t,uint32_t>>{{8,5},{40,1},{52,1},{60,1}}){bad=attach;put(bad,at,v);check(send(bad,root)!=0,"malformed FSP domain metadata rejected");}
    auto withoutPid=query;put(withoutPid,4,14);put(withoutPid,18,24,2);put(withoutPid,40,1);check(send(withoutPid,root)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments,"FSP attachment requires PID marker");
    bad=attach;put(bad,12,0x12345678);check(send(bad,root)==0 && ram.read32(ipc+40)==0,"guest PID bytes are ignored in favor of current process context");
    check(send(query,root)==0 && ram.read32(ipc+40)==Ipc::UnknownCommand,"attached FSP still rejects unsupported filesystem operation");
    check(send(attach,root)==0 && ram.read32(ipc+40)==Ipc::AlreadyInitialized,"duplicate process attachment is explicit");
    failedReply(clone,root);send(clone,root);const auto copied=ram.read32(ipc+12);check(copied==root+1,"failed clone consumes no handle ID");
    check(send(query,copied)==0 && ram.read32(ipc+40)==Ipc::UnknownCommand && send(attach,copied)==0 && ram.read32(ipc+40)==Ipc::AlreadyInitialized,"clone shares attached root and domain identity");
    send(lookup,manager);const auto independent=ram.read32(ipc+12);send(convert,independent);
    check(send(query,independent)==0 && ram.read32(ipc+40)==Ipc::NotInitialized,"independent FSP session has independent attachment state");
    check(send(attach,independent)==0 && ram.read32(ipc+40)==0,"independent domain attaches to same current process");
    Ipc::Buffer close{};put(close,0,2);send(close,root);check(send(query,root)==Ipc::SessionClosed && send(query,copied)==0 && ram.read32(ipc+40)==Ipc::UnknownCommand,"closing parent endpoint preserves clone");
    cpu.writeRegister(0,root);p.dispatch(0x16,cpu,bus);check(send(query,root)==Process::InvalidHandle,"released FSP endpoint remains invalid");
    auto closeObject=query;put(closeObject,4,8);put(closeObject,16,2,1);put(closeObject,18,0,2);check(send(closeObject,copied)==0,"explicit domain root close succeeds");
    check(send(query,copied)==0 && ram.read32(ipc+40)==Ipc::InvalidObject && send(query,independent)==0 && ram.read32(ipc+40)==Ipc::UnknownCommand,"closed domain object does not affect independent connection");
    auto openSd=query;put(openSd,40,18);
    check(send(openSd,independent)==0 && ram.read32(ipc+40)==Ipc::FilesDisabled && ram.read32(ipc+16)==0,"SD open without explicitly configured root returns no object");
    auto invalidSd=openSd;put(invalidSd,4,13);put(invalidSd,18,17,2);
    check(send(invalidSd,independent)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments,"SD open rejects extra data even when disabled");
    auto pidSd=attach;put(pidSd,4,0x8000000C);put(pidSd,34,16,2);put(pidSd,56,18);
    check(send(pidSd,independent)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments,"SD open rejects PID-bearing request");
    std::string mountError;const auto sd=ReadOnlyFiles::load(std::filesystem::path(argv[1]).parent_path().parent_path()/"hle_files/root",mountError);
    if(!sd){std::cerr<<mountError;return 2;}p.setReadOnlyFiles(sd);
    failedReply(openSd,independent);
    check(send(openSd,independent)==0 && ram.read32(ipc+40)==0 && ram.read32(ipc+48)==2 && ram.read32(ipc+16)==1,"SD root object is published transactionally without failed-open ID gaps");
    const auto sdId=ram.read32(ipc+48);
    auto sdQuery=query;put(sdQuery,20,sdId);
    check(send(sdQuery,independent)==0 && ram.read32(ipc+40)==Ipc::UnknownCommand,"unimplemented SD methods do not return fabricated success");
    p.setReadOnlyFiles(nullptr);
    check(send(openSd,independent)==0 && ram.read32(ipc+40)==Ipc::FilesDisabled,"removing configured root blocks future SD opens");
    check(send(sdQuery,independent)==0 && ram.read32(ipc+40)==Ipc::UnknownCommand,"existing SD object survives configuration removal");
    auto closeSd=closeObject;put(closeSd,20,sdId);send(closeSd,independent);
    check(send(sdQuery,independent)==0 && ram.read32(ipc+40)==Ipc::InvalidObject,"closed SD object becomes invalid");
    p.setReadOnlyFiles(sd);
    uint32_t lastObject=0;unsigned objectCount=0;
    for(unsigned i=0;i<Process::DomainObjectLimit;++i){send(openSd,independent);if(ram.read32(ipc+40)==Ipc::OutOfObjects)break;check(ram.read32(ipc+40)==0,"SD open within domain quota");lastObject=ram.read32(ipc+48);++objectCount;}
    check(objectCount==Process::DomainObjectLimit-1 && ram.read32(ipc+40)==Ipc::OutOfObjects && ram.read32(ipc+16)==0,"SD objects obey shared domain quota and return no object on failure");
    put(closeSd,20,lastObject);send(closeSd,independent);send(openSd,independent);
    check(ram.read32(ipc+40)==0 && ram.read32(ipc+48)==lastObject+1,"SD quota failures consume no object ID");
    uint32_t last=0;unsigned count=0;
    for(unsigned i=0;i<128;++i){send(clone,independent);if(ram.read32(ipc+24)==Process::OutOfHandles)break;check(ram.read32(ipc+24)==0,"FSP clone within quota");last=ram.read32(ipc+12);++count;}
    check(count && ram.read32(ipc+24)==Process::OutOfHandles && !(ram.read32(ipc+4)&0x80000000U),"FSP sessions use common finite handle quota");
    cpu.writeRegister(0,copied);p.dispatch(0x16,cpu,bus);send(clone,independent);check(ram.read32(ipc+12)==last+1,"FSP quota failure consumes no ID");
    p.checkpoint(cpu,Process::State::Exited);check(send(attach,independent)==0 && ram.read32(ipc+40)==Process::InvalidState,"retired process cannot attach FSP");
    std::cout<<"FSP initialization failures="<<failures<<'\n';return failures?1:0;
}
