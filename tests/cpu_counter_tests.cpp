#include "machine.hpp"
#include <iostream>
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<text<<'\n'; } };
    Memory ram(4096);Bus bus(ram);CPU cpu(bus);
    ram.write32(0,0xD53BE020); // mrs x0,cntpct_el0 -- also emitted by original libnx wait.c
    check(!cpu.step() && cpu.getPC()==0,"unconfigured counter is an explicit CPU error");
    cpu.reset();cpu.setPSTATE(0xF0000000);cpu.setSystemCounter(0xFEDCBA9876543210ULL,19200000);
    check(cpu.step() && cpu.readRegister(0)==0xFEDCBA9876543210ULL && cpu.getPSTATE()==0xF0000000,"physical counter read preserves all bits and flags");
    ram.write32(4,0xD53BE001);check(cpu.step() && cpu.readRegister(1)==19200000,"frequency register reads injected model frequency");
    ram.write32(8,0xD53BE03F);const auto before=cpu.captureContext();check(cpu.step() && cpu.captureContext().registers==before.registers,"counter read to XZR discards result");
    cpu.reset();check(!cpu.step(),"CPU reset retires the prior clock binding");
    Machine m(false,true,true);check(m.load(argv[1]) && m.advanceTime(625),"virtual machine clock fixture");
    m.ram.setPermissions(0x10000,4096,Memory::All);
    m.ram.write32(0x10000,0xD53BE020);m.ram.write32(0x10004,0xD53BE001);
    m.ram.write32(0x10008,0xD40003C1); // svc #0x1e
    m.ram.write32(0x1000C,0xD53BE022);
    check(m.run(1).stop==Machine::Stop::Budget && m.cpu.readRegister(0)==12,"CNTPCT uses local virtual time");
    check(m.run(1).stop==Machine::Stop::Budget && m.cpu.readRegister(1)==19200000,"CNTFRQ matches reference HLE frequency");
    check(m.run(1).stop==Machine::Stop::Budget && m.cpu.readRegister(0)==12,"MRS and SVC GetSystemTick share the same clock");
    check(m.advanceTime(625) && m.run(1).stop==Machine::Stop::Budget && m.cpu.readRegister(2)==24,"counter follows host virtual-time advancement");
    std::cout<<"Counter failures="<<failures<<'\n';return failures?1:0;
}
