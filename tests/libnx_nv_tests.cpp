#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iostream>
#include <map>
static uint64_t read(const Ipc::Buffer& b,size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[at+i])<<(8*i);return v;}
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
int main(int argc,char** argv){
    if(argc!=3 && argc!=4)return 2;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream input(argv[2]);std::string name;uint64_t value;while(input>>name>>std::hex>>value)symbols[name]=value;
    if(!symbols.contains("nv_report") || !symbols.contains("report"))return 2;uint64_t instructions=0;std::array<uint64_t,16> expected{};
    for(uint64_t budget:{uint64_t(500000),uint64_t(7)}){
        Machine m(false,true,true);if(!m.load(argv[1]))return 2;uint64_t count=0;Machine::Result result{Machine::Stop::Budget,0};
        while(count<500000 && result.stop==Machine::Stop::Budget){result=m.run(budget);count+=result.instructions;}
        if(result.stop!=Machine::Stop::Exited)std::cerr<<m.error()<<'\n';
        check(result.stop==Machine::Stop::Exited && result.exitCode==0,"original NVIDIA initialization and cleanup return normally");
        const auto report=symbols.at("nv_report");auto get=[&](size_t i){return m.ram.read64(report+i*8);};
        check(get(0)==0x10000 && !get(1) && !get(3) && get(2) && get(4) && get(2)!=get(4),"both original attempts initialize independent NVIDIA service sessions");
        check(get(5)==1 && !m.process().frameCount(),"original cleanup completes without a rendered frame");
        if(argc==4)check(!get(6) && !get(8) && get(7) && get(9) && get(7)!=UINT32_MAX && get(9)!=UINT32_MAX && get(7)!=get(9),"original nvMapInit opens distinct process-scoped device descriptors");
        if(argc==4 && (std::string(argv[3])=="--alloc" || (std::string(argv[3])=="--cycle" || std::string(argv[3])=="--wrapper")))check(!get(10) && !get(12) && get(11) && get(13) && get(13)==get(11)+1,"original Nvmap Create publishes distinct unbound objects");
        if(argc==4 && (std::string(argv[3])=="--cycle" || std::string(argv[3])=="--wrapper")){
            const auto map=symbols.at("map_report");
            for(size_t offset:{size_t(0),size_t(8)}){
                auto field=[&](size_t i){return m.ram.read64(map+8*(offset+i));};
                check(field(0)==1 && !field(1) && field(2)==0x4242424242424242ULL,"original Alloc binds backing without changing CPU contents");
                check(!field(3) && field(4)==Process::InvalidHandle,"original Free succeeds once and rejects stale identifiers");
                if(std::string(argv[3])=="--wrapper")check(field(5)==get(11+offset/4) && field(5) && field(6)==1,"original wrapper receives its stable export ID and clears local state on Close");
            }
        }
        const auto base=symbols.at("report");
        check(m.ram.read64(base)==6 && m.ram.read64(base+112)==77 && !m.ram.read64(base+120),"constructor, project main and loader return checks remain valid");
        if(budget==500000){instructions=count;for(size_t i=0;i<16;++i)expected[i]=get(i);}else{check(count==instructions,"NV execution count is independent of slicing");for(size_t i=0;i<16;++i)check(get(i)==expected[i],"NV report is independent of slicing");}
    }
    Machine m(false,true,true);if(!m.load(argv[1]))return 2;const auto ipc=m.process().mainThread().localRegion;
    unsigned creates=0,initializes=0,closes=0,clones=0,registrations=0,retired=0,opens=0,deviceCloses=0,ioctls=0;uint64_t address=0,size=0,transferPc=0;uint32_t transfer=0,root=0,clone=0,device=0;
    for(unsigned i=0;i<500000;++i){
        const auto op=m.ram.fetch32(m.cpu.getPC());const bool svc=(op&0xFFE0001F)==0xD4000001;const auto id=(op>>5)&0xFFFF;
        bool cloning=false,initializing=false;Ipc::Buffer initialPacket{};CPU::Context initialContext{};
        bool binding=false;Ipc::Buffer bindingPacket{};CPU::Context bindingContext{};
        bool opening=false,closingDevice=false,creatingMap=false;uint64_t ioctlSource=0,ioctlBefore=0;
        if(svc && id==0x15){transferPc=m.cpu.getPC();address=m.cpu.readRegister(1);size=m.cpu.readRegister(2);check(size==0x10000 && !m.cpu.readRegister(3),"original NV client requests configured transfer size and Perm_None");}
        if(svc && id==0x21){
            Ipc::Buffer b{};for(size_t j=0;j<b.size();++j)b[j]=m.ram.read8(ipc+j);
            if(read(b,4)==0x80000009 && read(b,8)==4 && read(b,32)==0x49434653 && read(b,40)==3){
                ++initializes;const auto context=m.cpu.captureContext();
                initializing=true;initialPacket=b;initialContext=context;
                root=uint32_t(m.cpu.readRegister(0));
                check(read(b,12)==0xFFFF8001 && read(b,16)==transfer && read(b,48)==size,"original Initialize carries current-process and actual transfer-memory handles");
                check(!m.ram.canAccess(address,size,Memory::Read),"transfer source stays protected during IPC");
                auto send=[&](Ipc::Buffer packet){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0);};
                auto invalid=b;put(invalid,12,0);check(send(invalid)==0 && m.ram.read32(ipc+24)==Process::InvalidHandle,"driver validates process pseudo handle");
                invalid=b;put(invalid,16,m.process().mainHandle());check(send(invalid)==0 && m.ram.read32(ipc+24)==Process::InvalidHandle,"driver rejects non-transfer-memory copied handles");
                invalid=b;put(invalid,48,uint32_t(size+4096));check(send(invalid)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"driver validates transfer-memory size");
                invalid=b;put(invalid,8,2);check(send(invalid)==Ipc::UnsupportedFormat,"driver rejects an incorrect copied-handle layout");
                m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer && !m.ram.canAccess(address,size,Memory::Read),"protected driver reply preserves kernel transfer ownership");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);
            }
            if(read(b,0)==5 && read(b,4)==9 && read(b,16)==0x49434653 && read(b,24)==4){
                cloning=true;const auto context=m.cpu.captureContext();
                auto invalid=b;put(invalid,32,2);m.ram.writeBytes(ipc,invalid.data(),invalid.size());m.run(1);
                check(m.ram.read32(ipc+24)==Ipc::InvalidArguments,"tagged NV clone rejects unsupported tags");
                m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected clone reply creates no session handle");m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);
            }
            if(read(b,4)==0x8000000A && read(b,8)==1 && read(b,32)==0x49434653 && read(b,40)==8){
                ++registrations;const auto context=m.cpu.captureContext();auto invalid=b;put(invalid,48,0);put(invalid,52,0);
                m.ram.writeBytes(ipc,invalid.data(),invalid.size());m.run(1);check(m.ram.read32(ipc+24)==Ipc::InvalidArguments,"NV client registration rejects a foreign resource identity");
                m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected registration reply publishes no client state");m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                Ipc::Buffer early{};put(early,0,4);put(early,4,8);put(early,16,0x49434653);put(early,24,0);m.ram.writeBytes(ipc,early.data(),early.size());m.cpu.restoreContext(context);m.run(1);
                check(m.ram.read32(ipc+24)==Ipc::NotInitialized,"failed registration cannot enable device commands");
                m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);
            }
            Ipc::Layout layout;if(!Ipc::parseBuffers(b,&m.bus,ipc,layout) && layout.command==0 && layout.sends==1 && layout.sourceSize==10){
                std::string path;for(size_t j=0;j<layout.sourceSize;++j)path+=char(m.ram.read8(layout.source+j));if(path=="/dev/nvmap"){
                    ++opens;opening=true;const auto context=m.cpu.captureContext();const auto scratch=Process::HeapBase+0x180000;
                    std::array<uint8_t,10> old{};for(size_t j=0;j<old.size();++j)old[j]=m.ram.read8(scratch+j);
                    auto send=[&](const Ipc::Buffer& packet,uint32_t target){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);m.cpu.writeRegister(0,target);m.run(1);return m.cpu.readRegister(0);};
                    auto invalid=b;put(invalid,12,uint32_t(scratch));put(invalid,16,0);m.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>("/dev/wrong"),10);
                    check(send(invalid,root)==0 && m.ram.read32(ipc+24)==Ipc::FileNotFound,"unsupported device paths create no descriptor");
                    m.ram.setPermissions(scratch,Memory::PAGE_SIZE,Memory::None);check(send(invalid,root)==Ipc::InvalidBuffer,"device path requires readable RAM");m.ram.setPermissions(scratch,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    invalid=b;put(invalid,4,9);check(send(invalid,root)==0 && m.ram.read32(ipc+24)==Ipc::InvalidArguments,"Open rejects extra input payload");
                    m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected Open reply cannot publish a descriptor");m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    std::vector<uint32_t> fds;for(size_t j=0;j<Process::NvDescriptorLimit;++j){check(send(b,root)==0 && !m.ram.read32(ipc+24) && !m.ram.read32(ipc+36),"Open publishes fd and zero NvError");const auto fd=m.ram.read32(ipc+32);check(fd && fd!=UINT32_MAX && (fds.empty() || fd>fds.back()),"device descriptors increase without reuse");fds.push_back(fd);}
                    if(opens==1)check(fds.front()==1,"failed Open publication consumes no descriptor ID");
                    check(send(b,root)==0 && m.ram.read32(ipc+24)==Process::OutOfResource,"per-client device descriptor quota is enforced");
                    Ipc::Buffer close{};put(close,0,4);put(close,4,9);put(close,16,0x49434653);put(close,24,2);put(close,32,0);
                    check(send(close,root)==0 && m.ram.read32(ipc+24)==Process::InvalidHandle,"zero descriptor cannot be closed");
                    put(close,32,fds.front());m.ram.writeBytes(ipc,close.data(),close.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected Close reply cannot retire a descriptor");m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                    for(size_t j=0;j<fds.size();++j){put(close,32,fds[j]);check(send(close,j%2?root:clone)==0 && !m.ram.read32(ipc+24) && m.ram.read32(ipc+4)==9 && !m.ram.read32(ipc+32),"root and tagged clone share descriptor close state");}
                    put(close,32,fds.front());check(send(close,clone)==0 && m.ram.read32(ipc+24)==Process::InvalidHandle,"closed device descriptors stay invalid");
                    check(!m.ram.canAccess(address,size,Memory::Read),"closing devices does not release service-owned communication memory");m.ram.writeBytes(scratch,old.data(),old.size());m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);
                }
            }
            if(root && m.cpu.readRegister(0)==root && read(b,0)==4 && read(b,16)==0x49434653 && read(b,24)==2)closingDevice=true;
            if(read(b,0)==0x01110004 && read(b,68)==0xC008010E){
                const auto context=m.cpu.captureContext();const auto source=read(b,20);const auto original=m.ram.read64(source);
                auto replay=[&](Ipc::Buffer packet){m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                auto bad=b;put(bad,64,0);check(replay(bad)==Process::InvalidHandle,"GetId rejects an invalid device descriptor");
                bad=b;put(bad,68,0xC0080103);check(replay(bad)==Ipc::UnknownCommand && m.ram.read64(source)==original,"remote FromId remains explicit and does not publish an imported object");
                m.ram.write32(source+4,0);check(replay(b)==Process::InvalidHandle && m.ram.read32(source)==uint32_t(original),"GetId rejects a zero object without output publication");
                m.ram.write64(source,original);m.ram.writeBytes(ipc,b.data(),b.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);
                m.cpu.restoreContext(context);m.run(1);check(m.cpu.readRegister(0)==Process::InvalidPointer && m.ram.read64(source)==original,"protected GetId reply preserves caller data");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                check(!replay(b) && m.ram.read32(source)==uint32_t(original>>32),"GetId returns the local object export ID");
                const auto exported=m.ram.read32(source);
                m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);m.cpu.writeRegister(0,clone);m.run(1);
                check(!m.cpu.readRegister(0) && !m.ram.read32(ipc+24) && m.ram.read32(source)==exported,"cloned endpoint observes the same stable export ID");
                m.ram.write64(source,original);m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);
            }
            if(read(b,0)==0x01110004 && read(b,68)==0xC0200104){
                binding=true;bindingPacket=b;bindingContext=m.cpu.captureContext();
                const auto source=read(b,20);std::array<uint8_t,32> data{};for(size_t j=0;j<32;++j)data[j]=m.ram.read8(source+j);
                auto replay=[&](size_t field,uint32_t value){m.ram.writeBytes(source,data.data(),data.size());m.ram.write32(source+field,value);m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(bindingContext);m.run(1);return m.ram.read32(ipc+24);};
                check(replay(0,0)==Process::InvalidHandle,"Alloc requires an existing object");
                check(replay(4,1)==Ipc::InvalidArguments,"unsupported heapmask is explicit");
                check(replay(8,2)==Ipc::InvalidArguments,"unsupported access flags are explicit");
                check(replay(12,0x1800)==Ipc::InvalidArguments,"Alloc alignment must be a page-sized power of two");
                check(replay(12,0x40000)==Ipc::InvalidArguments,"alignment hints above the bounded 128 KiB profile fail explicitly");
                check(replay(24,Process::HeapBase+1)==Ipc::InvalidArguments,"CPU backing remains page-aligned independently of the requested alignment hint");
                check(replay(16,1)==Ipc::InvalidArguments,"nonlinear kind is unsupported");
                check(replay(20,1)==Ipc::InvalidArguments,"Alloc padding must be zero");
                check(replay(24,Process::HeapBase+Process::HeapCapacity)==Ipc::InvalidBuffer,"Alloc rejects backing outside the live heap");
                m.ram.writeBytes(source,data.data(),data.size());const auto originalBacking=m.ram.read64(source+24);
                m.ram.setPermissions(originalBacking,4096,Memory::Read);
                check(replay(0,uint32_t(data[0])|(uint32_t(data[1])<<8)|(uint32_t(data[2])<<16)|(uint32_t(data[3])<<24))==Ipc::InvalidBuffer,"writable Alloc requires writable backing");
                m.ram.setPermissions(originalBacking,4096,Memory::Read|Memory::Write);
                m.ram.writeBytes(source,data.data(),data.size());m.ram.writeBytes(ipc,b.data(),b.size());
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(bindingContext);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected reply cannot commit a memory binding");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(bindingContext);
            }
            if(read(b,0)==0x01110004 && read(b,48)==0x49434653 && read(b,56)==1 && read(b,68)==0xC0080101){
                creatingMap=true;++ioctls;ioctlSource=read(b,20);ioctlBefore=m.ram.read64(ioctlSource);                const auto context=m.cpu.captureContext();const uint64_t scratch=Process::HeapBase+0x170000;
                auto send=[&](Ipc::Buffer packet,uint32_t bytes){m.ram.write32(scratch,bytes);m.ram.write32(scratch+4,0xBADF00D);m.ram.writeBytes(ipc,packet.data(),packet.size());m.cpu.restoreContext(context);m.run(1);return m.cpu.readRegister(0)?uint32_t(m.cpu.readRegister(0)):m.ram.read32(ipc+24);};
                auto packet=b;put(packet,20,uint32_t(scratch));put(packet,32,uint32_t(scratch));
                check(send(packet,0)==Ipc::InvalidArguments && m.ram.read32(scratch+4)==0xBADF00D,"zero Create size cannot publish an object");
                check(send(packet,4095)==Ipc::InvalidArguments,"Create requires page-sized objects");
                auto malformed=packet;put(malformed,64,0);check(send(malformed,4096)==Process::InvalidHandle,"Create requires a live device descriptor");
                malformed=packet;put(malformed,32,uint32_t(scratch+4));check(send(malformed,4096)==Ipc::InvalidBuffer,"partially overlapping ioctl buffers are rejected");
                m.ram.write32(scratch,4096);m.ram.write32(scratch+4,0xBADF00D);m.ram.writeBytes(ipc,packet.data(),packet.size());
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);m.cpu.restoreContext(context);m.run(1);
                check(m.cpu.readRegister(0)==Process::InvalidPointer && m.ram.read32(scratch+4)==0xBADF00D,"protected reply cannot publish Create output");
                m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
                std::vector<uint32_t> objects;
                for(size_t index=0;index<Process::NvMapObjectLimit;++index){check(!send(packet,4096),"bounded object slot can be allocated");objects.push_back(m.ram.read32(scratch+4));}
                check(send(packet,4096)==Process::OutOfResource,"object quota rejects an additional Create");
                auto query=packet;put(query,68,0xC008010E);
                auto queryObject=[&](uint32_t object){m.ram.write32(scratch,0xBADF00D);m.ram.write32(scratch+4,object);m.ram.writeBytes(ipc,query.data(),query.size());m.cpu.restoreContext(context);m.run(1);return m.ram.read32(ipc+24);};
                check(queryObject(objects.front())==Process::InvalidState && m.ram.read32(scratch)==0xBADF00D,"local GetId policy rejects unbound objects without publishing output");
                auto release=packet;put(release,16,24);put(release,28,24);put(release,68,0xC0180105);
                auto freeObject=[&](uint32_t object){for(size_t at=0;at<24;at+=4)m.ram.write32(scratch+at,0);m.ram.write32(scratch,object);m.ram.writeBytes(ipc,release.data(),release.size());m.cpu.restoreContext(context);if(clone && (object&1))m.cpu.writeRegister(0,clone);m.run(1);return m.ram.read32(ipc+24);};
                for(auto object:objects)check(!freeObject(object) && m.ram.read32(scratch+16)==4096 && !m.ram.read64(scratch+8) && !m.ram.read32(scratch+20),"Free returns unbound object metadata and releases its slot");
                check(freeObject(objects.front())==Process::InvalidHandle,"Free rejects a retired object");
                check(queryObject(objects.front())==Process::InvalidHandle && m.ram.read32(scratch)==0xBADF00D,"GetId rejects retired object identifiers");
                check(!send(packet,Process::HeapCapacity),"logical byte budget permits one maximum-sized unbound object");const auto maximum=m.ram.read32(scratch+4);
                check(send(packet,4096)==Process::OutOfResource,"logical byte budget rejects oversubscription");
                check(!freeObject(maximum),"Free releases the logical byte budget");
                m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);
                check(read(b,64)==device && read(b,16)==8 && read(b,28)==8 && read(b,20)==read(b,32) && m.ram.read32(ioctlSource)==0x1000,"actual Nvmap Create uses the opened fd, 4 KiB size and aliased eight-byte ioctl data");
            }
        }
        const bool closing=svc && id==0x16 && transfer && m.cpu.readRegister(0)==transfer;
        const bool closingClone=svc && id==0x16 && clone && m.cpu.readRegister(0)==clone;
        const bool closingRoot=svc && id==0x16 && root && m.cpu.readRegister(0)==root;
        const auto result=m.run(1);
        if(binding){
            check(!m.cpu.readRegister(0) && !m.ram.read32(ipc+24),"original Alloc succeeds after rejected requests");
            const auto after=m.cpu.captureContext();Ipc::Buffer reply{};for(size_t j=0;j<reply.size();++j)reply[j]=m.ram.read8(ipc+j);
            m.ram.writeBytes(ipc,bindingPacket.data(),bindingPacket.size());m.cpu.restoreContext(bindingContext);m.run(1);
            check(m.ram.read32(ipc+24)==Process::InvalidState,"duplicate Alloc cannot replace a live binding");
            const auto source=read(bindingPacket,20);const auto backing=m.ram.read64(source+24);
            m.cpu.restoreContext(bindingContext);m.cpu.setPC(transferPc);m.cpu.writeRegister(1,backing);m.cpu.writeRegister(2,4096);m.cpu.writeRegister(3,Memory::None);m.run(1);
            check(m.cpu.readRegister(0)==Process::InvalidState && m.ram.canAccess(backing,4096,Memory::Read|Memory::Write),"live Nvmap binding prevents transfer-memory borrowing");
            m.ram.writeBytes(ipc,reply.data(),reply.size());m.cpu.restoreContext(after);
        }
        if(opening){device=m.ram.read32(ipc+32);check(!m.cpu.readRegister(0) && !m.ram.read32(ipc+24) && device,"original device Open succeeds after replay cleanup");}
        if(closingDevice){++deviceCloses;check(!m.cpu.readRegister(0) && !m.ram.read32(ipc+24) && !m.ram.read32(ipc+32),"original nvMapExit closes its device descriptor");device=0;}
        if(creatingMap)check(!m.cpu.readRegister(0) && !m.ram.read32(ipc+24) && m.ram.read32(ioctlSource)==uint32_t(ioctlBefore) && m.ram.read32(ioctlSource+4)!=uint32_t(ioctlBefore>>32),"successful Create preserves size and publishes an object identifier");
        if(initializing){
            const auto after=m.cpu.captureContext();Ipc::Buffer reply{};for(size_t j=0;j<reply.size();++j)reply[j]=m.ram.read8(ipc+j);
            m.ram.writeBytes(ipc,initialPacket.data(),initialPacket.size());m.cpu.restoreContext(initialContext);m.run(1);
            check(m.ram.read32(ipc+24)==Ipc::AlreadyInitialized && !m.ram.canAccess(address,size,Memory::Read),"duplicate Initialize cannot retain another reference or release borrowed memory");
            m.ram.writeBytes(ipc,reply.data(),reply.size());m.cpu.restoreContext(after);
        }
        if(svc && id==0x15){++creates;check(!m.cpu.readRegister(0),"original transfer-memory SVC succeeds");transfer=uint32_t(m.cpu.readRegister(1));}
        if(closing){++closes;check(!m.cpu.readRegister(0) && !m.ram.canAccess(address,size,Memory::Read),"sender handle close preserves service-owned borrowed memory");transfer=0;}
        if(cloning){++clones;clone=uint32_t(m.ram.read32(ipc+12));check(!m.cpu.readRegister(0) && !m.ram.read32(ipc+24) && clone && clone!=root,"original tagged clone shares initialized NV service state");}
        if(closingClone){check(!m.ram.canAccess(address,size,Memory::Read),"closing one endpoint keeps root-owned memory protected");clone=0;}
        if(closingRoot){++retired;check(m.ram.canAccess(address,size,Memory::Read|Memory::Write),"last service endpoint close restores memory before SDK cleanup/free");root=0;}
        if(result.stop!=Machine::Stop::Budget)break;
    }
    check(creates==2 && initializes==2 && closes==2 && clones==2 && registrations==2 && retired==2,"two original attempts initialize, clone, register and retire NV services");
    if(argc==4)check(opens==2,"actual original nvMapInit opens /dev/nvmap twice");
    if(argc==4)check(deviceCloses==2,"original nvMapExit closes both actual device descriptors");
    if(argc==4 && (std::string(argv[3])=="--alloc" || (std::string(argv[3])=="--cycle" || std::string(argv[3])=="--wrapper")))check(ioctls==2,"both original Create ioctl requests are captured");
    Machine reverse(false,true,true);if(!reverse.load(argv[1]))return 2;
    uint64_t held=0,heldSize=0,closePc=0,sendPc=0;uint32_t heldHandle=0,rootHandle=0,cloneHandle=0;
    for(unsigned i=0;i<500000;++i){
        const auto pc=reverse.cpu.getPC();const auto op=reverse.ram.fetch32(pc);const bool svc=(op&0xFFE0001F)==0xD4000001;const auto id=(op>>5)&0xFFFF;bool cloning=false,registering=false;
        if(svc && id==0x15){held=reverse.cpu.readRegister(1);heldSize=reverse.cpu.readRegister(2);}
        if(svc && id==0x16 && reverse.cpu.readRegister(0)==heldHandle)closePc=pc;
        if(svc && id==0x21 && reverse.ram.read32(reverse.process().mainThread().localRegion)==5 && reverse.ram.read32(reverse.process().mainThread().localRegion+24)==4){cloning=true;rootHandle=uint32_t(reverse.cpu.readRegister(0));}
        if(svc && id==0x21 && cloneHandle && reverse.ram.read32(reverse.process().mainThread().localRegion+4)==0x8000000A && reverse.ram.read32(reverse.process().mainThread().localRegion+40)==8){registering=true;sendPc=pc;}
        const auto result=reverse.run(1);if(svc && id==0x15)heldHandle=uint32_t(reverse.cpu.readRegister(1));
        if(cloning)cloneHandle=reverse.ram.read32(reverse.process().mainThread().localRegion+12);if(registering)break;if(result.stop!=Machine::Stop::Budget)break;
    }
    check(rootHandle && cloneHandle && closePc && sendPc && !reverse.ram.canAccess(held,heldSize,Memory::Read),"initialized and registered root/clone retain memory after sender close");
    uint64_t heapSvc=0;for(uint64_t at=0x10000;at<Process::HeapBase;at+=4)if(reverse.ram.canAccess(at,4,Memory::Execute) && reverse.ram.read32(at)==0xD4000021){heapSvc=at;break;}
    check(heapSvc!=0,"original heap-size syscall instruction is available for lifetime replay");
    const auto saved=reverse.cpu.captureContext();
    const auto reverseIpc=reverse.process().mainThread().localRegion,scratch=Process::HeapBase+0x180000;
    reverse.ram.writeBytes(scratch,reinterpret_cast<const uint8_t*>("/dev/nvmap"),10);
    Ipc::Buffer open{};put(open,0,0x00100004);put(open,4,8);put(open,8,10);put(open,12,uint32_t(scratch));put(open,32,0x49434653);
    auto sendEndpoint=[&](const Ipc::Buffer& packet,uint32_t endpoint){reverse.ram.writeBytes(reverseIpc,packet.data(),packet.size());reverse.cpu.restoreContext(saved);reverse.cpu.setPC(sendPc);reverse.cpu.writeRegister(0,endpoint);reverse.run(1);return reverse.cpu.readRegister(0);};
    check(sendEndpoint(open,rootHandle)==0 && !reverse.ram.read32(reverseIpc+24),"registered root creates a real logical device descriptor");const auto sharedFd=reverse.ram.read32(reverseIpc+32);
    if(heapSvc){reverse.cpu.setPC(heapSvc);reverse.cpu.writeRegister(1,0);reverse.run(1);check(reverse.cpu.readRegister(0)==Process::InvalidState && reverse.process().heapSize()==Process::HeapAlignment,"service-retained memory still prevents heap shrink without a sender handle");}
    auto closeEndpoint=[&](uint32_t handle){reverse.cpu.restoreContext(saved);reverse.cpu.setPC(closePc);reverse.cpu.writeRegister(0,handle);reverse.run(1);return reverse.cpu.readRegister(0);};
    check(closeEndpoint(rootHandle)==0 && !reverse.ram.canAccess(held,heldSize,Memory::Read),"closing root first preserves clone-owned memory");
    Ipc::Buffer closeFd{};put(closeFd,0,4);put(closeFd,4,9);put(closeFd,16,0x49434653);put(closeFd,24,2);put(closeFd,32,sharedFd);
    check(sharedFd && sendEndpoint(closeFd,cloneHandle)==0 && !reverse.ram.read32(reverseIpc+24) && !reverse.ram.canAccess(held,heldSize,Memory::Read),"device descriptor and registration survive root closure through the tagged clone");
    check(closeEndpoint(cloneHandle)==0 && reverse.ram.canAccess(held,heldSize,Memory::Read|Memory::Write),"last clone close restores source access in reverse cleanup order");
    Machine fault(false,true,true);if(!fault.load(argv[1]))return 2;uint64_t borrowed=0,length=0;uint32_t faultTransfer=0;
    for(unsigned i=0;i<500000;++i){
        const auto op=fault.ram.fetch32(fault.cpu.getPC());const bool creating=(op&0xFFE0001F)==0xD4000001 && ((op>>5)&0xFFFF)==0x15;
        const bool sendingClosed=(op&0xFFE0001F)==0xD4000001 && ((op>>5)&0xFFFF)==0x16 && faultTransfer && fault.cpu.readRegister(0)==faultTransfer;
        if(creating){borrowed=fault.cpu.readRegister(1);length=fault.cpu.readRegister(2);}
        const auto result=fault.run(1);if(creating){check(!fault.cpu.readRegister(0),"fault fixture creates actual transfer memory");faultTransfer=uint32_t(fault.cpu.readRegister(1));}if(sendingClosed)break;if(result.stop!=Machine::Stop::Budget)break;
    }
    check(borrowed && length && !fault.ram.canAccess(borrowed,length,Memory::Read),"fault fixture has protected borrowed pages");
    const auto missing=std::string(argv[1])+".missing";const auto pc=fault.cpu.getPC();
    check(!fault.load(missing.c_str()) && fault.cpu.getPC()==pc && !fault.ram.canAccess(borrowed,length,Memory::Read),"failed load preserves active transfer ownership");
    fault.cpu.setPC(Machine::RAM_SIZE);check(fault.run(1).stop==Machine::Stop::Error && fault.ram.canAccess(borrowed,length,Memory::Read|Memory::Write),"fault restores borrowed memory without a dangling staged-memory pointer");
    check(fault.load(argv[1]),"reload builds an independent transfer-memory context");
    const auto reloaded=fault.run(500000);check(reloaded.stop==Machine::Stop::Exited && reloaded.exitCode==0,"original NV cleanup still works after fault and reload");
    std::cout<<"NV instructions="<<instructions<<" failures="<<failures<<'\n';return failures?1:0;
}
