#include "cpu.hpp"
#include <bit>
#include <cstdint>

namespace {
uint64_t maskFor(unsigned bits) { return bits == 64 ? UINT64_MAX : (uint64_t(1) << bits) - 1; }
uint64_t shifted(uint64_t value, unsigned type, unsigned amount, unsigned width)
{
    const uint64_t mask = maskFor(width);
    value &= mask;
    if (amount == 0) return value;
    switch (type)
    {
    case 0: return (value << amount) & mask;
    case 1: return value >> amount;
    case 2: return (value >> amount) | ((value >> (width-1)) ? (mask ^ (mask >> amount)) : 0);
    default: return ((value >> amount) | (value << (width-amount))) & mask;
    }
}
uint64_t extended(uint64_t value, unsigned option)
{
    const unsigned bits = 8U << (option & 3);
    const uint64_t mask = maskFor(bits);
    value &= mask;
    if ((option & 4) && bits < 64 && (value & (uint64_t(1) << (bits-1)))) value |= ~mask;
    return value;
}
uint64_t unsignedHigh(uint64_t a, uint64_t b)
{
    const uint64_t a0 = uint32_t(a), a1 = a >> 32, b0 = uint32_t(b), b1 = b >> 32;
    const uint64_t p00 = a0*b0, p01 = a0*b1, p10 = a1*b0;
    const uint64_t middle = (p00 >> 32) + uint32_t(p01) + uint32_t(p10);
    return a1*b1 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
}
}

bool CPU::conditionHolds(uint32_t condition) const
{
    const bool n = (pstate & FLAG_N) != 0, z = (pstate & FLAG_Z) != 0;
    const bool c = (pstate & FLAG_C) != 0, v = (pstate & FLAG_V) != 0;
    bool result = true;
    switch (condition >> 1)
    {
    case 0: result = z; break;
    case 1: result = c; break;
    case 2: result = n; break;
    case 3: result = v; break;
    case 4: result = c && !z; break;
    case 5: result = n == v; break;
    case 6: result = !z && n == v; break;
    default: break; // AL and NV both pass in AArch64 ConditionHolds.
    }
    return (condition & 1) && condition != 15 ? !result : result;
}

uint64_t CPU::addWithCarry(uint64_t a, uint64_t b, bool carry, unsigned width, bool flags)
{
    const uint64_t mask = maskFor(width), sign = uint64_t(1) << (width-1);
    a &= mask; b &= mask;
    const uint64_t sum = a+b;
    const uint64_t result = (sum + uint64_t(carry)) & mask;
    if (flags)
    {
        const bool c = width == 32 ? ((sum+uint64_t(carry)) >> 32) != 0 :
                       (sum < a || (carry && sum == UINT64_MAX));
        const bool v = ((~(a^b) & (a^result)) & sign) != 0;
        pstate = (pstate & ~(FLAG_N|FLAG_Z|FLAG_C|FLAG_V)) |
            ((result & sign) ? FLAG_N : 0) | (result == 0 ? FLAG_Z : 0) |
            (c ? FLAG_C : 0) | (v ? FLAG_V : 0);
    }
    return result;
}

bool CPU::executeInteger(uint32_t instruction)
{
    const unsigned width = (instruction >> 31) ? 64 : 32;
    const uint64_t mask = maskFor(width);
    const uint32_t rd = instruction & 31, rn = (instruction >> 5) & 31, rm = (instruction >> 16) & 31;
    const uint64_t a = readArithmeticRegister(rn) & mask, b = readArithmeticRegister(rm) & mask;
    auto fail = [&]() { lastError = "Encodage entier reserve ou non pris en charge"; return false; };
    auto write = [&](uint64_t value) { writeArithmeticRegister(rd,value & mask); return true; };
    const bool sub = (instruction & 0x40000000) != 0, flags = (instruction & 0x20000000) != 0;

    if ((instruction & 0x1FE00000) == 0x1A400000) // CCMP/CCMN register or imm5
    {
        if (!flags || (instruction & 0x410)) return fail();
        if (conditionHolds((instruction >> 12)&15))
        {
            const uint64_t operand = (instruction & (1U<<11)) ? rm : b;
            addWithCarry(a,sub ? (~operand&mask) : operand,sub,width,true);
        }
        else pstate = (pstate & ~0xF0000000ULL) | (uint64_t(instruction&15)<<28);
        return true;
    }

    if ((instruction & 0x1F000000) == 0x11000000) // ADD/SUB immediate
    {
        if (instruction & 0x00800000) return fail();
        const uint64_t left = rn == 31 ? (sp & mask) : a;
        const uint64_t immediate = uint64_t((instruction >> 10) & 4095) << ((instruction & (1U<<22)) ? 12 : 0);
        const uint64_t value = addWithCarry(left,sub ? (~immediate & mask) : immediate,sub,width,flags);
        if (rd == 31 && !flags) sp = value; else writeArithmeticRegister(rd,value);
        return true;
    }
    if ((instruction & 0x1F000000) == 0x0B000000) // ADD/SUB shifted or extended
    {
        uint64_t left = a, right;
        const bool extend = (instruction & (1U<<21)) != 0;
        if (extend)
        {
            const unsigned amount = (instruction >> 10) & 7;
            if ((instruction & 0x00C00000) != 0 || amount > 4) return fail();
            left = rn == 31 ? (sp & mask) : a;
            right = (extended(readArithmeticRegister(rm),(instruction >> 13) & 7) << amount) & mask;
        }
        else
        {
            const unsigned amount = (instruction >> 10) & 63, type = (instruction >> 22) & 3;
            if (amount >= width || type == 3) return fail();
            right = shifted(b,type,amount,width);
        }
        const uint64_t value = addWithCarry(left,sub ? (~right & mask) : right,sub,width,flags);
        if (extend && rd == 31 && !flags) sp = value; else writeArithmeticRegister(rd,value);
        return true;
    }
    if ((instruction & 0x1F000000) == 0x0A000000) // AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS
    {
        const unsigned amount = (instruction >> 10) & 63, type = (instruction >> 22) & 3;
        if (amount >= width) return fail();
        uint64_t right = shifted(b,type,amount,width);
        if (instruction & (1U<<21)) right = ~right & mask;
        const unsigned operation = (instruction >> 29) & 3;
        const uint64_t value = operation == 1 ? a|right : operation == 2 ? a^right : a&right;
        if (operation == 3)
            pstate = (pstate & ~(FLAG_N|FLAG_Z|FLAG_C|FLAG_V)) |
                ((value >> (width-1)) ? FLAG_N : 0) | (value == 0 ? FLAG_Z : 0);
        return write(value);
    }
    if ((instruction & 0x1FE0FC00) == 0x1A000000) // ADC/SBC
        return write(addWithCarry(a,sub ? (~b & mask) : b,(pstate & FLAG_C) != 0,width,flags));

    if ((instruction & 0x1FE00000) == 0x1A800000) // Conditional select family
    {
        if (instruction & 0x20000800) return fail();
        const bool condition = conditionHolds((instruction >> 12) & 15);
        uint64_t value = b;
        if (!condition)
        {
            if (sub) value = ~value;
            if (instruction & (1U<<10)) ++value;
        }
        return write(condition ? a : value);
    }
    const uint32_t two = instruction & 0x7FE0FC00;
    if (two == 0x1AC00800 || two == 0x1AC00C00) // UDIV/SDIV
    {
        if (b == 0) return write(0);
        if (two == 0x1AC00800) return write(a/b);
        if (a == (uint64_t(1) << (width-1)) && b == mask) return write(a);
        if (width == 32)
            return write(static_cast<uint32_t>(std::bit_cast<int32_t>(uint32_t(a)) / std::bit_cast<int32_t>(uint32_t(b))));
        return write(static_cast<uint64_t>(std::bit_cast<int64_t>(a) / std::bit_cast<int64_t>(b)));
    }
    if (two >= 0x1AC02000 && two <= 0x1AC02C00 && (two & 0x3FF) == 0)
        return write(shifted(a,(two-0x1AC02000) >> 10,unsigned(b & (width-1)),width));

    const uint32_t one = instruction & 0x7FFFFC00;
    if (one >= 0x5AC00000 && one <= 0x5AC01400 && (one & 0x3FF) == 0)
    {
        const unsigned op = (one-0x5AC00000) >> 10;
        uint64_t value = 0;
        if (op == 0) for (unsigned i=0;i<width;++i) value |= ((a >> i)&1) << (width-1-i);
        else if (op <= 3)
        {
            const unsigned group = 1U << op;
            if (group > width/8) return fail();
            for (unsigned i=0;i<width/8;++i)
                value |= ((a >> (i*8)) & 255) << ((i/group*group + group-1-i%group)*8);
        }
        else if (op == 4)
            value = width == 64 ? std::countl_zero(a) : std::countl_zero(uint32_t(a));
        else
        {
            const uint64_t sign = a >> (width-1);
            for (int i=int(width)-2;i>=0 && ((a >> i)&1)==sign;--i) ++value;
        }
        return write(value);
    }
    if ((instruction & 0x1F000000) == 0x1B000000) // multiply families
    {
        if (instruction & 0x60000000) return fail();
        const unsigned type = (instruction >> 21) & 7, ra = (instruction >> 10) & 31;
        const bool subtract = (instruction & (1U<<15)) != 0;
        uint64_t product;
        if (type == 0) product = a*b;
        else if ((type == 1 || type == 5) && width == 64)
        {
            if (type == 5) product = uint64_t(uint32_t(a))*uint32_t(b);
            else product = static_cast<uint64_t>(int64_t(std::bit_cast<int32_t>(uint32_t(a))) * int64_t(std::bit_cast<int32_t>(uint32_t(b))));
        }
        else if ((type == 2 || type == 6) && width == 64 && ra == 31 && !subtract)
        {
            product = unsignedHigh(a,b);
            if (type == 2) product -= ((a >> 63) ? b : 0) + ((b >> 63) ? a : 0);
            return write(product);
        }
        else return fail();
        const uint64_t accumulator = readArithmeticRegister(ra);
        return write(subtract ? accumulator-product : accumulator+product);
    }
    if ((instruction & 0x1F800000) == 0x13000000) // SBFM/BFM/UBFM and shift/extend aliases
    {
        const unsigned op = (instruction >> 29)&3, r=(instruction >> 16)&63, s=(instruction >> 10)&63;
        if (op == 3 || ((instruction >> 22)&1) != (width == 64) || r >= width || s >= width) return fail();
        const uint64_t wmask = shifted(maskFor(s+1),3,r,width), tmask = maskFor(((s-r)&(width-1))+1);
        const uint64_t rotated = shifted(a,3,r,width);
        uint64_t value = rotated & wmask;
        if (op == 0) value = (value & tmask) | (((a >> s)&1) ? (mask & ~tmask) : 0);
        else if (op == 1)
        {
            const uint64_t old = readArithmeticRegister(rd) & mask;
            value = (old & ~tmask) | (((old & ~wmask) | value) & tmask);
        }
        else value &= tmask;
        return write(value);
    }
    if ((instruction & 0x1F800000) == 0x13800000) // EXTR, including immediate ROR alias
    {
        const unsigned amount = (instruction >> 10)&63;
        if ((instruction & 0x60200000) != 0 || ((instruction >> 22)&1) != (width == 64) || amount >= width) return fail();
        return write(amount == 0 ? b : (b >> amount) | (a << (width-amount)));
    }
    return fail();
}
