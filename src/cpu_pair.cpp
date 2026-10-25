#include "cpu.hpp"
#include "memory.hpp"

bool CPU::executeRegisterPair(uint32_t instruction)
{
    const uint32_t opc = instruction >> 30;
    const uint32_t mode = (instruction >> 23) & 3;
    const bool load = ((instruction >> 22) & 1) != 0;
    const bool writeback = mode == 1 || mode == 3;
    const uint32_t rt = instruction & 31;
    const uint32_t rn = (instruction >> 5) & 31;
    const uint32_t rt2 = (instruction >> 10) & 31;
    const bool signedWords = opc == 1 && load;
    // Integer W/X pairs and LDPSW; non-temporal/reserved encodings are rejected.
    if ((opc != 0 && opc != 2 && !signedWords) || mode == 0)
    {
        lastError = "Variante LDP/STP non prise en charge";
        return false;
    }
    // Reject constrained-unpredictable overlaps rather than inventing values.
    if ((load && rt == rt2) || (writeback && rn != 31 && (rn == rt || rn == rt2)))
    {
        lastError = "Chevauchement de registres LDP/STP non pris en charge";
        return false;
    }
    const uint64_t base = rn == 31 ? sp : readArithmeticRegister(rn);
    if (rn == 31 && (base & 15) != 0)
    {
        lastError = "Pile LDP/STP non alignee sur 16 octets";
        return false;
    }
    const size_t width = opc == 2 ? 8 : 4;
    const uint32_t imm7 = (instruction >> 15) & 127;
    const int64_t offset = (imm7 < 64 ? static_cast<int64_t>(imm7) : static_cast<int64_t>(imm7) - 128) * static_cast<int64_t>(width);
    // The prototype rejects address wraparound before performing any access.
    if ((offset < 0 && base < static_cast<uint64_t>(-offset)) ||
        (offset >= 0 && base > UINT64_MAX - static_cast<uint64_t>(offset)))
    {
        lastError = "Depassement d'adresse LDP/STP";
        return false;
    }
    const uint64_t nextBase = base + static_cast<uint64_t>(offset);
    const uint64_t address = mode == 1 ? base : nextBase;
    // Preflight both transfers. Device pairs are deliberately unsupported so a
    // failure cannot leave the first half of a pair written to a peripheral.
    if (!memory.validateRamAccess(address, width * 2, load ? Memory::Read : Memory::Write))
    {
        lastError = "Paire LDP/STP hors RAM ou traversant une region MMIO";
        return false;
    }
    if (load)
    {
        uint64_t first = width == 8 ? memory.read64(address) : memory.read32(address);
        uint64_t second = width == 8 ? memory.read64(address + width) : memory.read32(address + width);
        if (memory.hasError()) return false;
        if (signedWords) {
            if (first & 0x80000000ULL) first |= 0xFFFFFFFF00000000ULL;
            if (second & 0x80000000ULL) second |= 0xFFFFFFFF00000000ULL;
        }
        writeArithmeticRegister(rt, first);
        writeArithmeticRegister(rt2, second);
    }
    else
    {
        const uint64_t first = readArithmeticRegister(rt);
        const uint64_t second = readArithmeticRegister(rt2);
        if (width == 8)
        {
            memory.write64(address, first);
            if (memory.hasError()) return false;
            memory.write64(address + width, second);
        }
        else
        {
            memory.write32(address, static_cast<uint32_t>(first));
            if (memory.hasError()) return false;
            memory.write32(address + width, static_cast<uint32_t>(second));
        }
    }
    if (memory.hasError()) return false;
    if (writeback)
    {
        if (rn == 31) sp = nextBase;
        else writeArithmeticRegister(rn, nextBase);
    }
    return true;
}
