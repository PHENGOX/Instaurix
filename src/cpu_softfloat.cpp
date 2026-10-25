#include "cpu_fp_internal.hpp"
#include <array>
#include <bit>
#include <algorithm>
#include <cassert>

namespace fp_detail {
namespace {
// Finite binary64 FMA needs at most 3172 significant positions after exact
// alignment. 4096 bits cover all binary32/64 operations here, without allocation.
struct Big {
    static constexpr unsigned Words=128;
    std::array<uint32_t,Words> words{};
    explicit Big(uint64_t value=0) { words[0]=uint32_t(value); words[1]=uint32_t(value>>32); }
    int top() const {
        for (int i=Words-1;i>=0;--i) if(words[i]) return i*32+31-int(std::countl_zero(words[i]));
        return -1;
    }
    bool bit(unsigned at) const { return at<Words*32 && ((words[at/32]>>(at%32))&1); }
    void set(unsigned at) { assert(at<Words*32); words[at/32]|=uint32_t(1)<<(at%32); }
    bool below(unsigned at) const {
        for(unsigned i=0;i<std::min(at/32,Words);++i) if(words[i]) return true;
        return at/32<Words && at%32 && (words[at/32]&((uint32_t(1)<<(at%32))-1));
    }
    uint64_t extract(unsigned from) const {
        uint64_t value=0;
        for(unsigned i=0;i<64;++i) if(bit(from+i)) value|=uint64_t(1)<<i;
        return value;
    }
    Big shifted(unsigned amount) const {
        Big out;
        const unsigned whole=amount/32, partial=amount%32;
        for(unsigned i=0;i<Words;++i) if(words[i]) {
            assert(i+whole<Words);
            out.words[i+whole]|=words[i]<<partial;
            if(partial && (words[i]>>(32-partial))) {
                assert(i+whole+1<Words);
                out.words[i+whole+1]|=words[i]>>(32-partial);
            }
        }
        return out;
    }
    int compare(const Big& b) const {
        for(int i=Words-1;i>=0;--i) if(words[i]!=b.words[i]) return words[i]<b.words[i] ? -1 : 1;
        return 0;
    }
    void add(const Big& b) {
        uint64_t carry=0;
        for(unsigned i=0;i<Words;++i) { const uint64_t sum=uint64_t(words[i])+b.words[i]+carry; words[i]=uint32_t(sum); carry=sum>>32; }
        assert(!carry);
    }
    void subtract(const Big& b) {
        assert(compare(b)>=0);
        uint64_t borrow=0;
        for(unsigned i=0;i<Words;++i) {
            const uint64_t sub=uint64_t(b.words[i])+borrow, original=words[i];
            words[i]=uint32_t(original-sub); borrow=original<sub;
        }
    }
};

Big product(uint64_t a,uint64_t b)
{
    Big result;
    for(unsigned i=0;i<2;++i) {
        uint64_t carry=0;
        for(unsigned j=0;j<2;++j) {
            const uint64_t value=uint64_t(uint32_t(a>>(i*32)))*uint32_t(b>>(j*32))+result.words[i+j]+carry;
            result.words[i+j]=uint32_t(value); carry=value>>32;
        }
        result.words[i+2]=uint32_t(carry);
    }
    return result;
}

Big divide(Big numerator,const Big& denominator,bool& remainder)
{
    Big quotient;
    const int shift=numerator.top()-denominator.top();
    for(int i=shift;i>=0;--i) {
        const Big candidate=denominator.shifted(unsigned(i));
        if(numerator.compare(candidate)>=0) { numerator.subtract(candidate); quotient.set(unsigned(i)); }
    }
    remainder=numerator.top()>=0;
    return quotient;
}

Big squareRoot(const Big& input,bool& remainder)
{
    Big root, rest;
    for(int pair=input.top()/2;pair>=0;--pair) {
        rest=rest.shifted(2);
        rest.words[0]|=unsigned(input.bit(unsigned(pair*2))) | (unsigned(input.bit(unsigned(pair*2+1)))<<1);
        Big candidate=root.shifted(2); candidate.words[0]|=1;
        root=root.shifted(1);
        if(rest.compare(candidate)>=0) { rest.subtract(candidate); root.words[0]|=1; }
    }
    remainder=rest.top()>=0;
    return root;
}

struct Finite {
    uint64_t mantissa;
    int exponent;
    bool negative;
    Finite(uint64_t value,bool wide) {
        const Format f(wide);
        const unsigned fraction=wide ? 52 : 23, bias=wide ? 1023 : 127;
        const unsigned encoded=unsigned((value&f.exponent)>>fraction);
        mantissa=(value&f.fraction) | (encoded ? uint64_t(1)<<fraction : 0);
        exponent=int(encoded ? encoded : 1)-int(bias)-int(fraction);
        negative=(value&f.sign)!=0;
    }
};

uint64_t pack(const Big& magnitude,int power,bool negative,bool wide,uint32_t control,uint32_t& status)
{
    const Format f(wide);
    const uint64_t sign=negative ? f.sign : 0;
    const int high=magnitude.top();
    if(high<0) return sign;
    const int fraction=wide ? 52 : 23, bias=wide ? 1023 : 127, minimum=1-bias;
    int exponent=high+power;
    const bool tiny=exponent<minimum;
    // Arm's default profile detects tininess and FZ before rounding.
    if(tiny && (control&(1U<<24))) { status|=8; return sign; }
    const int shift=tiny ? minimum-fraction-power : high-fraction;
    uint64_t kept=shift>=0 ? magnitude.extract(unsigned(shift)) : magnitude.extract(0)<<unsigned(-shift);
    const bool inexact=shift>0 && magnitude.below(unsigned(shift));
    int comparison=-1;
    if(shift>0 && magnitude.bit(unsigned(shift-1))) comparison=magnitude.below(unsigned(shift-1)) ? 1 : 0;
    const unsigned rounding=(control>>22)&3;
    if(incrementForRounding(negative,rounding,kept,inexact,comparison)) ++kept;
    if(inexact) { status|=16; if(tiny) status|=8; }
    if(tiny) return sign|kept; // Carry into bit 'fraction' encodes the minimum normal.
    if(kept==(uint64_t(1)<<(fraction+1))) { kept>>=1; ++exponent; }
    if(exponent>bias) {
        status|=4|16;
        const bool infinity=rounding==0 || (rounding==1 && !negative) || (rounding==2 && negative);
        return sign | (infinity ? f.exponent : f.exponent-1);
    }
    return sign | (uint64_t(exponent+bias)<<fraction) | (kept&f.fraction);
}

uint64_t sum(Big left,int leftPower,bool leftSign,Big right,int rightPower,bool rightSign,
    bool wide,uint32_t control,uint32_t& status)
{
    const int power=std::min(leftPower,rightPower);
    left=left.shifted(unsigned(leftPower-power)); right=right.shifted(unsigned(rightPower-power));
    bool negative=leftSign;
    if(leftSign==rightSign) left.add(right);
    else {
        const int comparison=left.compare(right);
        if(comparison>=0) left.subtract(right);
        else { right.subtract(left); left=right; negative=rightSign; }
        if(left.top()<0) negative=((control>>22)&3)==2;
    }
    return pack(left,power,negative,wide,control,status);
}
}

uint64_t prepareFloatInput(uint64_t value,bool wide,uint32_t control,uint32_t& status)
{
    const Format f(wide); value&=f.mask;
    if((control&(1U<<24)) && f.subnormal(value)) { status|=128; return value&f.sign; }
    return value;
}

bool softwareArithmetic(uint64_t a,uint64_t b,uint64_t c,unsigned operation,bool wide,
    uint32_t control,uint64_t& result,uint32_t& status)
{
    const Format f(wide);
    const Finite x(a,wide),y(b,wide),z(c,wide);
    const bool aInf=(a&~f.sign)==f.exponent, bInf=(b&~f.sign)==f.exponent, cInf=(c&~f.sign)==f.exponent;
    const bool sign=x.negative!=y.negative;
    auto invalid=[&]() { status|=1; result=f.exponent|f.quiet; return true; };
    if(operation==0 || operation==5) {
        if((aInf && !y.mantissa && !bInf) || (bInf && !x.mantissa && !aInf)) return invalid();
        if(aInf || bInf) {
            if(operation==5 && cInf && z.negative!=sign) return invalid();
            result=(sign ? f.sign : 0)|f.exponent; return true;
        }
        if(operation==5 && cInf) { result=c; return true; }
        const Big multiplied=product(x.mantissa,y.mantissa);
        if(operation==0) result=pack(multiplied,x.exponent+y.exponent,sign,wide,control,status);
        else result=sum(multiplied,x.exponent+y.exponent,sign,Big(z.mantissa),z.exponent,z.negative,wide,control,status);
    }
    else if(operation==1) {
        if((aInf && bInf) || (!aInf && !bInf && !x.mantissa && !y.mantissa)) return invalid();
        if(aInf) { result=(sign ? f.sign : 0)|f.exponent; return true; }
        if(bInf || !x.mantissa) { result=sign ? f.sign : 0; return true; }
        if(!y.mantissa) { status|=2; result=(sign ? f.sign : 0)|f.exponent; return true; }
        const unsigned scale=wide ? 112 : 54;
        bool rest=false;
        Big quotient=divide(Big(x.mantissa).shifted(scale),Big(y.mantissa),rest);
        if(rest) quotient.words[0]|=1; // Sticky bit below all retained rounding positions.
        result=pack(quotient,x.exponent-y.exponent-int(scale),sign,wide,control,status);
    }
    else if(operation==2 || operation==3) {
        const bool rightSign=y.negative!=(operation==3);
        if(aInf && bInf && x.negative!=rightSign) return invalid();
        if(aInf) { result=a; return true; }
        if(bInf) { result=(rightSign ? f.sign : 0)|f.exponent; return true; }
        result=sum(Big(x.mantissa),x.exponent,x.negative,Big(y.mantissa),y.exponent,rightSign,wide,control,status);
    }
    else if(operation==4) {
        if(x.negative && (a&~f.sign)) return invalid();
        if(aInf || !x.mantissa) { result=a; return true; }
        uint64_t mantissa=x.mantissa; int power=x.exponent;
        if(power%2) { mantissa<<=1; --power; }
        const unsigned scale=wide ? 60 : 31;
        bool rest=false;
        Big root=squareRoot(Big(mantissa).shifted(scale*2),rest);
        if(rest) root.words[0]|=1;
        result=pack(root,power/2-int(scale),false,wide,control,status);
    }
    else return false;
    return true;
}

uint64_t convertFloatFormat(uint64_t value,bool fromWide,bool toWide,uint32_t control,uint32_t& status)
{
    value=prepareFloatInput(value,fromWide,control,status);
    const Format source(fromWide),target(toWide);
    const uint64_t sign=(value&source.sign) ? target.sign : 0;
    if(source.nan(value)) {
        if(source.signaling(value)) status|=1;
        if(control&(1U<<25)) return target.exponent|target.quiet;
        return sign|target.exponent|target.quiet|(toWide ? (value&source.fraction)<<29 : (value&source.fraction)>>29);
    }
    if((value&~source.sign)==source.exponent) return sign|target.exponent;
    const Finite number(value,fromWide);
    return pack(Big(number.mantissa),number.exponent,number.negative,toWide,control,status);
}
}
