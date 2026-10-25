#include "cpu.hpp"
#include "cpu_fp_internal.hpp"

namespace {
using fp_detail::Format;
uint64_t extremum(uint64_t a,uint64_t b,bool wide,bool minimum,bool number,
    uint32_t control,uint32_t& status)
{
    const Format f(wide);
    a=fp_detail::prepareFloatInput(a,wide,control,status);
    b=fp_detail::prepareFloatInput(b,wide,control,status);
    const bool signaling=f.signaling(a)||f.signaling(b);
    if (signaling) status|=1;
    if (number && !signaling && (f.nan(a)!=f.nan(b))) return f.nan(a) ? b : a;
    if (f.nan(a)||f.nan(b))
    {
        const uint64_t nan=f.signaling(a) ? a : f.signaling(b) ? b : f.nan(a) ? a : b;
        return (control&(1U<<25)) ? f.exponent|f.quiet : nan|f.quiet;
    }
    if (!(a&~f.sign) && !(b&~f.sign)) return minimum ? a|b : a&b;
    const bool less=((a^b)&f.sign) ? (a&f.sign)!=0 : ((a&f.sign) ? a>b : a<b);
    return minimum ? (less ? a : b) : (less ? b : a);
}

uint64_t roundIntegral(uint64_t value,bool wide,unsigned mode,bool exact,uint32_t control,uint32_t& status)
{
    const Format f(wide);
    value=fp_detail::prepareFloatInput(value,wide,control,status);
    if (f.nan(value))
    {
        if (f.signaling(value)) status|=1;
        return (control&(1U<<25)) ? f.exponent|f.quiet : value|f.quiet;
    }
    const unsigned fractionBits=wide ? 52 : 23, bias=wide ? 1023 : 127;
    const uint64_t magnitude=value&~f.sign, sign=value&f.sign;
    if (!magnitude || (magnitude&f.exponent)==f.exponent) return value;
    const int exponent=int((magnitude&f.exponent)>>fractionBits)-int(bias);
    if (exponent>=int(fractionBits)) return value;
    if (exponent<0)
    {
        const uint64_t half=uint64_t(bias-1)<<fractionBits;
        const bool increment=fp_detail::incrementForRounding(sign!=0,mode,0,true,
            magnitude<half ? -1 : magnitude>half ? 1 : 0);
        if (exact) status|=16;
        return sign | (increment ? uint64_t(bias)<<fractionBits : 0);
    }
    const unsigned shift=fractionBits-unsigned(exponent);
    const uint64_t unit=uint64_t(1)<<shift, remainder=magnitude&(unit-1);
    const uint64_t half=unit>>1;
    uint64_t result=magnitude&~(unit-1);
    if (fp_detail::incrementForRounding(sign!=0,mode,result>>shift,remainder!=0,
        remainder<half ? -1 : remainder>half ? 1 : 0)) result+=unit;
    if (exact && remainder) status|=16;
    return sign|result;
}
}

bool CPU::executeFpExtended(uint32_t ins,bool& recognized)
{
    enum class Kind { None, ToFloat, ToInteger, Round, MinMax, Pair, Across };
    Kind kind=Kind::None;
    bool scalar=false, minimum=false, number=false, sum=false, isUnsigned=false, exact=false;
    unsigned rounding=0;
    const uint32_t unary=ins&0xBFBFFC00, binary=ins&0xBFA0FC00;
    const uint32_t scalarUnary=ins&0xFFBFFC00, scalarBinary=ins&0xFFA0FC00;
    switch (unary) {
    case 0x0E21D800: case 0x2E21D800: kind=Kind::ToFloat; isUnsigned=ins&(1U<<29); break;
    case 0x0E21A800: case 0x2E21A800: kind=Kind::ToInteger; rounding=0; break;
    case 0x0E21C800: case 0x2E21C800: kind=Kind::ToInteger; rounding=4; break;
    case 0x0EA1A800: case 0x2EA1A800: kind=Kind::ToInteger; rounding=1; break;
    case 0x0E21B800: case 0x2E21B800: kind=Kind::ToInteger; rounding=2; break;
    case 0x0EA1B800: case 0x2EA1B800: kind=Kind::ToInteger; rounding=3; break;
    case 0x0E218800: kind=Kind::Round; rounding=0; break;
    case 0x2E218800: kind=Kind::Round; rounding=4; break;
    case 0x0EA18800: kind=Kind::Round; rounding=1; break;
    case 0x0E219800: kind=Kind::Round; rounding=2; break;
    case 0x0EA19800: kind=Kind::Round; rounding=3; break;
    case 0x2EA19800: kind=Kind::Round; rounding=(fpcr>>22)&3; break;
    case 0x2E219800: kind=Kind::Round; rounding=(fpcr>>22)&3; exact=true; break;
    case 0x2E30F800: case 0x2EB0F800: case 0x2E30C800: case 0x2EB0C800:
        kind=Kind::Across; minimum=ins&(1U<<23); number=!(ins&(1U<<12)); break;
    default: break;
    }
    if (kind==Kind::ToInteger) isUnsigned=ins&(1U<<29);
    if (kind==Kind::None) switch (binary) {
    case 0x0E20F400: case 0x0EA0F400: case 0x0E20C400: case 0x0EA0C400:
        kind=Kind::MinMax; minimum=ins&(1U<<23); number=!(ins&(1U<<12)); break;
    case 0x2E20D400: kind=Kind::Pair; sum=true; break;
    case 0x2E20F400: case 0x2EA0F400: case 0x2E20C400: case 0x2EA0C400:
        kind=Kind::Pair; minimum=ins&(1U<<23); number=!(ins&(1U<<12)); break;
    default: break;
    }
    if (kind==Kind::None) switch (scalarUnary) {
    case 0x1E244000: kind=Kind::Round; scalar=true; rounding=0; break;
    case 0x1E264000: kind=Kind::Round; scalar=true; rounding=4; break;
    case 0x1E24C000: kind=Kind::Round; scalar=true; rounding=1; break;
    case 0x1E254000: kind=Kind::Round; scalar=true; rounding=2; break;
    case 0x1E25C000: kind=Kind::Round; scalar=true; rounding=3; break;
    case 0x1E27C000: kind=Kind::Round; scalar=true; rounding=(fpcr>>22)&3; break;
    case 0x1E274000: kind=Kind::Round; scalar=true; rounding=(fpcr>>22)&3; exact=true; break;
    case 0x7E30D800: kind=Kind::Pair; scalar=true; sum=true; break;
    case 0x7E30F800: case 0x7EB0F800: case 0x7E30C800: case 0x7EB0C800:
        kind=Kind::Pair; scalar=true; minimum=ins&(1U<<23); number=!(ins&(1U<<12)); break;
    default: break;
    }
    if (kind==Kind::None) switch (scalarBinary) {
    case 0x1E204800: case 0x1E205800: case 0x1E206800: case 0x1E207800:
        kind=Kind::MinMax; scalar=true; minimum=ins&(1U<<12); number=ins&(1U<<13); break;
    default: break;
    }
    recognized=kind!=Kind::None;
    if (!recognized) return false;
    auto fail=[&]() { lastError="Mode ou taille FP de reduction/conversion/arrondi non pris en charge"; return false; };
    const bool q=ins&(1U<<30), wide=ins&(1U<<22);
    if ((!scalar && wide && !q) || (kind==Kind::Across && (wide || !q)) || (fpcr&~0x03C00000U)) return fail();
    const unsigned rd=ins&31, rn=(ins>>5)&31, rm=(ins>>16)&31;
    const unsigned bits=wide ? 64 : 32, count=scalar ? 1 : (q ? 128 : 64)/bits;
    const Format f(wide);
    const Vector a=vectors[rn], b=vectors[rm];
    Vector result{};
    uint32_t status=0;
    auto get=[&](const Vector& v,unsigned index) { return (v[index*bits/64]>>((index*bits)%64))&f.mask; };
    auto combine=[&](uint64_t left,uint64_t right,uint64_t& value) {
        if (sum) return fp_detail::evaluateFloat(left,right,0,2,wide,fpcr,value,status);
        value=extremum(left,right,wide,minimum,number,fpcr,status); return true;
    };
    if (kind==Kind::Across)
    {
        uint64_t first=0,second=0,value=0;
        if (!combine(get(a,0),get(a,1),first) || !combine(get(a,2),get(a,3),second) || !combine(first,second,value)) return fail();
        result={value,0};
    }
    else for (unsigned i=0;i<count;++i)
    {
        uint64_t value=0;
        if (kind==Kind::ToFloat) value=fp_detail::integerToFloat(get(a,i),bits,isUnsigned,wide,(fpcr>>22)&3,status);
        else if (kind==Kind::ToInteger) value=fp_detail::floatToInteger(fp_detail::prepareFloatInput(get(a,i),wide,fpcr,status),wide,bits,isUnsigned,rounding,status)&f.mask;
        else if (kind==Kind::Round) value=roundIntegral(get(a,i),wide,rounding,exact,fpcr,status);
        else if (kind==Kind::MinMax) { if (!combine(get(a,i),get(b,i),value)) return fail(); }
        else
        {
            const Vector& source=scalar || i<count/2 ? a : b;
            const unsigned index=scalar ? 0 : (i%(count/2))*2;
            if (!combine(get(source,index),get(source,index+1),value)) return fail();
        }
        result[i*bits/64]|=value<<((i*bits)%64);
    }
    vectors[rd]=result; fpsr|=status; return true;
}
