#include "machine.hpp"
#include <cfenv>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#if defined(_M_X64) || defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace {
uint64_t read(std::istream& in, unsigned bytes) {
    uint64_t value = 0;
    for (unsigned i=0; i<bytes; ++i) {
        const int next = in.get();
        if (next == EOF) throw std::runtime_error("Truncated reference");
        value |= uint64_t(uint8_t(next)) << (i*8);
    }
    return value;
}
CPU::Context context(std::istream& in) {
    CPU::Context result{};
    for (unsigned i=0; i<31; ++i) result.registers[i] = read(in,8);
    result.sp = read(in,8); result.pstate = read(in,8);
    result.fpcr = uint32_t(read(in,4)); result.fpsr = uint32_t(read(in,4));
    for (auto& v : result.vectors) { v[0] = read(in,8); v[1] = read(in,8); }
    return result;
}
bool same(const CPU::Context& a, const CPU::Context& b) {
    return a.registers == b.registers && a.vectors == b.vectors && a.sp == b.sp &&
        a.pstate == b.pstate && a.fpcr == b.fpcr && a.fpsr == b.fpsr;
}
}

int main(int argc, char** argv)
{
    if (argc != 4) return 2;
    unsigned failures = 0, checks = 0;
    auto check = [&](bool ok,const char* message) {
        ++checks;
        if (!ok) { ++failures; if (failures < 30) std::cerr << "FAIL: " << message << '\n'; }
    };
    std::ifstream input(argv[1],std::ios::binary);
    char magic[8]{}; input.read(magic,8);
    if (std::memcmp(magic,"FPREF001",8)) return 2;
    const uint32_t count = uint32_t(read(input,4));
    if (!count || count > 100000) return 2;
    std::ifstream layout(argv[3]);
    std::string layoutMagic;
    uint32_t variantCount=0,layoutCount=0;
    if (!(layout>>layoutMagic>>variantCount>>layoutCount) || layoutMagic!="FP_LAYOUT_1" ||
        !variantCount || variantCount>10000 || layoutCount!=count) return 2;
    std::vector<std::pair<uint32_t,uint32_t>> blocks(variantCount);
    uint64_t total=0;
    for (auto& block : blocks)
    {
        if (!(layout>>std::hex>>block.first>>std::dec>>block.second) || !block.second) return 2;
        total+=block.second;
    }
    if (total!=count) return 2;
    unsigned variant=0,within=0;
    Memory ram(0x10000); Bus bus(ram); CPU cpu(bus);
    std::fenv_t hostSaved{}; std::fegetenv(&hostSaved);
    std::fesetround(FE_DOWNWARD); std::feclearexcept(FE_ALL_EXCEPT);
    std::feraiseexcept(FE_DIVBYZERO);
#if defined(_M_X64) || defined(__SSE2__)
    const unsigned originalMxcsr = _mm_getcsr();
    _mm_setcsr(originalMxcsr | (1U<<15) | (1U<<6));
    const unsigned testMxcsr = _mm_getcsr();
#endif
    for (uint32_t test=0; test<count; ++test)
    {
        const uint32_t instruction = uint32_t(read(input,4)), sample = uint32_t(read(input,4));
        if (within==blocks[variant].second) { ++variant; within=0; }
        if (instruction!=blocks[variant].first || sample!=within++)
        {
            std::cerr<<"Reference provenance mismatch at case "<<test<<", variant "<<variant<<'\n';
            return 2;
        }
        auto initial = context(input), expected = context(input);
        uint8_t expectedMemory[512]{}; input.read(reinterpret_cast<char*>(expectedMemory),512);
        if (!input) return 2;
        initial.pc = 0x1000;
        cpu.restoreContext(initial);
        uint8_t data[512]{};
        for (unsigned i=0; i<512; ++i) data[i] = uint8_t(i*37+sample*13);
        ram.writeBytes(0x4000,data,512);
        ram.write32(0x1000,instruction); ram.write32(0x1004,0xD503201F);
        ram.writeBytes(0x1008,data,32);
        const bool executed = cpu.step();
        bool equal = executed && same(cpu.captureContext(),expected) && cpu.getPC() == 0x1004;
        for (unsigned i=0; i<512; ++i) equal &= ram.read8(0x4000+i) == expectedMemory[i];
        if (!equal && failures < 30)
        {
            const auto actual = cpu.captureContext();
            std::cerr << "case " << test << " instruction=0x" << std::hex << instruction
                << " FPCR=" << initial.fpcr << " FPSR actual/expected=" << actual.fpsr << '/' << expected.fpsr
                << " V0=" << actual.vectors[0][0] << '/' << expected.vectors[0][0] << std::dec
                << " error=" << (cpu.getLastError() ? cpu.getLastError() : "none") << '\n';
        }
        check(equal,"independent FP/SIMD full-state and memory comparison");
        check(std::fegetround() == FE_DOWNWARD && std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO,
            "guest preserves host FP rounding and flags");
#if defined(_M_X64) || defined(__SSE2__)
        check(_mm_getcsr() == testMxcsr,"guest preserves host MXCSR, including FTZ and DAZ");
#endif
    }
    check(input.peek() == EOF,"reference has exact length");
#if defined(_M_X64) || defined(__SSE2__)
    _mm_setcsr(originalMxcsr);
#endif
    std::fesetenv(&hostSaved);
    CPU::Context saved{};
    for (unsigned i=0; i<32; ++i) { saved.vectors[i] = {uint64_t(i)+0xABC000,~uint64_t(i)}; saved.registers[i]=i*123; }
    saved.fpcr=0x02400000; saved.fpsr=0x08000013; saved.pstate=0xB0000042; saved.sp=0x4100;
    saved.pc=0x1000;
    cpu.restoreContext(saved);
    check(same(cpu.captureContext(),saved),"all V registers, FPCR and FPSR round trip");
    cpu.reset();
    check(cpu.captureContext().vectors == std::array<CPU::Vector,32>{} &&
        cpu.captureContext().fpcr == 0 && cpu.captureContext().fpsr == 0,"reset clears FP state");
    auto instruction = [&](uint32_t ins) { ram.write32(0x1000,ins); cpu.setPC(0x1000); return cpu.step(); };
    const uint32_t faddS = 0x1E222820; // fadd s0,s1,s2
    saved.vectors[1]={0x3F800000,123}; saved.vectors[2]={0x40000000,456};
    for (uint32_t mode : {1U<<8,1U<<19,1U<<26})
    {
        saved.fpcr=mode; cpu.restoreContext(saved);
        check(!instruction(faddS) && same(cpu.captureContext(),saved) && cpu.getPC()==0x1000,
            "unsupported FPCR mode fails without partial state");
    }
    saved.fpcr=0; saved.vectors[1]={1,123}; cpu.restoreContext(saved);
    auto expected=saved; expected.vectors[0]={0x40000000,0}; expected.fpsr|=16;
    check(instruction(faddS) && same(cpu.captureContext(),expected),"subnormal addition rounds and records inexact");
    saved.vectors[1]={0x00800000,0}; saved.vectors[2]={0x3F000000,0}; cpu.restoreContext(saved);
    expected=saved; expected.vectors[0]={0x00400000,0};
    check(instruction(0x1E220820) && same(cpu.captureContext(),expected),"exact subnormal product has no new underflow");
    // New conversion/FMA paths share the same transactional failure contract.
    saved.vectors[1]={0x3F800000,123}; saved.vectors[2]={0x40000000,456};
    saved.vectors[3]={0x40400000,789};
    for (uint32_t ins : {0x1E220020U,0x1E380020U,0x1E624020U,0x1F020C20U})
    {
        saved.fpcr=1U<<8; cpu.restoreContext(saved);
        check(!instruction(ins) && same(cpu.captureContext(),saved),"conversion/FMA rejects enabled FP traps");
    }
    saved.fpcr=0;
    for (unsigned operand : {1U,2U,3U})
    {
        auto tiny=saved; tiny.vectors[operand][0]=1; cpu.restoreContext(tiny);
        auto expectedFma=tiny; expectedFma.vectors[0]={operand==3 ? 0x40000000ULL : 0x40400000ULL,0}; expectedFma.fpsr|=16;
        check(instruction(0x1F020C20) && same(cpu.captureContext(),expectedFma),"FMA accepts tiny operands with one rounding");
    }
    auto tiny=saved;
    tiny.vectors[1]={0x00800000,123}; tiny.vectors[2]={0x3F000000,456}; tiny.vectors[3]={0,789};
    cpu.restoreContext(tiny);
    expected=tiny; expected.vectors[0]={0x00400000,0};
    check(instruction(0x1F020C20) && same(cpu.captureContext(),expected),"FMA exact tiny result accepted");
    tiny.vectors[1]={0x0010000000000000ULL,123}; cpu.restoreContext(tiny);
    expected=tiny; expected.vectors[0]={0,0}; expected.fpsr|=8|16;
    check(instruction(0x1E624020) && same(cpu.captureContext(),expected),"precision narrowing records underflow and inexact");
    auto vectorFault=saved;
    vectorFault.fpcr=0;
    vectorFault.vectors[1]={0x3F8000003F800000ULL,0x3F8000003F800000ULL};
    vectorFault.vectors[2]={0x0000000100000000ULL,0x3F8000003F800000ULL};
    cpu.restoreContext(vectorFault);
    auto vectorExpected=vectorFault;
    vectorExpected.vectors[0]={0x7F8000007F800000ULL,0x3F8000003F800000ULL}; vectorExpected.fpsr|=2|4|16;
    check(instruction(0x6E22FC20) && same(cpu.captureContext(),vectorExpected),
        "SIMD lanes accumulate divide-by-zero, overflow and inexact");
    vectorFault.vectors[2]={0x4000000040000000ULL,1};
    vectorFault.vectors[1][1]=1;
    cpu.restoreContext(vectorFault);
    vectorExpected=vectorFault; vectorExpected.vectors[0]={0x4040000040400000ULL,0};
    check(instruction(0x0E22D420) && same(cpu.captureContext(),vectorExpected),
        "2S floating operation ignores inactive upper lanes and clears them");
    vectorFault.fpcr=1U<<24; cpu.restoreContext(vectorFault);
    vectorExpected=vectorFault; vectorExpected.vectors[0]={0x4040000040400000ULL,0}; vectorExpected.fpsr|=128;
    check(instruction(0x4E22D420) && same(cpu.captureContext(),vectorExpected),
        "vector FZ flushes inputs and records IDC");
    cpu.restoreContext(vectorFault); vectorExpected=vectorFault;
    vectorExpected.vectors[0]=vectorFault.vectors[1];
    vectorExpected.vectors[0][0]&=0x7FFFFFFF7FFFFFFFULL;
    vectorExpected.vectors[0][1]&=0x7FFFFFFF7FFFFFFFULL;
    check(instruction(0x4EA0F820) && same(cpu.captureContext(),vectorExpected),
        "vector FABS copies subnormals independently of arithmetic FPCR modes");
    auto reductionFault=saved; reductionFault.fpcr=0;
    reductionFault.vectors[1]={0x3F8000007F800001ULL,0x3F8000003F800000ULL};
    reductionFault.vectors[2]={0x3F8000003F800000ULL,0x000000013F800000ULL};
    cpu.restoreContext(reductionFault);
    expected=reductionFault; expected.vectors[0]={0x400000007FC00001ULL,0x3F80000040000000ULL}; expected.fpsr|=1|16;
    check(instruction(0x6E22D420) && same(cpu.captureContext(),expected),
        "pairwise operations handle NaN and subnormal inputs together");
    for (uint32_t ins : {0x4E21D820U,0x4EA1B820U,0x4E218820U,0x4E22F420U,0x1E244020U})
    {
        auto unsupported=saved; unsupported.fpcr=1U<<8; cpu.restoreContext(unsupported);
        check(!instruction(ins) && same(cpu.captureContext(),unsupported),
            "new FP paths reject unsupported exception traps without effects");
    }
    auto roundTiny=saved; roundTiny.fpcr=0; roundTiny.fpsr=0;
    roundTiny.vectors[1]={1,0}; cpu.restoreContext(roundTiny);
    auto expectedTiny=roundTiny; expectedTiny.vectors[0]={0,0}; expectedTiny.fpsr=16;
    check(instruction(0x1E274020) && same(cpu.captureContext(),expectedTiny),
        "FRINTX accepts subnormal input and records inexact");
    cpu.restoreContext(roundTiny); expectedTiny.fpsr=0;
    check(instruction(0x1E27C020) && same(cpu.captureContext(),expectedTiny),
        "FRINTI does not record new inexact");
    const uint32_t invalid[] = {0x0EE28420, // invalid ADD V.1D
        0x1EE22820, // FP16 FADD not implemented
        0x1E220420, // FCCMP not implemented
        0x3DC00020 | (1U<<30), // invalid scalar/vector transfer size
        0x4E000C20, // invalid DUP imm5=0
        0x4E013C20, // UMOV X from byte: reserved
        0x9E260020, // invalid mixed sf/type FMOV
        0x1EE20020, // FP16 SCVTF not implemented
        0x1F820C20, // reserved FMA type
        0x9F020C20, // reserved sf for FMA
        0x1E02FC20, // fixed-point SCVTF not implemented
        0x4EE29C20, // MUL V.2D reserved
        0x0EE29420, // MLA V.1D reserved
        0x0EB1B820, // ADDV V.2S reserved
        0x4E605820, // CNT with non-byte size reserved
        0x4E601820, // REV16 element size cannot equal its block size
        0x2E024020, // EXT V.8B offset 8 reserved
        0x0EC23820, // ZIP V.1D reserved
        0x2F000420, // shift immh=0 reserved
        0x0EE20C20, // scalar-width .1D vector SQADD reserved
        0x0E62D420, // FADD V.1D reserved
        0x2E62FC20, // FDIV V.1D reserved
        0x0E62CC20, // FMLA V.1D reserved
        0x2EE1F820, // FSQRT V.1D reserved
        0x0E61D820, // SCVTF V.1D reserved
        0x0EE1B820, // FCVTZS V.1D reserved
        0x0E618820, // FRINTN V.1D reserved
        0x2E62D420, // FADDP V.1D reserved
        0x2E30F820, // FMAXV V.2S reserved
        0x6E70F820, // FMAXV V.2D reserved
        0x4C401020, // reserved structured opcode
        0x4C410020, // nonzero Rm without post-index
        0x4C600020, // reserved structured bit 21
        0x0C408C20, // LD2 .1D reserved
        0x6E020C20, // INS H source index not aligned to element size
        0x4E182C20, // SMOV D element reserved
        0x0EE2C020, // SMULL source .1D reserved
        0x0EE14820, // SQXTN source .2Q reserved
        0x5E224420 // non-saturating scalar SSHL B reserved
    };
    for (uint32_t ins : invalid)
    { cpu.restoreContext(saved); check(!instruction(ins) && same(cpu.captureContext(),saved),"invalid/unsupported FP/SIMD encoding rejected"); }
    // Transfer faults preserve vectors, integer base/writeback, FPCR and FPSR.
    const uint32_t ldrQPost=0x3CC10420, strQPost=0x3C810420;
    for (uint32_t ins : {ldrQPost,strQPost})
    for (uint64_t address : {uint64_t(0xFFF8),UINT64_MAX})
    {
        saved.registers[1]=address; cpu.restoreContext(saved);
        check(!instruction(ins) && same(cpu.captureContext(),saved),"Q transfer bounds/writeback fault is transactional");
    }
    saved.registers[1]=0x4FF8; cpu.restoreContext(saved);
    ram.write64(0x4FF8,0x1122334455667788ULL);
    ram.setPermissions(0x5000,0x1000,Memory::None);
    check(!instruction(strQPost) && same(cpu.captureContext(),saved) && ram.read64(0x4FF8)==0x1122334455667788ULL,
        "Q transfer cannot partially cross permissions");
    saved.registers[1]=0x4FF0; cpu.restoreContext(saved);
    ram.write64(0x4FF0,0xAABBCCDDEEFF0011ULL);
    check(!instruction(0xAD000820) && same(cpu.captureContext(),saved) &&
        ram.read64(0x4FF0)==0xAABBCCDDEEFF0011ULL && ram.read64(0x4FF8)==0x1122334455667788ULL,
        "Q pair preflights both elements before committing");
    ram.setPermissions(0x5000,0x1000,Memory::All);
    TestDevice device;
    check(bus.mapDevice(0x6000,device.size(),device,"FP fixture"),"map SIMD MMIO fixture");
    saved.registers[1]=0x6000; cpu.restoreContext(saved);
    check(!instruction(ldrQPost) && same(cpu.captureContext(),saved),"SIMD MMIO explicitly refused");
    for(uint32_t ins : {0x4CDF0020U,0x4C9F0020U})
    for(uint64_t address : {uint64_t(0xFFE0),uint64_t(0x6000),UINT64_MAX})
    {
        auto state=saved; state.registers[1]=address; cpu.restoreContext(state);
        check(!instruction(ins) && same(cpu.captureContext(),state),"structured transfer bounds/MMIO fault is transactional");
    }
    auto structureFault=saved; structureFault.registers[1]=0x4FE0;
    ram.write64(0x4FE0,0x1122334455667788ULL);
    ram.setPermissions(0x5000,0x1000,Memory::None);
    cpu.restoreContext(structureFault);
    check(!instruction(0x4C9F0020) && same(cpu.captureContext(),structureFault) &&
        ram.read64(0x4FE0)==0x1122334455667788ULL,"ST4 preflights all registers before writing");
    ram.setPermissions(0x5000,0x1000,Memory::All);
    structureFault.registers[1]=0x4100; structureFault.registers[2]=UINT64_MAX;
    cpu.restoreContext(structureFault);
    check(!instruction(0x4CC20020) && same(cpu.captureContext(),structureFault),"post-index overflow rejected before structured load");
    structureFault.sp=0x4108; cpu.restoreContext(structureFault);
    check(!instruction(0x4CDF03E0) && same(cpu.captureContext(),structureFault),"structured SP base alignment checked");
    structureFault.sp=0x4100; structureFault.registers[2]=123;
    cpu.restoreContext(structureFault);
    check(instruction(0xC85F7C20) && instruction(0x4C007020) && instruction(0xC8037C22) &&
        cpu.readRegister(3)==1,"structured stores invalidate exclusive reservations");
    // Process checkpoints include FP state at pause, exit, faults and reload.
    Machine machine(true);
    check(machine.load(argv[2]),"load checkpoint fixture");
    saved.pc=0; saved.registers[1]=0x4100; machine.cpu.restoreContext(saved);
    machine.ram.write32(0,0xD503201F);
    check(machine.run(1).stop==Machine::Stop::Budget && same(machine.process().mainThread().context,saved),
        "pause checkpoints all FP state");
    machine.ram.write32(4,0xD4000001 | (0xF000U<<5));
    check(machine.run(1).stop==Machine::Stop::Exited &&
        same(machine.process().mainThread().context,saved),"exit checkpoints all FP state");
    check(machine.load(argv[2]) && machine.cpu.captureContext().vectors == std::array<CPU::Vector,32>{} &&
        !machine.cpu.captureContext().fpcr && !machine.cpu.captureContext().fpsr,"reload resets FP state");
    saved.pc=0; machine.cpu.restoreContext(saved); machine.ram.write32(0,0);
    check(machine.run(1).stop==Machine::Stop::Error && same(machine.process().mainThread().context,saved),
        "fault checkpoints all FP state");
    check(!machine.load("missing-fp-fixture.elf") && same(machine.cpu.captureContext(),saved) &&
        same(machine.process().mainThread().context,saved),"failed reload preserves FP context");
    std::cout << count << " independent FP/SIMD cases; " << checks << " checks; " << failures << " failures\n";
    return failures ? 1 : 0;
}
