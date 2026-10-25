#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include "bus.hpp"

class Memory;

class CPU
{
public:
    static constexpr size_t REGISTER_COUNT = 32;

    explicit CPU(Memory& memory);
    explicit CPU(Bus& bus);

    void reset();
    void setSystemCounter(uint64_t ticks,uint64_t frequency) { counterValue=ticks;counterFrequency=frequency;counterAvailable=true; }
    // Raw little-endian halves of the 32 architectural 128-bit V registers.
    using Vector = std::array<uint64_t, 2>;
    struct Context
    {
        std::array<uint64_t, REGISTER_COUNT> registers{};
        uint64_t pc = 0, sp = 0, pstate = 0;
        uint64_t tpidrEl0 = 0, tpidrroEl0 = 0;
        std::array<Vector, 32> vectors{};
        uint32_t fpcr = 0, fpsr = 0;
    };
    Context captureContext() const;
    void restoreContext(const Context& context);

    uint64_t readRegister(size_t index) const;
    bool writeRegister(size_t index, uint64_t value);

    uint64_t getPC() const;
    void setPC(uint64_t value);

    uint64_t getSP() const;
    void setSP(uint64_t value);

    uint64_t getPSTATE() const;
    void setPSTATE(uint64_t value);

    bool step();
    enum class StepStatus { Executed, SystemCall, Error };
    struct StepResult { StepStatus status; uint16_t immediate = 0; };
    StepResult stepDetailed();

    bool hasError() const;
    const char* getLastError() const;
    void clearError();

    // ============================================================
    // ARM64 PSTATE - Flags
    // ============================================================

    static constexpr uint64_t FLAG_N = 1ULL << 31;
    static constexpr uint64_t FLAG_Z = 1ULL << 30;
    static constexpr uint64_t FLAG_C = 1ULL << 29;
    static constexpr uint64_t FLAG_V = 1ULL << 28;

private:
    Bus ownedBus;
    Bus& memory;

    uint64_t registers[REGISTER_COUNT];

    uint64_t pc;
    uint64_t currentInstructionPC;
    uint64_t sp;
    uint64_t pstate;
    uint64_t tpidrEl0 = 0;
    uint64_t tpidrroEl0 = 0;
    uint64_t counterValue=0,counterFrequency=0;
    bool counterAvailable=false;
    bool exclusiveValid = false;
    uint64_t exclusiveAddress = 0, exclusiveVersion = 0;
    size_t exclusiveSize = 0;
    std::array<Vector, 32> vectors{};
    uint32_t fpcr = 0, fpsr = 0;

    uint64_t readArithmeticRegister(uint32_t index) const;
    void writeArithmeticRegister(uint32_t index, uint64_t value);

    const char* lastError;

    bool checkRegister(size_t index);

    bool executeInstruction(uint32_t instruction);
    bool executeRegisterPair(uint32_t instruction);
    bool executeInteger(uint32_t instruction);
    bool executeLoadStore(uint32_t instruction);
    bool executeAtomic(uint32_t instruction);
    bool executeFloatingPoint(uint32_t instruction);
    bool executeFpExtended(uint32_t instruction, bool& recognized);
    bool executeSimdInteger(uint32_t instruction);
    bool executeSimdArithmetic(uint32_t instruction, bool& recognized);
    bool executeSimdMemory(uint32_t instruction);
    bool executeSimdStructure(uint32_t instruction);
    bool conditionHolds(uint32_t condition) const;
    uint64_t addWithCarry(uint64_t left, uint64_t right, bool carry, unsigned width, bool flags);
};
