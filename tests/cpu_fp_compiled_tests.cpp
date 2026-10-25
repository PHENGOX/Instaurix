#include "machine.hpp"
#include <bit>
#include <iostream>
#include <array>
#include <cmath>
#include "programs/cpu_fp/work.c"

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    FpResult native{}; fp_work(&native);
    ConversionResult converted{};
    conversion_work(&converted,-9007199254740993LL,16777219ULL);
    Machine machine(true);
    if (!machine.load(argv[1])) { std::cerr << machine.error(); return 1; }
    const auto result = machine.run(1000);
    bool ok = result.stop == Machine::Stop::Exited && result.exitCode == 0;
    if (!ok) std::cerr << "Guest failed: " << machine.error() << '\n';
    ok &= machine.ram.read32(0x12000) == std::bit_cast<uint32_t>(native.single);
    ok &= machine.ram.read32(0x12004) == native.positive;
    ok &= machine.ram.read64(0x12008) == std::bit_cast<uint64_t>(native.wide);
    const uint32_t a[] = {1,UINT32_MAX,0x80000000U,17}, b[] = {2,3,0x80000001U,23};
    for (unsigned i=0; i<4; ++i) ok &= machine.ram.read32(0x12030+i*4) == ((a[i]+b[i])^a[i]);
    static_assert(sizeof(ConversionResult)==48);
    const auto* convertedBytes = reinterpret_cast<const unsigned char*>(&converted);
    for (unsigned i=0; i<48; ++i) ok &= machine.ram.read8(0x12040+i)==convertedBytes[i];
    // Scalar unsigned reference for the multi-instruction vector/CCMP pipeline.
    std::array<uint8_t,112> simd{};
    auto put32=[&](unsigned at,uint32_t value) {
        for (unsigned i=0; i<4; ++i) simd[at+i]=uint8_t(value>>(i*8));
    };
    uint32_t products[4]{}, popcount=0, sum=0;
    for (unsigned i=0; i<4; ++i)
    {
        products[i]=a[i]*b[i]*2U;
        const uint32_t shifted=(products[i]>>1)|(products[i]&0x80000000U);
        const uint32_t comparison=(shifted^0x80000000U)>(a[i]^0x80000000U) ? UINT32_MAX : 0;
        put32(i*4,products[i]); put32(16+i*4,shifted); put32(32+i*4,comparison);
        put32(48+i*4,comparison ? 0x08080808U : 0);
        popcount+=std::popcount(comparison); sum+=products[i]+a[i];
    }
    put32(64,popcount); put32(68,sum);
    put32(72,popcount>0 && popcount<128); put32(76,popcount!=128);
    const uint32_t extracted[]={products[1],a[1],products[2],a[2]};
    for (unsigned i=0; i<4; ++i)
    {
        put32(80+i*4,extracted[i]);
        for (unsigned j=0; j<4; ++j) simd[96+i*4+j]=uint8_t(extracted[i]>>((3-j)*8));
    }
    for (unsigned i=0; i<simd.size(); ++i) ok &= machine.ram.read8(0x12080+i)==simd[i];
    const float fa[]={1.0001220703125f,1.5f,4.0f,9.0f}, fb[]={0.9998779296875f,2.0f,0.5f,3.0f},
        fc[]={-1.0f,0.25f,-2.0f,-1.5f};
    for (unsigned i=0; i<4; ++i)
    {
        const float sum=fa[i]+fb[i], product=fa[i]*fb[i], fused=std::fma(fa[i],fb[i],fc[i]);
        const float expected[]={sum,product,fused,fused/fb[i],std::sqrt(product),-sum,std::fabs(-sum)};
        for (unsigned j=0; j<7; ++j)
            ok &= machine.ram.read32(0x12120+j*16+i*4)==std::bit_cast<uint32_t>(expected[j]);
    }
    std::array<uint8_t,128> extension{};
    auto extPut=[&](unsigned at,uint32_t value) { for(unsigned i=0;i<4;++i) extension[at+i]=uint8_t(value>>(i*8)); };
    auto extFloat=[&](unsigned at,float value) { extPut(at,std::bit_cast<uint32_t>(value)); };
    const float pair[]={fa[0]+fa[1],fa[2]+fa[3],fb[0]+fb[1],fb[2]+fb[3]};
    for(unsigned i=0;i<4;++i) extFloat(i*4,pair[i]);
    extFloat(16,pair[0]+pair[1]); extFloat(20,9.0f); extFloat(24,1.0f); extFloat(28,fa[0]);
    const uint32_t rounded[]={1,2,4,9};
    const float floors[]={-1.0f,0.0f,-2.0f,-2.0f};
    for(unsigned i=0;i<4;++i)
    {
        extFloat(32+i*4,float(rounded[i])); extFloat(48+i*4,floors[i]);
        extPut(64+i*4,rounded[i]); extFloat(80+i*4,float(rounded[i]));
        extFloat(96+i*4,fb[i]); extFloat(112+i*4,fc[i]);
    }
    for(unsigned i=0;i<extension.size();++i) ok &= machine.ram.read8(0x12190+i)==extension[i];
    std::array<uint8_t,144> structured{};
    const uint32_t sa[]={1,2,3,4}, sb[]={0x80000065U,UINT32_MAX,103,104};
    std::array<uint8_t,64> table{};
    auto encode=[&](auto& output,unsigned at,uint64_t value,unsigned length) {
        for(unsigned i=0;i<length;++i) output[at+i]=uint8_t(value>>(i*8));
    };
    for(unsigned i=0;i<4;++i)
    {
        encode(structured,i*12,sa[i],4); encode(structured,i*12+4,sb[i],4);
        encode(structured,i*12+8,sa[i]+sb[i],4);
        encode(table,i*4,sa[i],4); encode(table,16+i*4,sb[i],4);
        encode(table,32+i*4,sa[i]+sb[i],4); encode(table,48+i*4,4,4);
        encode(structured,80+i*4,4,4); encode(structured,96+i*4,i==3 ? sb[0] : sa[i],4);
    }
    const uint8_t indices[]={0,15,16,31,32,47,48,63,64,255,1,17,33,49,7,8};
    for(unsigned i=0;i<16;++i)
    {
        structured[48+i]=indices[i]<64 ? table[indices[i]] : 0;
        structured[64+i]=indices[i]<64 ? table[indices[i]] : table[48+i];
    }
    encode(structured,112,UINT64_MAX,8); encode(structured,120,0xFFFF8000,8);
    encode(structured,128,0x12230,8); encode(structured,136,0x12270,8);
    for(unsigned i=0;i<structured.size();++i) ok &= machine.ram.read8(0x12240+i)==structured[i];
    std::array<uint8_t,128> tiny{};
    encode(tiny,0,0x00400000,4); encode(tiny,8,2,4); encode(tiny,12,0x18,4);
    encode(tiny,16,0x0008000000000000ULL,8); encode(tiny,32,2,8); encode(tiny,40,0x18,8);
    encode(tiny,52,8,4); encode(tiny,56,0x3F000000,4); encode(tiny,60,128,4); encode(tiny,72,8,8);
    for(unsigned i=0;i<4;++i) encode(tiny,80+i*4,2,4);
    encode(tiny,96,0x18,8); encode(tiny,108,0x18,4);
    for(unsigned i=0;i<tiny.size();++i) ok &= machine.ram.read8(0x12300+i)==tiny[i];
    std::array<uint8_t,192> math{};
    const uint32_t ma[]={0x7FFFFFFFU,0x80000000U,100,0xFFFFFF9CU}, mb[]={1,UINT32_MAX,1001,0xFFFFFC17U};
    const uint32_t sat[]={0x7FFFFFFFU,0x80000000U,1101,0xFFFFFBB3U};
    const uint32_t shifted[]={0x7FFFFFFFU,0x80000000U,550,0xFFFFFDD9U};
    const uint32_t roundedShift[]={0x7FFFFFFFU,0x80000000U,551,0xFFFFFDDAU};
    const uint32_t unsignedSat[]={0x80000000U,UINT32_MAX,1101,UINT32_MAX};
    const uint32_t logicalShift[]={0,0xFFFFFFFEU,550,0x7FFFFFFFU};
    const uint32_t plainShift[]={0xFFFFFFFEU,0,50,0xFFFFFFCEU};
    for(unsigned i=0;i<4;++i) {
        encode(math,i*4,sat[i],4); encode(math,16+i*4,shifted[i],4);
        encode(math,32+i*4,roundedShift[i],4); encode(math,80+i*4,unsignedSat[i],4);
        encode(math,96+i*4,logicalShift[i],4); encode(math,160+i*4,plainShift[i],4);
    }
    const int narrowSigned[]={32767,-32768,550,-551,32767,-32768,551,-550};
    for(unsigned i=0;i<8;++i) encode(math,48+i*2,uint16_t(narrowSigned[i]),2);
    const uint16_t narrowUnsigned[]={65535,0,551,0};
    for(unsigned i=0;i<4;++i) encode(math,64+i*2,narrowUnsigned[i],2);
    auto signed32=[](uint32_t x)->int64_t { return x&0x80000000U ? int64_t(x)-0x100000000LL : int64_t(x); };
    for(unsigned i=0;i<2;++i) {
        const uint64_t lowProduct=uint64_t(signed32(ma[i])*signed32(mb[i]));
        encode(math,112+i*8,lowProduct-uint64_t(ma[i])*mb[i],8);
        encode(math,128+i*8,uint64_t(signed32(ma[i+2])*signed32(mb[i+2]))*2,8);
    }
    encode(math,144,1U<<27,8); encode(math,176,0x7FFFFFFF,4); encode(math,180,1U<<27,4);
    for(unsigned i=0;i<math.size();++i) ok &= machine.ram.read8(0x123B0+i)==math[i];
    const auto& saved = machine.process().mainThread().context;
    ok &= saved.vectors == machine.cpu.captureContext().vectors && saved.fpcr == 0;
    std::cout << "FP C native bits: " << std::hex << std::bit_cast<uint32_t>(native.single) << ' '
        << std::bit_cast<uint64_t>(native.wide) << std::dec << "; instructions=" << result.instructions
        << "; match=" << ok << '\n';
    return ok ? 0 : 1;
}
