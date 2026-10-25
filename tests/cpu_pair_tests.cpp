#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
    };
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint32_t> instructions;
    for (unsigned char bytes[4]; file.read(reinterpret_cast<char*>(bytes), 4); )
        instructions.push_back(uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
                               (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24));
    if (instructions.size() != 27) { std::cerr << "Missing or invalid assembler vectors\n"; return 2; }
    struct Case { size_t vector; uint64_t base, address, next; bool load, wide, writeback; };
    const Case cases[] = {
        {0,0x800,0x810,0x800,false,true,false}, {1,0x800,0x810,0x800,true,true,false},
        {2,0x810,0x800,0x800,false,true,true}, {3,0x800,0x800,0x810,true,true,true},
        {4,0x800,0x808,0x800,false,false,false}, {5,0x800,0x808,0x800,true,false,false},
        {6,0x808,0x800,0x800,false,false,true}, {7,0x800,0x800,0x808,true,false,true},
        {12,0x800,0x9F8,0x800,false,true,false}, {13,0xA00,0x800,0xA00,true,true,false},
        {14,0x800,0x8FC,0x800,false,false,false}, {15,0x900,0x800,0x900,true,false,false},
        {16,0x800,0x800,0x810,false,true,true}, {17,0x810,0x800,0x800,true,true,true},
        {18,0x800,0x800,0x808,false,false,true}, {19,0x808,0x800,0x800,true,false,true},
        {3,0xFF8,0xFF8,0x1008,true,true,true} // inter-page pair
    };
    constexpr uint64_t first = 0x1122334455667788ULL, second = 0xAABBCCDDEEFF0011ULL;
    for (const auto& test : cases)
    {
        Memory ram(0x4000); Bus bus(ram); CPU cpu(bus);
        ram.write32(0, instructions[test.vector]);
        cpu.writeRegister(0, first); cpu.writeRegister(1, second); cpu.writeRegister(2, test.base);
        cpu.writeRegister(3, UINT64_MAX); cpu.writeRegister(4, UINT64_MAX);
        cpu.setPSTATE(0xF0000000);
        if (test.load)
        {
            if (test.wide) { ram.write64(test.address, first); ram.write64(test.address + 8, second); }
            else { ram.write32(test.address, uint32_t(first)); ram.write32(test.address + 4, uint32_t(second)); }
        }
        check(cpu.step(), "assembled W/X pair executes");
        check(cpu.getPC() == 4 && cpu.readRegister(2) == test.next && cpu.getPSTATE() == 0xF0000000,
              "pair addressing/writeback/flags");
        if (test.load)
            check(cpu.readRegister(3) == (test.wide ? first : uint32_t(first)) &&
                  cpu.readRegister(4) == (test.wide ? second : uint32_t(second)), "pair load values and W zero extension");
        else
            check(test.wide ? ram.read64(test.address) == first && ram.read64(test.address + 8) == second :
                              ram.read32(test.address) == uint32_t(first) && ram.read32(test.address + 4) == uint32_t(second),
                  "pair store values");
    }
    Memory ram(0x4000); Bus bus(ram); CPU cpu(bus);
    auto execute = [&](size_t index) { cpu.setPC(0); ram.write32(0,instructions[index]); return cpu.step(); };
    cpu.setSP(0x2000); cpu.writeRegister(29, first); cpu.writeRegister(30, second);
    check(execute(8) && cpu.getSP() == 0x1FF0, "STP saves stack frame");
    cpu.writeRegister(29, 0); cpu.writeRegister(30, 0);
    check(execute(9) && cpu.getSP() == 0x2000 && cpu.readRegister(29) == first && cpu.readRegister(30) == second,
          "LDP restores frame and return address");
    cpu.writeRegister(2, 0x800); cpu.writeRegister(31, first); cpu.writeRegister(1, second);
    check(execute(10) && ram.read64(0x800) == 0 && ram.read64(0x808) == second, "STP uses XZR");
    check(execute(11) && cpu.readRegister(31) == first && cpu.readRegister(4) == second, "LDP discards XZR destination");
    ram.write64(0x800, first);
    cpu.writeRegister(2, 0x800);
    check(execute(20) && cpu.readRegister(2) == first && cpu.readRegister(4) == second, "offset LDP may overwrite base");

    for(unsigned mode : {1U,2U,3U}) {
        for(uint32_t a : {0U,0x7FFFFFFFU,0x80000000U,0xFFFFFFFFU}) {
            cpu.reset(); cpu.writeRegister(2,0x808); cpu.setPSTATE(0xF0000000);
            // Signed -8 offset: post-index reads old base, other forms use adjusted base.
            const uint32_t encoded=(instructions[22]&~((3U<<23)|(127U<<15)))|(mode<<23)|(126U<<15);
            const uint64_t address=mode==1 ? 0x808 : 0x800;
            ram.write32(address,a); ram.write32(address+4,~a); ram.write32(0,encoded);
            const auto extended=[](uint32_t v) { return uint64_t(v)|((v&0x80000000U)?0xFFFFFFFF00000000ULL:0); };
            check(cpu.step() && cpu.readRegister(3)==extended(a) && cpu.readRegister(4)==extended(~a) &&
                cpu.readRegister(2)==(mode==2 ? 0x808U : 0x800U) && cpu.getPSTATE()==0xF0000000,
                "LDPSW sign extension, four-byte scaling, offset/pre/post-index and flags");
        }
    }

    TestDevice device;
    check(bus.mapDevice(0x1000, device.size(), device, "test"), "map pair fault fixture");
    auto fault = [&](uint32_t instruction, uint64_t base, uint64_t stack) {
        cpu.reset(); cpu.setSP(stack);
        cpu.writeRegister(0, first); cpu.writeRegister(1, second); cpu.writeRegister(2, base);
        cpu.writeRegister(3, 77); cpu.writeRegister(4, 88); cpu.setPSTATE(0xF0000000);
        ram.write32(0, instruction);
        return !cpu.step() && cpu.hasError() && cpu.getPC() == 0 && cpu.getSP() == stack &&
               cpu.readRegister(2) == base && cpu.readRegister(3) == 77 && cpu.readRegister(4) == 88 &&
               cpu.getPSTATE() == 0xF0000000;
    };
    ram.write64(0x3FF8, second);
    check(fault(instructions[16], 0x3FF8, 0x2000) && ram.read64(0x3FF8) == second, "STP bounds failure has no first write");
    check(fault(instructions[3], 0x3FF8, 0x2000), "LDP second element out of RAM preserves registers");
    ram.write64(0xFF8, first);
    check(fault(instructions[16], 0xFF8, 0x2000) && ram.read64(0xFF8) == first, "pair cannot cross RAM/MMIO");
    check(fault(instructions[3], 0x1000, 0x2000), "pair MMIO explicitly unsupported");
    check(fault(instructions[2], 8, 0x2000), "negative offset underflow");
    check(fault(instructions[0], UINT64_MAX - 7, 0x2000), "positive offset overflow");
    check(fault(instructions[8], 0x800, 0x2008), "SP alignment checked");
    check(fault(instructions[2] & ~(31U << 5), 0x800, 0x2000), "writeback/base-source overlap rejected");
    check(fault((instructions[1] & ~(31U << 10)) | (3U << 10), 0x800, 0x2000), "duplicate LDP destination rejected");
    check(fault(instructions[21],0x800,0x2000), "non-temporal integer pair rejected");
    check(fault(instructions[22] & ~(1U<<22),0x800,0x2000), "reserved signed-word store rejected");
    check(fault(instructions[22],0x3FFC,0x2000), "LDPSW second-word bounds failure preserves destinations");
    check(fault((instructions[22]&~(31U<<10))|(3U<<10),0x800,0x2000), "LDPSW duplicate destination rejected");
    cpu.reset();
    auto vectorContext = cpu.captureContext();
    vectorContext.vectors[0] = {first,second};
    vectorContext.vectors[1] = {~first,~second};
    vectorContext.registers[2] = 0x800;
    cpu.restoreContext(vectorContext); ram.write32(0,instructions[23]);
    check(cpu.step() && ram.read64(0x800)==first && ram.read64(0x808)==second &&
        ram.read64(0x810)==~first && ram.read64(0x818)==~second,
        "Q pair uses the vector bank rather than integer W/X pairs");
    cpu.reset(); ram.write32(0,instructions[24]);
    auto step = cpu.stepDetailed();
    check(step.status == CPU::StepStatus::SystemCall && step.immediate == 0xF000 && cpu.getPC() == 4 && !cpu.hasError(),
          "SVC is a host event with next PC");
    check(fault(instructions[26],0x800,0x2000), "BRK must not be decoded as SVC");

    Machine machine(true);
    check(machine.load(argv[2]), "load compiled C sample");
    auto result = machine.run(500);
    check(result.stop == Machine::Stop::Exited && result.exitCode == 0 && machine.ram.read64(0x1000) == 55 &&
          machine.cpu.getSP() == Machine::RAM_SIZE && machine.cpu.readRegister(29) == 0 && machine.cpu.readRegister(30) == 0,
          "compiled C: sum 1..10, loop, calls, stack restored, explicit exit");
    std::cout << "Compiled sample: instructions=" << result.instructions << " result=" << machine.ram.read64(0x1000) << '\n';
    result = machine.run(500);
    check(result.stop == Machine::Stop::Exited && result.instructions == 0, "exited process cannot restart");
    check(machine.load(argv[2]) && machine.run(1).stop == Machine::Stop::Budget, "load resets exit state and budget remains distinct");
    Machine disabled;
    disabled.ram.write32(0,instructions[24]);
    check(disabled.run(1).stop == Machine::Stop::Error, "local protocol disabled by default");
    Machine unknown(true);
    unknown.ram.write32(0,instructions[25]);
    check(unknown.run(1).stop == Machine::Stop::Error, "unknown SVC not silently successful");
    Machine nonzero(true);
    nonzero.ram.write32(0,instructions[24]); nonzero.cpu.writeRegister(0,7);
    result = nonzero.run(1);
    check(result.stop == Machine::Stop::Exited && result.exitCode == 7, "guest exit status retained");
    std::cout << "Pair and execution failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
