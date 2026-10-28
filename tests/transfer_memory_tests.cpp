#include "machine.hpp"
#include <iostream>
#include <vector>
int main(int argc,char** argv){
    if(argc!=2)return 2;unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;std::string error;Process p;
    if(!ProgramLoader::loadImage(ram,argv[1],image,error) || !p.initialize(ram,image,true,error,true))return 2;
    Bus bus(ram);CPU cpu(bus);const auto source=Process::HeapBase+0x10000,output=p.mainThread().localRegion+0x200;
    auto create=[&](uint64_t address,uint64_t size,uint64_t permission=0){cpu.writeRegister(1,address);cpu.writeRegister(2,size);cpu.writeRegister(3,permission);p.dispatch(0x15,cpu,bus);return uint32_t(cpu.readRegister(0));};
    auto close=[&](uint32_t id){cpu.writeRegister(0,id);p.dispatch(0x16,cpu,bus);return uint32_t(cpu.readRegister(0));};
    auto query=[&](){cpu.writeRegister(0,output);cpu.writeRegister(2,source);p.dispatch(0x06,cpu,bus);check(cpu.readRegister(0)==0,"query transfer source metadata");};
    ram.write64(source,0x1122334455667788ULL);
    check(create(source+1,4096)==Process::InvalidAddress && !cpu.readRegister(1),"unaligned transfer source fails without a handle");
    check(create(source,0)==Process::InvalidSize && create(source,4095)==Process::InvalidSize,"transfer size must contain full pages");
    check(create(source,4096,Memory::Read)==Process::InvalidState,"unsupported owner permission is rejected");
    check(create(Process::HomebrewEnvironment,4096)==Process::InvalidAddress,"transfer memory is restricted to live heap pages");
    ram.setPermissions(source,4096,Memory::Read);check(create(source,4096)==Process::InvalidState,"read-only source cannot be lent");ram.setPermissions(source,4096,Memory::Read|Memory::Write);
    check(create(source,4096)==0,"create transfer memory over a writable heap page");const auto handle=uint32_t(cpu.readRegister(1));
    check(handle && !ram.canAccess(source,4096,Memory::Read) && !ram.canAccess(source,4096,Memory::Write),"Perm_None removes guest data access");
    query();check(ram.read32(output+16)==5 && ram.read32(output+20)==1 && !ram.read32(output+24),"source remains heap with local borrowed attribute and no permissions");
    check(create(source,4096)==Process::InvalidState && !cpu.readRegister(1),"overlapping borrow cannot create a second object");
    cpu.writeRegister(1,0);p.dispatch(0x01,cpu,bus);check(cpu.readRegister(0)==Process::InvalidState && p.heapSize()==Process::HeapAlignment,"heap cannot discard active transfer memory");
    check(close(handle)==0 && ram.canAccess(source,4096,Memory::Read|Memory::Write) && ram.read64(source)==0x1122334455667788ULL,"close restores access without changing source bytes");
    query();check(!ram.read32(output+20) && ram.read32(output+24)==3,"close removes borrowed attributes");
    check(close(handle)==Process::InvalidHandle,"closed transfer handles stay invalid");
    std::vector<uint32_t> handles;
    for(size_t i=0;i<127;++i){check(create(source+i*4096,4096)==0,"bounded transfer handle allocation");handles.push_back(uint32_t(cpu.readRegister(1)));}
    check(create(source+127*4096,4096)==Process::OutOfHandles && !cpu.readRegister(1) && ram.canAccess(source+127*4096,4096,Memory::Read|Memory::Write),"exhausted quota does not borrow another page");
    for(auto id:handles)check(close(id)==0,"release every transfer handle");
    check(create(source,4096)==0,"transfer can be recreated after quota cleanup");p.checkpoint(cpu,Process::State::Faulted);
    check(ram.canAccess(source,4096,Memory::Read|Memory::Write),"fault retires outstanding transfer ownership");
    check(create(source,4096)==Process::InvalidState,"terminal process cannot create transfer memory");
    std::cout<<"Transfer-memory failures="<<failures<<'\n';return failures?1:0;
}
