#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <map>
#include <iostream>

static uint64_t read(const Ipc::Buffer& b,size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4){for(size_t i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i));}
int main(int argc,char** argv)
{
    if(argc!=3 && argc!=4)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream input(argv[2]);std::string name;uint64_t value;
    while(input>>name>>std::hex>>value)symbols[name]=value;
    if(!symbols.contains("report") || !symbols.contains("vi_report"))return 2;
    uint64_t expectedInstructions=0;std::array<uint64_t,32> expected{};
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;
        uint64_t total=0;Machine::Result result{Machine::Stop::Budget,0};
        while(total<500000 && result.stop==Machine::Stop::Budget){result=m.run(budget);total+=result.instructions;}
        if(result.stop!=Machine::Stop::Exited)std::cerr<<m.error()<<'\n';
        check(result.stop==Machine::Stop::Exited && result.exitCode==0,"original VI client returns through normal app cleanup");
        const auto report=symbols.at("vi_report");auto get=[&](size_t i){return m.ram.read64(report+i*8);};
        check(get(0)==1 && get(1)==0 && get(2) && get(3) && get(2)!=get(3),"original viInitialize obtains independent application and binder relay sessions");
        check(get(4)==0 && get(6)==0 && get(5)==1 && get(7)==1,"two original default-display opens return the local logical display ID");
        check(get(8)==Ipc::UnknownCommand && get(9)==UINT64_MAX && get(10)==UINT64_MAX,"unsupported resolution returns no fabricated geometry");
        check(get(11)==0 && get(12)==1 && get(13)==1,"original VI client parses the local native-window parcel and initializes its layer");
        if(argc==4){
            check(get(20)==0x1000 && !get(21) && get(22)==1,"original Binder client initializes references to the returned producer");
            check(get(23)==0,"original producer native-handle query returns its availability event");
            check(!get(25) && !get(26) && !get(27) && !get(28) && !get(29) && get(30)!=0,"original Binder close and VI layer close retire client state");
        }
        check(get(14)==0 && !get(15) && get(16)==0 && get(17)!=0,"original display close clears client state and rejects repeated close");
        check(!get(18) && !get(19),"original viExit clears both service sessions");
        const auto base=symbols.at("report");check(m.ram.read64(base)==6 && m.ram.read64(base+112)==77 && !m.ram.read64(base+120),"startup constructor/main checks pass without dependency diagnostic");
        check(m.process().frameCount()==0,"VI initialization does not fabricate a rendered frame");
        if(budget==500000){expectedInstructions=total;for(size_t i=0;i<32;++i)expected[i]=get(i);}
        else {check(total==expectedInstructions,"VI execution count is independent of slicing");for(size_t i=0;i<32;++i)check(get(i)==expected[i],"VI report is independent of slicing");}
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;
    const auto ipc=m.process().mainThread().localRegion,report=symbols.at("vi_report");
    Ipc::Buffer open{};uint64_t svc=0;std::vector<std::string> names;bool managedCall=false;
    for(unsigned i=0;i<500000 && m.ram.read64(report+56)!=1;++i){
        const auto pc=m.cpu.getPC();const auto op=m.ram.fetch32(pc);bool send=(op&0xFFE0001F)==0xD4000001 && ((op>>5)&0xFFFF)==0x21;
        Ipc::Buffer b{};if(send)for(size_t j=0;j<b.size();++j)b[j]=m.ram.read8(ipc+j);
        const auto step=m.run(1);if(step.stop!=Machine::Stop::Budget)return 1;
        if(send){
            if(read(b,16)==0x49434653 && read(b,24)==1){std::string n;for(size_t j=32;j<40 && b[j];++j)n+=char(b[j]);if(n.starts_with("vi:"))names.push_back(n);}
            if(read(b,16)==0x49434653 && read(b,24)==1010){open=b;svc=pc;}
        }
    }
    check(names==std::vector<std::string>{"vi:m","vi:s","vi:u"},"original default client falls back from unavailable manager/system services to vi:u");
    if(!svc)return 1;
    const auto context=m.cpu.captureContext();const auto application=uint32_t(m.ram.read64(report+16)),relay=uint32_t(m.ram.read64(report+24));
    auto send=[&](const Ipc::Buffer& packet,uint32_t handle){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);m.cpu.setPC(svc);m.cpu.writeRegister(0,handle);m.run(1);return m.cpu.readRegister(0);};
    Ipc::Buffer close{};put(close,0,4);put(close,4,10);put(close,16,0x49434653);put(close,24,1020);put(close,32,1,8);
    auto bad=open;bad[32]='X';check(send(bad,application)==0 && m.ram.read32(ipc+24)==Ipc::PortNotFound,"unknown logical display name is rejected");
    bad=open;put(bad,4,23);check(send(bad,application)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"display name requires exactly 64 bytes");
    bad=close;put(bad,32,2,8);check(send(bad,application)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"wrong display ID cannot close a reference");
    m.ram.writeBytes(ipc,open.data(),open.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.cpu.setPC(svc);m.cpu.writeRegister(0,application);m.run(1);
    check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected open reply does not publish reference");m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
    for(unsigned i=0;i<62;++i)check(send(open,application)==0 && !m.ram.read32(ipc+24),"logical display references are bounded and shared by the application session");
    check(send(open,application)==0 && m.ram.read32(ipc+24)==Process::OutOfResource,"display reference quota refuses excess opens");
    for(unsigned i=0;i<64;++i)check(send(close,application)==0 && !m.ram.read32(ipc+24),"all successful display references close");
    check(send(close,application)==0 && m.ram.read32(ipc+24)==Process::InvalidState,"failed opens add no hidden reference");
    Ipc::Buffer unknown{};put(unknown,0,4);put(unknown,4,8);put(unknown,16,0x49434653);put(unknown,24,99);
    check(send(unknown,relay)==0 && m.ram.read32(ipc+24)==Ipc::UnknownCommand,"binder relay operations remain explicitly unimplemented");
    send(open,application);send(open,application);m.cpu.restoreContext(context);
    Ipc::Buffer create{};CPU::Context createContext{};uint64_t createPc=0;uint32_t appletHandle=0;bool openLayer=false;
    for(unsigned remaining=0;remaining<500000;++remaining){
        const auto pc=m.cpu.getPC();const auto op=m.ram.fetch32(pc);
        if((op&0xFFE0001F)==0xD4000001 && ((op>>5)&0xFFFF)==0x21){
            Ipc::Buffer b{};for(size_t j=0;j<b.size();++j)b[j]=m.ram.read8(ipc+j);
            Ipc::DomainRequest d;
            if(!Ipc::parseDomain(b,d) && d.object==6 && d.command==40){
                managedCall=true;create=b;createContext=m.cpu.captureContext();createPc=pc;appletHandle=uint32_t(m.cpu.readRegister(0));
                auto replay=[&](const Ipc::Buffer& packet){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(createContext);m.run(1);return m.cpu.readRegister(0);};
                auto wrong=b;put(wrong,20,7);
                check(replay(wrong)==0 && m.ram.read32(ipc+40)==Ipc::UnknownCommand && !m.process().managedLayerCount(),"window controller cannot allocate self-controller layers");
                auto extra=b;put(extra,18,17,2);put(extra,4,13);
                check(replay(extra)==0 && m.ram.read32(ipc+40)==Ipc::InvalidArguments && !m.process().managedLayerCount(),"layer creation rejects extra payload");
                m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(createContext);
                m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer && !m.process().managedLayerCount(),"protected reply cannot consume a layer ID");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(createContext);
            }
            // Original OpenLayer has a PID marker and one output map-alias descriptor.
            for(size_t at=16;at+96<=b.size();at+=16)if(read(b,at)==0x49434653 && read(b,at+8)==2020){
                openLayer=true;
                check((read(b,4)&0x80000000U) && ((read(b,0)>>24)&15)==1,"original VI OpenLayer sends PID and one receive descriptor");
                check(read(b,at+16+64,8)==1 && read(b,at+16+72,8)==m.process().appletResourceId(),"OpenLayer uses the allocated layer and process resource identity");
                const auto saved=m.cpu.captureContext();
                const auto flags=read(b,28),destination=read(b,24)|((flags>>28&15)<<32)|((flags>>2&0x3FFFFF)<<36);
                Ipc::Buffer outputBefore{};for(size_t j=0;j<outputBefore.size();++j)outputBefore[j]=m.ram.read8(destination+j);
                auto replayOpen=[&](const Ipc::Buffer& packet){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(saved);m.run(1);return m.cpu.readRegister(0);};
                Ipc::Buffer closeLayer{};put(closeLayer,0,4);put(closeLayer,4,10);put(closeLayer,16,0x49434653);put(closeLayer,24,2021);put(closeLayer,32,1,8);
                m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(saved);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected OpenLayer reply cannot publish a producer");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                check(replayOpen(b)==0 && !m.ram.read32(ipc+24) && m.ram.read64(ipc+32)==28,"valid OpenLayer publishes its bounded local parcel length");
                check(m.ram.read32(destination)==12 && m.ram.read32(destination+4)==16 && !m.ram.read32(destination+8) && m.ram.read32(destination+12)==28 && m.ram.read32(destination+24)==0x1000,"native-window parcel carries a typed producer object in the original reader layout");
                for(size_t j=28;j<outputBefore.size();++j)check(m.ram.read8(destination+j)==outputBefore[j],"parcel publication preserves unused output-buffer bytes");
                check(replayOpen(b)==0 && m.ram.read32(ipc+24)==Ipc::AlreadyInitialized,"duplicate layer open does not create another producer");
                Ipc::Buffer ref{};put(ref,0,4);put(ref,4,11);put(ref,16,0x49434653);put(ref,24,1);put(ref,32,0x1000);put(ref,36,1);put(ref,40,0);
                auto adjust=[&](uint32_t delta,uint32_t type){put(ref,36,delta);put(ref,40,type);const auto transport=send(ref,relay);return transport ? uint32_t(transport) : m.ram.read32(ipc+24);};
                m.ram.writeBytes(ipc,ref.data(),ref.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.cpu.setPC(svc);m.cpu.writeRegister(0,relay);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected Binder reply cannot increment references");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                check(adjust(UINT32_MAX,0)==Process::InvalidState,"Binder reference underflow is rejected");
                check(adjust(0,0)==Ipc::InvalidArguments && adjust(1,2)==Ipc::InvalidArguments,"Binder rejects unsupported deltas and reference types");
                for(unsigned i=0;i<64;++i)check(adjust(1,0)==0,"bounded Binder weak references increment");
                check(adjust(1,0)==Process::OutOfResource,"Binder reference quota prevents overflow");
                check(adjust(1,1)==0,"producer accepts an independent strong reference");
                check(send(closeLayer,application)==0 && !m.ram.read32(ipc+24),"owner closes its open layer");
                check(adjust(UINT32_MAX,1)==0,"strong reference can retire after layer close");
                for(unsigned i=0;i<64;++i)check(adjust(UINT32_MAX,0)==0,"weak references keep the closed producer alive until released");
                check(adjust(1,0)==Process::InvalidState,"producer cannot resurrect without an open layer or live references");
                check(send(closeLayer,application)==0 && m.ram.read32(ipc+24)==Process::InvalidState,"duplicate layer close is rejected");
                m.ram.writeBytes(destination,outputBefore.data(),outputBefore.size());
                auto invalid=b;put(invalid,at+16+72,m.process().appletResourceId()+1,8);
                check(replayOpen(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"OpenLayer rejects a foreign process resource ID");
                invalid=b;put(invalid,at+16+64,0,8);
                check(replayOpen(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"OpenLayer rejects zero layer ID");
                put(invalid,at+16+64,Process::ManagedLayerLimit+1,8);
                check(replayOpen(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"OpenLayer rejects unallocated layer IDs");
                invalid=b;invalid[at+16]='X';
                check(replayOpen(invalid)==0 && m.ram.read32(ipc+24)==Ipc::PortNotFound,"OpenLayer rejects an unknown display name");
                invalid=b;put(invalid,20,128);
                check(replayOpen(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"OpenLayer requires the original 256-byte output buffer");
                invalid=b;put(invalid,4,(read(b,4)&0x80000000U)|27);
                check(replayOpen(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"OpenLayer requires exactly 80 input bytes");
                invalid=b;put(invalid,12,UINT64_MAX,8);
                check(replayOpen(invalid)==0 && !m.ram.read32(ipc+24),"PID placeholder bytes do not override authenticated local process ownership");
                send(closeLayer,application);m.ram.writeBytes(destination,outputBefore.data(),outputBefore.size());
                invalid=b;put(invalid,8,3);
                check(replayOpen(invalid)==Ipc::UnsupportedFormat,"OpenLayer rejects copied-handle special headers");
                invalid=b;put(invalid,24,ipc);put(invalid,28,0);
                check(replayOpen(invalid)==Ipc::InvalidBuffer,"OpenLayer output cannot overlap the IPC request");
                invalid=b;put(invalid,20,Ipc::MaxTransfer+1);
                check(replayOpen(invalid)==Ipc::TransferLimit,"OpenLayer rejects oversized transfers before service execution");
                m.ram.setPermissions(destination&~uint64_t(Memory::PAGE_SIZE-1),Memory::PAGE_SIZE,Memory::Read);
                check(replayOpen(b)==Ipc::InvalidBuffer,"OpenLayer rejects protected output memory");
                m.ram.setPermissions(destination&~uint64_t(Memory::PAGE_SIZE-1),Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                send(close,application);send(close,application);
                check(replayOpen(b)==0 && m.ram.read32(ipc+24)==Process::InvalidState,"OpenLayer requires a display opened by its application service");
                send(open,application);send(open,application);
                for(size_t j=0;j<outputBefore.size();++j)check(m.ram.read8(destination+j)==outputBefore[j],"invalid OpenLayer calls leave native-window output unchanged");
                for(size_t layer=2;layer<=Process::ManagedLayerLimit;++layer){
                    m.ram.writeBytes(ipc,create.data(),create.size());m.cpu.restoreContext(createContext);m.cpu.writeRegister(0,appletHandle);
                    m.run(1);
                    check(m.cpu.readRegister(0)==0 && !m.ram.read32(ipc+40) && m.ram.read64(ipc+48)==layer,"managed layer IDs are distinct and process scoped");
                }
                m.ram.writeBytes(ipc,create.data(),create.size());m.cpu.restoreContext(createContext);m.run(1);
                check(m.ram.read32(ipc+40)==Process::OutOfResource && m.process().managedLayerCount()==Process::ManagedLayerLimit,"layer quota rejects extra allocation without mutation");
                m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(saved);
            }
        }
        if(m.run(1).stop!=Machine::Stop::Budget)break;
    }
    check(managedCall,"actual original layer creation requests SelfController command 40");
    check(openLayer && createPc && !m.process().managedLayerCount(),"original execution reaches OpenLayer and process exit retires all logical layers");
    check(m.load(argv[1]) && !m.process().managedLayerCount(),"successful reload starts with no logical layers");
    for(unsigned i=0;i<500000 && !m.process().managedLayerCount();++i)if(m.run(1).stop!=Machine::Stop::Budget)break;
    check(m.process().managedLayerCount()==1,"reloaded original client creates its own layer");
    const auto liveId=m.process().appletResourceId();
    const auto missing=std::string(argv[1])+".missing";
    check(!m.load(missing.c_str()) && m.process().managedLayerCount()==1 && m.process().appletResourceId()==liveId,"failed reload preserves process layer ownership");
    m.cpu.setPC(Machine::RAM_SIZE);
    check(m.run(1).stop==Machine::Stop::Error && !m.process().managedLayerCount(),"CPU fault retires process layers");
    std::cout<<"VI instructions="<<expectedInstructions<<" failures="<<failures<<'\n';return failures?1:0;
}
