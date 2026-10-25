#include "cpu.hpp"
#include "memory.hpp"
#include <bit>
#include <cstdint>

CPU::CPU(Memory& memory)
: ownedBus(memory), memory(ownedBus),
  registers{},
  pc(0),
  sp(0),
  pstate(0),
  lastError(nullptr)
{
    reset();
}

CPU::CPU(Bus& bus) : ownedBus(bus.getMemory()), memory(bus) { reset(); }

void CPU::reset()
{
    counterAvailable=false;counterValue=counterFrequency=0;
    for (size_t i = 0; i < REGISTER_COUNT; ++i)
    {
        registers[i] = 0;
    }

    pc = 0;
    currentInstructionPC = 0;
    sp = 0;
    pstate = 0;
    tpidrEl0 = 0;
    tpidrroEl0 = 0;
    exclusiveValid = false;

    vectors = {};
    fpcr = fpsr = 0;

    lastError = nullptr;
}

CPU::Context CPU::captureContext() const
{
    Context context;
    for (size_t i = 0; i < REGISTER_COUNT; ++i) context.registers[i] = registers[i];
    context.pc = pc; context.sp = sp; context.pstate = pstate;
    context.tpidrEl0 = tpidrEl0; context.tpidrroEl0 = tpidrroEl0;
    context.vectors = vectors; context.fpcr = fpcr; context.fpsr = fpsr;
    return context;
}

void CPU::restoreContext(const Context& context)
{
    exclusiveValid = false;
    for (size_t i = 0; i < REGISTER_COUNT; ++i) registers[i] = context.registers[i];
    pc = context.pc; sp = context.sp; pstate = context.pstate;
    tpidrEl0 = context.tpidrEl0; tpidrroEl0 = context.tpidrroEl0;
    vectors = context.vectors; fpcr = context.fpcr; fpsr = context.fpsr;
}

bool CPU::checkRegister(size_t index)
{
    if (index >= REGISTER_COUNT)
    {
        lastError = "Registre CPU invalide";
        return false;
    }

    return true;
}

uint64_t CPU::readArithmeticRegister(uint32_t index) const
{
    // X31 représente XZR
    if (index == 31)
    {
        return 0;
    }

    // Sécurité contre les index invalides
    if (index >= REGISTER_COUNT)
    {
        return 0;
    }

    return registers[index];
}

void CPU::writeArithmeticRegister(uint32_t index, uint64_t value)
{
    // X31 représente XZR : les écritures sont ignorées
    if (index == 31)
    {
        return;
    }

    // Sécurité contre les index invalides
    if (index >= REGISTER_COUNT)
    {
        return;
    }

    registers[index] = value;
}

uint64_t CPU::readRegister(size_t index) const
{
    if (index >= REGISTER_COUNT)
    {
        return 0;
    }

    return registers[index];
}

bool CPU::writeRegister(size_t index, uint64_t value)
{
    if (!checkRegister(index))
    {
        return false;
    }

    registers[index] = value;
    return true;
}

uint64_t CPU::getPC() const
{
    return pc;
}

void CPU::setPC(uint64_t value)
{
    pc = value;
}

uint64_t CPU::getSP() const
{
    return sp;
}

void CPU::setSP(uint64_t value)
{
    sp = value;
}

uint64_t CPU::getPSTATE() const
{
    return pstate;
}

void CPU::setPSTATE(uint64_t value)
{
    pstate = value;
}

bool CPU::step()
{
    return stepDetailed().status == StepStatus::Executed;
}
CPU::StepResult CPU::stepDetailed()
{
    clearError();

    if ((pc & 3) != 0) { exclusiveValid = false; lastError = "PC non aligne"; return {StepStatus::Error}; }

    if (!memory.isValidRange(pc, 4))
    {
        lastError = "Lecture instruction hors limites";
        exclusiveValid = false;
        return {StepStatus::Error};
    }

    // Sauvegarder l'adresse de l'instruction actuelle
    currentInstructionPC = pc;

    uint32_t instruction = memory.fetch32(pc);

    if (memory.hasError())
    {
        exclusiveValid = false;
        lastError = memory.getLastError();
        return {StepStatus::Error};
    }

    // Architectural SVC decode; interpretation of the immediate belongs to the host.
    if ((instruction & 0xFFE0001F) == 0xD4000001)
    {
        exclusiveValid = false;
        pc += 4;
        return {StepStatus::SystemCall, static_cast<uint16_t>((instruction >> 5) & 0xFFFF)};
    }
    const Context saved = captureContext();
    const bool executed = executeInstruction(instruction);
    if (!executed || memory.hasError())
    {
        restoreContext(saved);
        if (memory.hasError()) lastError = memory.getLastError();
        return {StepStatus::Error};
    }


    pc += 4;

    return {StepStatus::Executed};
}

// ============================================================
// V0.085 — Décodage immédiat logique ARM64
// ============================================================

static uint64_t rotateRight(uint64_t value, uint32_t rotation, uint32_t width)
{
    rotation &= width - 1;

    if (rotation == 0)
        return value;

    return (value >> rotation) | (value << (width - rotation));
}

static bool decodeLogicalImmediate(
    uint32_t instruction,
    uint64_t& immediate
)
{
    uint32_t n = (instruction >> 22) & 1;
    uint32_t immr = (instruction >> 16) & 0x3F;
    uint32_t imms = (instruction >> 10) & 0x3F;
    if (n && !(instruction & 0x80000000)) return false;

    uint32_t value = (n << 6) | ((~imms) & 0x3F);

    if (value == 0)
        return false;

    uint32_t len = 0;
    uint32_t temp = value;

    while (temp >>= 1)
    {
        len++;
    }

    if (len < 1)
        return false;

    uint32_t size = 1U << len;
    uint32_t levels = size - 1;

    uint32_t s = imms & levels;
    uint32_t r = immr & levels;

    if (s == levels)
        return false;

    uint64_t pattern = (1ULL << (s + 1)) - 1;

    pattern = rotateRight(pattern, r, size);

    if (size < 64)
    {
        uint64_t mask = (1ULL << size) - 1;
        pattern &= mask;
    }

    immediate = 0;

    for (uint32_t i = 0; i < 64; i += size)
    {
        immediate |= pattern << i;
    }

    return true;
}

bool CPU::executeInstruction(uint32_t instruction)
{
    if((instruction&0xFFFFFFE0U)==0xD53BE020U || (instruction&0xFFFFFFE0U)==0xD53BE000U) {
        if(!counterAvailable) { lastError="Compteur systeme non configure";return false; }
        writeArithmeticRegister(instruction&31,(instruction&0x20) ? counterValue : counterFrequency);return true;
    }
    if ((instruction & 0xBF000000) == 0x0C000000)
        return executeSimdStructure(instruction);
    // SIMD memory shares the integer load/store classes: test V before dispatch.
    if ((instruction & (1U << 26)) &&
        ((instruction & 0x3E000000) == 0x3C000000 ||
         (instruction & 0x3F000000) == 0x1C000000 ||
         (instruction & 0x3E000000) == 0x2C000000))
        return executeSimdMemory(instruction);
    if ((instruction & 0x0E000000) == 0x0E000000 ||
        (instruction & 0xFFFFFC00) == 0xD53B4400 ||
        (instruction & 0xFFFFFC00) == 0xD51B4400)
        return executeFloatingPoint(instruction);
    if ((instruction & 0x3F000000) == 0x08000000 ||
        (instruction & 0x3F200C00) == 0x38200000 ||
        (instruction & 0xFFFFF0FF) == 0xD503305F ||
        (instruction & 0xFFFFF09F) == 0xD503309F)
        return executeAtomic(instruction);
    const uint32_t group = instruction & 0x1F000000;
    if (group == 0x11000000 || group == 0x0B000000 || group == 0x0A000000 ||
        group == 0x1A000000 || group == 0x1B000000 ||
        (instruction & 0x1F000000) == 0x13000000)
        return executeInteger(instruction);
    if ((instruction & 0x3E000000) == 0x38000000 || (instruction & 0x3F000000) == 0x18000000)
        return executeLoadStore(instruction);
    if ((instruction & 0xFF000010) == 0x54000000)
    {
        int64_t immediate = (instruction >> 5) & 0x7FFFF;
        if (immediate & 0x40000) immediate -= 0x80000;
        if (conditionHolds(instruction & 15)) pc = currentInstructionPC + static_cast<uint64_t>(immediate * 4) - 4;
        return true;
    }
    // Move-wide W variants: writing W always zero-extends into X.
    const uint32_t moveWide = instruction & 0xFF800000;
    if (moveWide == 0x52800000 || moveWide == 0x72800000 || moveWide == 0x12800000)
    {
        const uint32_t hw = (instruction >> 21) & 3;
        if (hw >= 2) { lastError = "Decalage MOV W reserve"; return false; }
        const uint32_t rd = instruction & 31;
        const uint32_t shift = hw * 16;
        const uint32_t immediate = ((instruction >> 5) & 0xFFFF) << shift;
        uint32_t value = immediate;
        if (moveWide == 0x72800000)
            value = (static_cast<uint32_t>(readArithmeticRegister(rd)) & ~(0xFFFFU << shift)) | immediate;
        else if (moveWide == 0x12800000) value = ~immediate;
        writeArithmeticRegister(rd,value);
        return true;
    }
    // Only the EL0 thread-pointer registers used by the reference runtime.
    if ((instruction & 0xFFFFFFE0) == 0xD53BD060 || (instruction & 0xFFFFFFE0) == 0xD53BD040)
    {
        writeArithmeticRegister(instruction & 31, (instruction & 0x20) ? tpidrroEl0 : tpidrEl0);
        return true;
    }
    if ((instruction & 0xFFFFFFE0) == 0xD51BD040)
    {
        tpidrEl0 = readArithmeticRegister(instruction & 31);
        return true;
    }
    if ((instruction & 0x3E000000) == 0x28000000)
        return executeRegisterPair(instruction);

    // ARM64 NOP
    if (instruction == 0xD503201F)
    {
        return true;
    }

    // ============================================================
    // V0.082 — ADR / ADRP
    // ============================================================
    if ((instruction & 0x1F000000) == 0x10000000)
    {
        uint32_t rd = instruction & 0x1F;

        uint32_t immlo =
            (instruction >> 29) & 0x3;

        uint32_t immhi =
            (instruction >> 5) & 0x7FFFF;

        int64_t immediate =
            (static_cast<int64_t>(immhi) << 2) | immlo;

        // Sign-extension sur 21 bits
        if (immediate & (1LL << 20))
        {
            immediate |= ~((1LL << 21) - 1);
        }

        // PC de l'instruction en cours
        uint64_t base = currentInstructionPC;

        // ADRP : adresse alignée sur une page
        if (instruction & 0x80000000)
        {
            base &= ~0xFFFULL;
            immediate *= 4096;
        }

        uint64_t result = base + static_cast<uint64_t>(immediate);

        writeArithmeticRegister(rd, result);

        return true;
    }

    // ============================================================
    // V0.083 — CBZ / CBNZ
    // ============================================================
    if ((instruction & 0x7E000000) == 0x34000000)
    {
        bool is64Bit = (instruction & 0x80000000) != 0;
        bool branchIfNonZero = (instruction & 0x01000000) != 0;
        uint32_t rt = instruction & 0x1F;
        int64_t immediate = static_cast<int64_t>((instruction >> 5) & 0x7FFFF);
        if (immediate & 0x40000) immediate |= ~0x7FFFFLL;
        uint64_t value = readArithmeticRegister(rt);
        if (!is64Bit) value = static_cast<uint32_t>(value);

        if ((value != 0) == branchIfNonZero)
        {
            pc = currentInstructionPC + static_cast<uint64_t>(immediate * 4) - 4;
        }
        return true;
    }

    // ============================================================
    // V0.084 — TBZ / TBNZ
    // ============================================================
    if ((instruction & 0x7E000000) == 0x36000000)
    {
        bool branchIfSet = (instruction & 0x01000000) != 0;
        uint32_t rt = instruction & 0x1F;
        uint32_t bit = ((instruction >> 31) & 1) << 5;
        bit |= (instruction >> 19) & 0x1F;
        int64_t immediate = static_cast<int64_t>((instruction >> 5) & 0x3FFF);
        if (immediate & 0x2000) immediate |= ~0x3FFFLL;
        bool isSet = (readArithmeticRegister(rt) & (1ULL << bit)) != 0;

        if (isSet == branchIfSet)
        {
            pc = currentInstructionPC + static_cast<uint64_t>(immediate * 4) - 4;
        }
        return true;
    }

    // MOVZ Xd, #imm16, LSL #shift
    if ((instruction & 0xFF800000) == 0xD2800000)
    {
        uint32_t rd = instruction & 0x1F;

        uint64_t immediate =
            (instruction >> 5) & 0xFFFF;

        uint32_t hw =
            (instruction >> 21) & 0x3;

        uint32_t shift = hw * 16;

        writeArithmeticRegister(rd, immediate << shift);

        return true;
    }

    // ARM64 MOVK Xd, #imm16, LSL #shift
    if ((instruction & 0xFF800000) == 0xF2800000)
    {
        uint32_t rd = instruction & 0x1F;

        uint64_t immediate =
            (instruction >> 5) & 0xFFFF;

        uint32_t hw =
            (instruction >> 21) & 0x3;

        uint32_t shift = hw * 16;

        uint64_t mask =
            0xFFFFULL << shift;

        uint64_t current =
            readArithmeticRegister(rd);

        uint64_t value =
            (current & ~mask) |
            (immediate << shift);

        writeArithmeticRegister(rd, value);

        return true;
    }

    // ARM64 MOVN Xd, #imm16, LSL #shift
    if ((instruction & 0xFF800000) == 0x92800000)
    {
        uint32_t rd = instruction & 0x1F;

        uint64_t immediate =
            (instruction >> 5) & 0xFFFF;

        uint32_t hw =
            (instruction >> 21) & 0x3;

        uint32_t shift = hw * 16;

        uint64_t value =
            ~(immediate << shift);

     writeArithmeticRegister(rd, value);

        return true;
    }

    // Logical immediate: the non-flag-setting destination may be SP.
    if ((instruction & 0x1F800000) == 0x12000000)
    {
        const uint32_t rd = instruction & 31, rn = (instruction >> 5)&31, op = (instruction >> 29)&3;
        const unsigned width = (instruction >> 31) ? 64 : 32;
        uint64_t immediate;
        if (!decodeLogicalImmediate(instruction,immediate)) { lastError = "Immediat logique invalide"; return false; }
        const uint64_t left = readArithmeticRegister(rn);
        uint64_t value = op == 1 ? left|immediate : op == 2 ? left^immediate : left&immediate;
        if (width == 32) value = uint32_t(value);
        if (op == 3)
            pstate = (pstate & ~(FLAG_N|FLAG_Z|FLAG_C|FLAG_V)) |
                ((value >> (width-1)) ? FLAG_N : 0) | (value == 0 ? FLAG_Z : 0);
        if (rd == 31 && op != 3) sp = value; else writeArithmeticRegister(rd,value);
        return true;
    }
    // ============================================================
    // V0.57 — B (Branch Always)
    // Encodage : 0x14000000
    // ============================================================

    if ((instruction & 0xFC000000) == 0x14000000)
    {
        int32_t immediate =
            static_cast<int32_t>(instruction & 0x03FFFFFF);

        if (immediate & 0x02000000)
        {
            immediate |= ~0x03FFFFFF;
        }

        int64_t offset =
            static_cast<int64_t>(immediate) * 4;

        pc = currentInstructionPC + static_cast<uint64_t>(offset) - 4;

        return true;
    }

    // ============================================================
    // V0.58 — BL (Branch with Link)
    // Encodage : 0x94000000
    // ============================================================

    if ((instruction & 0xFC000000) == 0x94000000)
    {
        int32_t immediate =
            static_cast<int32_t>(instruction & 0x03FFFFFF);

        if (immediate & 0x02000000)
        {
            immediate |= ~0x03FFFFFF;
        }

        int64_t offset =
            static_cast<int64_t>(immediate) * 4;

        // Adresse de retour : instruction suivante
        writeRegister(30, pc + 4);

        // Branchement relatif
        pc = currentInstructionPC + static_cast<uint64_t>(offset) - 4;

        return true;
    }

    // ============================================================
    // V0.059 — BR (Branch to Register)
    // Encodage : D61F0000
    // ============================================================

    if ((instruction & 0xFFFFFC1F) == 0xD61F0000)
    {
        uint32_t rn =
            (instruction >> 5) & 0x1F;

        uint64_t targetAddress =
            readArithmeticRegister(rn);

        // Compensation de l'incrémentation de PC
        pc = targetAddress - 4;

        return true;
    }

    // ============================================================
    // V0.59 — RET (Return)
    // Encodage : D65F0000
    // RET X30 par défaut
    // ============================================================

    if ((instruction & 0xFFFFFC1F) == 0xD65F0000)
    {
        uint32_t rn =
            (instruction >> 5) & 0x1F;

        uint64_t returnAddress =
            readArithmeticRegister(rn);

        pc = returnAddress - 4;

        return true;
    }

    // ============================================================
    // V0.060 — BLR (Branch with Link to Register)
    // Encodage : D63F0000
    // BLR Xn
    // ============================================================

    if ((instruction & 0xFFFFFC1F) == 0xD63F0000)
    {
        uint32_t rn =
            (instruction >> 5) & 0x1F;

        uint64_t targetAddress =
            readArithmeticRegister(rn);

        // Sauvegarde de l'adresse de retour dans X30
        writeRegister(30, pc + 4);

        // Branchement vers l'adresse du registre
        // Compensation de l'incrémentation du PC
        pc = targetAddress - 4;

        return true;
    }

    lastError = "Instruction CPU inconnue";
    return false;
}

bool CPU::hasError() const
{
    return lastError != nullptr;
}

const char* CPU::getLastError() const
{
    return lastError;
}

void CPU::clearError()
{
    lastError = nullptr;
    memory.clearError();
}





