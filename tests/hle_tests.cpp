#include "machine.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc,char** argv)
{
    if(argc!=5) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { std::cerr<<"FAIL: "<<text<<'\n'; ++failures; } };
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    if(bytes.size()!=64) { std::cerr<<"Invalid assembler fixture\n"; return 2; }
    auto word=[&](size_t i) { const auto at=i*4; return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|(uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24); };
    Memory ram(0x4000); CPU cpu(ram);
    CPU::Context original;
    for(size_t i=0;i<CPU::REGISTER_COUNT;++i) original.registers[i]=0x1122334400000000ULL+i;
    original.pc=0; original.sp=0x3000; original.pstate=0xA0000000;
    original.tpidrEl0=0x12345678; original.tpidrroEl0=0x200;
    cpu.restoreContext(original);
    auto captured=cpu.captureContext();
    check(captured.registers==original.registers && captured.pc==0 && captured.sp==original.sp &&
          captured.pstate==original.pstate && captured.tpidrEl0==original.tpidrEl0 && captured.tpidrroEl0==0x200,"complete integer thread context round-trip");
    auto execute=[&](size_t index) { cpu.setPC(0); ram.write32(0,word(index)); return cpu.step(); };
    check(execute(0) && cpu.readRegister(0)==0x200,"MRS TPIDRRO_EL0");
    check(execute(1) && cpu.readRegister(1)==0x12345678,"MRS TPIDR_EL0");
    cpu.writeRegister(2,0xABCDEF);
    check(execute(2) && cpu.captureContext().tpidrEl0==0xABCDEF,"MSR TPIDR_EL0");
    check(!execute(3) && cpu.hasError() && cpu.captureContext().tpidrroEl0==0x200,"guest cannot write TPIDRRO_EL0");
    check(!execute(4) && cpu.hasError(),"unsupported privileged thread register");
    const auto zeroRegister=cpu.readRegister(31);
    check(execute(5) && cpu.readRegister(31)==zeroRegister,"MRS to XZR discarded");
    check(execute(6) && cpu.captureContext().tpidrEl0==0,"MSR from XZR supplies zero");
    cpu.writeRegister(0,UINT64_MAX);
    check(execute(11) && cpu.readRegister(0)==0x12340000,"MOVZ W zero extension");
    check(execute(12) && cpu.readRegister(0)==0x12345678,"MOVK W merges low 32 bits");
    check(execute(13) && cpu.readRegister(1)==0xFFFFFFFFULL,"MOVN W zero extension");
    check(execute(14) && cpu.readRegister(31)==zeroRegister,"MOVZ WZR discarded");
    check(!execute(15) && cpu.hasError(),"reserved MOV W shift rejected");
    cpu.reset(); check(cpu.captureContext().tpidrEl0==0 && cpu.captureContext().tpidrroEl0==0,"reset clears thread registers");

    Machine machine(false,true);
    check(machine.load(argv[2]),"load reference HLE ELF");
    const uint64_t tlr=Machine::RAM_SIZE-Process::StackSize-2*Memory::PAGE_SIZE+0x200;
    check(machine.process().state()==Process::State::Ready && machine.process().mainThread().id==1 &&
          machine.process().mainThread().localRegion==tlr && machine.cpu.captureContext().tpidrroEl0==tlr,"initial process/thread/TLR");
    check(machine.ram.canAccess(tlr,0x200,Memory::Read|Memory::Write) && !machine.ram.canAccess(tlr,4,Memory::Execute) && machine.ram.read64(tlr)==0,"TLR initialized RW and NX");
    auto result=machine.run(500);
    check(result.stop==Machine::Stop::Exited && result.exitCode==0 && machine.cpu.readRegister(0)==123 &&
          machine.cpu.readRegister(2)==55 && machine.ram.read64(Process::HeapBase)==55 && machine.ram.read64(tlr+0x180)==55,
          "compiled C uses allocated heap, thread local memory, QueryMemory and ExitProcess");
    check(machine.ram.read64(0x12000)==Process::HeapBase && machine.ram.read64(0x12008)==0x200000 &&
          machine.ram.read32(0x12010)==5 && machine.ram.read32(0x12018)==3,"guest QueryMemory heap layout");
    check(machine.process().state()==Process::State::Exited && machine.process().mainThread().state==Process::State::Exited &&
          machine.process().mainThread().context.pc==machine.cpu.getPC() && machine.process().mainThread().context.registers[2]==55,
          "exit snapshots process and thread state");
    std::cout<<"HLE sample: instructions="<<result.instructions<<" result="<<machine.cpu.readRegister(2)<<'\n';
    check(machine.run(1).instructions==0,"exited process stays exited");
    check(machine.load(argv[2]) && machine.process().heapSize()==0 && !machine.ram.canAccess(Process::HeapBase,1,Memory::Read),"reload resets heap mappings");
    check(machine.run(0).stop==Machine::Stop::Budget && machine.process().state()==Process::State::Paused,"zero budget checkpoints pause");
    auto findSvc=[&](uint16_t id) {
        const uint32_t encoding=0xD4000001U|(uint32_t(id)<<5);
        for(uint64_t address=0x10000;address<0x11000;address+=4) if(machine.ram.read32(address)==encoding) return address;
        return uint64_t(0);
    };
    const auto heapSvc=findSvc(1), querySvc=findSvc(6), exitSvc=findSvc(7), unknownSvc=findSvc(0xF001);
    check(heapSvc && querySvc && exitSvc && unknownSvc,"assembler SVC instructions found");
    auto invoke=[&](uint64_t pc,uint64_t x0,uint64_t x1,uint64_t x2) {
        machine.cpu.setPC(pc); machine.cpu.writeRegister(0,x0); machine.cpu.writeRegister(1,x1); machine.cpu.writeRegister(2,x2);
        return machine.run(1);
    };
    constexpr uint64_t out=0x12000;
    machine.ram.write64(0x12050,0xC0FFEE);
    result=invoke(heapSvc,0x12050,Process::HeapAlignment,0);
    check(result.stop==Machine::Stop::Budget && machine.cpu.readRegister(0)==0 && machine.cpu.readRegister(1)==Process::HeapBase &&
          machine.ram.read64(0x12050)==0xC0FFEE && machine.process().heapSize()==0x200000,"SetHeapSize register ABI, no kernel write to wrapper pointer");
    check(machine.cpu.getPC()==heapSvc+4 && machine.process().mainThread().context.pc==heapSvc+4,"resume PC after service");
    machine.ram.write64(Process::HeapBase,77);
    invoke(heapSvc,0,0x400000,0);
    check(machine.cpu.readRegister(0)==0 && machine.ram.read64(Process::HeapBase)==77,"heap growth preserves existing data");
    machine.ram.write64(Process::HeapBase+0x200000,99);
    invoke(heapSvc,0,0x200000,0);
    check(machine.cpu.readRegister(0)==0 && !machine.ram.canAccess(Process::HeapBase+0x200000,8,Memory::Read),"heap shrink unmaps removed pages");
    invoke(heapSvc,0,0x400000,0);
    check(machine.ram.read64(Process::HeapBase+0x200000)==0,"heap regrowth zeroes discarded pages");
    invoke(heapSvc,0,0x200001,0);
    check(machine.cpu.readRegister(0)==Process::InvalidSize && machine.process().heapSize()==0x400000,"misaligned heap size rejected without changing heap");
    invoke(heapSvc,0,0x200000000ULL,0);
    check(machine.cpu.readRegister(0)==Process::InvalidSize,"size above 4GiB rejected");
    invoke(heapSvc,0,0xA00000,0);
    check(machine.cpu.readRegister(0)==Process::OutOfMemory && machine.process().heapSize()==0x400000,"heap local capacity limit");
    invoke(heapSvc,0,Process::HeapCapacity,0);
    check(machine.cpu.readRegister(0)==0 && machine.process().heapSize()==Process::HeapCapacity,"maximum heap supported");
    invoke(heapSvc,0,0x400000,0);
    invoke(querySvc,out,0x12050,Process::HeapBase+123);
    check(machine.cpu.readRegister(0)==0 && machine.cpu.readRegister(1)==0 && machine.ram.read64(0x12050)==0xC0FFEE &&
          machine.ram.read64(out)==Process::HeapBase && machine.ram.read64(out+8)==0x400000 && machine.ram.read32(out+16)==5 &&
          machine.ram.read64(out+28)==0 && machine.ram.read32(out+36)==0,"QueryMemory register PageInfo and zero reference fields");
    const uint64_t crossPage=Process::HeapBase+Memory::PAGE_SIZE-16;
    machine.ram.write64(crossPage+40,0xCAFE);
    invoke(querySvc,crossPage,0,Process::HeapBase);
    check(machine.cpu.readRegister(0)==0 && machine.ram.read64(crossPage)==Process::HeapBase &&
          machine.ram.read64(crossPage+8)==0x400000 && machine.ram.read32(crossPage+24)==3 &&
          machine.ram.read64(crossPage+40)==0xCAFE,"QueryMemory copy crosses writable pages without touching its suffix");
    invoke(querySvc,out,0,0);
    check(machine.ram.read64(out)==0 && machine.ram.read64(out+8)==0x10000 && machine.ram.read32(out+16)==0,"query unmapped range without reading it");
    invoke(querySvc,out,0,tlr);
    check(machine.ram.read64(out)==tlr-0x200 && machine.ram.read64(out+8)==4096 && machine.ram.read32(out+16)==12,"query thread-local page");
    invoke(querySvc,out,0,Machine::RAM_SIZE-16);
    check(machine.ram.read32(out+16)==11 && machine.ram.read32(out+24)==3,"query stack state and rights");
    invoke(querySvc,out,0,0x10000);
    check(machine.ram.read32(out+16)==3 && machine.ram.read32(out+24)==5,"query executable code");
    invoke(querySvc,out,0,0x12000);
    check(machine.ram.read32(out+16)==4,"query mutable image data");
    machine.ram.write64(out,0xABCDEF);
    invoke(querySvc,out,0,UINT64_MAX);
    check(machine.cpu.readRegister(0)==Process::InvalidAddress && machine.ram.read64(out)==0xABCDEF,"outside local address space rejected");
    for(uint64_t pointer : {uint64_t(0x10000),uint64_t(0x12FF0),UINT64_MAX,uint64_t(Machine::DEVICE_BASE)})
    {
        if(pointer==0x12FF0) machine.ram.write64(pointer,0xDEADBEEF);
        invoke(querySvc,pointer,123,Process::HeapBase);
        check(machine.cpu.readRegister(0)==Process::InvalidPointer && machine.cpu.readRegister(1)==123 && !machine.bus.hasError(),"invalid query output returns guest error, not CPU memory fault");
        if(pointer==0x12FF0) check(machine.ram.read64(pointer)==0xDEADBEEF,"cross-page output failure leaves prefix untouched");
    }
    invoke(heapSvc,0,0,0);
    check(machine.process().heapSize()==0 && machine.cpu.readRegister(1)==Process::HeapBase && !machine.ram.canAccess(Process::HeapBase,8,Memory::Read),"zero heap size unmaps heap");
    result=invoke(unknownSvc,44,55,66);
    check(result.stop==Machine::Stop::Error && machine.process().state()==Process::State::Faulted && machine.cpu.readRegister(0)==44 &&
          machine.error().find("61441")!=std::string::npos,"unknown SVC stops with diagnostic, no success fabricated");
    check(machine.run(1).stop==Machine::Stop::Error && machine.run(1).instructions==0,"fault cannot be bypassed by running again");
    check(machine.load(argv[2]),"reload clears terminal process fault");
    result=invoke(exitSvc,0xBEEF,0,0);
    check(result.stop==Machine::Stop::Exited && result.exitCode==0 && machine.cpu.readRegister(0)==0xBEEF,"ExitProcess ignores X0 as an exit status");

    Machine disabled;
    check(disabled.load(argv[2]) && disabled.run(500).stop==Machine::Stop::Error,"HLE disabled by default");
    Machine enabled(false,true);
    check(!enabled.load(argv[3]),"raw image cannot silently become a reference process");
    check(enabled.load(argv[2]),"load process for atomic profile validation");
    enabled.run(1);
    const auto before=enabled.cpu.captureContext();
    std::ifstream elf(argv[2],std::ios::binary);
    std::vector<uint8_t> conflicting((std::istreambuf_iterator<char>(elf)),{});
    auto readLE=[&](size_t at,size_t n) { uint64_t v=0; for(size_t i=0;i<n;++i) v|=uint64_t(conflicting.at(at+i))<<(8*i); return v; };
    auto put=[&](size_t at,uint64_t v) { for(size_t i=0;i<8;++i) conflicting.at(at+i)=static_cast<uint8_t>(v>>(8*i)); };
    const size_t phoff=static_cast<size_t>(readLE(32,8));
    const auto path=std::filesystem::path(argv[4])/"hle-layout-conflict.elf";
    for(uint64_t address : {Process::HeapBase,tlr-0x200})
    {
        put(phoff+2*56+16,address); // Move last data segment: still valid ELF, conflicting only with HLE reservations.
        { std::ofstream file(path,std::ios::binary); file.write(reinterpret_cast<const char*>(conflicting.data()),conflicting.size()); }
        Memory independent(Machine::RAM_SIZE); ProgramLoader::Image image; std::string error;
        check(ProgramLoader::loadElf(independent,conflicting,image,error),"profile conflict remains structurally valid ELF");
        check(!enabled.load(path.string().c_str()) && enabled.cpu.captureContext().registers==before.registers &&
              enabled.cpu.getPC()==before.pc && enabled.process().state()==Process::State::Paused && enabled.ram.read32(0x10000)!=0,
              "failed HLE layout preserves prior process and CPU");
    }
    std::filesystem::remove(path);
    std::cout<<"HLE and thread-context failures="<<failures<<'\n';
    return failures==0 ? 0 : 1;
}
