#include "machine.hpp"
#include <iostream>
#include "programs/cpu_integer/work.c"
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    Machine machine(true);
    if(!machine.load(argv[1])) { std::cerr<<machine.error()<<'\n'; return 1; }
    const auto expected64=work64(seed_a,seed_b);
    const auto expected32=work32(seed_c,seed_d);
    const auto result=machine.run(5000);
    const bool ok=result.stop==Machine::Stop::Exited && result.exitCode==0 &&
        machine.cpu.readRegister(2)==expected64 && machine.cpu.readRegister(3)==expected32 &&
        machine.ram.read64(0x12000)==expected64 && machine.ram.read32(0x12008)==expected32 &&
        machine.cpu.getSP()==Machine::RAM_SIZE && machine.cpu.readRegister(29)==0 && machine.cpu.readRegister(30)==0;
    std::cout<<"Native/ARM64 C comparison: instructions="<<result.instructions<<" value64="<<std::hex<<machine.cpu.readRegister(2)
        <<" expected64="<<expected64<<" value32="<<machine.cpu.readRegister(3)<<" expected32="<<expected32<<std::dec<<'\n';
    if(!ok) std::cerr<<machine.error()<<'\n';
    return ok ? 0 : 1;
}
