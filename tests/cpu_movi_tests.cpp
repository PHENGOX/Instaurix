#include "machine.hpp"
#include <fstream>
#include <iostream>
static uint64_t read(std::istream& input,unsigned size)
{ uint64_t v=0;for(unsigned i=0;i<size;++i) { int c=input.get();if(c<0) throw 1;v|=uint64_t(uint8_t(c))<<(8*i); }return v; }
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    std::ifstream in(argv[1],std::ios::binary);char magic[8]{};in.read(magic,8);
    if(std::string(magic,8)!="MOVI0001") return 2;
    const auto count=read(in,4);if(count!=1024) return 2;
    unsigned failures=0;
    Memory ram(4096);Bus bus(ram);CPU cpu(bus);
    for(unsigned i=0;i<count;++i) {
        const auto opcode=uint32_t(read(in,4));CPU::Vector expected{read(in,8),read(in,8)};
        CPU::Context before{};before.pstate=0xF0000000;before.fpcr=0x400000;before.fpsr=0x8000010;
        before.sp=0x800;before.tpidrEl0=0x123;before.tpidrroEl0=0x456;
        for(unsigned j=0;j<32;++j) { before.registers[j]=0xF0000000ULL+j;before.vectors[j]={~uint64_t(j),0xABCDEF0000000000ULL+j}; }
        cpu.reset();cpu.restoreContext(before);ram.write32(0,opcode);
        const bool executed=cpu.step();const auto after=cpu.captureContext();
        auto wanted=before;wanted.pc=4;wanted.vectors[opcode&31]=expected;
        if(!executed || after.pc!=wanted.pc || after.vectors!=wanted.vectors || after.registers!=wanted.registers ||
           after.pstate!=wanted.pstate || after.fpcr!=wanted.fpcr || after.fpsr!=wanted.fpsr ||
           after.sp!=wanted.sp || after.tpidrEl0!=wanted.tpidrEl0 || after.tpidrroEl0!=wanted.tpidrroEl0) ++failures;
    }
    std::cout<<"MOVI independent cases="<<count<<" failures="<<failures<<'\n';return failures?1:0;
}
