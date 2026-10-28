#include "cpu.hpp"
#include "memory.hpp"

bool CPU::executeLoadStore(uint32_t instruction)
{
    const uint32_t rt = instruction & 31;
    auto fail = [&]() { lastError = "Unsupported LDR/STR encoding or access"; return false; };
    if ((instruction & 0x3F000000) == 0x18000000) // LDR literal W/X or LDRSW
    {
        const unsigned opc = instruction >> 30;
        if (opc == 3) return fail(); // PRFM, not a data load
        int64_t immediate = (instruction >> 5) & 0x7FFFF;
        if (immediate & 0x40000) immediate -= 0x80000;
        const uint64_t address = currentInstructionPC + static_cast<uint64_t>(immediate * 4);
        uint64_t value = opc == 1 ? memory.read64(address) : memory.read32(address);
        if (memory.hasError()) return false;
        if (opc == 2 && (value & 0x80000000)) value |= 0xFFFFFFFF00000000ULL;
        writeArithmeticRegister(rt,value);
        return true;
    }
    const unsigned size = instruction >> 30, opc = (instruction >> 22) & 3;
    const size_t width = size_t(1) << size;
    const bool load = opc != 0, sign = opc >= 2;
    if ((opc == 2 && size == 3) || (opc == 3 && size >= 2)) return fail();
    const uint32_t rn = (instruction >> 5) & 31;
    const uint64_t base = rn == 31 ? sp : readArithmeticRegister(rn);
    if (rn == 31 && (base & 15)) { lastError = "Unaligned LDR/STR stack"; return false; }
    uint64_t offset = 0;
    bool writeback = false, post = false;
    if (instruction & (1U<<24)) offset = uint64_t((instruction >> 10) & 4095) << size;
    else if (instruction & (1U<<21)) // register offset
    {
        if (((instruction >> 10) & 3) != 2) return fail();
        const unsigned option = (instruction >> 13) & 7;
        if ((option & 2) == 0) return fail();
        offset = readArithmeticRegister((instruction >> 16) & 31);
        if ((option & 1) == 0)
        {
            offset = uint32_t(offset);
            if ((option & 4) && (offset & 0x80000000)) offset |= 0xFFFFFFFF00000000ULL;
        }
        if (instruction & (1U<<12)) offset <<= size;
    }
    else
    {
        const unsigned mode = (instruction >> 10) & 3;
        if (mode == 2) return fail(); // unprivileged LDTR/STTR not modeled
        int64_t immediate = (instruction >> 12) & 511;
        if (immediate & 256) immediate -= 512;
        offset = static_cast<uint64_t>(immediate);
        post = mode == 1;
        writeback = mode != 0;
    }
    if (writeback && rn != 31 && rn == rt) return fail();
    // Local flat-memory policy rejects numeric address wraparound, as pairs do.
    if ((offset >> 63) ? base < (uint64_t(0)-offset) : base > UINT64_MAX-offset) return fail();
    const uint64_t address = post ? base : base + offset;
    if (!memory.isValidRange(address,width)) return fail();
    if (load)
    {
        uint64_t value = width == 8 ? memory.read64(address) : width == 4 ? memory.read32(address) :
                         width == 2 ? memory.read16(address) : memory.read8(address);
        if (memory.hasError()) return false;
        if (sign)
        {
            const unsigned bits = unsigned(width * 8);
            if (value & (uint64_t(1) << (bits-1))) value |= UINT64_MAX << bits;
            if (opc == 3) value = uint32_t(value);
        }
        writeArithmeticRegister(rt,value);
    }
    else
    {
        const uint64_t value = readArithmeticRegister(rt);
        if (width == 8) memory.write64(address,value);
        else if (width == 4) memory.write32(address,uint32_t(value));
        else if (width == 2) memory.write16(address,uint16_t(value));
        else memory.write8(address,uint8_t(value));
        if (memory.hasError()) return false;
    }
    if (writeback)
    {
        if (rn == 31) sp = base+offset; else writeArithmeticRegister(rn,base+offset);
    }
    return true;
}
