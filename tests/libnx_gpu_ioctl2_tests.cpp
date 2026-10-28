#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_nv.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;const bool baseline=argc==4 && std::string(argv[3])=="--baseline";if(argc==4 && !baseline)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("gpu_as_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){result=m.run(budget);count+=result.instructions;}
        check(result.stop==Machine::Stop::Exited && !result.exitCode && !m.process().frameCount(),"original transport client exits without rendering");
        for(unsigned i=0;i<2;++i){const auto base=symbols.at("gpu_as_report")+i*256;auto get=[&](unsigned j){return m.ram.read64(base+j*8);};
            check(!get(0) && !get(1) && !get(3) && !get(5) && get(6) && get(11) && get(15)==Process::TimedOut,"original full channel initializes before attempting transport");
            check(!get(26) && get(27)==0x40100000000ULL && get(28)==1,"original AppendEntry serializes a synthetic unmapped reference descriptor");
            check(get(29)==(baseline?Ipc::UnsupportedFormat:Process::InvalidAddress) && get(30)==1 && !get(31),"later preflight rejects the unmapped synthetic address without acknowledging work");
            check(!get(16) && get(17)==UINT32_MAX && !get(18) && !get(19) && !get(20) && get(21)==UINT32_MAX && !get(22) && get(23)==UINT32_MAX && !get(24),"original failed-submission cleanup retires all channel resources");
        }
        if(budget==500000)instructions=count;else check(count==instructions,"transport boundary is independent of execution slicing");
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion,scratch=Process::HeapBase+0x170000;unsigned kicks=0;uint32_t clone=0;bool exited=false;
    for(unsigned step=0;step<500000;++step){
        bool cloning=false,kicking=false;
        if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
            Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);const auto context=m.cpu.captureContext();cloning=read(packet,0)==5 && read(packet,24)==4;
            if(read(packet,0)==0x01220004 && read(packet,72)==11){
                kicking=true;++kicks;Ipc::NvIoctl plan;const auto parsed=Ipc::parseNvIoctl(packet,m.bus,ipc,plan);
                check(read(packet,4)==0xC0B && read(packet,8)==0 && read(packet,16)==1 && read(packet,100)==0 && read(packet,104)==0,"actual SDK ioctl2 uses two null indexed X descriptors and a null receive-static buffer");
                if(baseline)check(parsed==Ipc::UnsupportedFormat,"old parser rejects the actual ioctl2 transport layout");
                else {
                    check(!parsed && plan.command==11 && plan.request==0xC018481B && plan.size==24 && plan.extraSize==8 && plan.source==plan.destination,"actual SDK packet resolves its parameter, extra input and receive ranges");
                    check(m.ram.read64(plan.source)==plan.extraSource && m.ram.read32(plan.source+8)==1 && m.ram.read32(plan.source+12)==4 && !m.ram.read64(plan.source+16) && m.ram.read64(plan.extraSource)==0x40100000000ULL,"original KickoffPb carries the original descriptor array and local fence input");
                    std::array<uint8_t,24> original{};for(size_t i=0;i<original.size();++i)original[i]=m.ram.read8(plan.source+i);const auto entry=m.ram.read64(plan.extraSource);
                    auto send=[&](const Ipc::Buffer& p,uint32_t ep=0){m.ram.writeBytes(ipc,p.data(),p.size());m.cpu.restoreContext(context);if(ep)m.cpu.writeRegister(0,ep);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                    check(send(packet)==Process::InvalidAddress && send(packet,clone)==Process::InvalidAddress,"root and clone reject the unmapped synthetic address after transport validation");
                    for(const auto fd:{0U,UINT32_MAX}){auto bad=packet;put(bad,80,fd);check(send(bad)==Process::InvalidHandle,"ioctl2 still checks descriptor ownership");}
                    auto bad=packet;put(bad,40,uint32_t(scratch+4096));m.ram.setPermissions(scratch+4096,4096,Memory::None);check(send(bad)==Ipc::InvalidBuffer,"ioctl2 validates accessibility of the entire extra input");m.ram.setPermissions(scratch+4096,4096,Memory::Read|Memory::Write);
                    bad=packet;put(bad,40,uint32_t(ipc));check(send(bad)==Ipc::InvalidBuffer,"extra input cannot alias the IPC reply");bad=packet;put(bad,40,uint32_t(plan.destination));check(send(bad)==Ipc::InvalidBuffer,"extra input cannot alias the receive range");
                    auto notifier=packet;put(notifier,28,uint32_t(scratch));put(notifier,52,uint32_t(scratch));put(notifier,84,0xC018480C);for(size_t i=0;i<24;i+=4)m.ram.write32(scratch+i,0);
                    check(send(notifier)==Ipc::UnknownCommand && m.process().gpuErrorNotifierEnabled(uint32_t(context.registers[0]),plan.fd)==std::optional<bool>{true},"command 11 cannot execute command-1 notifier methods");
                    auto priority=packet;put(priority,24,4);put(priority,28,uint32_t(scratch));put(priority,48,0);put(priority,52,0);put(priority,56,0);put(priority,84,0x4004480D);m.ram.write32(scratch,50);
                    check(send(priority)==Ipc::UnknownCommand && m.process().gpuChannelPriority(uint32_t(context.registers[0]),plan.fd)==std::optional<uint32_t>{kicks==1?100U:150U},"write-only ioctl2 cannot bypass command separation to alter priority");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected reply cannot acknowledge unsupported submission");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    for(size_t i=0;i<original.size();++i)check(m.ram.read8(plan.source+i)==original[i],"all original ioctl parameters and fence bytes remain unchanged");check(m.ram.read64(plan.extraSource)==entry,"original descriptor bytes remain unchanged");
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }
            }
        }
        const auto result=m.run(1);if(cloning)clone=m.ram.read32(ipc+12);if(kicking)check(baseline?m.cpu.readRegister(0)==Ipc::UnsupportedFormat:!m.cpu.readRegister(0) && m.ram.read32(ipc+24)==Process::InvalidAddress,"actual submission returns the expected kernel or CMIF boundary");if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
    }
    check(exited && kicks==2,"both original kickoff requests reach the measured boundary and clean up");
    std::cout<<"GPU ioctl2 instructions="<<instructions<<" baseline="<<baseline<<" failures="<<failures<<'\n';return failures?1:0;
}
