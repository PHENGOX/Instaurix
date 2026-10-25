#pragma once
#include <cstdint>

// Shared implementation details for scalar and Advanced SIMD floating point.
namespace fp_detail {
struct Format {
    uint64_t mask, sign, exponent, fraction, quiet;
    explicit Format(bool wide) : mask(wide ? UINT64_MAX : 0xFFFFFFFFULL),
        sign(wide ? 1ULL<<63 : 1ULL<<31), exponent(wide ? 0x7FF0000000000000ULL : 0x7F800000ULL),
        fraction(wide ? 0xFFFFFFFFFFFFFULL : 0x7FFFFFULL), quiet(wide ? 1ULL<<51 : 1ULL<<22) {}
    bool nan(uint64_t x) const { return (x & exponent) == exponent && (x & fraction); }
    bool signaling(uint64_t x) const { return nan(x) && !(x & quiet); }
    bool subnormal(uint64_t x) const { return !(x & exponent) && (x & fraction); }
};
uint64_t integerToFloat(uint64_t value, unsigned integerBits, bool isUnsigned,
    bool wide, unsigned rounding, uint32_t& status);
uint64_t floatToInteger(uint64_t value, bool wide, unsigned integerBits, bool isUnsigned,
    unsigned rounding, uint32_t& status);
bool incrementForRounding(bool negative, unsigned mode, uint64_t kept, bool remainder,
    int halfComparison);
bool evaluateFloat(uint64_t a, uint64_t b, uint64_t c, unsigned operation, bool wide,
    uint32_t control, uint64_t& result, uint32_t& status);
uint64_t prepareFloatInput(uint64_t value,bool wide,uint32_t control,uint32_t& status);
bool softwareArithmetic(uint64_t a,uint64_t b,uint64_t c,unsigned operation,bool wide,
    uint32_t control,uint64_t& result,uint32_t& status);
uint64_t convertFloatFormat(uint64_t value,bool fromWide,bool toWide,uint32_t control,uint32_t& status);
}
