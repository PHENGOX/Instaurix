#include "cpu.hpp"

namespace {
uint64_t maskFor(unsigned bits) { return UINT64_MAX>>(64-bits); }
uint64_t get(const CPU::Vector& v,unsigned i,unsigned bits) { return (v[i*bits/64]>>((i*bits)%64))&maskFor(bits); }
void put(CPU::Vector& v,unsigned i,unsigned bits,uint64_t value) { v[i*bits/64]|=(value&maskFor(bits))<<((i*bits)%64); }
uint64_t arithmeticRight(uint64_t x,unsigned amount,unsigned bits) {
    const uint64_t mask=maskFor(bits),sign=uint64_t(1)<<(bits-1);
    if(amount>=bits) return (x&sign) ? mask : 0;
    uint64_t result=x>>amount;
    if((x&sign) && amount) result|=mask^(mask>>amount);
    return result;
}
uint64_t saturatingAdd(uint64_t x,uint64_t y,unsigned bits,bool isUnsigned,bool subtract,bool& saturated) {
    const uint64_t mask=maskFor(bits),sign=uint64_t(1)<<(bits-1);
    const uint64_t raw=subtract ? x-y : x+y, result=raw&mask;
    if(isUnsigned) {
        const bool overflow=subtract ? x<y : bits==64 ? raw<x : raw>mask;
        if(overflow) { saturated=true; return subtract ? 0 : mask; }
    }
    else {
        const bool overflow=((subtract ? x^y : ~(x^y))&(x^result)&sign)!=0;
        if(overflow) { saturated=true; return (x&sign) ? sign : sign-1; }
    }
    return result;
}
uint64_t variableShift(uint64_t x,uint64_t y,unsigned bits,bool isUnsigned,bool rounding,bool saturate,bool& saturated) {
    const unsigned byte=unsigned(y&255);
    const int shift=byte<128 ? int(byte) : int(byte)-256;
    const uint64_t mask=maskFor(bits),sign=uint64_t(1)<<(bits-1);
    if(shift>=0) {
        const unsigned amount=unsigned(shift);
        const uint64_t result=amount>=bits ? 0 : (x<<amount)&mask;
        bool overflow=false;
        if(amount) {
            if(amount>=bits) overflow=x!=0;
            else overflow=isUnsigned ? (x>>(bits-amount))!=0 : arithmeticRight(result,amount,bits)!=x;
        }
        if(saturate && overflow) { saturated=true; return isUnsigned ? mask : (x&sign) ? sign : sign-1; }
        return result;
    }
    const unsigned amount=unsigned(-shift);
    if(rounding && amount>=bits) return isUnsigned && amount==bits ? (x>>(bits-1)) : 0;
    uint64_t result=isUnsigned ? (amount>=bits ? 0 : x>>amount) : arithmeticRight(x,amount,bits);
    if(rounding) result=(result+((x>>(amount-1))&1))&mask;
    return result;
}
uint64_t narrow(uint64_t x,unsigned sourceBits,unsigned mode,bool& saturated) {
    const unsigned bits=sourceBits/2;
    const uint64_t mask=maskFor(bits),sign=uint64_t(1)<<(bits-1), sourceMask=maskFor(sourceBits),sourceSign=uint64_t(1)<<(sourceBits-1);
    if(mode==1) { if(x>mask) { saturated=true; return mask; } }
    else if(x&sourceSign) {
        if(mode==2) { saturated=true; return 0; }
        const uint64_t minimum=(~mask|sign)&sourceMask;
        if((x^sourceSign)<(minimum^sourceSign)) { saturated=true; return sign; }
    }
    else {
        const uint64_t maximum=mode==2 ? mask : sign-1;
        if(x>maximum) { saturated=true; return maximum; }
    }
    return x&mask;
}
}

bool CPU::executeSimdArithmetic(uint32_t ins,bool& recognized)
{
    enum class Kind { None, Add, Shift, Abs, Widen, Narrow };
    Kind kind=Kind::None;
    const bool scalar=(ins&0xDF000000)==0x5E000000;
    const uint32_t normalized=scalar ? ins&~0x50000000U : ins;
    const uint32_t three=normalized&0xBF20FC00, two=normalized&0xBF3FFC00;
    unsigned narrowMode=0;
    switch(three) {
    case 0x0E200C00: case 0x2E200C00: case 0x0E202C00: case 0x2E202C00: kind=Kind::Add; break;
    case 0x0E204400: case 0x2E204400: case 0x0E205400: case 0x2E205400:
    case 0x0E204C00: case 0x2E204C00: case 0x0E205C00: case 0x2E205C00: kind=Kind::Shift; break;
    case 0x0E20C000: case 0x2E20C000: case 0x0E208000: case 0x2E208000:
    case 0x0E20A000: case 0x2E20A000: kind=Kind::Widen; break;
    default: break;
    }
    if(kind==Kind::None) switch(two) {
    case 0x0E207800: case 0x2E207800: kind=Kind::Abs; break;
    case 0x0E214800: kind=Kind::Narrow; narrowMode=0; break;
    case 0x2E214800: kind=Kind::Narrow; narrowMode=1; break;
    case 0x2E212800: kind=Kind::Narrow; narrowMode=2; break;
    default: break;
    }
    recognized=kind!=Kind::None;
    if(!recognized) return false;
    auto fail=[&]() { lastError="Unsupported SIMD arithmetic size or encoding"; return false; };
    const unsigned rd=ins&31,rn=(ins>>5)&31,rm=(ins>>16)&31,size=(ins>>22)&3,bits=8U<<size;
    const bool q=ins&(1U<<30),isUnsigned=ins&(1U<<29);
    const bool wideOrNarrow=kind==Kind::Widen || kind==Kind::Narrow;
    if((wideOrNarrow && size==3) || (kind==Kind::Widen && scalar) ||
        (!scalar && !wideOrNarrow && !q && size==3) ||
        (scalar && kind==Kind::Shift && !(ins&(1U<<11)) && size!=3)) return fail();
    const Vector a=vectors[rn],b=vectors[rm],old=vectors[rd];
    Vector result{};
    bool saturated=false;
    if(kind==Kind::Widen) {
        const unsigned lanes=64/bits,outputBits=bits*2;
        const uint64_t mask=maskFor(bits),sign=uint64_t(1)<<(bits-1);
        for(unsigned i=0;i<lanes;++i) {
            const unsigned at=i+(q ? lanes : 0);
            uint64_t x=get(a,at,bits),y=get(b,at,bits);
            if(!isUnsigned) { if(x&sign) x|=~mask; if(y&sign) y|=~mask; }
            uint64_t value=x*y;
            if(three==0x0E208000 || three==0x2E208000) value+=get(old,i,outputBits);
            else if(three==0x0E20A000 || three==0x2E20A000) value=get(old,i,outputBits)-value;
            put(result,i,outputBits,value);
        }
    }
    else if(kind==Kind::Narrow) {
        const unsigned lanes=scalar ? 1 : 64/bits;
        if(!scalar && q) result[0]=old[0];
        for(unsigned i=0;i<lanes;++i) {
            const uint64_t value=narrow(get(a,i,bits*2),bits*2,narrowMode,saturated);
            put(result,i+(!scalar && q ? lanes : 0),bits,value);
        }
    }
    else {
        const unsigned lanes=scalar ? 1 : (q ? 128 : 64)/bits;
        const uint64_t mask=maskFor(bits),sign=uint64_t(1)<<(bits-1);
        for(unsigned i=0;i<lanes;++i) {
            const uint64_t x=get(a,i,bits),y=get(b,i,bits);
            uint64_t value=0;
            if(kind==Kind::Add) value=saturatingAdd(x,y,bits,isUnsigned,ins&(1U<<13),saturated);
            else if(kind==Kind::Shift) value=variableShift(x,y,bits,isUnsigned,ins&(1U<<12),ins&(1U<<11),saturated);
            else if(x==sign) { saturated=true; value=sign-1; }
            else value=((ins&(1U<<29)) || (x&sign)) ? (uint64_t(0)-x)&mask : x;
            put(result,i,bits,value);
        }
    }
    vectors[rd]=result;
    if(saturated) fpsr|=1U<<27;
    return true;
}
