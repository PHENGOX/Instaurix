#include "cpu.hpp"
#include "memory.hpp"
#include "test_device.hpp"
#include <iostream>
int main(){
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Memory ram(0x4000);Bus bus(ram);CPU cpu(bus);TestDevice device;
    bus.mapDevice(0x8000,device.size(),device,"cache-test");
    ram.write32(0,0xD53B0020);cpu.setPSTATE(0xF0000000);
    check(cpu.step() && cpu.readRegister(0)==0x8424C004 && cpu.getPSTATE()==0xF0000000,"CTR_EL0 exposes the local geometry without altering flags");
    const auto ctr=cpu.readRegister(0);check((4ULL<<((ctr>>16)&15))==64 && (4ULL<<((ctr>>20)&15))==16,"cache line and exclusive granule sizes agree with the local memory model");
    for(uint64_t address:{uint64_t(0x1000),uint64_t(0x103f),uint64_t(0x3fff)}){
        cpu.reset();cpu.writeRegister(8,address);cpu.setPSTATE(0xA0000000);ram.write32(0,0xD50B7E28);
        ram.write64(0x1000,0x12345678);const auto version=ram.reservationVersion(0x1000);
        check(cpu.step() && cpu.readRegister(8)==address && cpu.getPSTATE()==0xA0000000,"DC CIVAC accepts unaligned accessible RAM and preserves state");
        check(ram.read64(0x1000)==0x12345678 && ram.reservationVersion(0x1000)==version,"coherent maintenance neither writes RAM nor invalidates reservations");
    }
    ram.setPermissions(0x1000,4096,Memory::Read);cpu.reset();cpu.writeRegister(8,0x1001);check(cpu.step(),"maintenance accepts readable non-writable RAM");
    for(uint64_t address:{uint64_t(0x1000),uint64_t(0x4000),uint64_t(0x8000),UINT64_MAX}){
        ram.setPermissions(0x1000,4096,Memory::None);cpu.reset();cpu.writeRegister(8,address);cpu.setPSTATE(0xF0000000);
        check(!cpu.step() && cpu.getPC()==0 && cpu.readRegister(8)==address && cpu.getPSTATE()==0xF0000000,"inaccessible, MMIO and overflowing addresses fail without architectural effects");
    }
    ram.setPermissions(0x1000,4096,Memory::Read|Memory::Write);cpu.reset();ram.write32(0,0xD50B7428);
    check(!cpu.step(),"DC ZVA remains unsupported rather than silently dropping its zeroing effects");
    return failures?1:0;
}
