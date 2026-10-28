#include "machine.hpp"
#include "ipc_nv.hpp"
#include "gpu_channel.hpp"
#include <fstream>
#include <iostream>
#include <map>

static uint8_t pattern(unsigned i,unsigned lifetime){return uint8_t((i*13+lifetime*29+3)^0xA5);}
struct Geometry { unsigned width,rows,sourcePitch,destinationPitch; };
static Geometry geometry(unsigned lifetime){return lifetime?Geometry{17,4,31,48}:Geometry{7,3,13,64};}
static uint8_t expectedByte(unsigned i,unsigned lifetime){const auto g=geometry(lifetime);return i/g.destinationPitch<g.rows && i%g.destinationPitch<g.width?pattern((i/g.destinationPitch)*g.sourcePitch+i%g.destinationPitch,lifetime):0xCC;}
static uint32_t expectedHash(unsigned lifetime){const auto g=geometry(lifetime);uint32_t h=2166136261u;for(unsigned y=0;y<g.rows;++y)for(unsigned x=0;x<g.width;++x)h=(h^pattern(y*g.sourcePitch+x,lifetime))*16777619u;return h;}
static unsigned missingBytes(unsigned lifetime){const auto g=geometry(lifetime);unsigned n=0;for(unsigned y=0;y<g.rows;++y)for(unsigned x=0;x<g.width;++x)n+=pattern(y*g.sourcePitch+x,lifetime)!=0xCC;return n;}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;
    const bool suppress=argc==4 && std::string_view(argv[3])=="--suppress-copy";
    const bool corrupt=argc==4 && std::string_view(argv[3])=="--corrupt-padding";
    const bool fault=suppress || corrupt;
    if(argc==4 && !fault)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;
    while(file>>name>>std::hex>>address)symbols[name]=address;
    if(!symbols.contains("gpu_as_report") || !symbols.contains("report"))return 2;
    uint64_t baseline=0;
    // The two normal runs and the observer never write guest state.
    for(const uint64_t budget:std::array<uint64_t,3>{500000,7,1}){
        if(fault && budget!=1)continue;
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
                    kick=true;const unsigned lifetime=kicks/2;const bool overlap=kicks%2;const auto g=geometry(lifetime);
                    const auto report=symbols.at("gpu_as_report")+lifetime*512;
                    backing=m.ram.read64(report+32*8);const auto va=m.ram.read64(report+36*8);
                    endpoint=uint32_t(m.cpu.readRegister(0));before=m.process().gpuChannelState(endpoint,plan.fd);
                    progress=m.process().gpuChannelExecution(endpoint,plan.fd);
                    check(before.has_value(),"submission owns a live GPU channel");
                    check(m.ram.read64(plan.extraSource)==(va|(18ULL<<42)),"guest submits its own eighteen-word stream");
                    const std::array<uint32_t,18> expected{0x60010000,0xB197,0x60018000,0xB0B5,0x80000040,0x20088100,
                        uint32_t((va+0x4000)>>32),uint32_t(va+0x4000),uint32_t((va+(overlap?0x4001:0x6000))>>32),
                        uint32_t(va+(overlap?0x4001:0x6000)),g.sourcePitch,g.destinationPitch,g.width,g.rows,0x838680C0,0x80000040,0x20010014,0xC02D0000u|lifetime};
                    for(unsigned i=0;i<18;++i)check(m.ram.read32(backing+i*4)==expected[i],"guest generates all pitched geometry and command fields");
                    std::array<uint8_t,72> mapped{};
                    check(m.process().readGpuMemory(m.bus,endpoint,uint32_t(m.ram.read64(report+2*8)),Gpu::VirtualAddress{va},mapped),"guest commands resolve through the original GPU mapping");
                    for(unsigned i=0;i<72;++i)check(mapped[i]==uint8_t(expected[i/4]>>((i%4)*8)),"mapped bytes match guest-generated 2D commands");
                    sourceVersion=m.ram.reservationVersion(backing+0x4000);destinationVersion=m.ram.reservationVersion(backing+0x6000);
                    // Only this separately registered negative test perturbs a command.
                    if(suppress && !kicks)m.ram.write32(backing+14*4,0);
                    ++kicks;
                }
            }
            result=m.run(budget);count+=result.instructions;
            if(kick){
                const bool overlap=(kicks-1)%2;const unsigned lifetime=(kicks-1)/2;const auto g=geometry(lifetime);
                if(corrupt && kicks==1)m.ram.write8(backing+0x6000+g.width,0);
                const auto ipc=m.process().mainThread().localRegion;
                check(!m.cpu.readRegister(0),"transport succeeds independently of the driver result");
                check(m.ram.read32(ipc+24)==(overlap?Ipc::InvalidArguments:0),"copy succeeds and overlapping copy is explicitly refused");
                const auto after=m.process().gpuChannelState(endpoint,plan.fd);
                check(m.ram.reservationVersion(backing+0x4000)==sourceVersion,"GPU copy preserves source reservation versions");
                if(overlap)check(after==before && m.process().gpuChannelExecution(endpoint,plan.fd)==progress && m.ram.reservationVersion(backing+0x6000)==destinationVersion,"refused copy preserves channel state, submission count and destination versions");
                else {
                    check(after && after->bindings[0]==0xB197 && after->bindings[4]==0xB0B5,"guest binds separate graphics and copy engines");
                    check(after && after->copy[4].configured==255 && after->copy[4].length==g.width && after->copy[4].rows==g.rows && after->copy[4].sourcePitch==g.sourcePitch && after->copy[4].destinationPitch==g.destinationPitch && m.process().gpuChannelExecution(endpoint,plan.fd)==std::optional<Process::GpuChannelExecution>{{0xC02D0000u|lifetime,true,1}},"only the successful guest submission publishes its registers and progress");
                    check((m.ram.reservationVersion(backing+0x6000)!=destinationVersion)==!(suppress && kicks==1),"destination reservations change exactly when the transfer executes");
                    check(m.ram.read8(backing+0x5FFF)==0xE1 && m.ram.read8(backing+0x6100)==0xD2,"2D copy preserves both adjacent guards");
                    for(unsigned i=0;i<256;++i)check(m.ram.read8(backing+0x6000+i)==((corrupt && kicks==1 && i==g.width)?0:((suppress && kicks==1)?0xCC:expectedByte(i,lifetime))),"2D result preserves every copied row and padding byte");
                }
            }
        }
        // Horizon ExitProcess carries no status; the inherited guest exit wrapper records main's return.
        check(result.stop==Machine::Stop::Exited && !result.exitCode && m.ram.read64(symbols.at("report")+14*8)==unsigned(fault),"guest exit wrapper records its own copy verification result");
        check(!m.process().frameCount(),"memory copy does not claim rendering");
        if(budget==1)check(kicks==4,"guest submits two accepted and two rejected guest-generated 2D submissions");
        for(unsigned lifetime=0;lifetime<2;++lifetime){
            const auto report=symbols.at("gpu_as_report")+lifetime*512;auto get=[&](unsigned word){return m.ram.read64(report+word*8);};
            const bool missing=suppress && !lifetime,padding=corrupt && !lifetime;const auto g=geometry(lifetime);
            check(!get(0) && !get(1) && !get(3) && !get(5) && get(32) && !get(33) && get(34) && !get(35),"original libnx initializes and maps the copy buffer");
            check(get(54)==g.width && get(55)==g.rows && get(56)==g.sourcePitch && get(57)==g.destinationPitch && get(58)==256-g.width*g.rows,"guest reports its generated geometry and padding verification count");
            check(!get(26) && !get(29) && get(28)==1 && !get(30) && !get(31) && get(48)==18,"original successful Kickoff consumes the guest queue");
            check(!get(39) && get(40)==(missing?missingBytes(lifetime):0) && get(41)==unsigned(padding) && get(42)==g.width*g.rows,"guest checks source, all copied bytes, padding and guards with exact mismatch counts");
            check(get(43)==expectedHash(lifetime) && (missing?get(44)!=get(43):get(44)==get(43)),"independent host hash corroborates guest verification");
            check(get(45)==unsigned(fault),"guest retains a sticky failure through cleanup and later success");
            check(!get(49) && get(50) && get(51)==1 && get(52)==(missing?missingBytes(lifetime):unsigned(padding)) && !get(53),"guest observes overlap refusal and explicitly discards rejected work");
            check(!get(37) && !get(38) && get(15)==Process::TimedOut && !get(16) && get(17)==UINT32_MAX && !get(18) && !get(19) && get(21)==UINT32_MAX && !get(22) && get(23)==UINT32_MAX && !get(24),"original libnx cleans up both lifetimes without fake completion events");
        }
        if(!fault){if(!baseline)baseline=count;else check(count==baseline,"guest result is independent of CPU execution slicing");}
        std::cout<<"Guest instructions: "<<count<<", observed submissions: "<<kicks<<'\n';
    }
    return failures?1:0;
}
