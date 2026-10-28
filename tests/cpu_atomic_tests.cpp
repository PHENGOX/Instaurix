#include "cpu.hpp"
#include "memory.hpp"
#include "test_device.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<uint8_t> code((std::istreambuf_iterator<char>(input)), {});
    if (code.size() != 206 * 4) { std::cerr << "Invalid assembler fixture\n"; return 2; }
    Memory ram(0x5000); Bus bus(ram); CPU cpu(bus);
    ram.writeBytes(0, code.data(), code.size());
    unsigned failures = 0, checks = 0;
    auto check = [&](bool ok, const char* name) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    auto run = [&](size_t index) { cpu.setPC(index * 4); return cpu.step(); };
    constexpr uint64_t a = 0x88776655FFEEDDCCULL, b = 0x123456789ABCDEF0ULL;
    auto setup = [&]() {
        cpu.reset(); cpu.writeRegister(2, 0x2000); cpu.writeRegister(1,b);
        cpu.writeRegister(5,~b); cpu.writeRegister(3,UINT64_MAX);
        cpu.setPSTATE(0xF0000000); ram.write64(0x2000,a); ram.write64(0x2008,~a);
    };
    auto maskFor = [](unsigned width) { return UINT64_MAX >> (64 - width * 8); };
    {
        Memory cold(0x4000);Bus shared(cold);CPU first(shared),second(shared);cold.writeBytes(0,code.data(),code.size());cold.setPermissions(0x2000,4096,Memory::All);
        first.writeRegister(2,0x2000);second.writeRegister(2,0x2010);first.writeRegister(1,11);second.writeRegister(1,22);first.setPC(12*4);second.setPC(12*4);
        check(first.step() && second.step(),"two CPUs reserve cold distinct granules in the same page");first.setPC(14*4);second.setPC(14*4);
        check(first.step() && !first.readRegister(3) && second.step() && !second.readRegister(3) && cold.read64(0x2000)==11 && cold.read64(0x2010)==22,"first tracking-row allocation causes no false sharing between exclusive monitors");
        cold.reset();cold.writeBytes(0,code.data(),code.size());first.reset();first.writeRegister(2,0x2000);first.writeRegister(1,33);first.setPC(12*4);check(first.step(),"reserve cold page after reset epoch changes");cold.write8(0x2010,0);first.setPC(14*4);check(first.step() && !first.readRegister(3) && cold.read64(0x2000)==33,"first metadata allocation after reset preserves other granules' exclusive reservations");
    }
    // Four sizes, plain/acquire loads and plain/release stores.
    for (unsigned size = 0; size < 4; ++size)
    for (unsigned acquire = 0; acquire < 2; ++acquire)
    for (unsigned release = 0; release < 2; ++release)
    {
        setup(); const unsigned width = 1U << size; const uint64_t mask = maskFor(width);
        check(run(size*4+acquire) && cpu.readRegister(0) == (a & mask), "exclusive load/zero extension");
        check(run(size*4+2+release) && cpu.readRegister(3) == 0 &&
            ram.read64(0x2000) == ((a & ~mask) | (b & mask)), "exclusive store succeeds, width preserved");
        check(run(size*4+2+release) && cpu.readRegister(3) == 1, "store consumes reservation");
        check(cpu.getPSTATE() == 0xF0000000, "exclusive flags unchanged");
    }
    for (unsigned size = 0; size < 2; ++size)
    for (unsigned acquire = 0; acquire < 2; ++acquire)
    for (unsigned release = 0; release < 2; ++release)
    {
        setup(); const unsigned width = 4U << size; const uint64_t mask = maskFor(width);
        if (!size) ram.write32(0x2004,uint32_t(~a));
        check(run(16+size*4+acquire) && cpu.readRegister(0) == (a & mask) &&
            cpu.readRegister(4) == (~a & mask), "exclusive pair loads both elements");
        check(run(18+size*4+release) && cpu.readRegister(3) == 0, "exclusive pair commits");
        check((ram.read64(0x2000) & mask) == (b & mask) &&
            ((size ? ram.read64(0x2008) : ram.read32(0x2004)) == (~b & mask)), "pair store contents");
    }
    for (unsigned size = 0; size < 4; ++size)
    {
        setup(); const uint64_t mask = maskFor(1U << size);
        check(run(24+size*2) && cpu.readRegister(0) == (a & mask), "LDAR sizes");
        check(run(25+size*2) && ram.read64(0x2000) == ((a & ~mask) | (b & mask)), "STLR sizes");
        check(run(size*4+2) && cpu.readRegister(3) == 1, "LDAR does not reserve");
    }
    // Mathematical reference over boundary values, independent of decoder masks.
    const uint64_t values[] = {0,1,UINT64_MAX,UINT64_MAX/2,0x8000000080008080ULL,a,b};
    for (unsigned size = 0; size < 4; ++size)
    for (unsigned order = 0; order < 4; ++order)
    for (unsigned op = 0; op < 10; ++op)
    for (uint64_t left : values)
    for (uint64_t right : values)
    {
        setup(); const unsigned bits = 8U << size; const uint64_t mask = maskFor(1U << size);
        left &= mask; right &= mask;
        ram.write64(0x2000,(a & ~mask) | left);
        cpu.writeRegister(1,right); cpu.writeRegister(0, op == 9 ? right : UINT64_MAX);
        uint64_t expected = left;
        // Signed comparison via sign and magnitude, without overflowing signed C++.
        const bool leftNegative = (left >> (bits-1)) != 0, rightNegative = (right >> (bits-1)) != 0;
        const bool less = leftNegative != rightNegative ? leftNegative : left < right;
        switch (op)
        {
        case 0: expected = (left+right)&mask; break;
        case 1: expected = left & (~right & mask); break;
        case 2: expected = left ^ right; break;
        case 3: expected = left | right; break;
        case 4: expected = less ? right : left; break;
        case 5: expected = less ? left : right; break;
        case 6: expected = std::max(left,right); break;
        case 7: expected = std::min(left,right); break;
        case 8: expected = right; break;
        case 9: expected = left == right ? right : left; break;
        }
        const size_t index = 32+size*40+order*10+op;
        check(run(index) && cpu.readRegister(0) == left &&
            ram.read64(0x2000) == ((a & ~mask) | expected) &&
            cpu.readRegister(2) == 0x2000 && cpu.getPSTATE() == 0xF0000000 && cpu.getPC() == index*4+4,
            "LSE/CAS width, order, old value, boundary result, flags and PC");
        if (op == 9)
        {
            // Matching comparison with a DIFFERENT replacement operand.
            cpu.writeRegister(0,left); cpu.writeRegister(1,~left);
            check(run(index) && cpu.readRegister(0) == left &&
                (ram.read64(0x2000)&mask) == (~left & mask), "CAS matching replacement");
        }
    }
    // Monitor lifetime and interference, including same-value and ABA writes.
    setup(); check(run(12), "reserve X"); ram.write64(0x2000,a);
    check(run(14) && cpu.readRegister(3) == 1 && ram.read64(0x2000) == a, "same-value write invalidates");
    setup(); run(12); ram.write64(0x2000,b); ram.write64(0x2000,a);
    check(run(14) && cpu.readRegister(3) == 1, "ABA invalidates");
    setup(); run(12); ram.write8(0x200F,0);
    check(run(14) && cpu.readRegister(3) == 1, "write elsewhere in granule invalidates");
    setup(); run(12); ram.write64(0x2010,0);
    check(run(14) && cpu.readRegister(3) == 0, "another granule does not invalidate");
    for (size_t clear : {192U,193U})
    { setup(); run(12); check(run(clear) && run(14) && cpu.readRegister(3) == 1, "CLREX clears"); }
    setup(); run(12); auto context = cpu.captureContext(); cpu.restoreContext(context);
    check(run(14) && cpu.readRegister(3) == 1, "context restore clears monitor");
    setup(); run(12); cpu.setPC(197*4);
    check(cpu.stepDetailed().status == CPU::StepStatus::SystemCall && run(14) && cpu.readRegister(3) == 1,
        "SVC clears monitor");
    setup(); run(12); cpu.reset(); cpu.writeRegister(2,0x2000);
    check(run(14) && cpu.readRegister(3) == 1, "reset clears monitor");
    setup(); run(12); cpu.writeRegister(2,0x2010);
    check(run(14) && cpu.readRegister(3) == 1, "wrong address fails");
    setup(); run(12);
    check(run(10) && cpu.readRegister(3) == 1, "wrong size fails");
    setup(); run(12); const uint8_t same[] = {0xCC}; ram.writeBytes(0x2000,same,1);
    check(run(14) && cpu.readRegister(3) == 1, "bulk writes invalidate");
    setup(); run(12); ram.setPermissions(0x2000,0x1000,Memory::All);
    check(run(14) && cpu.readRegister(3) == 1, "mapping change invalidates");
    setup(); run(12); CPU other(bus); other.writeRegister(2,0x2000); other.writeRegister(1,7);
    other.setPC(31*4); check(other.step(), "second CPU writes through STLR");
    check(run(14) && cpu.readRegister(3) == 1 && ram.read64(0x2000) == 7, "shared RAM monitor interference");
    setup(); run(12);
    for (size_t barrier : {194U,195U,196U}) check(run(barrier), "DMB DSB ISB execute");
    check(run(14) && cpu.readRegister(3) == 0, "barriers preserve monitor");
    setup(); cpu.setSP(0x2000); cpu.writeRegister(31,123);
    check(run(198) && run(199) && ram.read64(0x2000) == b && cpu.readRegister(31) == 123,
        "SP base and discarded exclusive destinations");
    setup(); check(run(200) && ram.read64(0x2000) == a+b, "LSE ZR destination still writes");
    setup(); ram.write64(0x2000,0);
    check(run(201) && ram.read64(0x2000) == b, "CAS ZR comparison");
    // Run a real retry loop with an intervening write forcing the first attempt to fail.
    setup(); cpu.setPC(202*4); check(cpu.step(), "retry loop loads");
    ram.write64(0x2000,41);
    for (unsigned i=0; i<7; ++i) check(cpu.step(), "retry loop executes");
    check(cpu.getPC() == 206*4 && ram.read64(0x2000) == 42 && cpu.readRegister(3) == 0,
        "retry loop recovers from contention");
    TestDevice device; check(bus.mapDevice(0x4000,device.size(),device,"atomic-test"), "map MMIO fixture");
    for (size_t index : {12U,14U,20U,22U,30U,31U,152U,161U})
    for (uint64_t address : std::array<uint64_t,4>{0x2001,0x5000,0x4000,UINT64_MAX})
    {
        setup(); cpu.writeRegister(2,address); cpu.writeRegister(0,77); cpu.writeRegister(4,88);
        const auto before = cpu.captureContext();
        check(!run(index) && cpu.hasError() && cpu.getPC() == index*4 &&
            cpu.readRegister(0) == before.registers[0] && cpu.readRegister(3) == before.registers[3] &&
            cpu.readRegister(4) == before.registers[4] && ram.read64(0x2000) == a,
            "fault preserves registers and RAM");
    }
    for (size_t index : {14U,22U,31U,152U,161U})
    {
        setup(); ram.setPermissions(0x2000,0x1000,Memory::Read);
        check(!run(index) && cpu.readRegister(3) == UINT64_MAX && ram.read64(0x2000) == a,
            "write permission fault has no partial effects");
        ram.setPermissions(0x2000,0x1000,Memory::All);
    }
    // Data load permissions, SP alignment and invalid encodings must be checked
    // before changing destinations, flags or memory.
    for (size_t index : {12U,20U,30U,152U,161U})
    {
        setup(); ram.setPermissions(0x2000,0x1000,Memory::Write);
        check(!run(index) && cpu.getPC() == index*4 && cpu.readRegister(3) == UINT64_MAX,
            "read permission checked");
        ram.setPermissions(0x2000,0x1000,Memory::All);
    }
    setup(); cpu.setSP(0x2008);
    check(!run(198), "SP must be 16-byte aligned even for 8-byte load");
    auto word = [&](size_t i) {
        return uint32_t(code[i*4]) | (uint32_t(code[i*4+1])<<8) |
            (uint32_t(code[i*4+2])<<16) | (uint32_t(code[i*4+3])<<24);
    };
    const uint32_t invalid[] = {
        word(12) & ~(31U<<16), // LDXR requires Rs=31
        word(12) & ~(31U<<10), // scalar requires Rt2=31
        word(14) & ~(31U<<16) | (1U<<16), // status overlaps source
        word(14) & ~(31U<<16) | (2U<<16), // status overlaps base
        word(20) & ~(31U<<10), // duplicate pair destinations
        word(20) & ~(3U<<30), // CASP is not a supported exclusive pair
        word(30) & ~(1U<<15), // not LDAR
        word(152) | (15U<<12), // reserved LSE operation
        word(196) & ~(15U<<8), // reserved ISB option
        word(195) | (3U<<5) // reserved barrier kind
    };
    for (uint32_t instruction : invalid)
    {
        setup(); ram.write32(0x1000,instruction); cpu.setPC(0x1000);
        const auto before = cpu.captureContext();
        check(!cpu.step() && cpu.hasError() && cpu.captureContext().registers == before.registers &&
            cpu.getPC() == 0x1000 && cpu.getPSTATE() == before.pstate && ram.read64(0x2000) == a,
            "reserved/unsupported encoding has no effects");
    }
    setup(); run(12); cpu.setPC(1); check(!cpu.step(), "fetch alignment fault");
    check(run(14) && cpu.readRegister(3) == 1, "fetch fault clears reservation");
    setup(); run(12); ram.reset(); ram.writeBytes(0,code.data(),code.size());
    check(run(14) && cpu.readRegister(3) == 1, "RAM reset invalidates reservation");
    std::cout << checks << " atomic checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
