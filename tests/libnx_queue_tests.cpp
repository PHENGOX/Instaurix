#include "machine.hpp"
#include "ipc_binder.hpp"
#include <fstream>
#include <iostream>
#include <map>
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4){for(size_t i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i));}
int main(int argc,char** argv){
    if(argc!=3)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream input(argv[2]);std::string name;uint64_t value;while(input>>name>>std::hex>>value)symbols[name]=value;
    if(!symbols.contains("queue_report") || !symbols.contains("vi_report"))return 2;
    std::array<uint64_t,16> expected{};uint64_t expectedInstructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(count<500000 && result.stop==Machine::Stop::Budget){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && result.exitCode==0,"original queue client exits normally");
        const auto report=symbols.at("queue_report");auto get=[&](size_t i){return m.ram.read64(report+i*8);};
        check(!get(0) && get(1)==1280 && get(2)==720 && !get(3) && !get(4),"original connection parses local geometry and zero pending buffers");
        check(get(5)==Ipc::AlreadyInitialized,"duplicate connection fails");
        check(!get(6) && get(7)==1280,"original bqQuery reads the same width as Connect");
        check(!get(8) && get(9)==Ipc::NotInitialized && !get(10) && !get(11),"disconnect and reconnect follow queue state");
        const auto vi=symbols.at("vi_report");check(!m.ram.read64(vi+21*8) && !m.ram.read64(vi+27*8) && !m.ram.read64(vi+28*8),"original Binder and layer cleanup complete");
        check(!m.process().frameCount() && !m.process().managedLayerCount(),"no frame and resources retire at exit");
        if(budget==500000){expectedInstructions=count;for(size_t i=0;i<16;++i)expected[i]=get(i);}
        else{check(count==expectedInstructions,"instruction count is independent of slicing");for(size_t i=0;i<16;++i)check(get(i)==expected[i],"report is independent of slicing");}
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;bool captured=false;
    for(unsigned i=0;i<500000;++i){
        const auto op=m.ram.fetch32(m.cpu.getPC());
        if((op&0xFFE0001F)==0xD4000001 && ((op>>5)&0xFFFF)==0x21){
            Ipc::Buffer b{};for(size_t j=0;j<b.size();++j)b[j]=m.ram.read8(ipc+j);Ipc::BinderTransaction plan;
            if(!Ipc::parseBinderTransaction(b,m.bus,ipc,plan) && plan.code==10){
                captured=true;const auto context=m.cpu.captureContext();
                std::vector<uint8_t> source(size_t(plan.sourceSize)),output(size_t(plan.destinationSize));
                for(size_t j=0;j<source.size();++j)source[j]=m.ram.read8(plan.source+j);
                for(size_t j=0;j<output.size();++j)output[j]=m.ram.read8(plan.destination+j);
                // The current original client chooses legacy command 0 at the reference HOS version.
                check(b[3]==1 && b[32]==0x53,"original legacy Binder transaction is captured");
                auto send=[&](const Ipc::Buffer& packet){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0);};
                auto service=[&](const Ipc::Buffer& packet,uint32_t code){check(send(packet)==0 && m.ram.read32(ipc+24)==code,"service result matches queue state");};
                auto invalid=b;put(invalid,48,0x7fffffff);service(invalid,Ipc::InvalidArguments);
                invalid=b;put(invalid,56,1);service(invalid,Ipc::InvalidArguments);
                invalid=b;put(invalid,8,Ipc::MaxTransfer+1);check(send(invalid)==Ipc::TransferLimit,"oversized input is refused");
                invalid=b;put(invalid,24,ipc);put(invalid,28,0);check(send(invalid)==Ipc::InvalidBuffer,"output cannot overlap IPC memory");
                invalid=b;put(invalid,24,plan.source);put(invalid,28,0);check(send(invalid)==Ipc::InvalidBuffer,"input and output cannot overlap");
                auto malformed=source;malformed[24]='X';m.ram.writeBytes(plan.source,malformed.data(),malformed.size());service(b,Ipc::InvalidArguments);
                malformed=source;malformed[0]++;m.ram.writeBytes(plan.source,malformed.data(),malformed.size());service(b,Ipc::InvalidArguments);
                malformed=source;malformed[malformed.size()-8]=1;m.ram.writeBytes(plan.source,malformed.data(),malformed.size());service(b,Ipc::InvalidArguments);
                m.ram.writeBytes(plan.source,source.data(),source.size());
                m.ram.setPermissions(plan.destination&~uint64_t(Memory::PAGE_SIZE-1),Memory::PAGE_SIZE,Memory::Read);
                check(send(b)==Ipc::InvalidBuffer,"output requires writable memory");
                m.ram.setPermissions(plan.destination&~uint64_t(Memory::PAGE_SIZE-1),Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected reply cannot publish connection state");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                for(size_t j=0;j<output.size();++j)check(m.ram.read8(plan.destination+j)==output[j],"failed calls preserve output bytes");
                service(b,0);service(b,Ipc::AlreadyInitialized);
                check(m.ram.read32(plan.destination)==20 && m.ram.read32(plan.destination+4)==16 && m.ram.read32(plan.destination+16)==1280 && m.ram.read32(plan.destination+20)==720,"reply parcel matches the original reader");
                for(size_t j=36;j<output.size();++j)check(m.ram.read8(plan.destination+j)==output[j],"unused reply bytes remain unchanged");
                Ipc::Buffer ref{};put(ref,0,4);put(ref,4,11);put(ref,16,0x49434653);put(ref,24,1);put(ref,32,plan.object);put(ref,36,UINT32_MAX);put(ref,40,1);service(ref,Process::InvalidState);
                Ipc::Buffer close{};put(close,0,4);put(close,4,10);put(close,16,0x49434653);put(close,24,2021);put(close,32,1,8);
                m.ram.writeBytes(ipc,close.data(),close.size());m.cpu.restoreContext(context);m.cpu.writeRegister(0,m.ram.read64(symbols.at("vi_report")+16));m.run(1);
                check(m.ram.read32(ipc+24)==Process::InvalidState,"connected layer cannot close before disconnect");
                auto disconnectSource=source;disconnectSource.resize(source.size()-8);
                auto word=[&](size_t at,uint32_t v){for(size_t j=0;j<4;++j)disconnectSource[at+j]=uint8_t(v>>(8*j));};
                word(0,uint32_t(disconnectSource.size()-16));word(12,uint32_t(disconnectSource.size()));word(disconnectSource.size()-4,2);
                auto disconnect=b;put(disconnect,52,11);put(disconnect,8,disconnectSource.size());
                auto query=b;put(query,52,9);put(query,8,disconnectSource.size());
                auto querySource=disconnectSource;
                auto queryWord=[&](uint32_t v){for(size_t j=0;j<4;++j)querySource[querySource.size()-4+j]=uint8_t(v>>(8*j));m.ram.writeBytes(plan.source,querySource.data(),querySource.size());};
                for(uint32_t what=0;what<3;++what){
                    queryWord(what);service(query,0);
                    const uint32_t wanted=what==0?1280:what==1?720:1;
                    check(m.ram.read32(plan.destination)==8 && m.ram.read32(plan.destination+4)==16 && m.ram.read32(plan.destination+12)==24 && m.ram.read32(plan.destination+16)==wanted && !m.ram.read32(plan.destination+20),"Query parcel contains one value and zero Binder status");
                }
                const auto beforeUnknown=m.ram.read64(plan.destination+16);
                for(uint32_t what:{3U,6U,7U,UINT32_MAX}){queryWord(what);service(query,Ipc::UnknownCommand);check(m.ram.read64(plan.destination+16)==beforeUnknown,"unknown Query leaves output unchanged");}
                queryWord(0);auto malformedQuery=query;put(malformedQuery,8,source.size());service(malformedQuery,Ipc::InvalidArguments);
                m.ram.writeBytes(ipc,query.data(),query.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected Query reply does not alter queue connection");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                service(b,Ipc::InvalidArguments); // Query payload cannot be interpreted as CONNECT.
                m.ram.writeBytes(plan.source,disconnectSource.data(),disconnectSource.size());service(disconnect,0);service(disconnect,Ipc::NotInitialized);
                queryWord(0);service(query,Ipc::NotInitialized);
                Ipc::Buffer automatic{};put(automatic,0,0x01110004);put(automatic,4,0xC0C);
                for(size_t j=0;j<24;++j)automatic[16+j]=b[8+j];
                put(automatic,48,0x49434653);put(automatic,56,3);put(automatic,64,plan.object);put(automatic,68,10);
                m.ram.writeBytes(plan.source,source.data(),source.size());service(automatic,0);
                auto automaticQuery=automatic;put(automaticQuery,68,9);put(automaticQuery,16,querySource.size());queryWord(1);service(automaticQuery,0);
                check(m.ram.read32(plan.destination+16)==720,"AutoSelect Query matches legacy height");
                invalid=automatic;put(invalid,8,1);check(send(invalid)==Ipc::UnsupportedFormat,"non-null AutoSelect fallback is unsupported");
                m.ram.writeBytes(plan.source,disconnectSource.data(),disconnectSource.size());service(disconnect,0);
                m.ram.writeBytes(plan.source,source.data(),source.size());m.ram.writeBytes(plan.destination,output.data(),output.size());m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);break;
            }
        }
        if(m.run(1).stop!=Machine::Stop::Budget)break;
    }
    check(captured,"original bqConnect request is captured");const auto final=m.run(500000);
    check(final.stop==Machine::Stop::Exited && final.exitCode==0,"original execution continues after replay");
    Machine live(false,true,true);if(!live.load(argv[1]))return 2;
    const auto queueReport=symbols.at("queue_report");
    for(unsigned i=0;i<500000 && live.ram.read64(queueReport+8)!=1280;++i)if(live.run(1).stop!=Machine::Stop::Budget)break;
    const auto pc=live.cpu.getPC();const auto resource=live.process().appletResourceId();
    const auto missing=std::string(argv[1])+".missing";
    check(!live.load(missing.c_str()) && live.cpu.getPC()==pc && live.process().appletResourceId()==resource,"failed reload preserves a connected process");
    for(unsigned i=0;i<500000 && !live.ram.read64(queueReport+40);++i)if(live.run(1).stop!=Machine::Stop::Budget)break;
    check(live.ram.read64(queueReport+40)==Ipc::AlreadyInitialized,"failed reload preserves the actual queue connection");
    live.cpu.setPC(Machine::RAM_SIZE);
    check(live.run(1).stop==Machine::Stop::Error && !live.process().managedLayerCount(),"CPU fault retires a connected producer");
    check(live.load(argv[1]),"successful reload creates fresh queue resources");
    const auto reloaded=live.run(500000);
    check(reloaded.stop==Machine::Stop::Exited && reloaded.exitCode==0 && !live.ram.read64(queueReport),"fresh process connects independently after a fault");
    std::cout<<"Queue instructions="<<expectedInstructions<<" failures="<<failures<<'\n';return failures?1:0;
}
