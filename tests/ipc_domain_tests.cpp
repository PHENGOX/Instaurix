#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <tuple>
static uint64_t get(const Ipc::Buffer& b,size_t at,size_t size=4)
{ uint64_t v=0;for(size_t i=0;i<size;++i) v|=uint64_t(b[at+i])<<(8*i);return v; }
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t size=4)
{ for(size_t i=0;i<size;++i) b[at+i]=uint8_t(v>>(8*i)); }
static Ipc::Buffer snapshot(const Memory& ram,uint64_t at)
{ Ipc::Buffer b{};for(size_t i=0;i<b.size();++i) b[i]=ram.read8(at+i);return b; }
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    unsigned failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    Ipc::Buffer registration{},lookup{},convert{},openProxy{},openFunctions{},closeObject{};
    Machine traced(false,true,true);if(!traced.load(argv[1])) { std::cerr<<traced.error()<<'\n';return 1; }
    const auto resourceId=traced.process().appletResourceId();
    const auto tls=traced.process().mainThread().localRegion;
    unsigned conversions=0,sends=0,unimplemented=0;std::vector<uint32_t> returned,closed;
    Machine::Result step{Machine::Stop::Budget,0};
    for(unsigned i=0;i<100000 && step.stop==Machine::Stop::Budget;++i) {
        const auto op=traced.ram.fetch32(traced.cpu.getPC());const bool ipc=(op&0xFFE0001F)==0xD4000001 && ((op>>5)&0xFFFF)==0x21;
        Ipc::Buffer request{};if(ipc) request=snapshot(traced.ram,tls);
        step=traced.run(1);
        if(!ipc) continue;
        const auto response=snapshot(traced.ram,tls);const auto transport=traced.cpu.readRegister(0);
        if(get(request,0)==5 && get(request,24)==0) {
            convert=request;++conversions;
            check(transport==0 && get(response,4)==9 && get(response,24)==0 && get(response,32)==1,
                "original SDK conversion receives one root object ID as a u32");
        }
        if(get(request,0)!=4) continue;
        const bool pid=(get(request,4)&0x80000000U)!=0;const size_t cmif=pid?32:16;
        if(get(request,cmif)==0x49434653) {
            if(pid && get(request,cmif+8)==0) registration=request;
            if(get(request,cmif+8)==1 && std::string(reinterpret_cast<const char*>(request.data()+cmif+16),8)=="appletOE") lookup=request;
            continue;
        }
        Ipc::DomainRequest decoded;
        if(Ipc::parseDomain(request,decoded)) continue;
        check(transport==0,"all actual SDK domain requests have successful transport");
        if(decoded.close) {
            closeObject=request;closed.push_back(decoded.object);
            check(request==response,"SDK object close has no fabricated CMIF response");continue;
        }
        ++sends;
        if(decoded.object==1) openProxy=request;
        if(decoded.object==2 && decoded.command==20) openFunctions=request;
        check(get(response,32)==0x4F434653,"domain responses use the domain SFCO header");
        if(get(response,40)==0 && get(response,16)==1) {
            check(!(get(response,4)&0x80000000U),"domain objects do not consume kernel handles");
            check(get(response,16)==1 && get(response,4)==13,"one successful output domain object");returned.push_back(uint32_t(get(response,48)));
        } else if(get(response,40)==0) {
            check(get(response,16)==0,"scalar and event replies do not publish domain objects");
            if(decoded.object==7 && decoded.command==1) check(get(response,48,8)==resourceId && resourceId!=0,"SDK gets the real process-scoped resource identity");
            else if(decoded.object==5 && decoded.command==0) check(get(response,8)==2 && get(response,12)!=0,"message event is a copied kernel handle");
            else check(decoded.object==5 && decoded.command==9 && get(response,48,1)==1,"headless application starts with its assigned virtual focus");
        } else {
            check(get(response,16)==0 && get(response,40)==Ipc::UnknownCommand,"unimplemented applet functionality remains an explicit domain service error");
            check((decoded.object==7 && decoded.command==10) || (decoded.object==6 && decoded.command==13),"foreground acquisition and cleanup policy remain unsupported");
            if(decoded.command==13) check(decoded.payloadSize==3,"non-word-sized native payload is accepted structurally");
            ++unimplemented;
        }
    }
    check(step.stop==Machine::Stop::Exited && step.exitCode==16217 && conversions==1 && sends==14 && unimplemented==2 &&
        returned==std::vector<uint32_t>{2,3,4,5,6,7,8,9,10} && closed==std::vector<uint32_t>{10,9,8,7,6,5,4,3,2},
        "actual SDK creates proxy and eight typed interfaces then closes them after the real unsupported operation");
    if(returned.size()!=9) return 1;

    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;Process p;std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error,true),"domain replay fixture");
    Bus bus(ram);CPU cpu(bus);const auto ipc=p.mainThread().localRegion;
    const uint8_t port[]{'s','m',':',0};ram.writeBytes(0x43300,port,sizeof port);cpu.writeRegister(1,0x43300);p.dispatch(0x1F,cpu,bus);
    const auto manager=uint32_t(cpu.readRegister(1));
    auto send=[&](const Ipc::Buffer& b,uint32_t handle) { ram.writeBytes(ipc,b.data(),b.size());cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);return cpu.readRegister(0); };
    auto failedReply=[&](const Ipc::Buffer& b,uint32_t handle) {
        ram.writeBytes(ipc,b.data(),b.size());ram.setPermissions(ipc,4096,Memory::Read);cpu.writeRegister(0,handle);p.dispatch(0x21,cpu,bus);
        check(cpu.readRegister(0)==Process::InvalidPointer && snapshot(ram,ipc)==b,"protected reply leaves request and domain state intact");
        ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
    };
    check(send(registration,manager)==0 && ram.read32(ipc+24)==0 && send(lookup,manager)==0,"obtain applet root through SDK packets");
    const auto root=ram.read32(ipc+12);
    check(send(openProxy,root)!=0,"domain request is refused before conversion");
    failedReply(convert,root);check(send(openProxy,root)!=0,"failed conversion did not publish domain state");
    check(send(convert,root)==0 && ram.read32(ipc+32)==1,"retry conversion publishes root ID 1");
    check(send(convert,root)==0 && ram.read32(ipc+24)==Process::InvalidState,"duplicate conversion rejected without replacing table");
    failedReply(openProxy,root);
    for(auto [at,v,size]:std::vector<std::tuple<size_t,uint64_t,size_t>>{{0,6,4},{4,0x8000000D,4},{8,5,4},{32,3,1},
        {33,1,1},{34,15,2},{36,0,4},{40,1,4},{44,1,4},{48,0,4},{52,1,4},{60,1,4}}) {
        auto bad=openProxy;put(bad,at,v,size);
        check(send(bad,root)!=0 && snapshot(ram,ipc)==bad,"malformed domain packet rejected without side effects");
    }
    auto wrongProcess=openProxy;put(wrongProcess,20,p.mainHandle());
    check(send(wrongProcess,root)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments && ram.read32(ipc+16)==0,
        "proxy cannot bind a copied thread handle as a process identity");
    auto nonzeroReserved=openProxy;put(nonzeroReserved,64,1,8);
    check(send(nonzeroReserved,root)==0 && ram.read32(ipc+40)==Ipc::InvalidArguments,"proxy reserved parameter must remain zero");
    check(send(openProxy,root)==0 && ram.read32(ipc+48)==2,"failed proxy calls consumed no object ID");
    failedReply(openFunctions,root);
    check(send(openFunctions,root)==0 && ram.read32(ipc+48)==3,"child creation is transactional");
    auto close=closeObject;put(close,20,3);
    check(send(close,root)==0,"close child object without closing endpoint");
    auto invoke=openFunctions;put(invoke,20,3);
    check(send(invoke,root)==0 && ram.read32(ipc+40)==Ipc::InvalidObject && ram.read32(ipc+16)==0,"closed domain IDs stay invalid");
    check(send(close,root)==Ipc::InvalidObject && snapshot(ram,ipc)==close,"double object close is an explicit transport error");
    check(send(openFunctions,root)==0 && ram.read32(ipc+48)==4,"freed slot gets a new object ID");
    auto cloneRequest=convert;put(cloneRequest,24,2);
    check(send(cloneRequest,root)==0 && ram.read32(ipc+24)==0,"clone converted endpoint");const auto cloned=ram.read32(ipc+12);
    check(cloned==root+1,"domain objects did not consume kernel handle IDs");
    put(invoke,20,4);
    check(send(invoke,cloned)==0 && ram.read32(ipc+40)==Ipc::UnknownCommand,"clone shares the same live domain objects");
    put(close,20,4);check(send(close,cloned)==0,"clone can close a shared object");
    check(send(invoke,root)==0 && ram.read32(ipc+40)==Ipc::InvalidObject,"object close is visible across clones");
    Ipc::Buffer endpointClose{};put(endpointClose,0,2);check(send(endpointClose,root)==0,"close original endpoint");
    check(send(openFunctions,root)==Ipc::SessionClosed,"closed endpoint cannot dispatch domain messages");
    check(send(openFunctions,cloned)==0 && ram.read32(ipc+48)==5,"remaining clone keeps domain alive");
    cpu.writeRegister(0,root);p.dispatch(0x16,cpu,bus);
    check(send(openFunctions,root)==Process::InvalidHandle,"released endpoint ID remains invalid");
    check(send(lookup,manager)==0,"independent applet session");const auto other=ram.read32(ipc+12);
    check(send(convert,other)==0,"independent domain conversion");put(invoke,20,5);
    check(send(invoke,other)==0 && ram.read32(ipc+40)==Ipc::InvalidObject,"independent domain does not inherit another domain's IDs");
    check(send(openProxy,other)==0 && ram.read32(ipc+48)==2,"independent domain starts its own object namespace");
    std::vector<uint32_t> ids;
    for(size_t i=0;i<Process::DomainObjectLimit-2;++i) {
        check(send(openFunctions,other)==0 && ram.read32(ipc+40)==0,"objects within domain quota");ids.push_back(ram.read32(ipc+48));
    }
    check(send(openFunctions,other)==0 && ram.read32(ipc+40)==Ipc::OutOfObjects && ram.read32(ipc+16)==0,
        "domain quota returns no object and no false success");
    put(close,20,ids.front());check(send(close,other)==0,"free domain slot");
    check(send(openFunctions,other)==0 && ram.read32(ipc+48)==ids.back()+1,"quota failure did not consume an object ID");
    check(send(invoke,other)==0 && ram.read32(ipc+40)==Ipc::UnknownCommand,"same numeric ID refers only to the current domain's live object");
    auto inputObjects=openFunctions;inputObjects[17]=1;
    check(send(inputObjects,other)==Ipc::UnsupportedFormat && snapshot(ram,ipc)==inputObjects,"unsupported input-object transfer is not silently accepted");
    auto query=convert;put(query,24,3);
    check(send(query,cloned)==0 && ram.read32(ipc+4)==9,"generic pointer-buffer control still works on converted sessions");
    check(send(endpointClose,cloned)==0 && send(endpointClose,other)==0,"close remaining domain endpoints");
    std::cout<<"SDK domain sends="<<sends<<" outputs="<<returned.size()<<" closes="<<closed.size()<<" failures="<<failures<<'\n';
    return failures?1:0;
}
