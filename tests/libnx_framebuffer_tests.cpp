#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_binder.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint32_t read(const Ipc::Buffer& b,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[at+i])<<(8*i);return v;}
int main(int argc,char** argv){
    if(argc<3 || argc>5)return 2;const bool wide=argc>=4 && std::string(argv[3])=="--wide";
    const bool present=wide || (argc>=4 && std::string(argv[3])=="--present");if(argc>=4 && !present)return 2;
    const uint32_t width=wide?37:16,height=wide?137:16,pitch=wide?192:64,surfaceSize=wide?49152:8192;
    const size_t rgbSize=size_t(width)*height*3;
    unsigned failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<"FAIL: "<<message<<std::endl;}};
    std::map<std::string,uint64_t> symbols;std::ifstream file(argv[2]);std::string name;uint64_t value;while(file>>name>>std::hex>>value)symbols[name]=value;
    if(!symbols.contains("framebuffer_report"))return 2;
    uint64_t instructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        std::shared_ptr<const Frame> firstFrame;
        while((result.stop==Machine::Stop::Budget || result.stop==Machine::Stop::Frame) && count<2000000){
            result=m.run(budget,present);count+=result.instructions;
            if(result.stop==Machine::Stop::Frame){
                const auto frame=m.process().frame();const auto ordinal=m.process().frameCount();
                check(ordinal<=2 && frame && frame->rgb.size()==rgbSize,"each original framebufferEnd publishes exactly one complete frame");
                if(frame && frame->rgb.size()==rgbSize)for(size_t y=0;y<height;++y)for(size_t x=0;x<width;++x){const auto at=(y*width+x)*3;const uint8_t mask=ordinal==2?255:0;check(frame->rgb[at]==uint8_t((x*17)^mask) && frame->rgb[at+1]==uint8_t((y*17)^mask) && frame->rgb[at+2]==uint8_t(((x^y)*17)^mask),"every frame matches the original linear-to-GOB conversion");}
                if(ordinal==1)firstFrame=frame;
            }
        }
        auto get=[&](size_t i){return m.ram.read64(symbols.at("framebuffer_report")+i*8);};
        if(result.stop!=Machine::Stop::Exited)std::cerr<<"Framebuffer stopped at PC="<<std::hex<<m.cpu.getPC()<<std::dec<<" error="<<(m.cpu.getLastError()?m.cpu.getLastError():"none")<<'\n';
        check(result.stop==Machine::Stop::Exited && !result.exitCode,"original framebufferCreate success and cleanup return normally");
        check(!get(0) && get(1)==1 && get(2)==3 && get(3)==1 && !get(4) && get(5)==1,"original framebufferCreate configures both slots and owns backing before close");
        check(m.process().frameCount()==(present?2U:0U) && !m.process().managedLayerCount(),"expected frames are produced and layers retire");
        if(present){
            check(firstFrame && firstFrame!=m.process().frame() && firstFrame->rgb[0]==0 && firstFrame->rgb[45]==255,"first snapshot survives the second frame and original backing cleanup");
            check(!get(6) && get(7)==1 && get(8)==pitch && get(9)==1 && get(10)==1 && get(11)==pitch && get(12)==1,"original MakeLinear/Begin/End complete two frames");
            const auto frame=m.process().frame();check(frame && frame->width==width && frame->height==height && frame->rgb.size()==rgbSize,"original framebuffer produces the requested RGB geometry");
            if(argc==5 && budget==500000 && frame){std::string error;check(frame->savePng(argv[4],error),"original framebuffer snapshot can be exported as PNG");}
            if(frame && frame->rgb.size()==rgbSize)for(size_t y=0;y<height;++y)for(size_t x=0;x<width;++x){const auto at=(y*width+x)*3;check(frame->rgb[at]==uint8_t(255-x*17) && frame->rgb[at+1]==uint8_t(255-y*17) && frame->rgb[at+2]==uint8_t(255-(x^y)*17),"original software GOB conversion preserves every pixel");}
        }
        const auto window=symbols.at("window_report");check(m.ram.read64(window+13*8)==107 && !m.ram.read64(window+19*8),"NV/fence failure cleanup leaves the original event quota and window cleanup intact");
        if(budget==500000)instructions=count;else check(count==instructions,"framebuffer execution is independent of slicing");
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;unsigned allocations=0,registrations=0,creates=0,queues=0;uint64_t backing=0;
    for(unsigned step=0;step<2000000;++step){
        bool allocation=false,registration=false;const auto op=m.ram.fetch32(m.cpu.getPC());
        if(op==0xD4000421){
            Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=m.ram.read8(ipc+i);
            if(read(packet,0)==0x01110004 && read(packet,68)==0xC0080101){++creates;check(m.ram.read32(read(packet,20))==surfaceSize*2,"original framebuffer requests two padded surfaces");}
            if(read(packet,0)==0x01110004 && read(packet,68)==0xC0200104){
                ++allocations;allocation=true;const auto source=read(packet,20);const auto address=m.ram.read64(source+24);
                backing=address;
                check(m.ram.read32(source+12)==0x20000 && !(address%4096) && (wide || address%0x20000),"original framebuffer supplies page-aligned CPU memory with a separate 128 KiB alignment request");
            }
            Ipc::BinderTransaction transaction;
            if(present && !Ipc::parseBinderTransaction(packet,m.bus,ipc,transaction) && transaction.code==7){
                ++queues;const auto context=m.cpu.captureContext();const auto count=m.process().frameCount();const auto previousFrame=m.process().frame();
                std::vector<uint8_t> input(size_t(transaction.sourceSize));for(size_t i=0;i<input.size();++i)input[i]=m.ram.read8(transaction.source+i);
                auto send=[&](const Ipc::Buffer& message){m.ram.writeBytes(ipc,message.data(),message.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                m.ram.write32(transaction.source+120,1);check(send(packet)==Ipc::InvalidArguments,"block-linear Queue rejects unsupported cropping");m.ram.writeBytes(transaction.source,input.data(),input.size());
                const auto selected=backing+uint64_t(m.ram.read32(transaction.source+96))*surfaceSize;
                m.ram.setPermissions(selected,surfaceSize,Memory::None);check(send(packet)==Ipc::InvalidBuffer,"unreadable block-linear surface cannot publish a frame");m.ram.setPermissions(selected,surfaceSize,Memory::Read|Memory::Write);
                m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer && m.process().frameCount()==count && m.process().frame()==previousFrame,"protected queue reply preserves frame and slot ownership");m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                if(queues==1){
                    // Hold original slot zero while acquiring and presenting the second map extent.
                    auto dequeue=packet;const bool automatic=read(packet,0)==0x01110004;const size_t descriptor=automatic?16:8,cmif=automatic?48:32;
                    auto put=[&](Ipc::Buffer& bytes,size_t at,uint32_t value){for(size_t i=0;i<4;++i)bytes[at+i]=uint8_t(value>>(i*8));};
                    put(dequeue,descriptor,116);put(dequeue,cmif+20,3);
                    m.ram.write32(transaction.source,100);m.ram.write32(transaction.source+12,116);
                    for(size_t i=0;i<5;++i)m.ram.write32(transaction.source+96+i*4,std::array<uint32_t,5>{1,width,height,1,0xB00}[i]);
                    check(!send(dequeue) && m.ram.read32(transaction.destination+16)==1,"second registered slot can be acquired while slot zero is held");
                    for(size_t i=0;i<surfaceSize;++i)m.ram.write8(backing+surfaceSize+i,42);
                    m.ram.writeBytes(transaction.source,input.data(),input.size());m.ram.write32(transaction.source+96,1);
                    check(!send(packet) && m.process().frameCount()==count+1,"second surface queues from its distinct map offset");
                    const auto frame=m.process().frame();check(frame && frame->rgb.size()==rgbSize,"second surface produces a complete snapshot");if(frame)for(auto pixel:frame->rgb)check(pixel==42,"second surface pixels come from its retained extent offset rather than slot zero");
                    m.ram.writeBytes(transaction.source,input.data(),input.size());
                }
                const auto before=m.process().frameCount();check(!send(packet) && m.process().frameCount()==before+1,"original block-linear Queue succeeds after rejected replays");
                const auto after=m.cpu.captureContext();Ipc::Buffer response{};for(size_t i=0;i<response.size();++i)response[i]=m.ram.read8(ipc+i);
                std::vector<uint8_t> output(size_t(transaction.destinationSize));for(size_t i=0;i<output.size();++i)output[i]=m.ram.read8(transaction.destination+i);
                check(send(packet)==Ipc::NotInitialized && m.process().frameCount()==before+1,"duplicate Queue cannot present the released slot twice");
                m.ram.writeBytes(transaction.destination,output.data(),output.size());m.ram.writeBytes(ipc,response.data(),response.size());m.cpu.restoreContext(after);continue;
            }
            if(!Ipc::parseBinderTransaction(packet,m.bus,ipc,transaction) && transaction.code==14){
                ++registrations;registration=true;const auto flat=transaction.source+112;
                check(m.ram.read32(flat+4)==width && m.ram.read32(flat+8)==height && m.ram.read32(flat+80)==surfaceSize,"original block-linear descriptor reports the measured dimensions and surface extent");
                check(m.ram.read32(flat+40+17*4)==3 && m.ram.read32(flat+40+21*4)==0xfe && m.ram.read32(flat+40+22*4)==4 && m.ram.read32(flat+40+27*4)==surfaceSize,"original plane requests block-linear layout and sixteen-GOB block height");
                check(m.ram.read32(flat+40+20*4)==(registrations-1)*surfaceSize,"each original surface retains its distinct map offset");
                const auto context=m.cpu.captureContext();
                std::vector<uint8_t> input(size_t(transaction.sourceSize));for(size_t i=0;i<input.size();++i)input[i]=m.ram.read8(transaction.source+i);
                auto send=[&](){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                for(const auto word:{20U,21U,22U,27U,28U,80U}){
                    const auto address=flat+40+word*4;const auto previous=m.ram.read32(address);m.ram.write32(address,previous+1);
                    check(send()==Ipc::InvalidArguments,"invalid offset, kind, block height, extent or padding cannot configure a slot");
                    m.ram.writeBytes(transaction.source,input.data(),input.size());
                }
                for(const auto dimension:{13U,14U})for(const auto invalid:{0U,257U,UINT32_MAX}){
                    m.ram.write32(flat+(dimension==13?4:8),invalid);m.ram.write32(flat+40+dimension*4,invalid);
                    check(send()==Ipc::InvalidArguments,"zero, excessive and overflowing dimensions cannot configure a slot");m.ram.writeBytes(transaction.source,input.data(),input.size());
                }
                if(wide && registrations==2){
                    // Width 38 has the same padded pitch/extent but must not mix with slot zero's width 37.
                    m.ram.write32(flat+4,38);m.ram.write32(flat+40+13*4,38);
                    check(send()==Ipc::InvalidArguments,"a layer cannot mix otherwise valid incompatible geometries");m.ram.writeBytes(transaction.source,input.data(),input.size());
                }
                // Reply failure must not publish the otherwise valid slot.
                m.ram.writeBytes(ipc,packet.data(),packet.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected registration reply cannot publish a block-linear slot");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                // RequestBuffer must return all descriptor bytes, including the second offset.
                Ipc::QueueRegistration parsed;Ipc::Transfer reply;
                check(!Ipc::parseQueueRegistration(m.bus,transaction,parsed,reply),"original descriptor parses after rejected replays");
                if(parsed.surface){
                    Ipc::QueueSurfaces surfaces{};std::array<uint32_t,64> slots{};surfaces[parsed.slot]=parsed.surface;slots[parsed.slot]=parsed.object;
                    auto request=transaction;request.code=1;request.sourceSize=100;
                    m.ram.write32(transaction.source,84);m.ram.write32(transaction.source+12,100);uint64_t acquired=uint64_t(1)<<parsed.slot,next=0;
                    check(!Ipc::buildQueueAccess(m.bus,request,slots,acquired,reply,next,surfaces) && next==acquired && reply.bytes.size()==396,"block-linear RequestBuffer preserves ownership");
                    if(reply.bytes.size()==396)for(size_t i=0;i<364;++i)check(reply.bytes[28+i]==input[112+i],"RequestBuffer preserves the original block-linear descriptor byte for byte");
                    m.ram.writeBytes(transaction.source,input.data(),input.size());
                }
                m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);
            }
        }
        const auto result=m.run(1);
        if(allocation)check(!m.cpu.readRegister(0) && !m.ram.read32(ipc+24),"original Alloc succeeds");
        if(registration)check(!m.cpu.readRegister(0) && !m.ram.read32(ipc+24),"original block-linear registration succeeds");
        if(result.stop!=Machine::Stop::Budget)break;
    }
    check(creates==1 && allocations==1 && registrations==2,"original client registers both surfaces after allocating one shared map");
    if(present)check(queues==2,"trace observes both original framebufferEnd queue requests");
    std::cout<<"Framebuffer instructions="<<instructions<<" allocations="<<allocations<<" registrations="<<registrations<<" failures="<<failures<<std::endl;return failures?1:0;
}
