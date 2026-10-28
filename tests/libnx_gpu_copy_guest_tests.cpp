#include "machine.hpp"
#include "ipc_nv.hpp"
#include "gpu_channel.hpp"
#include <fstream>
#include <iostream>
#include <map>

static uint8_t pattern(unsigned i,unsigned lifetime){return uint8_t((i*13+lifetime*29+3)^0xA5);}
static uint32_t expectedHash(unsigned lifetime){uint32_t h=2166136261u;for(unsigned i=0;i<64;++i)h=(h^pattern(i,lifetime))*16777619u;return h;}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;
    const bool suppress=argc==4 && std::string_view(argv[3])=="--suppress-copy";
    if(argc==4 && !suppress)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;
    while(file>>name>>std::hex>>address)symbols[name]=address;
    if(!symbols.contains("gpu_as_report") || !symbols.contains("report"))return 2;
    uint64_t baseline=0;
    // The two normal runs and the observer never write guest state.
    for(const uint64_t budget:std::array<uint64_t,3>{500000,7,1}){
        if(suppress && budget!=1)continue;
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;
        unsigned kicks=0;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(result.stop==Machine::Stop::Budget && count<500000){
            bool kick=false;Ipc::NvIoctl plan{};uint32_t endpoint=0;uint64_t backing=0,sourceVersion=0,destinationVersion=0;
            std::optional<Gpu::ChannelState> before;
            std::optional<Process::GpuChannelExecution> progress;
            if(budget==1 && m.ram.fetch32(m.cpu.getPC())==0xD4000421){
                const auto ipc=m.process().mainThread().localRegion;Ipc::Buffer packet{};
                for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);
                if(!Ipc::parseNvIoctl(packet,m.bus,ipc,plan) && plan.command==11 && plan.request==0xC018481B){
                    kick=true;const unsigned lifetime=kicks/2;const bool overlap=kicks%2;
                    const auto report=symbols.at("gpu_as_report")+lifetime*512;
                    backing=m.ram.read64(report+32*8);const auto va=m.ram.read64(report+36*8);
                    endpoint=uint32_t(m.cpu.readRegister(0));before=m.process().gpuChannelState(endpoint,plan.fd);
                    progress=m.process().gpuChannelExecution(endpoint,plan.fd);
                    check(before.has_value(),"submission owns a live GPU channel");
                    check(m.ram.read64(plan.extraSource)==(va|(16ULL<<42)),"guest submits its own sixteen-word stream");
                    const std::array<uint32_t,16> expected{0x60010000,0xB197,0x60018000,0xB0B5,0x80000040,0x20048100,
                        uint32_t((va+0x4000)>>32),uint32_t(va+0x4000),uint32_t((va+(overlap?0x4001:0x6000))>>32),
                        uint32_t(va+(overlap?0x4001:0x6000)),0x20018106,64,0x818680C0,0x80000040,0x20010014,0xC0DE0000u|lifetime};
                    for(unsigned i=0;i<16;++i)check(m.ram.read32(backing+i*4)==expected[i],"guest generates the documented class, address, length and launch fields");
                    std::array<uint8_t,64> mapped{};
                    check(m.process().readGpuMemory(m.bus,endpoint,uint32_t(m.ram.read64(report+2*8)),Gpu::VirtualAddress{va},mapped),"guest command buffer resolves through its original GPU mapping");
                    for(unsigned i=0;i<64;++i)check(mapped[i]==uint8_t(expected[i/4]>>((i%4)*8)),"mapped command bytes match guest-generated fields");
                    sourceVersion=m.ram.reservationVersion(backing+0x4000);destinationVersion=m.ram.reservationVersion(backing+0x6000);
                    // Only this separately registered negative test perturbs a command.
                    if(suppress && !kicks)m.ram.write32(backing+12*4,0);
                    ++kicks;
                }
            }
            result=m.run(budget);count+=result.instructions;
            if(kick){
                const bool overlap=(kicks-1)%2;const unsigned lifetime=(kicks-1)/2;
                const auto ipc=m.process().mainThread().localRegion;
                check(!m.cpu.readRegister(0),"transport succeeds independently of the driver result");
                check(m.ram.read32(ipc+24)==(overlap?Ipc::InvalidArguments:0),"copy succeeds and overlapping copy is explicitly refused");
                const auto after=m.process().gpuChannelState(endpoint,plan.fd);
                check(m.ram.reservationVersion(backing+0x4000)==sourceVersion,"GPU copy preserves source reservation versions");
                if(overlap)check(after==before && m.process().gpuChannelExecution(endpoint,plan.fd)==progress && m.ram.reservationVersion(backing+0x6000)==destinationVersion,"refused copy preserves channel state, submission count and destination versions");
                else {
                    check(after && after->bindings[0]==0xB197 && after->bindings[4]==0xB0B5,"guest binds separate graphics and copy engines");
                    check(after && after->copy[4].configured==31 && after->copy[4].length==64 && m.process().gpuChannelExecution(endpoint,plan.fd)==std::optional<Process::GpuChannelExecution>{{0xC0DE0000u|lifetime,true,1}},"only the successful guest submission publishes its registers and progress");
                    check(m.ram.reservationVersion(backing+0x6000)!=destinationVersion || (suppress && kicks==1),"completed transfer updates destination reservations");
                    for(unsigned i=0;i<64;++i)check(m.ram.read8(backing+0x6000+i)==((suppress && kicks==1)?0xCC:pattern(i,lifetime)),"transfer result matches guest-generated source");
                }
            }
        }
        // Horizon ExitProcess carries no status; the inherited guest exit wrapper records main's return.
        check(result.stop==Machine::Stop::Exited && !result.exitCode && m.ram.read64(symbols.at("report")+14*8)==unsigned(suppress),"guest exit wrapper records its own copy verification result");
        check(!m.process().frameCount(),"memory copy does not claim rendering");
        if(budget==1)check(kicks==4,"guest submits two independent successful and two rejected transfers");
        for(unsigned lifetime=0;lifetime<2;++lifetime){
            const auto report=symbols.at("gpu_as_report")+lifetime*512;auto get=[&](unsigned word){return m.ram.read64(report+word*8);};
            const bool missing=suppress && !lifetime;
            check(!get(0) && !get(1) && !get(3) && !get(5) && get(32) && !get(33) && get(34) && !get(35),"original libnx initializes and maps the copy buffer");
            check(!get(26) && !get(29) && get(28)==1 && !get(30) && !get(31) && get(48)==16,"original successful Kickoff consumes the guest queue");
            check(!get(39) && (missing?get(40)>0:!get(40)) && !get(41) && get(42)==64,"guest checks source, destination and guards");
            check(get(43)==expectedHash(lifetime) && (missing?get(44)!=get(43):get(44)==get(43)),"independent host hash corroborates guest verification");
            check(get(45)==unsigned(suppress),"guest retains a sticky failure through cleanup and later success");
            check(!get(49) && get(50) && get(51)==1 && (missing?get(52)>0:!get(52)) && !get(53),"guest observes overlap refusal and explicitly discards rejected work");
            check(!get(37) && !get(38) && get(15)==Process::TimedOut && !get(16) && get(17)==UINT32_MAX && !get(18) && !get(19) && get(21)==UINT32_MAX && !get(22) && get(23)==UINT32_MAX && !get(24),"original libnx cleans up both lifetimes without fake completion events");
        }
        if(!suppress){if(!baseline)baseline=count;else check(count==baseline,"guest result is independent of CPU execution slicing");}
        std::cout<<"Guest instructions: "<<count<<", observed submissions: "<<kicks<<'\n';
    }
    return failures?1:0;
}
