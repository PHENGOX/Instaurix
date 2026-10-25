#include "machine.hpp"
#include "memory_region.hpp"
#include <iostream>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        if (!ok) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
    };
    Memory ram(0x2000);
    Bus bus(ram);
    CPU cpu(bus);
    TestDevice device;
    check(bus.mapDevice(0x1000, 0x10, device, "test"), "mapping");
    ram.write64(0x100C, 0x1122334455667788ULL);
    bus.write64(0x100C, 0);
    check(bus.hasError() && ram.read64(0x100C) == 0x1122334455667788ULL, "MMIO end must not write RAM");
    bus.clearError();
    ram.write64(0xFFC, 0xAABBCCDD);
    bus.write64(0xFFC, 0);
    check(bus.hasError() && ram.read64(0xFFC) == 0xAABBCCDD, "MMIO start must not write RAM");
    bus.clearError();
    bus.read32(UINT64_MAX);
    check(bus.hasError(), "out of range bus error");
    bus.clearError();
    cpu.writeRegister(1, 0x1000);
    ram.write32(0, 0xB9400022); // LDR W2, [X1]
    check(cpu.step() && cpu.readRegister(2) == 0x53543245, "CPU reads MMIO");
    cpu.setPC(0);
    cpu.writeRegister(1, 0x1008); // STATUS is read only
    ram.write32(0, 0xB8004C20); // STR W0, [X1,#4]!: DATA, writable
    check(cpu.step() && cpu.readRegister(1) == 0x100C, "CPU indexed MMIO");
    cpu.setPC(0);
    cpu.writeRegister(1, 0x1000);
    ram.write32(0, 0xB8008C20); // STR W0, [X1,#8]!: STATUS, read only
    check(!cpu.step() && cpu.hasError() && cpu.getPC() == 0 && cpu.readRegister(1) == 0x1000,
          "fault preserves CPU state");
    ram.write32(0, 0xD503201F);
    check(cpu.step(), "CPU recovers after bus fault");
    cpu.setPC(1);
    check(!cpu.step() && cpu.hasError(), "unaligned instruction");
    cpu.writeRegister(0, 0x12345678);
    cpu.writeRegister(1, 0x808);
    cpu.setPC(0);
    ram.write32(0, 0xB81F8C20); // STR W0, [X1,#-8]!
    check(cpu.step() && cpu.readRegister(1) == 0x800 && ram.read32(0x800) == 0x12345678,
          "negative pre-index is an unscaled signed byte offset");

    // Independent bit-by-bit expected masks, all element widths and rotations.
    int masks = 0;
    for (unsigned width = 2; width <= 64; width *= 2)
        for (unsigned ones = 1; ones < width; ++ones)
            for (unsigned rotation = 0; rotation < width; ++rotation)
            {
                uint64_t expected = 0;
                for (unsigned bit = 0; bit < 64; ++bit)
                    if (((bit + rotation) % width) < ones) expected |= 1ULL << bit;
                unsigned imms = ((~(width - 1U) << 1) | (ones - 1)) & 63;
                uint32_t instruction = 0xB20003E0 | ((width == 64 ? 1U : 0U) << 22) |
                                       (rotation << 16) | (imms << 10); // ORR X0, XZR, #mask
                ram.write32(0, instruction);
                cpu.setPC(0);
                check(cpu.step() && cpu.readRegister(0) == expected, "logical immediate mask");
                ++masks;
            }
    ram.write32(0, 0x324003E0); // 32-bit logical instruction with reserved N=1
    cpu.setPC(0);
    check(!cpu.step(), "reserved 32-bit mask");
    MemoryBackedDevice adapter(ram);
    check(adapter.size() == ram.size(), "memory adapter concrete");

    Machine machine;
    machine.ram.write32(0, 0xD503201F);
    machine.ram.write32(4, 0xD503201F);
    machine.ram.write32(8, 0xD503201F);
    machine.bus.write32(Machine::IRQ_BASE + 4, 1U << Machine::TIMER_IRQ);
    machine.bus.write64(Machine::TIMER_BASE + 8, 2);
    machine.cpu.writeRegister(1, Machine::TIMER_BASE + 0x10);
    machine.cpu.writeRegister(0, 1);
    machine.ram.write32(0, 0xB9000020); // guest enables timer with STR W0, [X1]
    auto result = machine.run(10);
    check(result.stop == Machine::Stop::Interrupt && result.instructions == 2 && machine.cpu.getPC() == 8,
          "guest CPU -> bus -> timer -> IRQ -> run stop");
    machine.bus.write32(Machine::IRQ_BASE, 1U << Machine::TIMER_IRQ);
    result = machine.run(1);
    check(result.stop == Machine::Stop::Budget && result.instructions == 1, "resume after IRQ acknowledgement");
    machine.timer.tick(8);
    check(machine.timer.getCounter() == 1, "timer preserves remainder across periods");
    machine.cpu.setPC(12);
    result = machine.run(1);
    // Pending IRQ is reported before execution, even after a previous budget stop.
    check(result.stop == Machine::Stop::Interrupt && result.instructions == 0, "pending IRQ priority");
    machine.bus.write32(Machine::TIMER_BASE + 0x10, 0);
    machine.bus.write32(Machine::IRQ_BASE, 1U << Machine::TIMER_IRQ);
    result = machine.run(1);
    check(result.stop == Machine::Stop::Error && result.instructions == 0 && !machine.error().empty(),
          "unknown instruction stops machine with error");
    check(!machine.load("nonexistent-program-for-test.bin") && !machine.error().empty(), "missing program error");
    std::cout << masks << " logical masks checked; failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
