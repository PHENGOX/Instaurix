#include "cpu.hpp"
#include "cpu_fp_internal.hpp"
#include <bit>

namespace fp_detail {
bool incrementForRounding(bool negative, unsigned mode, uint64_t kept, bool remainder,
    int halfComparison)
{
    if (!remainder) return false;
    switch (mode) {
    case 0: return halfComparison > 0 || (halfComparison == 0 && (kept&1));
    case 1: return !negative;
    case 2: return negative;
    case 3: return false;
    case 4: return halfComparison >= 0; // ties away from zero
    default: return false;
    }
}

// Integer-only conversion avoids undefined out-of-range casts and differences
// between the host's signed/unsigned conversion instructions.
uint64_t integerToFloat(uint64_t value, unsigned integerBits, bool isUnsigned,
    bool wide, unsigned rounding, uint32_t& status)
{
    const uint64_t integerMask = UINT64_MAX >> (64-integerBits);
    value &= integerMask;
    const bool negative = !isUnsigned && (value & (uint64_t(1) << (integerBits-1)));
    uint64_t magnitude = negative ? (uint64_t(0)-value)&integerMask : value;
    if (!magnitude) return 0;
    const unsigned fractionBits = wide ? 52 : 23, bias = wide ? 1023 : 127;
    unsigned exponent = 63-std::countl_zero(magnitude);
    if (exponent > fractionBits)
    {
        const unsigned shift = exponent-fractionBits;
        const uint64_t remainder = magnitude & ((uint64_t(1)<<shift)-1);
        const uint64_t half = uint64_t(1)<<(shift-1);
        magnitude >>= shift;
        if (incrementForRounding(negative,rounding,magnitude,remainder!=0,
            remainder<half ? -1 : remainder>half ? 1 : 0)) ++magnitude;
        if (remainder) status |= 16;
        if (magnitude == (uint64_t(1)<<(fractionBits+1))) { magnitude >>= 1; ++exponent; }
    }
    else magnitude <<= fractionBits-exponent;
    return (negative ? (wide ? 1ULL<<63 : 1ULL<<31) : 0) |
        (uint64_t(exponent+bias)<<fractionBits) | (magnitude & ((uint64_t(1)<<fractionBits)-1));
}

uint64_t floatToInteger(uint64_t value, bool wide, unsigned integerBits, bool isUnsigned,
    unsigned rounding, uint32_t& status)
{
    const Format f(wide);
    const unsigned fractionBits = wide ? 52 : 23, bias = wide ? 1023 : 127;
    const bool negative = (value&f.sign)!=0;
    const uint64_t limit = isUnsigned ? (UINT64_MAX>>(64-integerBits)) :
        (uint64_t(1)<<(integerBits-1)) - (negative ? 0 : 1);
    auto invalid = [&]() -> uint64_t {
        status |= 1;
        if (f.nan(value) || (negative && isUnsigned)) return 0;
        return negative ? uint64_t(0)-limit : limit;
    };
    if ((value&f.exponent)==f.exponent) return invalid();
    const unsigned encodedExponent = unsigned((value&f.exponent)>>fractionBits);
    const int exponent = int(encodedExponent ? encodedExponent : 1)-int(bias);
    uint64_t magnitude = (value&f.fraction) | (encodedExponent ? uint64_t(1)<<fractionBits : 0);
    if (!magnitude) return 0;
    if (exponent >= 64) return invalid();
    bool inexact = false;
    const int shift = int(fractionBits)-exponent;
    if (shift > 0)
    {
        const uint64_t remainder = shift >= 64 ? magnitude : magnitude & ((uint64_t(1)<<shift)-1);
        const uint64_t half = shift > 64 ? UINT64_MAX : uint64_t(1)<<(shift-1);
        magnitude = shift >= 64 ? 0 : magnitude >> shift;
        inexact = remainder!=0;
        if (incrementForRounding(negative,rounding,magnitude,inexact,
            remainder<half ? -1 : remainder>half ? 1 : 0)) ++magnitude;
    }
    else magnitude <<= unsigned(-shift);
    if (magnitude>limit || (negative && isUnsigned && magnitude)) return invalid();
    if (inexact) status |= 16;
    return negative ? uint64_t(0)-magnitude : magnitude;
}

// Shared scalar/vector arithmetic semantics, including NaN priority and FPSR.
bool evaluateFloat(uint64_t a, uint64_t b, uint64_t c, unsigned operation, bool wide,
    uint32_t control, uint64_t& result, uint32_t& status)
{
    if (control & ~0x03C00000U) return false;
    const Format f(wide);
    const bool fused=operation==5;
    a=prepareFloatInput(a,wide,control,status);
    b=prepareFloatInput(b,wide,control,status);
    if(fused) c=prepareFloatInput(c,wide,control,status);
    if (f.nan(a) || f.nan(b) || (fused && f.nan(c)))
    {
        if (f.signaling(a) || f.signaling(b) || (fused && f.signaling(c))) status |= 1;
        if (fused)
        {
            result=f.signaling(c) ? c : f.signaling(a) ? a : f.signaling(b) ? b :
                f.nan(c) ? c : f.nan(a) ? a : b;
            const bool invalidProduct=((a&~f.sign)==f.exponent && !(b&~f.sign)) ||
                ((b&~f.sign)==f.exponent && !(a&~f.sign));
            if (invalidProduct && f.nan(c) && !f.signaling(c)) { status |= 1; result=f.exponent; }
        }
        else result=f.signaling(a) ? a : f.signaling(b) ? b : f.nan(a) ? a : b;
        if (control & (1U<<25)) result=f.exponent;
        result |= f.quiet;
        return true;
    }
    return softwareArithmetic(a,b,c,operation,wide,control,result,status);
}


}

using namespace fp_detail;

bool CPU::executeFloatingPoint(uint32_t ins)
{
    auto fail = [&]() { lastError = "Instruction ou mode FP/SIMD non pris en charge"; return false; };
    const unsigned rd = ins & 31, rn = (ins >> 5) & 31, rm = (ins >> 16) & 31;
    bool simdArithmetic=false;
    const bool simdArithmeticResult=executeSimdArithmetic(ins,simdArithmetic);
    if(simdArithmetic) return simdArithmeticResult;
    bool extended=false;
    const bool extendedResult=executeFpExtended(ins,extended);
    if (extended) return extendedResult;
    const uint32_t sys = ins & 0xFFFFFFE0;
    if (sys == 0xD53B4400 || sys == 0xD53B4420)
    { writeArithmeticRegister(rd,sys == 0xD53B4400 ? fpcr : fpsr); return true; }
    if (sys == 0xD51B4400 || sys == 0xD51B4420)
    {
        const uint32_t value = uint32_t(readArithmeticRegister(rd));
        if (sys == 0xD51B4400) fpcr = value & 0x07C89F00U;
        else fpsr = value & 0x0800009FU;
        return true;
    }
    // Non-saturating integer SIMD arithmetic and bytewise logic.
    const unsigned q = (ins >> 30) & 1, size = (ins >> 22) & 3;
    const uint32_t three = ins & 0x9F20FC00;
    if (three == 0x0E208400 || three == 0x0E201C00)
    {
        Vector result{};
        const Vector a = vectors[rn], b = vectors[rm], old = vectors[rd];
        if (three == 0x0E208400)
        {
            if (!q && size == 3) return fail();
            const unsigned bits = 8U << size, count = (q ? 128 : 64) / bits;
            const uint64_t mask = UINT64_MAX >> (64-bits);
            for (unsigned lane = 0; lane < count; ++lane)
            {
                const unsigned at = lane*bits, shift = at%64;
                const uint64_t x = (a[at/64] >> shift)&mask, y = (b[at/64] >> shift)&mask;
                const uint64_t value = (ins & (1U<<29)) ? x-y : x+y;
                result[at/64] |= (value&mask) << shift;
            }
        }
        else for (unsigned i=0; i<(q ? 2U : 1U); ++i)
        {
            const unsigned operation = size + ((ins & (1U<<29)) ? 4 : 0);
            switch (operation) {
            case 0: result[i] = a[i]&b[i]; break;
            case 1: result[i] = a[i]&~b[i]; break;
            case 2: result[i] = a[i]|b[i]; break;
            case 3: result[i] = a[i]|~b[i]; break;
            case 4: result[i] = a[i]^b[i]; break;
            case 5: result[i] = (old[i]&a[i]) | (~old[i]&b[i]); break;
            case 6: result[i] = (old[i]&~b[i]) | (a[i]&b[i]); break;
            case 7: result[i] = (old[i]&b[i]) | (a[i]&~b[i]); break;
            }
        }
        vectors[rd] = result; return true;
    }
    const uint32_t copy = ins & 0xBFE0FC00;
    if (copy == 0x0E000C00 || copy == 0x0E003C00 || (ins & 0xFFE0FC00) == 0x4E001C00)
    {
        const unsigned imm5 = (ins >> 16)&31;
        if (!imm5) return fail();
        const unsigned laneSize = std::countr_zero(imm5), bits = 8U << laneSize;
        if (laneSize > 3) return fail();
        const uint64_t mask = UINT64_MAX >> (64-bits);
        const unsigned lane = imm5 >> (laneSize+1), at = lane*bits;
        if (copy == 0x0E000C00) // DUP from GPR
        {
            if (lane || (!q && bits == 64)) return fail();
            Vector result{};
            for (unsigned offset=0; offset<(q ? 128U : 64U); offset+=bits)
                result[offset/64] |= (readArithmeticRegister(rn)&mask) << (offset%64);
            vectors[rd] = result;
        }
        else if (copy == 0x0E003C00) // UMOV
        {
            if (q != (bits == 64)) return fail();
            writeArithmeticRegister(rd,(vectors[rn][at/64] >> (at%64))&mask);
        }
        else // INS from GPR preserves every other lane.
        {
            const uint64_t field = mask << (at%64);
            vectors[rd][at/64] = (vectors[rd][at/64]&~field) |
                ((readArithmeticRegister(rn)&mask) << (at%64));
        }
        return true;
    }
    const uint32_t vectorBinary=ins&0xBFA0FC00, vectorUnary=ins&0xBFBFFC00;
    unsigned vectorOperation=0;
    bool vectorFloat=true, vectorBitwise=false, vectorSubtract=false;
    if (vectorBinary==0x0E20D400) vectorOperation=2;
    else if (vectorBinary==0x0EA0D400) vectorOperation=3;
    else if (vectorBinary==0x2E20DC00) vectorOperation=0;
    else if (vectorBinary==0x2E20FC00) vectorOperation=1;
    else if (vectorBinary==0x0E20CC00 || vectorBinary==0x0EA0CC00)
    { vectorOperation=5; vectorSubtract=vectorBinary==0x0EA0CC00; }
    else if (vectorUnary==0x2EA1F800) vectorOperation=4;
    else if (vectorUnary==0x0EA0F800 || vectorUnary==0x2EA0F800) vectorBitwise=true;
    else vectorFloat=false;
    if (vectorFloat)
    {
        const bool wide=ins&(1U<<22);
        if (wide && !q) return fail();
        const unsigned bits=wide ? 64 : 32, count=(q ? 128 : 64)/bits;
        const Format f(wide);
        const Vector left=vectors[rn], right=vectors[rm], accumulator=vectors[rd];
        Vector result{};
        uint32_t status=0;
        auto element=[&](const Vector& v,unsigned lane) { return (v[(lane*bits)/64] >> ((lane*bits)%64))&f.mask; };
        for (unsigned lane=0; lane<count; ++lane)
        {
            uint64_t a=element(left,lane), b=element(right,lane), value=0;
            if (vectorBitwise) value=vectorUnary==0x0EA0F800 ? a&~f.sign : a^f.sign;
            else
            {
                if (vectorOperation==4) b=0;
                if (vectorSubtract) a ^= f.sign;
                if (!evaluateFloat(a,b,element(accumulator,lane),vectorOperation,wide,fpcr,value,status)) return fail();
            }
            result[(lane*bits)/64] |= value << ((lane*bits)%64);
        }
        // Commit only when all lanes succeeded; a later unsupported lane must
        // not leak either destination changes or FPSR flags from an earlier one.
        vectors[rd]=result; fpsr |= status; return true;
    }
    if ((ins & 0x9F000000) == 0x0E000000 || (ins & 0x9F000000) == 0x0F000000)
        return executeSimdInteger(ins);
    // Scalar floating point: FP16 and newer extensions remain unsupported.
    const unsigned type = (ins >> 22)&3;
    if (type > 1) return fail();
    const bool wide = type == 1;
    const Format f(wide);
    auto scalar = [&](uint64_t value) { vectors[rd] = {value&f.mask,0}; };
    const uint32_t move = ins & 0xFFFFFC00;
    if (move == 0x1E260000 || move == 0x9E660000)
    { writeArithmeticRegister(rd,vectors[rn][0]&f.mask); return true; }
    if (move == 0x1E270000 || move == 0x9E670000)
    { scalar(readArithmeticRegister(rn)); return true; }
    const uint32_t conversion = ins & 0x7FBFFC00; // ignore sf and S/D type only
    const bool toFloat = conversion == 0x1E220000 || conversion == 0x1E230000;
    unsigned integerRounding = 0;
    bool toInteger = true;
    switch (conversion & ~0x00010000U) {
    case 0x1E200000: integerRounding=0; break; // FCVTNS/NU
    case 0x1E240000: integerRounding=4; break; // FCVTAS/AU
    case 0x1E280000: integerRounding=1; break; // FCVTPS/PU
    case 0x1E300000: integerRounding=2; break; // FCVTMS/MU
    case 0x1E380000: integerRounding=3; break; // FCVTZS/ZU
    default: toInteger=false; break;
    }
    if (toFloat || toInteger)
    {
        if (fpcr & ~0x03C00000U) return fail();
        const unsigned integerBits = (ins>>31) ? 64 : 32;
        const bool isUnsigned = ins & (1U<<16);
        uint32_t status = 0;
        if (toFloat) scalar(integerToFloat(readArithmeticRegister(rn),integerBits,isUnsigned,
            wide,(fpcr>>22)&3,status));
        else
        {
            const uint64_t input=prepareFloatInput(vectors[rn][0],wide,fpcr,status);
            uint64_t value = floatToInteger(input,wide,integerBits,isUnsigned,integerRounding,status);
            writeArithmeticRegister(rd,integerBits==32 ? uint32_t(value) : value);
        }
        fpsr |= status; return true;
    }
    const uint32_t precision = ins & 0xFFFFFC00;
    if (precision == 0x1E22C000 || precision == 0x1E624000)
    {
        if (fpcr & ~0x03C00000U) return fail();
        const bool toWide = precision == 0x1E22C000;
        uint32_t status=0;
        const uint64_t result=convertFloatFormat(vectors[rn][0],!toWide,toWide,fpcr,status);
        vectors[rd] = {result,0}; fpsr |= status; return true;
    }
    if ((ins & 0xFF800000) == 0x1F000000) // FMADD/FMSUB/FNMADD/FNMSUB
    {
        if (fpcr & ~0x03C00000U) return fail();
        uint64_t a = vectors[rn][0]&f.mask, b = vectors[rm][0]&f.mask;
        uint64_t c = vectors[(ins>>10)&31][0]&f.mask;
        const bool negateAddend = ins & (1U<<21), subtract = ins & (1U<<15);
        if (negateAddend) c ^= f.sign;
        if (negateAddend != subtract) a ^= f.sign;
        uint32_t status = 0;
        uint64_t result = 0;
        if (!evaluateFloat(a,b,c,5,wide,fpcr,result,status)) return fail();
        scalar(result); fpsr |= status; return true;
    }
    // All remaining supported scalar forms have sf=0, S=0 and fixed 11110.
    if ((ins & 0xFF000000) != 0x1E000000) return fail();
    if ((ins & 0xFF201FE0) == 0x1E201000) // FMOV immediate
    {
        const unsigned imm = (ins >> 13)&255, fracBits = wide ? 52 : 23;
        const uint64_t exponent = ((imm&64) ? (wide ? 0x3FC : 0x7C) : (wide ? 0x400 : 0x80)) | ((imm >> 4)&3);
        scalar(((imm&128) ? f.sign : 0) | (exponent << fracBits) | (uint64_t(imm&15) << (fracBits-4)));
        return true;
    }
    const uint32_t unary = ins & 0xFFBFFC00;
    uint64_t a = vectors[rn][0]&f.mask, b = vectors[rm][0]&f.mask;
    if (unary == 0x1E204000) { scalar(a); return true; }
    if (unary == 0x1E20C000) { scalar(a&~f.sign); return true; }
    if (unary == 0x1E214000) { scalar(a^f.sign); return true; }
    if ((ins & 0xFF200C00) == 0x1E200C00) // FCSEL
    { scalar(conditionHolds((ins >> 12)&15) ? a : b); return true; }
    const bool compare = (ins & 0xFF20FC07) == 0x1E202000;
    const uint32_t binary = ins & 0xFF20FC00;
    unsigned operation = 0;
    const bool sqrt = unary == 0x1E21C000;
    if (!compare && !sqrt)
    {
        if (binary == 0x1E200800) operation = 0;
        else if (binary == 0x1E201800) operation = 1;
        else if (binary == 0x1E202800) operation = 2;
        else if (binary == 0x1E203800) operation = 3;
        else return fail();
    }
    // Reject unsupported FPCR settings rather than silently approximating them.
    if (fpcr & ~0x03C00000U) return fail(); // DN, FZ and RMode; traps remain unsupported.
    if (sqrt) { operation = 4; b = 0; }
    if (compare && (ins & 8)) { if (rm != 0) return fail(); b = 0; }
    if (compare)
    {
        uint32_t status=0;
        a=prepareFloatInput(a,wide,fpcr,status); b=prepareFloatInput(b,wide,fpcr,status);
        fpsr|=status;
        uint64_t flags = 0;
        if (f.nan(a) || f.nan(b))
        {
            flags = FLAG_C|FLAG_V;
            if ((ins&16) || f.signaling(a) || f.signaling(b)) fpsr |= 1;
        }
        else if (a == b || (!(a&~f.sign) && !(b&~f.sign))) flags = FLAG_Z|FLAG_C;
        else
        {
            const bool less = ((a^b)&f.sign) ? (a&f.sign)!=0 : ((a&f.sign) ? a>b : a<b);
            flags = less ? FLAG_N : FLAG_C;
        }
        pstate = (pstate & ~0xF0000000ULL) | flags;
        return true;
    }
    uint32_t status = 0;
    uint64_t result = 0;
    if (!evaluateFloat(a,b,0,operation,wide,fpcr,result,status)) return fail();
    fpsr |= status; scalar(result); return true;
}
