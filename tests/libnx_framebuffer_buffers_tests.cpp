#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_binder.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
int main(int argc,char** argv){
    if(argc!=3)return 2;unsigned failures=0;
    auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t address;
    while(file>>name>>std::hex>>address)symbols[name]=address;if(!symbols.contains("framebuffer_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;unsigned ordinal=0;std::shared_ptr<const Frame> first;
        Machine::Result result{Machine::Stop::Budget,0};
        while((result.stop==Machine::Stop::Budget || result.stop==Machine::Stop::Frame) && count<3000000){
            result=m.run(budget,true);count+=result.instructions;if(result.stop!=Machine::Stop::Frame)continue;
            const unsigned buffers=ordinal<2?1:ordinal<6?2:3,frame=ordinal-(buffers==1?0:buffers==2?2:6);++ordinal;
            const auto snapshot=m.process().frame();check(ordinal<=12 && m.process().frameCount()==ordinal && snapshot && snapshot->width==16 && snapshot->height==16 && snapshot->rgb.size()==768,"each original End publishes one complete expected snapshot");
            if(snapshot && snapshot->rgb.size()==768)for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){const auto at=(y*16+x)*3;
                check(snapshot->rgb[at]==uint8_t(x*17+frame*29+buffers*7) && snapshot->rgb[at+1]==uint8_t(y*17+frame*11+buffers*13) && snapshot->rgb[at+2]==uint8_t((x^y)*17+frame*19+buffers*23),"rotation never mixes pixels from another slot or generation");}
            const auto sequence=m.ram.read64(symbols.at("framebuffer_report")+(buffers-1)*128+5*8);
            check(((sequence>>(frame*3))&7)==frame%buffers,"original client follows the local fair reuse order");if(ordinal==1)first=snapshot;
        }
        check(result.stop==Machine::Stop::Exited && !result.exitCode && ordinal==12 && !m.process().managedLayerCount(),"all three configurations exit and retire their layers");
        if(ordinal!=12)std::cerr<<"Stopped after "<<ordinal<<" frames, exit="<<result.exitCode<<" PC="<<std::hex<<m.cpu.getPC()<<std::dec<<" error="<<(m.cpu.getLastError()?m.cpu.getLastError():"none")<<'\n';
        for(unsigned buffers=1;buffers<=3;++buffers){
            const auto base=symbols.at("framebuffer_report")+(buffers-1)*128;auto get=[&](size_t i){return m.ram.read64(base+i*8);};uint64_t sequence=0;
            if(ordinal!=12){std::cerr<<"Configuration "<<buffers<<':';for(size_t i=0;i<10;++i)std::cerr<<' '<<get(i);std::cerr<<'\n';}
            for(unsigned f=0;f<buffers*2;++f)sequence|=uint64_t(f%buffers)<<(f*3);
            check(!get(0) && get(1)==1 && get(2)==((1U<<buffers)-1) && !get(3) && get(4)==64 && get(5)==sequence && get(6)==buffers*2,"original Create/MakeLinear/Begin/End handles the requested buffer count");
            check(get(7)==1 && !get(8) && get(9)==1,"Close and repeated Close release each buffer configuration");
        }
        check(first && first!=m.process().frame() && first->rgb[0]==7 && first->rgb[45]==6,"first immutable snapshot survives all later slots and map cleanup");
        const auto window=symbols.at("window_report");check(m.ram.read64(window+13*8)==107 && !m.ram.read64(window+19*8),"three allocation lifetimes preserve the event quota and normal window cleanup");
        if(budget==500000)instructions=count;else check(count==instructions,"buffer-count execution is independent of slicing");
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;
    unsigned creates=0,allocations=0;std::array<unsigned,3> registrations{},queues{};bool exited=false;
    for(unsigned step=0;step<3000000;++step){
        if(m.ram.fetch32(m.cpu.getPC())==0xD4000421){
            Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);
            if(read(packet,0)==0x01110004 && read(packet,68)==0xC0080101){++creates;check(creates<=3 && m.ram.read32(read(packet,20))==creates*8192,"each original Create requests exactly its configured surface count");}
            if(read(packet,0)==0x01110004 && read(packet,68)==0xC0200104)++allocations;
            Ipc::BinderTransaction plan;
            if(creates>=1 && creates<=3 && !Ipc::parseBinderTransaction(packet,m.bus,ipc,plan) && (plan.code==14 || plan.code==7)){
                const auto context=m.cpu.captureContext();std::vector<uint8_t> input(size_t(plan.sourceSize));for(size_t i=0;i<input.size();++i)input[i]=m.ram.read8(plan.source+i);
                auto send=[&](const Ipc::Buffer& message){m.ram.writeBytes(ipc,message.data(),message.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                if(plan.code==14){
                    auto& n=registrations[creates-1];check(m.ram.read32(plan.source+96)==n && m.ram.read32(plan.source+232)==n*8192,"registration uses a distinct bounded extent for each original slot");
                    m.ram.write32(plan.source+232,creates*8192);check(send(packet)==Ipc::InvalidArguments,"registration cannot extend beyond its map");m.ram.writeBytes(plan.source,input.data(),input.size());
                    if(n){m.ram.write32(plan.source+232,0);check(send(packet)==Ipc::InvalidArguments,"two slots cannot alias the same block-linear map extent");m.ram.writeBytes(plan.source,input.data(),input.size());}++n;
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
                }else{
                    auto& n=queues[creates-1];check(m.ram.read32(plan.source+96)==n%creates,"trace confirms original slot reuse order");
                    const auto frames=m.process().frameCount();const auto previous=m.process().frame();
                    m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,4096,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                    check(m.cpu.readRegister(0)==Process::InvalidPointer && m.process().frameCount()==frames && m.process().frame()==previous,"failed Queue reply preserves frame, cursor and acquisition");m.ram.setPermissions(ipc,4096,Memory::Read|Memory::Write);
                    if(!n){
                        const bool automatic=read(packet,0)==0x01110004;const size_t descriptor=automatic?16:8,cmif=automatic?48:32;
                        auto dequeue=packet;put(dequeue,descriptor,116);put(dequeue,cmif+20,3);
                        auto prepareDequeue=[&](){m.ram.writeBytes(plan.source,input.data(),input.size());m.ram.write32(plan.source,100);m.ram.write32(plan.source+12,116);for(size_t i=0;i<5;++i)m.ram.write32(plan.source+96+i*4,std::array<uint32_t,5>{1,16,16,1,0xB00}[i]);};
                        for(unsigned slot=1;slot<creates;++slot){prepareDequeue();check(!send(dequeue) && m.ram.read32(plan.destination+16)==slot,"remaining buffers can all be acquired without reuse of an owned slot");}
                        prepareDequeue();check(!send(dequeue) && m.ram.read32(plan.destination+16)==UINT32_MAX && m.ram.read32(plan.destination+68)==uint32_t(-11),"exhausted buffers return WouldBlock without another acquisition");
                        auto cancel=packet;put(cancel,descriptor,144);put(cancel,cmif+20,8);
                        for(unsigned slot=1;slot<creates;++slot){
                            std::vector<uint8_t> parcel(144);std::copy(input.begin(),input.begin()+96,parcel.begin());m.ram.writeBytes(plan.source,parcel.data(),parcel.size());m.ram.write32(plan.source,128);m.ram.write32(plan.source+12,144);m.ram.write32(plan.source+96,slot);m.ram.write32(plan.source+100,36);
                            check(!send(cancel) && m.process().frameCount()==frames,"Cancel releases owned extra buffers without presenting a frame");
                        }
                        m.ram.writeBytes(plan.source,input.data(),input.size());
                    }
                    check(!send(packet) && m.process().frameCount()==frames+1,"original Queue succeeds once after rollback and exhaustion probes");++n;
                    const auto after=m.cpu.captureContext();Ipc::Buffer response{};for(size_t i=0;i<response.size();++i)response[i]=m.ram.read8(ipc+i);
                    std::vector<uint8_t> output(size_t(plan.destinationSize));for(size_t i=0;i<output.size();++i)output[i]=m.ram.read8(plan.destination+i);
                    check(send(packet)==Ipc::NotInitialized && m.process().frameCount()==frames+1,"duplicate Queue cannot publish a second frame or advance reuse order");
                    m.ram.writeBytes(plan.destination,output.data(),output.size());m.ram.writeBytes(ipc,response.data(),response.size());m.cpu.restoreContext(after);continue;
                }
            }
        }
        const auto result=m.run(1);if(result.stop!=Machine::Stop::Budget){exited=result.stop==Machine::Stop::Exited && !result.exitCode;break;}
    }
    check(exited && creates==3 && allocations==3 && registrations==std::array<unsigned,3>{1,2,3} && queues==std::array<unsigned,3>{2,4,6} && m.process().frameCount()==12,"actual NV/Binder trace completes all buffer-count lifetimes without extra frames");
    std::cout<<"Buffer configurations=1,2,3 instructions="<<instructions<<" frames=12 failures="<<failures<<'\n';return failures?1:0;
}
