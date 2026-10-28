#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
int main(int argc,char** argv){
    if(argc!=3)return 2;
    unsigned failures=0;auto check=[&](bool value,const char* message){if(!value){++failures;std::cerr<<"FAIL: "<<message<<std::endl;}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;
    if(!symbols.contains("fence_report") || !symbols.contains("report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;
        uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode,"original fence initialization and cleanup return normally");
        auto get=[&](size_t i){return m.ram.read64(symbols.at("fence_report")+8*i);};
        for(size_t at:{size_t(0),size_t(8)})check(!get(at) && !get(at+1) && get(at+2) && get(at+2)!=UINT32_MAX && get(at+3)==UINT32_MAX && !get(at+4),"original nvFenceInit/Exit opens and closes its control descriptor");
        check(get(2)!=get(10) && !m.process().frameCount(),"independent lifetimes use distinct descriptors and produce no frames");
        check(m.ram.read64(symbols.at("report"))==6 && m.ram.read64(symbols.at("report")+112)==77,"constructors and project main remain valid");
        if(budget==500000)instructions=count;else check(count==instructions,"instruction count is independent of slicing");
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;
    unsigned opens=0;
    for(unsigned step=0;step<500000;++step){
        const auto op=m.ram.fetch32(m.cpu.getPC());
        if(op==0xD4000421){
            Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);
            Ipc::Layout layout;
            if(!Ipc::parseBuffers(packet,&m.bus,ipc,layout) && layout.command==0 && layout.sourceSize==16){
                ++opens;const auto context=m.cpu.captureContext();const uint64_t scratch=Process::HeapBase+0x170000;
                auto send=[&](const Ipc::Buffer& b){m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected control Open reply publishes no descriptor");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                std::array<uint32_t,Process::NvDescriptorLimit> descriptors{};
                for(size_t i=0;i<descriptors.size();++i){
                    auto b=packet;const std::string_view path=i%2?"/dev/nvmap":"/dev/nvhost-ctrl";
                    m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>(path.data()),path.size());put(b,8,uint32_t(path.size()));put(b,12,uint32_t(scratch));
                    check(!send(b),"mixed device descriptors share the service quota");descriptors[i]=m.ram.read32(ipc+32);
                    Ipc::Buffer ioctl{};put(ioctl,0,0x01110004);put(ioctl,4,0xC0B);put(ioctl,16,8);put(ioctl,20,uint32_t(scratch+256));put(ioctl,28,8);put(ioctl,32,uint32_t(scratch+256));put(ioctl,48,0x49434653);put(ioctl,56,1);put(ioctl,64,descriptors[i]);put(ioctl,68,0xC0080101);
                    m.ram.write32(scratch+256,4096);m.ram.write32(scratch+260,0xBADF00D);
                    const auto status=send(ioctl);check(status==(i%2?0:Ipc::UnknownCommand),"nvmap ioctls cannot create objects on control descriptors");
                    if(!(i%2))check(m.ram.read32(scratch+260)==0xBADF00D,"unsupported control ioctl leaves caller data unchanged");
                }
                if(opens==1)check(descriptors.front()==1,"failed control Open consumes no descriptor ID");
                check(send(packet)==Process::OutOfResource,"control and nvmap opens enforce a common descriptor limit");
                Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,descriptors.front());
                m.ram.writeBytes(ipc,close.data(),close.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected Close cannot retire a control descriptor");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                for(auto fd:descriptors){put(close,32,fd);check(!send(close),"both device types close and reclaim their slots");}
                put(close,32,descriptors.front());check(send(close)==Process::InvalidHandle,"retired control descriptors remain invalid");
                m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
            }
        }
        if(m.run(1).stop!=Machine::Stop::Budget)break;
    }
    check(opens==2,"both original control-device Open requests are observed");
    std::cout<<"Fence instructions="<<instructions<<" failures="<<failures<<std::endl;return failures?1:0;
}