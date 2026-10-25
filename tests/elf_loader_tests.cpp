#include "machine.hpp"
#include "program_loader.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
void put(std::vector<uint8_t>& bytes, size_t at, uint64_t value, size_t count)
{
    for (size_t i = 0; i < count; ++i) bytes.at(at+i) = static_cast<uint8_t>(value >> (8*i));
}
std::vector<uint8_t> fixture()
{
    std::vector<uint8_t> bytes(0x3008);
    bytes[0]=0x7F; bytes[1]='E'; bytes[2]='L'; bytes[3]='F'; bytes[4]=2; bytes[5]=1; bytes[6]=1;
    put(bytes,16,2,2); put(bytes,18,183,2); put(bytes,20,1,4); put(bytes,24,0x1000,8);
    put(bytes,32,64,8); put(bytes,52,64,2); put(bytes,54,56,2); put(bytes,56,3,2);
    for (size_t i=0; i<3; ++i)
    {
        const size_t at=64+i*56;
        put(bytes,at,1,4); put(bytes,at+4,i==0 ? 5 : i==1 ? 4 : 6,4);
        put(bytes,at+8,0x1000+i*0x1000,8); put(bytes,at+16,0x1000+i*0x1000,8);
        put(bytes,at+32,8,8); put(bytes,at+40,i==2 ? 0x1008 : 8,8); put(bytes,at+48,4096,8);
    }
    put(bytes,0x1000,0xD2800000,4); // MOVZ X0, #0
    put(bytes,0x1004,0xD41E0001,4); // SVC #0xF000, local test exit
    put(bytes,0x2000,55,8); put(bytes,0x3000,0x1122334455667788ULL,8);
    return bytes;
}
bool save(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream file(path,std::ios::binary);
    return bool(file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())));
}
}
int main(int argc, char** argv)
{
    if (argc != 4) return 2;
    int failures=0, rejected=0;
    auto check=[&](bool ok,const char* name) { if(!ok) { std::cerr<<"FAIL: "<<name<<'\n'; ++failures; } };
    Memory memory(0x20000);
    ProgramLoader::Image image;
    std::string error;
    const auto valid=fixture();
    check(ProgramLoader::loadElf(memory,valid,image,error),"valid three-segment ELF");
    check(image.format==ProgramLoader::Format::Elf64 && image.entry==0x1000 && image.stack==0x20000,"ELF entry and initial stack");
    check(memory.read64(0x2000)==55 && memory.read64(0x3000)==0x1122334455667788ULL &&
          memory.read64(0x3008)==0 && memory.read64(0x4000)==0,"rodata, initialized data, BSS across pages");
    check(memory.canAccess(0x1000,4,Memory::Read|Memory::Execute) && !memory.canAccess(0x1000,4,Memory::Write) &&
          memory.canAccess(0x3000,8,Memory::Read|Memory::Write) && !memory.canAccess(0x3000,4,Memory::Execute),"segment permissions");
    check(!memory.canAccess(0,4,Memory::Read) && !memory.canAccess(0xF000,4,Memory::Read) &&
          memory.canAccess(0x10000,0x10000,Memory::Read|Memory::Write) && !memory.canAccess(0x1FFF0,4,Memory::Execute),
          "unmapped memory, guard page and non-executable stack");
    Bus bus(memory); CPU cpu(bus); cpu.setPC(image.entry); cpu.setSP(image.stack);
    check(cpu.step() && cpu.stepDetailed().status==CPU::StepStatus::SystemCall,"ELF instructions execute");
    const auto originalInstruction=memory.read32(0x1000);
    bus.clearError(); bus.write32(0x1000,0);
    check(bus.hasError(),"bus reports text write denial");
    memory.clearError(); check(memory.read32(0x1000)==originalInstruction,"text unchanged on denied write");
    cpu.setPC(0x3000);
    check(!cpu.step() && cpu.hasError() && cpu.getPC()==0x3000,"CPU execution denied in data");
    cpu.setPC(0x1FFF0);
    check(!cpu.step() && cpu.hasError(),"CPU execution denied in stack");

    // Stage a store/load in the executable segment and assert fault precision.
    auto stores=valid; put(stores,0x1000,0xF9000020,4); // STR X0, [X1]
    check(ProgramLoader::loadElf(memory,stores,image,error),"load store-fault fixture");
    cpu.reset(); cpu.setPC(0x1000); cpu.writeRegister(0,99); cpu.writeRegister(1,0x2000);
    check(!cpu.step() && cpu.getPC()==0x1000 && memory.read64(0x2000)==55,"CPU cannot write rodata");
    auto loads=valid; put(loads,0x1000,0xF9400020,4); // LDR X0, [X1]
    check(ProgramLoader::loadElf(memory,loads,image,error),"load read-fault fixture");
    cpu.reset(); cpu.setPC(0x1000); cpu.writeRegister(0,99); cpu.writeRegister(1,0);
    check(!cpu.step() && cpu.getPC()==0x1000 && cpu.readRegister(0)==99,"CPU denied load preserves destination");

    Memory old(0x20000); old.write64(0x8000,0xABCDEF); old.setPermissions(0x8000,4096,Memory::Read);
    ProgramLoader::Image oldImage{ProgramLoader::Format::Raw,0x55,0x66};
    auto reject=[&](const std::vector<uint8_t>& bytes,const char* name) {
        const bool refused=!ProgramLoader::loadElf(old,bytes,oldImage,error);
        check(refused && !error.empty(),name);
        check(old.read64(0x8000)==0xABCDEF && !old.canAccess(0x8000,8,Memory::Write) &&
              oldImage.entry==0x55 && oldImage.stack==0x66,"failed ELF leaves memory/permissions/metadata untouched");
        ++rejected;
    };
    for(size_t length=0;length<64;++length) reject(std::vector<uint8_t>(valid.begin(),valid.begin()+length),"truncated ELF header");
    reject(std::vector<uint8_t>(valid.begin(),valid.begin()+231),"truncated program header table");
    reject(std::vector<uint8_t>(valid.begin(),valid.end()-1),"truncated data segment");
    auto mutate=[&](size_t at,uint64_t value,size_t width,const char* name) { auto bytes=valid; put(bytes,at,value,width); reject(bytes,name); };
    mutate(0,0,1,"bad magic"); mutate(4,1,1,"ELF32 rejected"); mutate(5,2,1,"big endian rejected");
    mutate(6,0,1,"ident version"); mutate(7,3,1,"unsupported OS ABI"); mutate(8,1,1,"unsupported ABI version");
    mutate(16,3,2,"PIE/ET_DYN rejected"); mutate(18,62,2,"x86 ELF rejected"); mutate(20,0,4,"header version");
    mutate(48,1,4,"architecture flags"); mutate(52,63,2,"header size"); mutate(54,55,2,"program entry size");
    mutate(56,0,2,"no segments"); mutate(56,129,2,"too many segments"); mutate(56,0xFFFF,2,"extended phnum rejected");
    mutate(32,UINT64_MAX,8,"table offset overflow"); mutate(32,0,8,"table in ELF header");
    mutate(24,0x1002,8,"unaligned entry"); mutate(24,0x2000,8,"entry in rodata"); mutate(24,0x1008,8,"entry outside file-backed code");
    mutate(64,2,4,"dynamic interpreter requirements rejected"); mutate(64,3,4,"PT_INTERP rejected"); mutate(64,7,4,"TLS rejected");
    mutate(68,7,4,"writable executable segment rejected"); mutate(68,0x100,4,"unknown segment flags");
    mutate(72,UINT64_MAX,8,"segment offset overflow"); mutate(96,UINT64_MAX,8,"segment file size overflow");
    mutate(104,4,8,"filesz greater than memsz"); mutate(80,UINT64_MAX,8,"virtual address overflow");
    mutate(112,3,8,"non power-of-two alignment"); mutate(72,0x1001,8,"file/virtual incongruence");
    mutate(136,0x1000,8,"overlapping pages"); mutate(192,0xF000,8,"guard-page collision");
    mutate(192,0x10000,8,"stack collision"); mutate(216,UINT64_MAX,8,"memory size overflow");
    auto gnuStack=valid; put(gnuStack,176,0x6474E551,4); put(gnuStack,180,7,4); reject(gnuStack,"executable GNU stack");
    auto pureBss=valid; put(pureBss,208,0,8);
    check(ProgramLoader::loadElf(memory,pureBss,image,error) && memory.read64(0x3000)==0,"zero-file-size segment initialized as BSS");

    Memory permissions(0x4000); Bus permissionBus(permissions); CPU permissionCPU(permissionBus);
    permissions.write64(0xFF8,0x1234); permissions.write64(0x1000,0x5678);
    check(permissions.setPermissions(0x1000,4096,Memory::Read),"protect second page");
    permissions.write64(0xFFC,0);
    check(permissions.hasError() && permissions.read64(0xFF8)==0x1234 && permissions.read64(0x1000)==0x5678,
          "cross-page write denied before either page changes");
    // STP X0,X1,[X2] spans a writable and a read-only page.
    permissions.write32(0,0xA9000440); permissionCPU.writeRegister(2,0xFF8);
    check(!permissionCPU.step() && permissions.read64(0xFF8)==0x1234,"pair permission preflight prevents first store");
    permissions.clearError();
    check(permissions.setPermissions(0x1000,4096,Memory::None),"deny second page reads");
    check(permissions.read64(0xFFC)==0 && permissions.hasError(),"cross-page read denied as a whole");
    permissions.clearError();
    const Memory& constantMemory=permissions;
    check(constantMemory.read32(0x1000)==0 && permissions.hasError(),"const reads enforce permissions too");
    permissions.clearError();
    permissions.setPermissions(0,4096,Memory::Execute);
    permissionBus.clearError();
    check(permissionBus.fetch32(0)==0xA9000440 && !permissionBus.hasError(),"execute-only page fetches without read permission");
    check(permissionBus.read32(0)==0 && permissionBus.hasError(),"execute permission does not grant data read");
    permissionBus.clearError();
    check(!permissions.setPermissions(1,4096,Memory::Read),"unaligned permission change rejected");
    check(!permissions.setPermissions(0,0,Memory::Read),"empty permission change rejected");
    check(!permissions.setPermissions(0,4096,128),"unknown permission bits rejected");
    permissions.reset(Memory::None);
    check(!permissions.canAccess(0,1,Memory::Read),"memory reset may start unmapped");

    Machine machine(true);
    check(machine.load(argv[1]) && machine.cpu.getPC()==0x10000,"load compiler-produced ELF at nonzero entry");
    check(machine.ram.read64(0x12000)==10 && machine.ram.read64(0x12008)==0 && machine.ram.read64(0x12010)==0,"compiled data and BSS");
    auto result=machine.run(500);
    check(result.stop==Machine::Stop::Exited && result.exitCode==0 && machine.cpu.readRegister(2)==55 &&
          machine.ram.read64(0x12008)==55 && machine.cpu.getSP()==Machine::RAM_SIZE,"compiled ELF runs with protected segments");
    std::cout<<"ELF sample: instructions="<<result.instructions<<" result="<<machine.cpu.readRegister(2)<<'\n';
    const auto currentPC=machine.cpu.getPC();
    machine.interrupts.raiseIRQ(4); machine.device.write32(4,77); machine.timer.write64(8,99);
    const auto invalidPath=std::filesystem::path(argv[3])/"invalid-loader-fixture.elf";
    check(save(invalidPath,{0,0,0,0}),"write invalid ELF test file");
    check(!machine.load(invalidPath.string().c_str()) && machine.cpu.getPC()==currentPC &&
          machine.ram.read64(0x12008)==55 && machine.interrupts.isPending(4) && machine.device.read32(4)==77,
          "failed Machine load keeps previous process and devices");
    check(machine.load(argv[1]) && machine.ram.read64(0x12008)==0 && !machine.interrupts.isPending(4) &&
          machine.device.read32(4)==0 && machine.timer.getPeriod()==0 && machine.cpu.getPC()==0x10000,
          "successful reload resets BSS, CPU and devices");
    check(machine.load(argv[2]) && machine.cpu.getPC()==0 && machine.ram.canAccess(0,4096,Memory::All),"raw loading remains available after protected ELF");
    result=machine.run(4);
    check(result.stop==Machine::Stop::Budget && machine.cpu.readRegister(2)==12,"original raw sample still runs");
    std::filesystem::remove(invalidPath);
    std::cout<<"Rejected ELF fixtures="<<rejected<<" failures="<<failures<<'\n';
    return failures==0 ? 0 : 1;
}
