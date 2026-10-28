#include "machine.hpp"
#include "ipc.hpp"
#include "ipc_binder.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint64_t read(const Ipc::Buffer& b,size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4){for(size_t i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i));}
struct RegistrationCapture { Ipc::Buffer packet{};Ipc::BinderTransaction transaction{};CPU::Context context{};uint32_t root=0,clone=0;uint64_t borrowed=0; };
static bool stopAtRegistration(Machine& machine,RegistrationCapture& capture){
    const auto ipc=machine.process().mainThread().localRegion;
    for(unsigned step=0;step<500000;++step){
        const auto op=machine.ram.fetch32(machine.cpu.getPC());bool cloning=false;
        if(op==0xD40002A1)capture.borrowed=machine.cpu.readRegister(1);
        if(op==0xD4000421){
            Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=machine.ram.read8(ipc+i);
            if(read(packet,4)==0x80000009 && read(packet,40)==3)capture.root=uint32_t(machine.cpu.readRegister(0));
            cloning=read(packet,0)==5 && read(packet,24)==4 && read(packet,32)==1;
            Ipc::BinderTransaction transaction;
            if(!Ipc::parseBinderTransaction(packet,machine.bus,ipc,transaction) && transaction.code==14){
                capture.packet=packet;capture.transaction=transaction;capture.context=machine.cpu.captureContext();machine.run(1);
                return !machine.cpu.readRegister(0) && !machine.ram.read32(ipc+24);
            }
        }
        const auto result=machine.run(1);if(cloning)capture.clone=machine.ram.read32(ipc+12);if(result.stop!=Machine::Stop::Budget)return false;
    }
    return false;
}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4 && argc!=5)return 2;
    const bool presentMode=argc>=4 && std::string(argv[3])=="--present";
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream input(argv[2]);std::string name;uint64_t value;while(input>>name>>std::hex>>value)symbols[name]=value;
    if(!symbols.contains("window_report") || !symbols.contains("vi_report"))return 2;
    const auto report=symbols.at("window_report");std::array<uint64_t,32> expected{};uint64_t expectedInstructions=0;
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(count<500000 && result.stop==Machine::Stop::Budget){result=m.run(budget);count+=result.instructions;}
        auto get=[&](size_t i){return m.ram.read64(report+i*8);};
        check(result.stop==Machine::Stop::Exited && result.exitCode==0,"original native window and application cleanup exit normally");
        check(!get(0) && get(1)==1 && get(2)==1 && !get(3) && get(4)==UINT64_MAX,"original nwindowCreate connects without configured slots or a dequeued buffer");
        check(!get(5) && get(6)==1280 && get(7)==720,"original native window reads default queue dimensions");
        check(get(8)==1 && get(9)==Process::TimedOut && get(16)==Process::TimedOut,"original eventWait sees no available slots through shared event handles");
        check(get(10)==Process::InvalidState && get(11)==Process::InvalidHandle,"guest cannot clear or signal the service-owned read endpoint");
        check(get(13)>0 && get(13)<128 && get(14)==Process::OutOfHandles && get(15)==1 && !get(17),"native event handles obey quota, have distinct IDs and can be closed/reacquired");
        check(!get(19) && !get(20) && !get(21) && !get(22) && !get(23),"original nwindowClose releases its event, Binder connection and layer");
        const auto base=symbols.at("report");check(m.ram.read64(base)==6 && m.ram.read64(base+112)==77,"constructor and project main execute");
        check(m.process().frameCount()==(presentMode?2:0) && !m.process().managedLayerCount(),"only successful presentation creates frames and process resources retire");
        if(presentMode){
            const auto frame=m.process().frame();check(frame && frame->width==16 && frame->height==16 && frame->rgb.size()==768,"native CPU presentation retains the last complete 16x16 RGB snapshot");
            if(frame && frame->rgb.size()==768)for(size_t y=0;y<16;++y)for(size_t x=0;x<16;++x){const size_t pixel=(y*16+x)*3;check(frame->rgb[pixel]==255-x*17 && frame->rgb[pixel+1]==255-y*17 && frame->rgb[pixel+2]==255-(x^y)*17,"presentation snapshots preserve channel order and pixels after backing overwrite/free");}
            if(argc==5 && frame){std::string error;check(frame->savePng(argv[4],error),"native CPU snapshot exports as PNG");}
            auto item=[&](size_t i){return m.ram.read64(symbols.at("present_report")+8*i);};
            for(size_t at:{size_t(0),size_t(7)})check(!item(at) && !item(at+1) && !item(at+2) && item(at+3)==UINT64_MAX && !item(at+4) && item(at+5)==16 && item(at+6)==16,"original nwindowQueueBuffer presents and returns slot zero to availability");
            check(item(14)==1 && item(15)==0x4242424242424242ULL,"original client caches metadata and overwrites CPU backing after both snapshots");
        }
        if(argc>=4){
            const auto registration=symbols.at("registration_report");auto field=[&](size_t i){return m.ram.read64(registration+i*8);};
            check(!field(0) && !field(1) && field(2)==1 && !field(3) && field(4) && !field(5),"original registration path has initialized NV and bound Nvmap backing");
            check(!field(6) && field(7)==1 && !field(8),"original registration makes one free slot available");
            check(field(9)==0x4242424242424242ULL && !field(11) && field(12)==1 && !field(13) && !field(14) && field(15)==Process::InvalidHandle,"original client releases buffers before freeing backing and preserves CPU contents");
            if(std::string(argv[3])=="--dequeue"){
                auto item=[&](size_t i){return m.ram.read64(symbols.at("dequeue_report")+8*i);};
                check(!item(0) && !item(1) && !item(2) && !item(3) && item(4)==1,"original Dequeue and RequestBuffer acquire slot zero with an empty fence");
                check(item(5)==Process::TimedOut && !item(6) && item(7)==UINT64_MAX && !item(8),"Cancel restores availability after acquisition clears it");
                check(!item(9) && !item(10) && item(11)==1 && !item(12) && !item(13) && item(14)==UINT64_MAX && item(15)==1,"original client reuses the same slot and its requested-buffer cache");
            }
        }
        if(budget==500000){expectedInstructions=count;for(size_t i=0;i<32;++i)expected[i]=get(i);}
        else{check(count==expectedInstructions,"window instruction count is independent of slicing");for(size_t i=0;i<32;++i)check(get(i)==expected[i],"window report is independent of slicing");}
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;bool captured=false;
    for(unsigned i=0;i<500000;++i){
        const auto op=m.ram.fetch32(m.cpu.getPC());
        if((op&0xFFE0001F)==0xD4000001 && ((op>>5)&0xFFFF)==0x21){
            Ipc::Buffer b{};for(size_t j=0;j<b.size();++j)b[j]=m.ram.read8(ipc+j);
            if(read(b,0)==4 && read(b,16)==0x49434653 && read(b,24)==2 && read(b,32)==0x1000 && read(b,36)==0xF){
                captured=true;const auto context=m.cpu.captureContext();
                auto send=[&](const Ipc::Buffer& packet){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0);};
                auto invalid=b;put(invalid,32,UINT32_MAX);
                check(send(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"native handle query rejects foreign producer IDs");
                invalid=b;put(invalid,36,0);
                check(send(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"native handle query rejects unknown selectors");
                invalid=b;put(invalid,4,11);
                check(send(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"native handle query requires exactly eight input bytes");
                m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected native-handle reply cannot publish a handle");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);break;
            }
        }
        if(m.run(1).stop!=Machine::Stop::Budget)break;
    }
    check(captured,"original Binder native-handle packet is captured");const auto final=m.run(500000);
    check(final.stop==Machine::Stop::Exited && final.exitCode==0,"original window continues after negative replay");
    check(m.ram.read64(report+13*8)==expected[13],"failed publication and invalid calls consume no hidden handle slots");
    if(argc>=4){
        Machine probe(false,true,true);if(!probe.load(argv[1]))return 2;const auto probeIpc=probe.process().mainThread().localRegion;unsigned registrations=0,dequeues=0,requests=0,cancels=0,queues=0;Ipc::Buffer nvPacket{};CPU::Context nvContext{};uint64_t backing=0;std::vector<uint8_t> nativeDescriptor;
        for(unsigned step=0;step<500000;++step){
            if(probe.ram.fetch32(probe.cpu.getPC())==0xD4000421){
                Ipc::Buffer packet{};for(size_t i=0;i<packet.size();++i)packet[i]=probe.ram.read8(probeIpc+i);
                if(read(packet,0)==0x01110004 && read(packet,68)==0xC008010E){nvPacket=packet;nvContext=probe.cpu.captureContext();}
                if(read(packet,0)==0x01110004 && read(packet,68)==0xC0200104)backing=probe.ram.read64(read(packet,20)+24);
                Ipc::BinderTransaction transaction;
                if(presentMode && !Ipc::parseBinderTransaction(packet,probe.bus,probeIpc,transaction) && transaction.code==7){
                    ++queues;check(transaction.sourceSize==192 && probe.ram.read32(transaction.source+100)==84,"original QueueBuffer carries the complete 84-byte input");
                    const auto context=probe.cpu.captureContext();const auto beforeCount=probe.process().frameCount();const auto previous=probe.process().frame();
                    std::vector<uint8_t> input(size_t(transaction.sourceSize));for(size_t i=0;i<input.size();++i)input[i]=probe.ram.read8(transaction.source+i);
                    std::vector<uint8_t> pixels(768);for(size_t pixel=0;pixel<256;++pixel)for(size_t channel=0;channel<3;++channel)pixels[pixel*3+channel]=probe.ram.read8(backing+pixel*4+channel);
                    std::vector<uint8_t> beforeOutput(size_t(transaction.destinationSize));for(size_t i=0;i<beforeOutput.size();++i)beforeOutput[i]=probe.ram.read8(transaction.destination+i);
                    auto send=[&](){probe.ram.writeBytes(probeIpc,packet.data(),packet.size());probe.cpu.restoreContext(context);probe.run(1);return probe.cpu.readRegister(0)?uint32_t(probe.cpu.readRegister(0)):probe.ram.read32(probeIpc+24);};
                    auto mutate=[&](size_t at,uint32_t value){probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.write32(transaction.source+at,value);return send();};
                    check(mutate(96,64)==Ipc::InvalidArguments && mutate(100,80)==Ipc::InvalidArguments,"Queue validates slot index and input length");
                    for(size_t at:{size_t(108),size_t(128),size_t(140),size_t(156)})check(mutate(at,1)==Ipc::InvalidArguments,"unsupported timestamp, crop, transform or nonempty fence cannot be queued");
                    check(mutate(152,0)==Ipc::InvalidArguments,"unsupported swap interval cannot submit");
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.setPermissions(backing,4096,Memory::None);
                    check(send()==Ipc::InvalidBuffer,"unreadable backing cannot publish a reply, frame or ownership change");probe.ram.setPermissions(backing,4096,Memory::Read|Memory::Write);
                    const auto outputPage=transaction.destination&~uint64_t(Memory::PAGE_SIZE-1);probe.ram.setPermissions(outputPage,Memory::PAGE_SIZE,Memory::Read);check(send()==Ipc::InvalidBuffer,"unwritable queue parcel cannot publish presentation");probe.ram.setPermissions(outputPage,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    probe.ram.writeBytes(probeIpc,packet.data(),packet.size());probe.ram.setPermissions(probeIpc,Memory::PAGE_SIZE,Memory::Read);probe.cpu.restoreContext(context);probe.run(1);
                    check(probe.cpu.readRegister(0)==Process::InvalidPointer,"protected queue response cannot publish presentation");probe.ram.setPermissions(probeIpc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    check(probe.process().frameCount()==beforeCount && probe.process().frame()==previous,"failed queues preserve the previous snapshot and frame counter");
                    for(size_t i=0;i<beforeOutput.size();++i)check(beforeOutput[i]==probe.ram.read8(transaction.destination+i),"failed queues preserve output parcel bytes");
                    check(!send() && probe.process().frameCount()==beforeCount+1,"original QueueBuffer publishes exactly one frame after failed attempts");
                    check(probe.process().frame() && probe.process().frame()->rgb==pixels,"queued RGB snapshot matches all original RGBA source pixels");
                    check(probe.ram.read32(transaction.destination)==20 && probe.ram.read32(transaction.destination+16)==16 && probe.ram.read32(transaction.destination+20)==16 && !probe.ram.read32(transaction.destination+28) && !probe.ram.read32(transaction.destination+32),"QueueBuffer replies with bounded geometry, no backlog and successful status");
                    const auto after=probe.cpu.captureContext();Ipc::Buffer response{};for(size_t i=0;i<response.size();++i)response[i]=probe.ram.read8(probeIpc+i);
                    std::vector<uint8_t> output(size_t(transaction.destinationSize));for(size_t i=0;i<output.size();++i)output[i]=probe.ram.read8(transaction.destination+i);
                    const auto snapshot=probe.process().frame();check(send()==Ipc::NotInitialized && probe.process().frameCount()==beforeCount+1 && probe.process().frame()==snapshot,"duplicate QueueBuffer cannot present an unowned slot twice");
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.writeBytes(transaction.destination,output.data(),output.size());probe.ram.writeBytes(probeIpc,response.data(),response.size());probe.cpu.restoreContext(after);continue;
                }
                if((std::string(argv[3])=="--dequeue" || presentMode) && !Ipc::parseBinderTransaction(packet,probe.bus,probeIpc,transaction) && (transaction.code==1 || transaction.code==3 || transaction.code==8)){
                    const auto context=probe.cpu.captureContext();const bool automatic=read(packet,0)==0x01110004;const size_t descriptor=automatic?16:8,cmif=automatic?48:32;
                    std::vector<uint8_t> input(size_t(transaction.sourceSize));for(size_t i=0;i<input.size();++i)input[i]=probe.ram.read8(transaction.source+i);
                    auto send=[&](const Ipc::Buffer& b){probe.ram.writeBytes(probeIpc,b.data(),b.size());probe.cpu.restoreContext(context);probe.run(1);return probe.cpu.readRegister(0)?uint32_t(probe.cpu.readRegister(0)):probe.ram.read32(probeIpc+24);};
                    auto mutate=[&](size_t at,uint32_t value){probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.write32(transaction.source+at,value);return send(packet);};
                    check(mutate(96,64)==Ipc::InvalidArguments,"access rejects an invalid slot or async argument");
                    if(transaction.code==3){++dequeues;check(mutate(100,17)==Ipc::InvalidArguments && mutate(108,0)==Ipc::InvalidArguments,"Dequeue validates its fixed geometry and format");}
                    if(transaction.code==1){++requests;check(mutate(96,63)==Ipc::NotInitialized,"RequestBuffer requires an owned registered slot");}
                    if(transaction.code==8){++cancels;check(mutate(108,1)==Ipc::InvalidArguments,"Cancel rejects nonempty GPU fences");}
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.writeBytes(probeIpc,packet.data(),packet.size());probe.ram.setPermissions(probeIpc,Memory::PAGE_SIZE,Memory::Read);probe.cpu.restoreContext(context);probe.run(1);
                    check(probe.cpu.readRegister(0)==Process::InvalidPointer,"protected access reply cannot change ownership");probe.ram.setPermissions(probeIpc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    if(transaction.code==8){
                        const auto scratch=Process::HeapBase+0x190000;std::vector<uint8_t> source(116);for(size_t i=0;i<96;++i)source[i]=input[i];
                        probe.ram.writeBytes(scratch,source.data(),source.size());probe.ram.write32(scratch,100);probe.ram.write32(scratch+12,116);probe.ram.write32(scratch+96,1);probe.ram.write32(scratch+100,16);probe.ram.write32(scratch+104,16);probe.ram.write32(scratch+108,1);probe.ram.write32(scratch+112,0xB00);
                        auto raw=packet;put(raw,descriptor,116);put(raw,descriptor+4,scratch);put(raw,cmif+20,3);
                        check(!send(raw) && probe.ram.read32(transaction.destination+68)==uint32_t(-11),"failed Cancel leaves the only slot acquired and reports WouldBlock");
                    }
                    check(!send(packet),"original access succeeds after rejected replays");
                    const auto after=probe.cpu.captureContext();Ipc::Buffer response{};for(size_t i=0;i<response.size();++i)response[i]=probe.ram.read8(probeIpc+i);
                    std::vector<uint8_t> output(size_t(transaction.destinationSize));for(size_t i=0;i<output.size();++i)output[i]=probe.ram.read8(transaction.destination+i);
                    if(transaction.code==3){
                        check(probe.ram.read32(transaction.destination)==56 && !probe.ram.read32(transaction.destination+16) && probe.ram.read32(transaction.destination+20)==1 && probe.ram.read32(transaction.destination+24)==36 && !probe.ram.read32(transaction.destination+32),"Dequeue serializes slot zero and a complete empty multi-fence");
                        check(!send(packet) && probe.ram.read32(transaction.destination+16)==UINT32_MAX && probe.ram.read32(transaction.destination+68)==uint32_t(-11),"exhausted Dequeue reports native WouldBlock without acquiring another slot");
                        auto queue=packet;put(queue,cmif+20,7);check(send(queue)==Ipc::InvalidArguments,"malformed QueueBuffer cannot publish a frame or release ownership");
                        auto remove=packet;put(remove,descriptor,104);put(remove,cmif+20,14);probe.ram.write32(transaction.source,88);probe.ram.write32(transaction.source+12,104);probe.ram.write32(transaction.source+96,0);probe.ram.write32(transaction.source+100,0);
                        check(send(remove)==Process::InvalidState,"acquired slot cannot be unregistered");
                        auto disconnect=packet;put(disconnect,descriptor,100);put(disconnect,cmif+20,11);probe.ram.write32(transaction.source,84);probe.ram.write32(transaction.source+12,100);probe.ram.write32(transaction.source+96,2);
                        check(send(disconnect)==Process::InvalidState,"Disconnect requires cancellation of acquired slots");
                    }else if(transaction.code==1){
                        check(probe.ram.read32(transaction.destination)==380 && probe.ram.read32(transaction.destination+16)==1 && probe.ram.read32(transaction.destination+20)==364 && probe.ram.read32(transaction.destination+28)==0x47424652 && probe.ram.read32(transaction.destination+72)==1,"RequestBuffer returns the complete registered native descriptor");
                        check(nativeDescriptor.size()==364,"the original GCC descriptor was captured");for(size_t i=0;i<nativeDescriptor.size();++i)check(nativeDescriptor[i]==probe.ram.read8(transaction.destination+28+i),"RequestBuffer preserves every byte of the original registered descriptor");
                        check(!send(packet),"RequestBuffer is repeatable for the current owner");
                    }else {
                        check(send(packet)==Ipc::NotInitialized,"duplicate Cancel cannot release an already free slot");
                        auto request=packet;put(request,descriptor,100);put(request,cmif+20,1);probe.ram.write32(transaction.source,84);probe.ram.write32(transaction.source+12,100);probe.ram.write32(transaction.source+96,0);
                        check(send(request)==Ipc::NotInitialized,"RequestBuffer cannot access a registered but unacquired slot");
                    }
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.writeBytes(transaction.destination,output.data(),output.size());probe.ram.writeBytes(probeIpc,response.data(),response.size());probe.cpu.restoreContext(after);continue;
                }
                if(!Ipc::parseBinderTransaction(packet,probe.bus,probeIpc,transaction) && transaction.code==14){
                    ++registrations;const bool automatic=read(packet,0)==0x01110004;const size_t descriptor=automatic?16:8,cmif=automatic?48:32;auto field=[&](size_t offset){return probe.ram.read32(transaction.source+offset);};
                    const std::string_view token="android.gui.IGraphicBufferProducer";const size_t args=24+((2*(token.size()+1)+3)&~size_t(3));const auto flat=args+16;
                    check(transaction.object==0x1000 && !transaction.flags && transaction.destinationSize==1024,"original registration targets the opened producer through the expected transport");
                    check(field(4)==16 && !field(8) && field(12)==transaction.sourceSize && !field(args) && field(args+4)==1 && !field(args+12),"GCC producer serializes the original parcel, slot and flattened-object header");
                    check(field(flat)==0x47424652 && field(flat+4)==16 && field(flat+8)==16 && field(flat+12)==16 && field(flat+16)==1 && field(flat+24)==42 && !field(flat+32),"original graphic-buffer metadata matches the requested linear RGBA profile");
                    const auto ints=field(flat+36);check(field(args+8)==40+4*ints && transaction.sourceSize==flat+40+4*ints,"GCC variable-length member has the expected flattened byte count");
                    std::cout<<"Registration parcel bytes="<<transaction.sourceSize<<" native ints="<<ints<<" flattened bytes="<<field(args+8)<<" automatic="<<automatic<<std::endl;
                    check(field(flat+40)==UINT32_MAX && field(flat+44) && field(flat+52)==0xDAFFCAFF,"original native handle contains the bound Nvmap export ID and native magic");
                    std::vector<uint8_t> before(size_t(transaction.destinationSize));for(size_t i=0;i<before.size();++i)before[i]=probe.ram.read8(transaction.destination+i);
                    const auto context=probe.cpu.captureContext();std::vector<uint8_t> input(size_t(transaction.sourceSize));
                    for(size_t i=0;i<input.size();++i)input[i]=probe.ram.read8(transaction.source+i);
                    nativeDescriptor.assign(input.begin()+112,input.end());
                    auto send=[&](const Ipc::Buffer& b){probe.ram.writeBytes(probeIpc,b.data(),b.size());probe.cpu.restoreContext(context);probe.run(1);return probe.cpu.readRegister(0)?uint32_t(probe.cpu.readRegister(0)):probe.ram.read32(probeIpc+24);};
                    auto mutate=[&](size_t at,uint32_t value){probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.write32(transaction.source+at,value);return send(packet);};
                    check(mutate(args,64)==Ipc::InvalidArguments,"slot index is bounded");
                    check(mutate(flat+4,17)==Ipc::InvalidArguments,"inconsistent buffer dimensions are rejected");
                    check(mutate(flat+36,80)==Ipc::InvalidArguments,"native integer count must match the profile");
                    check(mutate(flat+44,UINT32_MAX)==Process::InvalidHandle,"unowned Nvmap IDs cannot be registered");
                    check(mutate(flat+40+4*20,4)==Ipc::InvalidArguments,"nonzero plane offset is outside the bounded profile");
                    check(mutate(flat+40+4*18,60)==Ipc::InvalidArguments,"plane pitch must match row extent");
                    check(mutate(flat+40+4*15,0)==Ipc::InvalidArguments,"native color format is validated");
                    check(mutate(args+4,0)==Ipc::InvalidArguments,"null registration requires its exact shorter parcel");
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());
                    probe.ram.setPermissions(backing,4096,Memory::Read);check(send(packet)==Ipc::InvalidBuffer,"registered backing must remain writable");probe.ram.setPermissions(backing,4096,Memory::Read|Memory::Write);
                    const auto destinationPage=transaction.destination&~uint64_t(Memory::PAGE_SIZE-1);
                    probe.ram.setPermissions(destinationPage,Memory::PAGE_SIZE,Memory::Read);check(send(packet)==Ipc::InvalidBuffer,"unwritable reply parcel cannot configure a slot");probe.ram.setPermissions(destinationPage,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    probe.ram.writeBytes(probeIpc,packet.data(),packet.size());probe.ram.setPermissions(probeIpc,Memory::PAGE_SIZE,Memory::Read);probe.cpu.restoreContext(context);probe.run(1);
                    check(probe.cpu.readRegister(0)==Process::InvalidPointer,"protected CMIF reply cannot configure a slot");probe.ram.setPermissions(probeIpc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    for(size_t i=0;i<before.size();++i)check(before[i]==probe.ram.read8(transaction.destination+i),"failed publication leaves the reply parcel unchanged");
                    check(!send(packet),"original registration succeeds after failed attempts");
                    check(!probe.ram.read32(transaction.destination) && probe.ram.read32(transaction.destination+4)==16 && probe.ram.read32(transaction.destination+12)==16,"registration returns a valid empty reply parcel");
                    const auto after=probe.cpu.captureContext();Ipc::Buffer response{};for(size_t i=0;i<response.size();++i)response[i]=probe.ram.read8(probeIpc+i);
                    std::vector<uint8_t> successfulReply(before.size());for(size_t i=0;i<successfulReply.size();++i)successfulReply[i]=probe.ram.read8(transaction.destination+i);
                    check(send(packet)==Ipc::AlreadyInitialized,"duplicate registration cannot overwrite an occupied slot");
                    for(uint32_t slot=1;slot<64;++slot)check(!mutate(args,slot),"all bounded slots can hold references to the same live object");
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());
                    auto removal=packet;put(removal,descriptor,104);probe.ram.write32(transaction.source,88);probe.ram.write32(transaction.source+12,104);probe.ram.write32(transaction.source+args,63);probe.ram.write32(transaction.source+args+4,0);
                    const auto removed=send(removal), removedAgain=send(removal);if(removed || removedAgain!=Ipc::NotInitialized)std::cerr<<"Removal statuses="<<removed<<","<<removedAgain<<std::endl;check(!removed && removedAgain==Ipc::NotInitialized,"null registration removes a slot exactly once");
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.write32(transaction.source+args,63);
                    Ipc::Buffer automaticPacket{};put(automaticPacket,0,0x01110004);put(automaticPacket,4,0xC0C);put(automaticPacket,16,transaction.sourceSize);put(automaticPacket,20,transaction.source);put(automaticPacket,28,transaction.destinationSize);put(automaticPacket,32,transaction.destination);put(automaticPacket,48,0x49434653);put(automaticPacket,56,3);put(automaticPacket,64,transaction.object);put(automaticPacket,68,14);
                    check(!send(automaticPacket),"AutoSelect transport registers the same bounded native profile");
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.write32(transaction.source,88);probe.ram.write32(transaction.source+12,104);probe.ram.write32(transaction.source+args+4,0);
                    for(uint32_t slot=1;slot<64;++slot){probe.ram.write32(transaction.source+args,slot);check(!send(removal),"extra stress slots are removed before returning to the original client");}
                    auto freeMap=nvPacket;put(freeMap,16,24);put(freeMap,28,24);const auto scratch=Process::HeapBase+0x180000;put(freeMap,20,scratch);put(freeMap,32,scratch);put(freeMap,68,0xC0180105);
                    auto nvSend=[&](const Ipc::Buffer& b){probe.ram.writeBytes(probeIpc,b.data(),b.size());probe.cpu.restoreContext(nvContext);probe.run(1);return probe.cpu.readRegister(0)?uint32_t(probe.cpu.readRegister(0)):probe.ram.read32(probeIpc+24);};
                    for(size_t i=0;i<24;i+=4)probe.ram.write32(scratch+i,0);
                    // The source parcel is restored before reading the object ID for the lifetime tests.
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.write32(scratch,field(flat+44));
                    check(nvSend(freeMap)==Process::InvalidState,"registered backing cannot be freed while slots reference it");
                    Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,read(nvPacket,64));
                    check(nvSend(close)==Process::InvalidState,"owning device cannot close while its object is registered");
                    Ipc::Buffer closeSession{};put(closeSession,0,2);check(nvSend(closeSession)==Process::InvalidState,"IPC session Close cannot strand a registered backing");
                    auto disconnect=packet;put(disconnect,descriptor,100);put(disconnect,cmif+20,11);
                    probe.ram.write32(transaction.source,84);probe.ram.write32(transaction.source+12,100);probe.ram.write32(transaction.source+args,2);
                    probe.ram.writeBytes(probeIpc,disconnect.data(),disconnect.size());probe.ram.setPermissions(probeIpc,Memory::PAGE_SIZE,Memory::Read);probe.cpu.restoreContext(context);probe.run(1);
                    check(probe.cpu.readRegister(0)==Process::InvalidPointer,"protected Disconnect cannot retire slot references");probe.ram.setPermissions(probeIpc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    check(nvSend(freeMap)==Process::InvalidState,"failed Disconnect preserves backing ownership");
                    probe.ram.writeBytes(transaction.source,input.data(),input.size());probe.ram.writeBytes(transaction.destination,successfulReply.data(),successfulReply.size());probe.ram.writeBytes(probeIpc,response.data(),response.size());probe.cpu.restoreContext(after);
                    continue;
                }
            }
            if(probe.run(1).stop!=Machine::Stop::Budget)break;
        }
        check(registrations==1,"one original SetPreallocatedBuffer transaction is captured");
        if(std::string(argv[3])=="--dequeue")check(dequeues==2 && requests==1 && cancels==2,"original SDK performs two Dequeues/Cancels and requests buffer metadata only once");
        if(presentMode)check(dequeues==2 && requests==1 && !cancels && queues==2,"original SDK performs two Dequeue/Queue cycles and requests metadata only once");
        Machine lifetime(false,true,true);if(!lifetime.load(argv[1]))return 2;RegistrationCapture capturedLifetime;
        const bool ready=stopAtRegistration(lifetime,capturedLifetime);check(ready && capturedLifetime.root && capturedLifetime.clone,"endpoint lifetime replay reaches a registered buffer with both NV endpoints");
        if(ready){
            const auto ipc=lifetime.process().mainThread().localRegion;uint64_t closePc=0;
            for(uint64_t at=0x10000;at<Process::HeapBase;at+=4)if(lifetime.ram.canAccess(at,4,Memory::Execute) && lifetime.ram.read32(at)==0xD40002C1){closePc=at;break;}
            check(closePc!=0,"original CloseHandle instruction is available");
            const auto after=lifetime.cpu.captureContext();auto close=[&](uint32_t handle){lifetime.cpu.restoreContext(after);lifetime.cpu.setPC(closePc);lifetime.cpu.writeRegister(0,handle);lifetime.run(1);return lifetime.cpu.readRegister(0);};
            const auto missing=std::string(argv[1])+".missing";check(!lifetime.load(missing.c_str()),"failed reload preserves the registered process");
            check(!close(capturedLifetime.root) && close(capturedLifetime.clone)==Process::InvalidState,"one NV endpoint may close but the last endpoint cannot strand registered backing");
            check(!lifetime.ram.canAccess(capturedLifetime.borrowed,0x10000,Memory::Read),"blocked last close preserves communication-memory ownership");
            auto disconnect=capturedLifetime.packet;const bool automatic=read(disconnect,0)==0x01110004;put(disconnect,automatic?16:8,100);put(disconnect,(automatic?48:32)+20,11);
            const auto source=capturedLifetime.transaction.source;lifetime.ram.write32(source,84);lifetime.ram.write32(source+12,100);lifetime.ram.write32(source+96,2);
            lifetime.ram.writeBytes(ipc,disconnect.data(),disconnect.size());lifetime.cpu.restoreContext(capturedLifetime.context);lifetime.run(1);
            check(!lifetime.cpu.readRegister(0) && !lifetime.ram.read32(ipc+24),"Disconnect releases all registered references through the live display client");
            check(!close(capturedLifetime.clone) && lifetime.ram.canAccess(capturedLifetime.borrowed,0x10000,Memory::Read|Memory::Write),"last NV close succeeds after Disconnect and restores communication memory");
        }
        Machine fault(false,true,true);if(!fault.load(argv[1]))return 2;RegistrationCapture capturedFault;const bool faultReady=stopAtRegistration(fault,capturedFault);check(faultReady,"fault replay reaches a registered buffer");
        if(faultReady){
            fault.cpu.setPC(Machine::RAM_SIZE);check(fault.run(1).stop==Machine::Stop::Error && !fault.process().managedLayerCount() && fault.ram.canAccess(capturedFault.borrowed,0x10000,Memory::Read|Memory::Write),"process fault drops registered slots and restores borrowed memory");
            check(fault.load(argv[1]),"fresh reload clears registration state");const auto result=fault.run(500000);check(result.stop==Machine::Stop::Exited && !result.exitCode,"original registration and cleanup work again after fault and reload");
        }
        if(presentMode){
            Machine automatic(false,true,true);if(!automatic.load(argv[1]))return 2;RegistrationCapture capturedAuto;const bool autoReady=stopAtRegistration(automatic,capturedAuto);check(autoReady,"AutoSelect presentation replay reaches registered backing");
            if(autoReady){
                const auto ipc=automatic.process().mainThread().localRegion,scratch=Process::HeapBase+0x190000;std::array<uint8_t,96> token{};
                for(size_t i=0;i<token.size();++i)token[i]=automatic.ram.read8(capturedAuto.transaction.source+i);
                Ipc::Buffer packet{};put(packet,0,0x01110004);put(packet,4,0xC0C);put(packet,20,scratch);put(packet,28,1024);put(packet,32,capturedAuto.transaction.destination);put(packet,48,0x49434653);put(packet,56,3);put(packet,64,capturedAuto.transaction.object);
                auto send=[&](uint32_t command,uint32_t bytes){put(packet,16,bytes);put(packet,68,command);automatic.ram.writeBytes(ipc,packet.data(),packet.size());automatic.cpu.restoreContext(capturedAuto.context);automatic.run(1);return automatic.cpu.readRegister(0)?uint32_t(automatic.cpu.readRegister(0)):automatic.ram.read32(ipc+24);};
                automatic.ram.writeBytes(scratch,token.data(),token.size());automatic.ram.write32(scratch,100);automatic.ram.write32(scratch+12,116);automatic.ram.write32(scratch+96,1);automatic.ram.write32(scratch+100,16);automatic.ram.write32(scratch+104,16);automatic.ram.write32(scratch+108,1);automatic.ram.write32(scratch+112,0xB00);
                check(!send(3,116),"AutoSelect Dequeue acquires the registered CPU slot");
                for(size_t i=0;i<192;i+=4)automatic.ram.write32(scratch+i,0);automatic.ram.writeBytes(scratch,token.data(),token.size());automatic.ram.write32(scratch,176);automatic.ram.write32(scratch+12,192);automatic.ram.write32(scratch+100,84);automatic.ram.write32(scratch+152,1);
                check(!send(7,192) && automatic.process().frameCount()==1 && automatic.process().frame() && automatic.process().frame()->rgb==std::vector<uint8_t>(768,0x42),"AutoSelect QueueBuffer publishes the same bounded snapshot profile");
                check(send(7,192)==Ipc::NotInitialized && automatic.process().frameCount()==1,"AutoSelect duplicate queue cannot present twice");
            }
            Machine frames(false,true,true);if(!frames.load(argv[1]))return 2;
            const auto first=frames.run(500000,true);const auto firstImage=frames.process().frame();
            check(first.stop==Machine::Stop::Frame && frames.process().frameCount()==1 && firstImage,"first native QueueBuffer stops after one published frame");
            const auto second=frames.run(500000,true);const auto secondImage=frames.process().frame();
            check(second.stop==Machine::Stop::Frame && frames.process().frameCount()==2 && secondImage && secondImage!=firstImage,"resuming produces a new snapshot without replaying the first submission");
            if(firstImage && secondImage)check(firstImage->rgb.front()==0 && secondImage->rgb.front()==255,"older snapshots remain independent when guest backing is reused");
            const auto finished=frames.run(500000,true);check(finished.stop==Machine::Stop::Exited && frames.process().frameCount()==2,"presentation resumes through original cleanup without an extra frame");
        }
    }
    std::cout<<"Window instructions="<<expectedInstructions<<" event copies="<<expected[13]<<" failures="<<failures<<'\n';return failures?1:0;
}
