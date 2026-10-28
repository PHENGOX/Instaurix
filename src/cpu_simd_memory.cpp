#include "cpu.hpp"
#include "memory.hpp"

bool CPU::executeSimdStructure(uint32_t ins)
{
    auto fail=[&]() { lastError="Unsupported SIMD structure LD/ST encoding or address"; return false; };
    const bool q=ins&(1U<<30), post=ins&(1U<<23), load=ins&(1U<<22);
    const unsigned rm=(ins>>16)&31, opcode=(ins>>12)&15, size=(ins>>10)&3;
    const unsigned rn=(ins>>5)&31, rt=ins&31;
    if ((ins&(1U<<21)) || (!post && rm)) return fail();
    unsigned repeat=1, structures=1;
    switch (opcode) {
    case 0: structures=4; break;
    case 2: repeat=4; break;
    case 4: structures=3; break;
    case 6: repeat=3; break;
    case 7: break;
    case 8: structures=2; break;
    case 10: repeat=2; break;
    default: return fail();
    }
    if (!q && size==3 && structures!=1) return fail();
    const unsigned registerBytes=q ? 16 : 8, elementBytes=1U<<size;
    const unsigned elements=registerBytes/elementBytes, count=repeat*structures, total=count*registerBytes;
    const uint64_t address=rn==31 ? sp : readArithmeticRegister(rn);
    if (rn==31 && (address&15)) return fail();
    const uint64_t increment=post ? (rm==31 ? total : readArithmeticRegister(rm)) : 0;
    if (increment>UINT64_MAX-address) return fail();
    if (!memory.validateRamAccess(address,total,load ? Memory::Read : Memory::Write)) return false;
    Vector captured[4]{};
    uint8_t bytes[64]{};
    if (load)
    {
        for (unsigned i=0;i<total;++i) bytes[i]=memory.read8(address+i);
        if (memory.hasError()) return false;
    }
    else for(unsigned i=0;i<count;++i) captured[i]=vectors[(rt+i)&31];
    for (unsigned r=0;r<repeat;++r)
    for (unsigned e=0;e<elements;++e)
    for (unsigned s=0;s<structures;++s)
    for (unsigned b=0;b<elementBytes;++b)
    {
        const unsigned at=((r*elements+e)*structures+s)*elementBytes+b;
        const unsigned reg=r*structures+s, offset=e*elementBytes+b;
        if (load) captured[reg][offset/8]|=uint64_t(bytes[at])<<((offset%8)*8);
        else bytes[at]=uint8_t(captured[reg][offset/8]>>((offset%8)*8));
    }
    if (load) for(unsigned i=0;i<count;++i) vectors[(rt+i)&31]=captured[i];
    else if (!memory.getMemory().writeBytes(address,bytes,total)) return false;
    if (post) { if (rn==31) sp=address+increment; else writeArithmeticRegister(rn,address+increment); }
    return true;
}

bool CPU::executeSimdMemory(uint32_t ins)
{
    auto fail = [&]() { lastError = "Unsupported FP/SIMD memory encoding or address"; return false; };
    const unsigned rt = ins & 31, rn = (ins >> 5) & 31;
    unsigned rt2 = (ins >> 10) & 31, width = 0;
    bool load = false, writeback = false, post = false, pair = false, literal = false;
    uint64_t base = rn == 31 ? sp : readArithmeticRegister(rn), offset = 0;
    if ((ins & 0x3F000000) == 0x1C000000)
    {
        const unsigned opc = ins >> 30;
        if (opc == 3) return fail();
        width = 4U << opc; load = literal = true;
        int64_t immediate = (ins >> 5) & 0x7FFFF;
        if (immediate & 0x40000) immediate -= 0x80000;
        offset = uint64_t(immediate * 4); base = currentInstructionPC;
    }
    else if ((ins & 0x3E000000) == 0x2C000000)
    {
        const unsigned opc = ins >> 30, mode = (ins >> 23) & 3;
        if (opc == 3 || mode == 0) return fail();
        width = 4U << opc; pair = true; load = ins & (1U << 22);
        writeback = mode == 1 || mode == 3; post = mode == 1;
        if (load && rt == rt2) return fail();
        int64_t immediate = (ins >> 15) & 127;
        if (immediate & 64) immediate -= 128;
        offset = uint64_t(immediate * width);
    }
    else if ((ins & 0x3E000000) == 0x3C000000)
    {
        const unsigned size = ins >> 30, opc = (ins >> 22) & 3;
        const unsigned scale = size + ((opc & 2) ? 4 : 0);
        if (scale > 4) return fail();
        width = 1U << scale; load = opc & 1;
        if (ins & (1U << 24)) offset = uint64_t((ins >> 10) & 4095) << scale;
        else if (ins & (1U << 21))
        {
            if (((ins >> 10) & 3) != 2) return fail();
            const unsigned option = (ins >> 13) & 7;
            if (!(option & 2)) return fail();
            offset = readArithmeticRegister((ins >> 16) & 31);
            if (!(option & 1))
            {
                offset = uint32_t(offset);
                if ((option & 4) && (offset & 0x80000000)) offset |= 0xFFFFFFFF00000000ULL;
            }
            if (ins & (1U << 12)) offset <<= scale;
        }
        else
        {
            const unsigned mode = (ins >> 10) & 3;
            if (mode == 2) return fail();
            writeback = mode != 0; post = mode == 1;
            int64_t immediate = (ins >> 12) & 511;
            if (immediate & 256) immediate -= 512;
            offset = uint64_t(immediate);
        }
    }
    else return fail();
    if (!literal && rn == 31 && (sp & 15)) return fail();
    if ((offset >> 63) ? base < uint64_t(0)-offset : base > UINT64_MAX-offset) return fail();
    const uint64_t next = base + offset, address = post ? base : next;
    const unsigned total = pair ? width * 2 : width;
    if (!memory.validateRamAccess(address, total, load ? Memory::Read : Memory::Write)) return false;
    auto read = [&](uint64_t at) {
        Vector value{};
        for (unsigned i = 0; i < width; ++i) value[i / 8] |= uint64_t(memory.read8(at + i)) << ((i % 8) * 8);
        return value;
    };
    if (load)
    {
        const Vector first = read(address), second = pair ? read(address + width) : Vector{};
        if (memory.hasError()) return false;
        vectors[rt] = first;
        if (pair) vectors[rt2] = second;
    }
    else
    {
        uint8_t bytes[32]{};
        for (unsigned i = 0; i < width; ++i)
        {
            bytes[i] = uint8_t(vectors[rt][i/8] >> ((i%8)*8));
            if (pair) bytes[width+i] = uint8_t(vectors[rt2][i/8] >> ((i%8)*8));
        }
        if (!memory.getMemory().writeBytes(address,bytes,total)) return false;
    }
    // Integer base and vector destination are distinct register banks.
    if (writeback) { if (rn == 31) sp = next; else writeArithmeticRegister(rn,next); }
    return true;
}
