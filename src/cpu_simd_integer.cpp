#include "cpu.hpp"
#include <bit>

namespace {
uint64_t maskFor(unsigned bits) { return UINT64_MAX >> (64-bits); }
uint64_t lane(const CPU::Vector& v, unsigned index, unsigned bits) {
    const unsigned at = index*bits;
    return (v[at/64] >> (at%64)) & maskFor(bits);
}
void put(CPU::Vector& v, unsigned index, unsigned bits, uint64_t value) {
    const unsigned at = index*bits;
    v[at/64] |= (value & maskFor(bits)) << (at%64);
}
}

bool CPU::executeSimdInteger(uint32_t ins)
{
    auto fail = [&]() { lastError = "Unsupported integer SIMD instruction or size"; return false; };
    const unsigned rd=ins&31, rn=(ins>>5)&31, rm=(ins>>16)&31;
    const bool q=ins&(1U<<30);
    const unsigned size=(ins>>22)&3, bits=8U<<size, lanes=(q ? 128 : 64)/bits;
    const Vector a=vectors[rn], b=vectors[rm], old=vectors[rd];
    Vector result{};
    if((ins&0xBFF8FC00)==0x2F00E400) // MOVI Dd / Vd.2D, byte-mask immediate.
    {
        const unsigned immediate=(((ins>>16)&7)<<5)|((ins>>5)&31);
        uint64_t value=0;
        for(unsigned byte=0;byte<8;++byte) if(immediate&(1U<<byte)) value|=uint64_t(255)<<(8*byte);
        vectors[rd]={value,q ? value : 0};return true;
    }
    if ((ins&0xBFE08C00)==0x0E000000) // TBL/TBX, tables always use full 128-bit registers
    {
        const unsigned tables=((ins>>13)&3)+1, bytes=q ? 16 : 8;
        const bool extend=ins&(1U<<12);
        for(unsigned i=0;i<bytes;++i)
        {
            const unsigned index=unsigned(lane(b,i,8));
            const uint64_t value=index<tables*16 ? lane(vectors[(rn+index/16)&31],index%16,8) :
                extend ? lane(old,i,8) : 0;
            put(result,i,8,value);
        }
        vectors[rd]=result; return true;
    }
    const uint32_t copy=ins&0xBFE0FC00;
    const bool insert=(ins&0xFFE08400)==0x6E000400;
    if (copy==0x0E000400 || copy==0x0E002C00 || insert) // DUP element, SMOV, INS element
    {
        const unsigned imm5=(ins>>16)&31;
        if (!imm5) return fail();
        const unsigned elementSize=std::countr_zero(imm5);
        if (elementSize>3) return fail();
        const unsigned elementBits=8U<<elementSize, index=imm5>>(elementSize+1);
        const uint64_t elementMask=maskFor(elementBits);
        if (insert)
        {
            const unsigned imm4=(ins>>11)&15;
            if (imm4&((1U<<elementSize)-1)) return fail();
            const uint64_t value=lane(a,imm4>>elementSize,elementBits);
            result=old;
            const unsigned at=index*elementBits;
            result[at/64]&=~(elementMask<<(at%64));
            put(result,index,elementBits,value);
        }
        else if (copy==0x0E002C00)
        {
            if (elementBits>32 || (!q && elementBits==32)) return fail();
            uint64_t value=lane(a,index,elementBits);
            if (value&(uint64_t(1)<<(elementBits-1))) value|=~elementMask;
            writeArithmeticRegister(rd,q ? value : uint32_t(value)); return true;
        }
        else
        {
            if (!q && elementBits==64) return fail();
            const uint64_t value=lane(a,index,elementBits);
            for(unsigned i=0;i<(q ? 128U : 64U)/elementBits;++i) put(result,i,elementBits,value);
        }
        vectors[rd]=result; return true;
    }
    const uint64_t mask=maskFor(bits), sign=uint64_t(1)<<(bits-1);
    const uint32_t three=ins&0xBF20FC00;
    bool supported=true, narrowOnly=false;
    switch (three) {
    case 0x0E209C00: case 0x0E209400: case 0x2E209400: // MUL/MLA/MLS
    case 0x0E206400: case 0x0E206C00: case 0x2E206400: case 0x2E206C00:
        narrowOnly=true; break; // SMAX/SMIN/UMAX/UMIN
    case 0x2E208C00: case 0x0E203400: case 0x0E203C00: case 0x2E203400:
    case 0x2E203C00: case 0x0E208C00: case 0x0E20BC00: break;
    default: supported=false; break;
    }
    if (supported)
    {
        if ((!q && size==3) || (narrowOnly && size==3)) return fail();
        for (unsigned i=0; i<lanes; ++i)
        {
            const uint64_t x=lane(a,i,bits), y=lane(b,i,bits), acc=lane(old,i,bits);
            uint64_t value=0;
            switch (three) {
            case 0x0E209C00: value=x*y; break;
            case 0x0E209400: value=acc+x*y; break;
            case 0x2E209400: value=acc-x*y; break;
            case 0x2E208C00: value=x==y ? mask : 0; break;
            case 0x0E208C00: value=(x&y) ? mask : 0; break;
            case 0x0E203400: value=(x^sign)>(y^sign) ? mask : 0; break;
            case 0x0E203C00: value=(x^sign)>=(y^sign) ? mask : 0; break;
            case 0x2E203400: value=x>y ? mask : 0; break;
            case 0x2E203C00: value=x>=y ? mask : 0; break;
            case 0x0E206400: value=(x^sign)>(y^sign) ? x : y; break;
            case 0x0E206C00: value=(x^sign)<(y^sign) ? x : y; break;
            case 0x2E206400: value=x>y ? x : y; break;
            case 0x2E206C00: value=x<y ? x : y; break;
            case 0x0E20BC00: {
                const Vector& source=i<lanes/2 ? a : b;
                const unsigned at=(i%(lanes/2))*2;
                value=lane(source,at,bits)+lane(source,at+1,bits); break;
            }
            }
            put(result,i,bits,value);
        }
        vectors[rd]=result; return true;
    }
    const uint32_t permute=ins&0xBF20FC00;
    if (permute==0x0E003800 || permute==0x0E007800 || permute==0x0E001800 ||
        permute==0x0E005800 || permute==0x0E002800 || permute==0x0E006800)
    {
        if (!q && size==3) return fail();
        const bool upper=ins&(1U<<14);
        for (unsigned i=0; i<lanes; ++i)
        {
            unsigned index=0;
            const Vector* source=nullptr;
            switch ((ins>>12)&3) {
            case 3: // ZIP: alternate the low/high halves of the two sources.
                source=(i&1) ? &b : &a; index=i/2+(upper ? lanes/2 : 0); break;
            case 1: // UZP: collect even/odd lanes from each source.
                source=i<lanes/2 ? &a : &b; index=(i%(lanes/2))*2+unsigned(upper); break;
            case 2: // TRN: alternate even/odd lanes of the two sources.
                source=(i&1) ? &b : &a; index=(i/2)*2+unsigned(upper); break;
            default: return fail();
            }
            put(result,i,bits,lane(*source,index,bits));
        }
        vectors[rd]=result; return true;
    }
    if ((ins&0xBFE08400)==0x2E000000) // EXT bytes from concatenated active halves
    {
        const unsigned offset=(ins>>11)&15, bytes=q ? 16 : 8;
        if (offset>=bytes) return fail();
        for (unsigned i=0; i<bytes; ++i)
            put(result,i,8,lane(i+offset<bytes ? a : b,(i+offset)%bytes,8));
        vectors[rd]=result; return true;
    }
    const uint32_t two=ins&0xBF3FFC00;
    if (two==0x0E31B800 || two==0x0E30A800 || two==0x0E31A800 ||
        two==0x2E30A800 || two==0x2E31A800)
    {
        if (size==3 || (size==2 && !q)) return fail();
        uint64_t value=lane(a,0,bits);
        for (unsigned i=1; i<lanes; ++i)
        {
            const uint64_t x=lane(a,i,bits);
            if (two==0x0E31B800) value=(value+x)&mask;
            else
            {
                const bool isUnsigned=ins&(1U<<29), minimum=ins&(1U<<16);
                const uint64_t bias=isUnsigned ? 0 : sign;
                if (minimum ? (x^bias)<(value^bias) : (x^bias)>(value^bias)) value=x;
            }
        }
        vectors[rd]={value,0}; return true;
    }
    if (two==0x0E201800 || two==0x2E200800 || two==0x0E200800 ||
        two==0x0E205800 || two==0x2E205800)
    {
        if (two==0x0E205800 || two==0x2E205800)
        {
            if (size!=0) return fail();
            for (unsigned i=0; i<(q ? 16U : 8U); ++i)
            {
                const unsigned x=unsigned(lane(a,i,8));
                put(result,i,8,two==0x0E205800 ? std::popcount(x) : ~x);
            }
        }
        else
        {
            const unsigned block=two==0x0E201800 ? 16 : two==0x2E200800 ? 32 : 64;
            if (bits>=block) return fail();
            for (unsigned i=0; i<lanes; ++i) put(result,i,bits,lane(a,i^((block/bits)-1),bits));
        }
        vectors[rd]=result; return true;
    }
    const uint32_t shift=ins&0xBF80FC00;
    if (shift==0x0F005400 || shift==0x0F000400 || shift==0x2F000400)
    {
        const unsigned imm=(ins>>16)&127;
        if (imm<8) return fail();
        const unsigned laneBits=1U<<(31-std::countl_zero(imm));
        if (laneBits==64 && !q) return fail();
        const bool left=shift==0x0F005400, arithmetic=shift==0x0F000400;
        const unsigned amount=left ? imm-laneBits : laneBits*2-imm;
        for (unsigned i=0; i<(q ? 128U : 64U)/laneBits; ++i)
        {
            const uint64_t x=lane(a,i,laneBits);
            const bool negative=arithmetic && (x&(uint64_t(1)<<(laneBits-1)));
            uint64_t value=0;
            if (left) value=x<<amount;
            else if (amount==laneBits) value=negative ? maskFor(laneBits) : 0;
            else
            {
                value=x>>amount;
                if (negative) value |= maskFor(laneBits) ^ (maskFor(laneBits)>>amount);
            }
            put(result,i,laneBits,value);
        }
        vectors[rd]=result; return true;
    }
    return fail();
}
