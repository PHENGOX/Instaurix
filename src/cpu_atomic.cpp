#include "cpu.hpp"
#include "memory.hpp"

// Instructions execute serially on the host. These operations are indivisible
// within that execution model; this is not a parallel host-thread memory model.
bool CPU::executeAtomic(uint32_t ins)
{
    auto fail = [&]() { lastError = "Encodage ou acces atomique non pris en charge"; return false; };
    if ((ins & 0xFFFFF0FF) == 0xD503305F) { exclusiveValid = false; return true; }
    if ((ins & 0xFFFFF09F) == 0xD503309F)
    {
        const unsigned kind = (ins >> 5) & 3, option = (ins >> 8) & 15;
        if (kind == 3 || (kind == 2 ? option != 15 : (option & 3) == 0)) return fail();
        return true; // DSB/DMB/ISB: no pending accesses or translated instruction cache.
    }
    const unsigned size = ins >> 30, width = 1U << size;
    const unsigned rn = (ins >> 5) & 31, rt = ins & 31, rs = (ins >> 16) & 31;
    const unsigned rt2 = (ins >> 10) & 31;
    const bool lse = (ins & 0x3F200C00) == 0x38200000;
    const bool ordered = !lse && (ins & (1U << 23));
    const bool pairBit = ins & (1U << 21);
    const bool cas = ordered && pairBit;
    const bool pair = !lse && !ordered && pairBit;
    const bool load = ins & (1U << 22);
    if (!lse)
    {
        if (cas) { if (rt2 != 31) return fail(); }
        else if (ordered)
        {
            if (rs != 31 || rt2 != 31 || !(ins & (1U << 15))) return fail();
        }
        else
        {
            if ((pair && size < 2) || (!pair && rt2 != 31) || (load && rs != 31)) return fail();
            if (load && pair && rt == rt2) return fail();
            if (!load && (rs == rt || (pair && rs == rt2) || (rn != 31 && rs == rn)))
                return fail(); // Constrained-unpredictable overlaps are rejected locally.
        }
    }
    const unsigned operation = (ins >> 12) & 15;
    if (lse && operation > 8) return fail();
    const uint64_t address = rn == 31 ? sp : readArithmeticRegister(rn);
    const unsigned total = pair ? width * 2 : width;
    if ((rn == 31 && (sp & 15)) || (address & (total - 1))) return fail();
    const bool reads = lse || cas || load;
    const bool writes = lse || cas || !load;
    const uint8_t permissions = uint8_t((reads ? Memory::Read : 0) | (writes ? Memory::Write : 0));
    if (!memory.validateRamAccess(address, total, permissions)) return false;
    auto read = [&](uint64_t at) -> uint64_t {
        return width == 8 ? memory.read64(at) : width == 4 ? memory.read32(at) :
               width == 2 ? memory.read16(at) : memory.read8(at);
    };
    auto write = [&](uint64_t at, uint64_t value) {
        uint8_t bytes[8]{};
        for (unsigned i = 0; i < width; ++i) bytes[i] = uint8_t(value >> (i * 8));
        memory.getMemory().writeBytes(at, bytes, width);
    };
    const uint64_t mask = UINT64_MAX >> (64 - width * 8);
    if (lse || cas)
    {
        const uint64_t old = read(address), operand = readArithmeticRegister(rs) & mask;
        if (memory.hasError()) return false;
        if (cas)
        {
            if (old == operand) write(address, readArithmeticRegister(rt));
            if (memory.hasError()) return false;
            writeArithmeticRegister(rs, old);
            return true;
        }
        uint64_t value = 0;
        const uint64_t sign = uint64_t(1) << (width * 8 - 1);
        switch (operation)
        {
        case 0: value = old + operand; break; // LDADD
        case 1: value = old & ~operand; break; // LDCLR
        case 2: value = old ^ operand; break; // LDEOR
        case 3: value = old | operand; break; // LDSET
        case 4: value = (old ^ sign) > (operand ^ sign) ? old : operand; break; // LDSMAX
        case 5: value = (old ^ sign) < (operand ^ sign) ? old : operand; break; // LDSMIN
        case 6: value = old > operand ? old : operand; break; // LDUMAX
        case 7: value = old < operand ? old : operand; break; // LDUMIN
        case 8: value = operand; break; // SWP
        }
        write(address, value);
        if (memory.hasError()) return false;
        writeArithmeticRegister(rt, old);
        return true;
    }
    if (load)
    {
        const uint64_t first = read(address), second = pair ? read(address + width) : 0;
        if (memory.hasError()) return false;
        writeArithmeticRegister(rt, first);
        if (pair) writeArithmeticRegister(rt2, second);
        if (!ordered)
        {
            exclusiveAddress = address; exclusiveSize = total;
            exclusiveVersion = memory.getMemory().reservationVersion(address);
            exclusiveValid = true;
        }
        return true;
    }
    const bool success = ordered || (exclusiveValid && exclusiveAddress == address && exclusiveSize == total &&
        exclusiveVersion == memory.getMemory().reservationVersion(address));
    if (!ordered) exclusiveValid = false;
    if (success)
    {
        const uint64_t first = readArithmeticRegister(rt), second = readArithmeticRegister(rt2);
        // Preallocate and commit the whole transfer, including a pair, together.
        uint8_t bytes[16]{};
        for (unsigned i = 0; i < width; ++i)
        {
            bytes[i] = uint8_t(first >> (i * 8));
            if (pair) bytes[width + i] = uint8_t(second >> (i * 8));
        }
        if (!memory.getMemory().writeBytes(address, bytes, total)) return false;
    }
    if (!ordered) writeArithmeticRegister(rs, success ? 0 : 1);
    return true;
}
