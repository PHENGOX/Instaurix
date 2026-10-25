#include <iostream>
#include <sstream>

#include "cpu.hpp"
#include "memory.hpp"
#include "bus.hpp"
#include "test_device.hpp"
#include "interrupt_controller.hpp"
#include "timer.hpp"

int runLegacyTests()
{

        Memory cpuMemory(0x1000);
        CPU cpu(cpuMemory);

    std::cout << "\n# v0.058 CPU REGISTERS\n\n";

    cpu.writeRegister(0, 0x1122334455667788ULL);
    std::cout << "Registre X0      : "
              << (cpu.readRegister(0) == 0x1122334455667788ULL ? "OK" : "FAIL")
              << "\n";

    cpu.writeRegister(31, 0xAABBCCDDEEFF0011ULL);
    std::cout << "Registre X31     : "
              << (cpu.readRegister(31) == 0xAABBCCDDEEFF0011ULL ? "OK" : "FAIL")
              << "\n";

    cpu.setPC(0x80000000ULL);
    std::cout << "PC               : "
              << (cpu.getPC() == 0x80000000ULL ? "OK" : "FAIL")
              << "\n";

    cpu.setSP(0x90000000ULL);
    std::cout << "SP               : "
              << (cpu.getSP() == 0x90000000ULL ? "OK" : "FAIL")
              << "\n";

    cpu.setPSTATE(0xF0000000ULL);
    std::cout << "PSTATE           : "
              << (cpu.getPSTATE() == 0xF0000000ULL ? "OK" : "FAIL")
              << "\n";

    bool invalidRegister = !cpu.writeRegister(32, 0x1234);

    std::cout << "Registre invalide : "
              << (invalidRegister ? "OK" : "FAIL")
              << "\n";

    cpu.reset();

    bool resetTest =
        cpu.readRegister(0) == 0 &&
        cpu.readRegister(31) == 0 &&
        cpu.getPC() == 0 &&
        cpu.getSP() == 0 &&
        cpu.getPSTATE() == 0;

    std::cout << "CPU Reset         : "
              << (resetTest ? "OK" : "FAIL")
              << "\n";

        // =========================================================
        // V0.19 — EXECUTION CPU
        // =========================================================

    cpuMemory.write32(
        0x0000,
        0xD503201F); // ARM64 NOP

    cpu.setPC(0x0000);

    bool nopExecuted =
        cpu.step() &&
        cpu.getPC() == 0x0004;

    // V0.19 MOVZ test
    cpuMemory.write32(
    0x0004,
    0xD2800542); // MOV X2, #42

    cpu.setPC(0x0004);

    bool movExecuted =
    cpu.step() &&
    cpu.readRegister(2) == 42 &&
    cpu.getPC() == 0x0008;

    std::cout << "Execution MOV      : "
          << (movExecuted ? "OK" : "FAIL")
          << "\n";

    // V0.20 ADD register test
    cpu.writeRegister(0, 10);
    cpu.writeRegister(1, 32);

    cpuMemory.write32(
        0x0008,
        0x8B010002); // ADD X2, X0, X1

        cpu.setPC(0x0008);

    bool addExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 42 &&
        cpu.getPC() == 0x000C;

    std::cout << "Execution ADD      : "
          << (addExecuted ? "OK" : "FAIL")
          << "\n";

    // V0.21 SUB register test
    cpu.writeRegister(0, 50);
    cpu.writeRegister(1, 8);

    cpuMemory.write32(
        0x000C,
        0xCB010002); // SUB X2, X0, X1

    cpu.setPC(0x000C);

    bool subExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 42 &&
        cpu.getPC() == 0x0010;

    std::cout << "Execution SUB      : "
          << (subExecuted ? "OK" : "FAIL")
          << "\n";

    // V0.22 ADD immediate test
    cpu.writeRegister(0, 32);

    cpuMemory.write32(
        0x0010,
        0x91002802); // ADD X2, X0, #10

    cpu.setPC(0x0010);

    bool addImmediateExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 42 &&
        cpu.getPC() == 0x0014;

    std::cout << "Execution ADD imm  : "
            << (addImmediateExecuted ? "OK" : "FAIL")
            << "\n";

    // V0.23 SUB immediate test
    cpu.writeRegister(0, 50);

    cpuMemory.write32(
        0x0014,
        0xD1002002); // SUB X2, X0, #8

    cpu.setPC(0x0014);

    bool subImmediateExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 42 &&
        cpu.getPC() == 0x0018;

    std::cout << "Execution SUB imm  : "
            << (subImmediateExecuted ? "OK" : "FAIL")
            << "\n";

    // V0.24 Multiple instructions test

    // MOV X0, #10
    cpuMemory.write32(
        0x0018,
        0xD2800140);

    // MOV X1, #32
    cpuMemory.write32(
        0x001C,
        0xD2800401);

    // ADD X2, X0, X1
    cpuMemory.write32(
        0x0020,
        0x8B010002);

    // SUB X3, X2, #5
    cpuMemory.write32(
        0x0024,
        0xD1001443); // SUB X3, X2, #5

    cpu.reset();
    cpu.setPC(0x0018);

    bool multipleInstructionsExecuted =
        cpu.step() &&
        cpu.step() &&
        cpu.step() &&
        cpu.step() &&
        cpu.readRegister(0) == 10 &&
        cpu.readRegister(1) == 32 &&
        cpu.readRegister(2) == 42 &&
        cpu.readRegister(3) == 37 &&
        cpu.getPC() == 0x0028;

    std::cout << "Programme multiple  : "
            << (multipleInstructionsExecuted ? "OK" : "FAIL")
            << "\n";

    // V0.26 Unknown instruction error message

    cpuMemory.write32(
        0x0028,
        0xFFFFFFFF); // Instruction inconnue

    cpu.setPC(0x0028);

    bool unknownInstruction =
        !cpu.step() &&
        cpu.hasError() &&
        cpu.getPC() == 0x0028;

    std::cout << "Instruction inconnue : "
            << (unknownInstruction ? "OK" : "FAIL")
            << "\n";

    if (cpu.hasError())
    {
        std::cout << "Message CPU         : "
                << cpu.getLastError()
                << "\n";
    }

    // V0.27 CPU error recovery test

    cpu.clearError();

    // Remplace l'instruction inconnue par un NOP
    cpuMemory.write32(
        0x0028,
        0xD503201F);

    cpu.setPC(0x0028);

    bool recoveryExecuted =
        !cpu.hasError() &&
        cpu.step() &&
        !cpu.hasError() &&
        cpu.getPC() == 0x002C;

    std::cout << "Recuperation CPU     : "
            << (recoveryExecuted ? "OK" : "FAIL")
            << "\n";

    // V0.28 MOVZ shift test

    cpu.reset();

    cpuMemory.write32(
        0x0030,
        0xD2A00541); // MOVZ X1, #42, LSL #16

    cpu.setPC(0x0030);

    bool movzShiftExecuted =
        cpu.step() &&
        cpu.readRegister(1) == (42ULL << 16) &&
        cpu.getPC() == 0x0034;

    std::cout << "MOVZ avec shift      : "
            << (movzShiftExecuted ? "OK" : "FAIL")
            << "\n";

    // V0.29 XZR test

    cpu.reset();

    cpu.writeRegister(0, 42);

    // ADD XZR, X0, X0
    cpuMemory.write32(0x0038, 0x8B00001F);

    cpu.setPC(0x0038);

    bool xzrWriteIgnored =
        cpu.step() &&
        cpu.readRegister(0) == 42 &&
        cpu.getPC() == 0x003C;

    std::cout << "Registre XZR        : "
            << (xzrWriteIgnored ? "OK" : "FAIL")
            << "\n";

    // Immediate ADD/SUB with Rd=31 address SP, not XZR.

    cpu.reset();
    cpu.writeRegister(0, 42);

    // ADD SP, X0, #10
    cpuMemory.write32(0x0040, 0x9100281F);

    cpu.setPC(0x0040);

    bool addXzrImmediate =
        cpu.step() &&
        cpu.readRegister(0) == 42 &&
        cpu.getSP() == 52 &&
        cpu.getPC() == 0x0044;

    // SUB SP, X0, #10
    cpuMemory.write32(0x0044, 0xD100281F);

    bool subXzrImmediate =
        cpu.step() &&
        cpu.readRegister(0) == 42 &&
        cpu.getSP() == 32 &&
        cpu.getPC() == 0x0048;

    std::cout << "ADD imm SP         : "
            << (addXzrImmediate ? "OK" : "FAIL")
            << "\n";

    std::cout << "SUB imm SP         : "
            << (subXzrImmediate ? "OK" : "FAIL")
            << "\n";

    // V0.32 MOVZ XZR test

    cpu.reset();

    // MOVZ XZR, #42
    cpuMemory.write32(0x0048, 0xD280053F);

    cpu.setPC(0x0048);

    bool movzXzrIgnored =
        cpu.step() &&
        cpu.readRegister(31) == 0 &&
        cpu.getPC() == 0x004C;

    std::cout << "MOVZ XZR           : "
            << (movzXzrIgnored ? "OK" : "FAIL")
            << "\n";

    // V0.33 ADD immediate shift test

    cpu.reset();
    cpu.writeRegister(0, 10);

    // ADD X1, X0, #1, LSL #12
    cpuMemory.write32(0x0050, 0x91400401);

    cpu.setPC(0x0050);

    bool addShiftExecuted =
        cpu.step() &&
        cpu.readRegister(1) == 4106 &&
        cpu.getPC() == 0x0054;

    std::cout << "ADD imm shift      : "
            << (addShiftExecuted ? "OK" : "FAIL")
            << "\n";

    // V0.34 SUB immediate shift test

    cpu.reset();
    cpu.writeRegister(0, 5000);

    // SUB X1, X0, #1, LSL #12
    cpuMemory.write32(0x0054, 0xD1400401);

    cpu.setPC(0x0054);

    bool subShiftExecuted =
        cpu.step() &&
        cpu.readRegister(1) == 904 &&
        cpu.getPC() == 0x0058;

    std::cout << "SUB imm shift      : "
            << (subShiftExecuted ? "OK" : "FAIL")
            << "\n";

    // V0.35 MOVK test

    cpu.reset();

    // MOVZ X0, #0xAAAA
    cpuMemory.write32(0x0058, 0xD2955540);

    // MOVK X0, #0x1234, LSL #16
    cpuMemory.write32(
        0x005C,
        0xF2A24680);

    cpu.setPC(0x0058);

    bool movkExecuted =
        cpu.step() &&
        cpu.step() &&
        cpu.readRegister(0) == 0x1234AAAAULL &&
        cpu.getPC() == 0x0060;

    std::cout << "Execution MOVK      : "
            << (movkExecuted ? "OK" : "FAIL")
            << "\n";


    // V0.35 MOVN test

    cpu.reset();

    // MOVN X1, #0x00FF
    cpuMemory.write32(
        0x0060,
        0x92801FE1);

    cpu.setPC(0x0060);

    bool movnExecuted =
        cpu.step() &&
        cpu.readRegister(1) == ~0x00FFULL &&
        cpu.getPC() == 0x0064;

    std::cout << "Execution MOVN      : "
            << (movnExecuted ? "OK" : "FAIL")
            << "\n";

    // ============================================================
    // V0.36 — TESTS INSTRUCTIONS LOGIQUES
    // ============================================================

    cpu.reset();

    // Valeurs de test
    cpu.writeRegister(0, 0xF0F0F0F0F0F0F0F0ULL);
    cpu.writeRegister(1, 0x0F0F0F0F0F0F0F0FULL);

    // ------------------------------------------------------------
    // AND X2, X0, X1
    // Résultat attendu : 0x0000000000000000
    // ------------------------------------------------------------

    uint32_t andInstruction =
        0x8A000000u |
        (1u << 16) |
        (0u << 5) |
        2u;

    cpuMemory.write32(0x0070, andInstruction);
    cpu.setPC(0x0070);

    bool andExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 0x0000000000000000ULL &&
        cpu.getPC() == 0x0074;

    std::cout << "Execution AND       : "
            << (andExecuted ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // ORR X2, X0, X1
    // Résultat attendu : 0xFFFFFFFFFFFFFFFF
    // ------------------------------------------------------------

    cpu.reset();

    cpu.writeRegister(0, 0xF0F0F0F0F0F0F0F0ULL);
    cpu.writeRegister(1, 0x0F0F0F0F0F0F0F0FULL);

    uint32_t orrInstruction =
        0xAA000000u |
        (1u << 16) |
        (0u << 5) |
        2u;

    cpuMemory.write32(0x0074, orrInstruction);
    cpu.setPC(0x0074);

    bool orrExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 0xFFFFFFFFFFFFFFFFULL &&
        cpu.getPC() == 0x0078;

    std::cout << "Execution ORR       : "
            << (orrExecuted ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // EOR X2, X0, X1
    // Résultat attendu : 0xFFFFFFFFFFFFFFFF
    // ------------------------------------------------------------

    cpu.reset();

    cpu.writeRegister(0, 0xF0F0F0F0F0F0F0F0ULL);
    cpu.writeRegister(1, 0x0F0F0F0F0F0F0F0FULL);

    uint32_t eorInstruction =
        0xCA000000u |
        (1u << 16) |
        (0u << 5) |
        2u;

    cpuMemory.write32(0x0078, eorInstruction);
    cpu.setPC(0x0078);

    bool eorExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 0xFFFFFFFFFFFFFFFFULL &&
        cpu.getPC() == 0x007C;

    std::cout << "Execution EOR       : "
            << (eorExecuted ? "OK" : "FAIL")
            << "\n";

    // ============================================================
    // V0.085 — AND / ORR / EOR immédiats
    // ============================================================

    {
        CPU cpu(cpuMemory);

        // AND X2, X0, #0xFF
        cpu.writeRegister(0, 0x1234);

        cpuMemory.write32(0, 0x92401C02);
        cpu.setPC(0);

        bool result = cpu.step();

        bool ok = result && cpu.readRegister(2) == 0x34;

        std::cout << "AND imm         : "
                << (ok ? "OK" : "FAIL") << '\n';
    }

    {
        CPU cpu(cpuMemory);

        // ORR X2, X0, #0xFF
        cpu.writeRegister(0, 0x1200);

        cpuMemory.write32(0, 0xB2401C02);
        cpu.setPC(0);

        bool result = cpu.step();

        uint64_t actual = cpu.readRegister(2);

        std::cout << "ORR résultat : 0x"
            << std::hex << actual << std::dec << '\n';

        bool ok = result && actual == 0x12FF;

        std::cout << "ORR imm         : "
                << (ok ? "OK" : "FAIL") << '\n';
    }

    {
        CPU cpu(cpuMemory);

        // EOR X2, X0, #0xFF
        cpu.writeRegister(0, 0x1234);

        cpuMemory.write32(0, 0xD2401C02);
        cpu.setPC(0);

        bool result = cpu.step();

        bool ok = result && cpu.readRegister(2) == 0x12CB;

        std::cout << "EOR imm         : "
                << (ok ? "OK" : "FAIL") << '\n';
    }

    // ============================================================
    // TEST ADD W (32 bits)
    // ============================================================

    cpu.reset();

    cpu.writeRegister(0, 50);
    cpu.writeRegister(1, 8);

    // ADD W2, W0, W1
    cpuMemory.write32(0x00, 0x0B010002);

    if (cpu.step() &&
        cpu.readRegister(2) == 58)
    {
        std::cout << "ADD W 32 bits       : OK\n";
    }
    else
    {
        std::cout << "ADD W 32 bits       : FAIL\n";
    }

    // ============================================================
    // TEST SUB W (32 bits)
    // ============================================================

    cpu.reset();

    cpu.writeRegister(0, 50);
    cpu.writeRegister(1, 8);

    // SUB W2, W0, W1
    cpuMemory.write32(0x00, 0x4B010002);

    if (cpu.step() &&
        cpu.readRegister(2) == 42)
    {
        std::cout << "SUB W 32 bits       : OK\n";
    }
    else
    {
        std::cout << "SUB W 32 bits       : FAIL\n";
    }

    // ============================================================
    // TEST ADD W immédiat (32 bits)
    // ============================================================

    cpu.reset();

    cpu.writeRegister(0, 32);

    // ADD W2, W0, #10
    cpuMemory.write32(0x00, 0x11002802);

    if (cpu.step() &&
        cpu.readRegister(2) == 42)
    {
        std::cout << "ADD W imm 32 bits   : OK\n";
    }
    else
    {
        std::cout << "ADD W imm 32 bits   : FAIL\n";
    }

    // ============================================================
    // V0.42 — TESTS CMP / CMN
    // ============================================================

    // ------------------------------------------------------------
    // CMP X0, X1
    // 10 - 10 = 0 → Flag Z activé
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00D0);

    cpu.writeRegister(0, 10);
    cpu.writeRegister(1, 10);

    // CMP X0, X1
    // Alias : SUBS XZR, X0, X1
    cpuMemory.write32(0x00D0, 0xEB01001F);

    bool cmpEqualOk =
        cpu.step() &&
        cpu.readRegister(31) == 0 &&
        (cpu.getPSTATE() & CPU::FLAG_Z);

    std::cout << "CMP égalité       : "
            << (cmpEqualOk ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // CMP X0, X1
    // 5 - 10 = -5 → Flag N activé
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00D4);

    cpu.writeRegister(0, 5);
    cpu.writeRegister(1, 10);

    // CMP X0, X1
    cpuMemory.write32(0x00D4, 0xEB01001F);

    bool cmpNegativeOk =
        cpu.step() &&
        (cpu.getPSTATE() & CPU::FLAG_N);

    std::cout << "CMP négatif       : "
            << (cmpNegativeOk ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // CMN X0, X1
    // 10 + 10 = 20 → Flag Z désactivé
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00D8);

    cpu.writeRegister(0, 10);
    cpu.writeRegister(1, 10);

    // CMN X0, X1
    // Alias : ADDS XZR, X0, X1
    cpuMemory.write32(0x00D8, 0xAB01001F);

    bool cmnOk =
        cpu.step() &&
        cpu.readRegister(31) == 0 &&
        (cpu.getPSTATE() & CPU::FLAG_Z) == 0 &&
        (cpu.getPSTATE() & CPU::FLAG_N) == 0;

    std::cout << "CMN addition      : "
            << (cmnOk ? "OK" : "FAIL")
            << "\n";

    // ============================================================
    // V0.43 — TEST B.EQ
    // ============================================================

    // B.EQ avec Z = 1 et offset +8
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(CPU::FLAG_Z);

    // Offset +8 : imm19 = 2
    cpuMemory.write32(0x00DC, 0x54000040);

    bool beqOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    std::cout << "B.EQ             : "
            << (beqOk ? "OK" : "FAIL")
            << "\n";

    // ============================================================
    // V0.44 — TEST B.NE (Branch if Not Equal)
    // ============================================================

    // Test 1 : B.NE doit sauter si Z = 0
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(0); // Z = 0

    // B.NE avec un offset de +8 octets
    cpuMemory.write32(0x00DC, 0x54000041);

    bool bneTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Test 2 : B.NE ne doit pas sauter si Z = 1
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(CPU::FLAG_Z); // Z = 1

    // B.NE avec un offset de +8 octets
    cpuMemory.write32(0x00DC, 0x54000041);

    bool bneNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E0;

    // Résultat global
    bool bneOk = bneTakenOk && bneNotTakenOk;

    std::cout << "B.NE             : "
            << (bneOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.45 — TEST B.GT (Branch if Greater Than)
    // ============================================================

    // Test 1 : B.GT doit sauter si Z = 0 et N = V = 0
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(0);

    cpuMemory.write32(0x00DC, 0x5400004C);

    bool bgtTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Test 2 : B.GT ne doit pas sauter si Z = 1
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(CPU::FLAG_Z);

    cpuMemory.write32(0x00DC, 0x5400004C);

    bool bgtNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E0;

    // Résultat global
    bool bgtOk = bgtTakenOk && bgtNotTakenOk;

    std::cout << "B.GT             : "
            << (bgtOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.46 — TEST B.LE (Branch if Less Than or Equal)
    // ============================================================

    // Test 1 : B.LE saute si Z = 1
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(CPU::FLAG_Z);

    cpuMemory.write32(0x00DC, 0x5400004D);

    bool bleZeroOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Test 2 : B.LE saute si N != V
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(CPU::FLAG_N);

    cpuMemory.write32(0x00DC, 0x5400004D);

    bool bleNegativeOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Test 3 : B.LE ne saute pas si Z = 0 et N = V
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(0);

    cpuMemory.write32(0x00DC, 0x5400004D);

    bool bleNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E0;

    // Résultat global
    bool bleOk =
        bleZeroOk &&
        bleNegativeOk &&
        bleNotTakenOk;

    std::cout << "B.LE             : "
            << (bleOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.47 — TEST B.LT (Branch if Less Than)
    // ============================================================

    // Test 1 : B.LT saute si N = 1 et V = 0
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(CPU::FLAG_N);

    cpuMemory.write32(0x00DC, 0x5400004B);

    bool bltTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Test 2 : B.LT saute si N = 0 et V = 1
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(CPU::FLAG_V);

    cpuMemory.write32(0x00DC, 0x5400004B);

    bool bltOverflowOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Test 3 : B.LT ne saute pas si N = V = 0
    cpu.reset();
    cpu.setPC(0x00DC);
    cpu.setPSTATE(0);

    cpuMemory.write32(0x00DC, 0x5400004B);

    bool bltNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E0;

    // Résultat global
    bool bltOk =
        bltTakenOk &&
        bltOverflowOk &&
        bltNotTakenOk;

    std::cout << "B.LT             : "
            << (bltOk ? "OK" : "FAIL")
            << std::endl;

    // Test B.GE : branchement pris si N == V
    cpu.reset();
    cpu.setPC(0x00DC);

    // B.GE +8 : 0x5400004A
    cpuMemory.write32(0x00DC, 0x5400004A);

    // Cas pris : N=0, V=0
    cpu.setPSTATE(0);

    bool bgeTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Cas non pris : N=1, V=0
    cpu.reset();
    cpu.setPC(0x00DC);
    cpuMemory.write32(0x00DC, 0x5400004A);
    cpu.setPSTATE(CPU::FLAG_N);

    bool bgeNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E0;

    bool bgeOk = bgeTakenOk && bgeNotTakenOk;

    std::cout << "B.GE             : "
            << (bgeOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.49 — TEST B.HI
    // ============================================================

    // B.HI +8 : 0x54000048
    cpu.reset();
    cpu.setPC(0x00DC);
    cpuMemory.write32(0x00DC, 0x54000048);

    // C=1 et Z=0 : branchement pris
    cpu.setPSTATE(CPU::FLAG_C);

    bool bhiTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // C=1 et Z=1 : branchement non pris
    cpu.reset();
    cpu.setPC(0x00DC);
    cpuMemory.write32(0x00DC, 0x54000048);
    cpu.setPSTATE(CPU::FLAG_C | CPU::FLAG_Z);

    bool bhiNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E0;

    bool bhiOk = bhiTakenOk && bhiNotTakenOk;

    std::cout << "B.HI             : "
            << (bhiOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.50 — TEST B.LS
    // ============================================================

    // B.LS +8 : 0x54000049

    // Cas pris : C=0, Z=0
    cpu.reset();
    cpu.setPC(0x00DC);
    cpuMemory.write32(0x00DC, 0x54000049);
    cpu.setPSTATE(0);

    bool blsTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Cas non pris : C=1, Z=0
    cpu.reset();
    cpu.setPC(0x00DC);
    cpuMemory.write32(0x00DC, 0x54000049);
    cpu.setPSTATE(CPU::FLAG_C);

    bool blsNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E0;

    // Cas pris : C=1, Z=1
    cpu.reset();
    cpu.setPC(0x00DC);
    cpuMemory.write32(0x00DC, 0x54000049);
    cpu.setPSTATE(CPU::FLAG_C | CPU::FLAG_Z);

    bool blsZeroOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    bool blsOk =
        blsTakenOk &&
        blsNotTakenOk &&
        blsZeroOk;

    std::cout << "B.LS             : "
            << (blsOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.51 — TEST B.VS
    // ============================================================

    // B.VS +8 : 0x54000046

    // Cas pris : V=1
    cpu.reset();
    cpu.setPC(0x00DC);
    cpuMemory.write32(0x00DC, 0x54000046);
    cpu.setPSTATE(CPU::FLAG_V);

    bool bvsTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E4;

    // Cas non pris : V=0
    cpu.reset();
    cpu.setPC(0x00DC);
    cpuMemory.write32(0x00DC, 0x54000046);
    cpu.setPSTATE(0);

    bool bvsNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00E0;

    bool bvsOk =
        bvsTakenOk &&
        bvsNotTakenOk;

    std::cout << "B.VS             : "
            << (bvsOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.52 — TEST B.VC
    // ============================================================

    // B.VC +8 : 0x54000047

    // Cas pris : V=0
    cpu.reset();
    cpu.setPC(0x00E8);
    cpuMemory.write32(0x00E8, 0x54000047);
    cpu.setPSTATE(0);

    bool bvcTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00F0;

    // Cas non pris : V=1
    cpu.reset();
    cpu.setPC(0x00E8);
    cpuMemory.write32(0x00E8, 0x54000047);
    cpu.setPSTATE(CPU::FLAG_V);

    bool bvcNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00EC;

    bool bvcOk =
        bvcTakenOk &&
        bvcNotTakenOk;

    std::cout << "B.VC             : "
            << (bvcOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.53 — TEST B.HS
    // ============================================================

    // B.HS +8 : 0x54000042

    // Cas pris : C=1
    cpu.reset();
    cpu.setPC(0x00F4);
    cpuMemory.write32(0x00F4, 0x54000042);
    cpu.setPSTATE(CPU::FLAG_C);

    bool bhsTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00FC;

    // Cas non pris : C=0
    cpu.reset();
    cpu.setPC(0x00F4);
    cpuMemory.write32(0x00F4, 0x54000042);
    cpu.setPSTATE(0);

    bool bhsNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x00F8;

    bool bhsOk =
        bhsTakenOk &&
        bhsNotTakenOk;

    std::cout << "B.HS             : "
            << (bhsOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.54 — TEST B.LO / B.CC
    // ============================================================

    // B.LO +8 : 0x54000043

    // Cas pris : C=0
    cpu.reset();
    cpu.setPC(0x0100);
    cpuMemory.write32(0x0100, 0x54000043);
    cpu.setPSTATE(0);

    bool bloTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x0108;

    // Cas non pris : C=1
    cpu.reset();
    cpu.setPC(0x0100);
    cpuMemory.write32(0x0100, 0x54000043);
    cpu.setPSTATE(CPU::FLAG_C);

    bool bloNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x0104;

    bool bloOk =
        bloTakenOk &&
        bloNotTakenOk;

    std::cout << "B.LO             : "
            << (bloOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.55 — TEST B.MI
    // ============================================================

    // B.MI +8 : 0x54000044

    // Cas pris : N=1
    cpu.reset();
    cpu.setPC(0x0110);
    cpuMemory.write32(0x0110, 0x54000044);
    cpu.setPSTATE(CPU::FLAG_N);

    bool bmiTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x0118;

    // Cas non pris : N=0
    cpu.reset();
    cpu.setPC(0x0110);
    cpuMemory.write32(0x0110, 0x54000044);
    cpu.setPSTATE(0);

    bool bmiNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x0114;

    bool bmiOk =
        bmiTakenOk &&
        bmiNotTakenOk;

    std::cout << "B.MI             : "
            << (bmiOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.56 — TEST B.PL
    // ============================================================

    // B.PL +8 : 0x54000045

    // Cas pris : N=0
    cpu.reset();
    cpu.setPC(0x0120);
    cpuMemory.write32(0x0120, 0x54000045);
    cpu.setPSTATE(0);

    bool bplTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x0128;

    // Cas non pris : N=1
    cpu.reset();
    cpu.setPC(0x0120);
    cpuMemory.write32(0x0120, 0x54000045);
    cpu.setPSTATE(CPU::FLAG_N);

    bool bplNotTakenOk =
        cpu.step() &&
        cpu.getPC() == 0x0124;

    bool bplOk =
        bplTakenOk &&
        bplNotTakenOk;

    std::cout << "B.PL             : "
            << (bplOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.57 — TEST B (Branch Always)
    // ============================================================

    // B +8 : 0x14000002

    cpu.reset();
    cpu.setPC(0x0130);
    cpuMemory.write32(0x0130, 0x14000002);

    bool balOk1 =
        cpu.step() &&
        cpu.getPC() == 0x0138;

    // Les flags ne doivent pas influencer B
    cpu.reset();
    cpu.setPC(0x0130);
    cpuMemory.write32(0x0130, 0x14000002);

    cpu.setPSTATE(
        CPU::FLAG_N |
        CPU::FLAG_Z |
        CPU::FLAG_C |
        CPU::FLAG_V
    );

    bool balOk2 =
        cpu.step() &&
        cpu.getPC() == 0x0138;

    bool balOk =
        balOk1 &&
        balOk2;

    std::cout << "B.AL             : "
            << (balOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.58 — TEST BL
    // ============================================================

    // BL +8 : 0x94000002

    cpu.reset();
    cpu.setPC(0x0140);
    cpuMemory.write32(0x0140, 0x94000002);

    bool blStepOk = cpu.step();

    bool blTargetOk =
        cpu.getPC() == 0x0148;

    bool blLinkOk =
        cpu.readRegister(30) == 0x0144;

    bool blOk =
        blStepOk &&
        blTargetOk &&
        blLinkOk;

    std::cout << "BL               : "
            << (blOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.59 — TEST RET
    // ============================================================

    // RET X30 : 0xD65F03C0

    cpu.reset();
    cpu.setPC(0x0200);

    cpu.writeRegister(30, 0x0300);
    cpuMemory.write32(0x0200, 0xD65F03C0);

    bool retStepOk =
        cpu.step();

    bool retTargetOk =
        cpu.getPC() == 0x0300;

    bool retOk =
        retStepOk &&
        retTargetOk;

    std::cout << "RET              : "
            << (retOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.059 — TEST BR
    // ============================================================

    // BR X1 : 0xD61F0020

    cpu.reset();
    cpu.setPC(0x0200);

    // X1 contient l'adresse de destination
    cpu.writeRegister(1, 0x0300);

    // BR X1
    cpuMemory.write32(
        0x0200,
        0xD61F0020
    );

    bool brStepOk =
        cpu.step();

    bool brTargetOk =
        cpu.getPC() == 0x0300;

    bool brOk =
        brStepOk &&
        brTargetOk;

    std::cout << "BR               : "
            << (brOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.060 — Test BLR
    // BLR X1 : saute vers l'adresse contenue dans X1
    // ============================================================

    cpu.reset();

    cpu.setPC(0x0400);

    // X1 contient l'adresse cible
    cpu.writeRegister(1, 0x0500);

    // BLR X1
    cpuMemory.write32(0x0400, 0xD63F0020);

    bool blrStepOk = cpu.step();

    bool blrTargetOk =
        cpu.getPC() == 0x0500;

    bool blrLinkOk =
        cpu.readRegister(30) == 0x0404;

    bool blrOk =
        blrStepOk &&
        blrTargetOk &&
        blrLinkOk;

    std::cout << "BLR              : "
            << (blrOk ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.061 — Test ADD/SUB avec SP
    // ============================================================

    cpu.reset();

    cpu.setSP(0x1000);
    cpu.setPC(0x0000);

    // ADD SP, SP, #16
    cpuMemory.write32(0x0000, 0x910043FF);

    bool addSPOk =
        cpu.step() &&
        cpu.getSP() == 0x1010;

    // SUB SP, SP, #16
    cpuMemory.write32(0x0004, 0xD10043FF);

    bool subSPOk =
        cpu.step() &&
        cpu.getSP() == 0x1000;

    std::cout << "ADD SP            : "
            << (addSPOk ? "OK" : "FAIL")
            << "\n";

    std::cout << "SUB SP            : "
            << (subSPOk ? "OK" : "FAIL")
            << "\n";

    // ============================================================
    // TEST SUB W immédiat (32 bits)
    // ============================================================

    cpu.reset();

    cpu.writeRegister(0, 50);

    // SUB W2, W0, #10
    cpuMemory.write32(0x00, 0x51002802);

    if (cpu.step() &&
        cpu.readRegister(2) == 40)
    {
        std::cout << "SUB W imm 32 bits   : OK\n";
    }
    else
    {
        std::cout << "SUB W imm 32 bits   : FAIL\n";
    }

    // ============================================================
    // TESTS ARM64 PSTATE FLAGS
    // ============================================================

    std::cout << "\n# TESTS PSTATE FLAGS\n";

    // SUB X2, X0, X1
    // Encodage : 0xCB010002

    // ------------------------------------------------------------
    // FLAG Z : 10 - 10 = 0
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00B0);

    cpu.writeRegister(0, 10);
    cpu.writeRegister(1, 10);

    cpuMemory.write32(0x00B0, 0xEB010002);

    cpu.step();

    std::cout << "Flag Z (Zero)       : "
            << ((cpu.getPSTATE() & CPU::FLAG_Z) ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // FLAG N : 5 - 10 = -5
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00B4);

    cpu.writeRegister(0, 5);
    cpu.writeRegister(1, 10);

    cpuMemory.write32(0x00B4, 0xEB010002);

    cpu.step();

    std::cout << "Flag N (Negative)   : "
            << ((cpu.getPSTATE() & CPU::FLAG_N) ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // FLAG C : 10 - 5 = 5, sans emprunt
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00B8);

    cpu.writeRegister(0, 10);
    cpu.writeRegister(1, 5);

    cpuMemory.write32(0x00B8, 0xEB010002);

    cpu.step();

    std::cout << "Flag C (Carry)      : "
            << ((cpu.getPSTATE() & CPU::FLAG_C) ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // FLAG V : débordement signé
    // 0x7FFFFFFFFFFFFFFF - (-1) = 0x8000000000000000
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00BC);

    cpu.writeRegister(0, 0x7FFFFFFFFFFFFFFFULL);
    cpu.writeRegister(1, 0xFFFFFFFFFFFFFFFFULL);

    cpuMemory.write32(0x00BC, 0xEB010002);

    cpu.step();

    std::cout << "Flag V (Overflow)   : "
            << ((cpu.getPSTATE() & CPU::FLAG_V) ? "OK" : "FAIL")
            << "\n";

    // ============================================================
    // TEST ADDS W - FLAGS
    // ============================================================

    cpu.reset();
    cpu.setPC(0x00C0);

    // 10 + 10 = 20
    cpu.writeRegister(0, 10);
    cpu.writeRegister(1, 10);

    // ADDS W2, W0, W1
    cpuMemory.write32(0x00C0, 0x2B010002);

    cpu.step();

    bool addsWOk =
        cpu.readRegister(2) == 20 &&
        (cpu.getPSTATE() & CPU::FLAG_Z) == 0 &&
        (cpu.getPSTATE() & CPU::FLAG_N) == 0;

    std::cout << "ADDS W flags      : "
            << (addsWOk ? "OK" : "FAIL")
            << "\n";

    // ============================================================
    // V0.41 — TESTS ADDS IMMÉDIAT
    // ============================================================

    // ------------------------------------------------------------
    // ADDS W immédiat : résultat normal
    // 10 + 10 = 20
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00C4);

    cpu.writeRegister(0, 10);

    // ADDS W2, W0, #10
    cpuMemory.write32(0x00C4, 0x31002802);

    cpu.step();

    bool addsWImmediateOk =
        cpu.readRegister(2) == 20 &&
        (cpu.getPSTATE() & CPU::FLAG_Z) == 0 &&
        (cpu.getPSTATE() & CPU::FLAG_N) == 0;

    std::cout << "ADDS W imm       : "
            << (addsWImmediateOk ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // ADDS W immédiat : retenue (Carry)
    // 0xFFFFFFFF + 1 = 0
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00C8);

    cpu.writeRegister(0, 0xFFFFFFFFULL);

    // ADDS W2, W0, #1
    cpuMemory.write32(0x00C8, 0x31000402);

    cpu.step();

    bool addsWCarryOk =
        cpu.readRegister(2) == 0 &&
        (cpu.getPSTATE() & CPU::FLAG_Z) &&
        (cpu.getPSTATE() & CPU::FLAG_C);

    std::cout << "ADDS W Carry     : "
            << (addsWCarryOk ? "OK" : "FAIL")
            << "\n";

    // ------------------------------------------------------------
    // ADDS X immédiat : overflow signé
    // 0x7FFFFFFFFFFFFFFF + 1
    // = 0x8000000000000000
    // ------------------------------------------------------------

    cpu.reset();
    cpu.setPC(0x00CC);

    cpu.writeRegister(0, 0x7FFFFFFFFFFFFFFFULL);

    // ADDS X2, X0, #1
    cpuMemory.write32(0x00CC, 0xB1000402);

    cpu.step();

    bool addsXOverflowOk =
        cpu.readRegister(2) == 0x8000000000000000ULL &&
        (cpu.getPSTATE() & CPU::FLAG_N) &&
        (cpu.getPSTATE() & CPU::FLAG_V);

    std::cout << "ADDS X Overflow  : "
            << (addsXOverflowOk ? "OK" : "FAIL")
            << "\n";

    // ============================================================
    // V0.37 — TESTS SECURISATION DES REGISTRES
    // ============================================================

    // Test lecture XZR
    cpu.reset();

    bool xzrReadSafe =
        cpu.readRegister(31) == 0;

    std::cout << "Lecture XZR securisee : "
            << (xzrReadSafe ? "OK" : "FAIL")
            << "\n";

    // Test écriture XZR via instruction
    cpu.reset();

    cpu.writeRegister(0, 123);
    cpu.writeRegister(31, 999);

    bool xzrWriteSafe =
        cpu.readRegister(31) == 999;

    std::cout << "Ecriture registre X31 : "
            << (xzrWriteSafe ? "OK" : "FAIL")
            << "\n";

    // Test registre invalide
    bool invalidRegisterSafe =
        !cpu.writeRegister(32, 123) &&
        cpu.hasError();

    std::cout << "Registre invalide securise : "
            << (invalidRegisterSafe ? "OK" : "FAIL")
            << "\n";

    std::cout << "Execution NOP      : "
          << (nopExecuted ? "OK" : "FAIL")
          << "\n";

    std::cout << "SWITCH2EMU v0.058\n";
    std::cout << "PAGED MEMORY / INTERRUPTS / TIMER\n";
    std::cout << "=================================\n\n";

    // =========================================================
    // MEMOIRE
    // =========================================================

    Memory memory(0x2000);

    // =========================================================
    // BUS
    // =========================================================

    Bus bus(memory);

    // =========================================================
    // TEST DEVICE
    // =========================================================

    TestDevice testDevice;

    bool testDeviceMapped =
        bus.mapDevice(
            0x1000,
            0x10,
            testDevice,
            "TestDevice");

    std::cout << "Mapping TestDevice : "
              << (testDeviceMapped ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // INTERRUPT CONTROLLER
    // =========================================================

    InterruptController interruptController;

    bool irqMapped =
        bus.mapDevice(
            0x2000,
            0x0C,
            interruptController,
            "IRQ Controller");

    std::cout << "Mapping IRQ Controller : "
              << (irqMapped ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // TIMER
    // =========================================================

    Timer timer(
        interruptController,
        4);

    bool timerMapped =
        bus.mapDevice(
            0x3000,
            0x18,
            timer,
            "Timer");

    std::cout << "Mapping Timer : "
              << (timerMapped ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // INTERRUPT TESTS
    // =========================================================

    interruptController.clearIRQ(3);

    interruptController.raiseIRQ(3);

    bool irqTest =
        interruptController.isPending(3);

    std::cout << "IRQ Controller   : "
              << (irqTest ? "OK" : "FAIL")
              << "\n";

    interruptController.clearIRQ(3);

    bool irqClearTest =
        !interruptController.isPending(3);

    std::cout << "IRQ Clear         : "
              << (irqClearTest ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // TIMER TESTS
    // =========================================================

    timer.write64(
        0x08,
        10);

    timer.write32(
        0x10,
        1);

    bool timerConfig =
        timer.read64(0x08) == 10 &&
        timer.read64(0x00) == 10;

    std::cout << "Timer Configuration : "
              << (timerConfig ? "OK" : "FAIL")
              << "\n";

    timer.tick(4);

    bool timerCountdown =
        timer.read64(0x00) == 6;

    std::cout << "Timer Countdown     : "
              << (timerCountdown ? "OK" : "FAIL")
              << "\n";

    timer.tick(6);

    bool timerIrq =
        interruptController.isPending(4);

    std::cout << "Timer IRQ           : "
              << (timerIrq ? "OK" : "FAIL")
              << "\n";

    interruptController.clearIRQ(4);

    bool timerIrqClear =
        !interruptController.isPending(4);

    std::cout << "Timer IRQ Clear     : "
              << (timerIrqClear ? "OK" : "FAIL")
              << "\n";

    timer.write32(
        0x10,
        0);

    bool timerDisable =
        timer.read32(0x10) == 0;

    std::cout << "Timer Disable       : "
              << (timerDisable ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // TEST DEVICE / MMIO
    // =========================================================

    bool idRead =
        bus.read32(0x1000) == 0x53543245;

    std::cout << "Lecture ID       : "
              << (idRead ? "OK" : "FAIL")
              << "\n";

    bus.write32(
        0x1004,
        0xAABBCCDD);

    bool controlTest =
        bus.read32(0x1004) == 0xAABBCCDD;

    std::cout << "CONTROL R/W      : "
              << (controlTest ? "OK" : "FAIL")
              << "\n";

    bool statusTest =
        bus.read32(0x1008) == 1;

    std::cout << "STATUS           : "
              << (statusTest ? "OK" : "FAIL")
              << "\n";

    bus.write32(
        0x100C,
        0xDEADBEEF);

    bool dataTest =
        bus.read32(0x100C) == 0xDEADBEEF;

    std::cout << "DATA R/W         : "
              << (dataTest ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // RAM VIA BUS
    // =========================================================

    bus.write32(
        0x0800,
        0xCAFEBABE);

    bool ramTest =
        bus.read32(0x0800) == 0xCAFEBABE;

    std::cout << "RAM via Bus      : "
              << (ramTest ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // ROUTAGE MMIO
    // =========================================================

    bus.write32(
        0x1004,
        0x11223344);

    bool mmioRouting =
        testDevice.read32(0x04) == 0x11223344;

    std::cout << "Routage MMIO     : "
              << (mmioRouting ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // LECTURE SEULE
    // =========================================================

    bus.clearError();

    bus.write32(
        0x1000,
        0xFFFFFFFF);

    bool idReadOnly =
        bus.read32(0x1000) == 0x53543245;

    std::cout << "ID lecture seule : "
              << (idReadOnly ? "OK" : "FAIL")
              << "\n";

    bus.clearError();

    bus.write32(
        0x1008,
        0xFFFFFFFF);

    bool statusReadOnly =
        bus.read32(0x1008) == 1;

    std::cout << "STATUS read-only : "
              << (statusReadOnly ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // ACCES MMIO INVALIDE
    // =========================================================

    bus.clearError();

    bus.write32(
        0x100E,
        0x12345678);

    bool invalidMmio =
        bus.hasError();

    std::cout << "Acces MMIO invalide : "
              << (invalidMmio ? "OK" : "FAIL")
              << "\n";

    if (bus.hasError())
    {
        std::cout << "Message : "
                  << bus.getLastError()
                  << "\n";
    }

    // =========================================================
    // DEPASSEMENT MMIO
    // =========================================================

    bus.clearError();

    bus.read64(0x100C);

    bool mmioOverflow =
        bus.hasError();

    std::cout << "Depassement MMIO : "
              << (mmioOverflow ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // TEST CHEVAUCHEMENT
    // =========================================================

    TestDevice secondDevice;

    bool overlapTest =
        !bus.mapDevice(
            0x1008,
            0x10,
            secondDevice,
            "Overlap");

    std::cout << "Chevauchement    : "
              << (overlapTest ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // V0.19 — PAGED MEMORY
    // =========================================================

    std::cout << "\nV0.19 PAGED MEMORY\n";
    std::cout << "==================\n";

    Memory pagedMemory(0x10000);

    // ---------------------------------------------------------
    // Page jamais utilisée
    // ---------------------------------------------------------

    bool untouchedPage =
        pagedMemory.read32(0x4000) == 0;

    std::cout << "Page non allouee     : "
              << (untouchedPage ? "OK" : "FAIL")
              << "\n";

    // ---------------------------------------------------------
    // Ecriture dans une page
    // ---------------------------------------------------------

    pagedMemory.write32(
        0x4000,
        0x12345678);

    bool pageWrite =
        pagedMemory.read32(0x4000)
        == 0x12345678;

    std::cout << "Ecriture page        : "
              << (pageWrite ? "OK" : "FAIL")
              << "\n";

    // ---------------------------------------------------------
    // Frontière de page
    // ---------------------------------------------------------

    pagedMemory.write8(
        0x0FFF,
        0xAA);

    pagedMemory.write8(
        0x1000,
        0xBB);

    bool pageBoundary =
        pagedMemory.read8(0x0FFF) == 0xAA &&
        pagedMemory.read8(0x1000) == 0xBB;

    std::cout << "Frontiere de page    : "
              << (pageBoundary ? "OK" : "FAIL")
              << "\n";

    // ---------------------------------------------------------
    // Read32 inter-page
    // ---------------------------------------------------------

    pagedMemory.write32(
        0x0FFE,
        0xAABBCCDD);

    bool crossPage32 =
        pagedMemory.read32(0x0FFE)
        == 0xAABBCCDD;

    std::cout << "Read32 inter-page    : "
              << (crossPage32 ? "OK" : "FAIL")
              << "\n";

    // ---------------------------------------------------------
    // Read64 inter-page
    // ---------------------------------------------------------

    pagedMemory.write64(
        0x0FFC,
        0x1122334455667788ULL);

    bool crossPage64 =
        pagedMemory.read64(0x0FFC)
        == 0x1122334455667788ULL;

    std::cout << "Read64 inter-page    : "
              << (crossPage64 ? "OK" : "FAIL")
              << "\n";

    // ---------------------------------------------------------
    // Mémoire virtuelle de 12 Go
    // ---------------------------------------------------------

    Memory largeMemory(
        12ULL *
        1024ULL *
        1024ULL *
        1024ULL);

    largeMemory.write32(
        0x80000000ULL,
        0xCAFEBABE);

    bool largeMemoryTest =
        largeMemory.read32(
            0x80000000ULL)
        == 0xCAFEBABE;

    std::cout << "Memoire 12 Go        : "
              << (largeMemoryTest ? "OK" : "FAIL")
              << "\n";

    // =========================================================
    // FIN
    // =========================================================

    std::cout << "\n=================================\n";
    std::cout << "V0.19 PAGED MEMORY\n";
    std::cout << "TESTS TERMINES\n";
    std::cout << "=================================\n";

    // ============================================================
    // V0.062 — TEST LDR / STR 64 BITS
    // ============================================================

    std::cout << "\nV0.062 LDR / STR\n";
    std::cout << "================\n";

    cpu.reset();

    // X0 contient la valeur à stocker
    cpu.writeRegister(0, 0x1122334455667788ULL);

    // X1 contient l'adresse mémoire
    cpu.writeRegister(1, 0x0800);

    // STR X0, [X1, #0]
    // Encodage : F9000020
    cpuMemory.write32(0x0100, 0xF9000020);

    // LDR X2, [X1, #0]
    // Encodage : F9400022
    cpuMemory.write32(0x0104, 0xF9400022);

    cpu.setPC(0x0100);

    bool strExecuted =
        cpu.step() &&
        cpuMemory.read64(0x0800) == 0x1122334455667788ULL;

    bool ldrExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 0x1122334455667788ULL;

    std::cout << "STR X0 mémoire     : "
            << (strExecuted ? "OK" : "FAIL")
            << "\n";

    std::cout << "LDR X2 mémoire     : "
            << (ldrExecuted ? "OK" : "FAIL")
            << "\n";

    std::cout << "\nV0.063 LDR / STR avec offset\n";
    std::cout << "============================\n";

    cpu.reset();

    cpu.writeRegister(0, 0xAABBCCDDEEFF0011ULL);
    cpu.writeRegister(1, 0x0800);

    // STR X0, [X1, #8]
    cpuMemory.write32(0x0100, 0xF9000420);

    // LDR X2, [X1, #8]
    cpuMemory.write32(0x0104, 0xF9400422);

    cpu.setPC(0x0100);

    bool strOffsetExecuted =
        cpu.step() &&
        cpuMemory.read64(0x0808) == 0xAABBCCDDEEFF0011ULL;

    bool ldrOffsetExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 0xAABBCCDDEEFF0011ULL;

    std::cout << "STR X0 [X1,#8] : "
            << (strOffsetExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "LDR X2 [X1,#8] : "
            << (ldrOffsetExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "\nV0.064 LDR / STR avec SP\n";
    std::cout << "=======================\n";

    cpu.reset();

    cpu.setSP(0x0900);
    cpu.writeRegister(0, 0x123456789ABCDEF0ULL);

    // STR X0, [SP, #8]
    cpuMemory.write32(0x0100, 0xF90007E0);

    // LDR X2, [SP, #8]
    cpuMemory.write32(0x0104, 0xF94007E2);

    cpu.setPC(0x0100);

    bool strSPExecuted =
        cpu.step() &&
        cpuMemory.read64(0x0908) == 0x123456789ABCDEF0ULL;

    bool ldrSPExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 0x123456789ABCDEF0ULL;

    std::cout << "STR X0 [SP,#8] : "
            << (strSPExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "LDR X2 [SP,#8] : "
            << (ldrSPExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "\nV0.065 Plusieurs offsets et limites\n";
    std::cout << "==================================\n";

    cpu.reset();

    cpu.writeRegister(0, 0x1122334455667788ULL);
    cpu.writeRegister(1, 0x0800);

    // STR X0, [X1, #16] (imm12 = 2)
    cpuMemory.write32(0x0100, 0xF9000820);

    // LDR X2, [X1, #16]
    cpuMemory.write32(0x0104, 0xF9400822);

    cpu.setPC(0x0100);

    bool offset16 =
        cpu.step() &&
        cpuMemory.read64(0x0810) == 0x1122334455667788ULL &&
        cpu.step() &&
        cpu.readRegister(2) == 0x1122334455667788ULL;

    std::cout << "Offset #16       : "
            << (offset16 ? "OK" : "FAIL") << "\n";

    // Test offset #24
    cpu.reset();

    cpu.writeRegister(0, 0xAABBCCDDEEFF0011ULL);
    cpu.writeRegister(1, 0x0800);

    // STR X0, [X1, #24] (imm12 = 3)
    cpuMemory.write32(0x0200, 0xF9000C20);

    // LDR X2, [X1, #24]
    cpuMemory.write32(0x0204, 0xF9400C22);

    cpu.setPC(0x0200);

    bool offset24 =
        cpu.step() &&
        cpuMemory.read64(0x0818) == 0xAABBCCDDEEFF0011ULL &&
        cpu.step() &&
        cpu.readRegister(2) == 0xAABBCCDDEEFF0011ULL;

    std::cout << "Offset #24       : "
            << (offset24 ? "OK" : "FAIL") << "\n";

    // Test accès hors limites
    cpu.reset();

    cpu.writeRegister(0, 0x123456789ABCDEF0ULL);
    cpu.writeRegister(1, 0xFFFFFFFFFFFFFFF8ULL);

    // STR X0, [X1, #8]
    cpuMemory.write32(0x0300, 0xF9000420);

    cpu.setPC(0x0300);

    bool invalidAccess = !cpu.step();

    std::cout << "Limite mémoire    : "
            << (invalidAccess ? "OK" : "FAIL") << "\n";

    std::cout << "\nV0.066 LDR / STR 32 bits\n";
    std::cout << "=======================\n";

    cpu.reset();

    cpu.writeRegister(0, 0xFFFFFFFFAABBCCDDULL);
    cpu.writeRegister(1, 0x0800);

    // STR W0, [X1, #8]
    cpuMemory.write32(0x0400, 0xB9000820);

    // LDR W2, [X1, #8]
    cpuMemory.write32(0x0404, 0xB9400822);

    cpu.setPC(0x0400);

    bool strWExecuted =
        cpu.step() &&
        cpuMemory.read32(0x0808) == 0xAABBCCDD;

    bool ldrWExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 0xAABBCCDDULL;

    std::cout << "STR W0 [X1,#8] : "
            << (strWExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "LDR W2 [X1,#8] : "
            << (ldrWExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "\nV0.067 LDR / STR 32 bits avec SP\n";
    std::cout << "===============================\n";

    cpu.reset();

    cpu.setSP(0x0900);
    cpu.writeRegister(0, 0xFFFFFFFFAABBCCDDULL);

    // STR W0, [SP, #8]
    cpuMemory.write32(0x0500, 0xB9000BE0);

    // LDR W2, [SP, #8]
    cpuMemory.write32(0x0504, 0xB9400BE2);

    cpu.setPC(0x0500);

    bool strWSPExecuted =
        cpu.step() &&
        cpuMemory.read32(0x0908) == 0xAABBCCDD;

    bool ldrWSPExecuted =
        cpu.step() &&
        cpu.readRegister(2) == 0xAABBCCDDULL;

    std::cout << "STR W0 [SP,#8] : "
            << (strWSPExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "LDR W2 [SP,#8] : "
            << (ldrWSPExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "\nV0.068 LDR / STR pré-indexé\n";
    std::cout << "===========================\n";

    cpu.reset();

    cpu.writeRegister(0, 0x1122334455667788ULL);
    cpu.writeRegister(1, 0x0800);

    // STR X0, [X1, #8]!
    cpuMemory.write32(0x0600, 0xF8008C20);

    // LDR X2, [X1, #8]!
    cpuMemory.write32(0x0604, 0xF8408C22);

    cpu.setPC(0x0600);

    bool strPreIndexExecuted =
        cpu.step() &&
        cpu.readRegister(1) == 0x0808 &&
        cpuMemory.read64(0x0808) == 0x1122334455667788ULL;

    cpu.writeRegister(1, 0x0800);
    bool ldrPreIndexExecuted =
        cpu.step() &&
        cpu.readRegister(1) == 0x0808 &&
        cpu.readRegister(2) == 0x1122334455667788ULL;

    std::cout << "STR X0 [X1,#8]! : "
            << (strPreIndexExecuted ? "OK" : "FAIL") << "\n";

    std::cout << "LDR X2 [X1,#8]! : "
            << (ldrPreIndexExecuted ? "OK" : "FAIL") << "\n";

    // ============================================================
    // V0.069 - LDR / STR 64 bits post-indexés
    // ============================================================

    std::cout << "\nV0.069 LDR / STR post-indexé 64 bits\n";
    std::cout << "====================================\n";

    cpu.reset();

    cpu.writeRegister(0, 0x1122334455667788ULL);
    cpu.writeRegister(1, 0x0800);

    // STR X0, [X1], #8
    cpuMemory.write32(0x0700, 0xF8008420);

    // LDR X2, [X1], #8
    cpuMemory.write32(0x0704, 0xF8408422);

    cpu.setPC(0x0700);

    bool strPost64 =
        cpu.step() &&
        cpuMemory.read64(0x0800) == 0x1122334455667788ULL &&
        cpu.readRegister(1) == 0x0808;

    cpu.writeRegister(1, 0x0800);
    bool ldrPost64 =
        cpu.step() &&
        cpu.readRegister(2) == 0x1122334455667788ULL &&
        cpu.readRegister(1) == 0x0808;

    std::cout << "STR X0 [X1],#8 : "
            << (strPost64 ? "OK" : "FAIL") << "\n";

    std::cout << "LDR X2 [X1],#8 : "
            << (ldrPost64 ? "OK" : "FAIL") << "\n";


    // ============================================================
    // V0.070 - LDR / STR 32 bits pré-indexés
    // ============================================================

    std::cout << "\nV0.070 LDR / STR pré-indexé 32 bits\n";
    std::cout << "===================================\n";

    cpu.reset();

    cpu.writeRegister(0, 0xFFFFFFFFAABBCCDDULL);
    cpu.writeRegister(1, 0x0800);

    // STR W0, [X1, #8]!
    cpuMemory.write32(0x0800, 0xB8008C20);

    // LDR W2, [X1, #8]!
    cpuMemory.write32(0x0804, 0xB8408C22);

    cpu.setPC(0x0800);

    bool strPre32 =
        cpu.step() &&
        cpu.readRegister(1) == 0x0808 &&
        cpuMemory.read32(0x0808) == 0xAABBCCDD;

    // On remet X1 à son adresse initiale avant le LDR
    cpu.writeRegister(1, 0x0800);

    bool ldrPre32 =
        cpu.step() &&
        cpu.readRegister(1) == 0x0808 &&
        cpu.readRegister(2) == 0xAABBCCDDULL;

    std::cout << "STR W0 [X1,#8]! : "
            << (strPre32 ? "OK" : "FAIL") << "\n";

    std::cout << "LDR W2 [X1,#8]! : "
            << (ldrPre32 ? "OK" : "FAIL") << "\n";


    // ============================================================
    // V0.071 - LDR / STR 32 bits post-indexés
    // ============================================================

    std::cout << "\nV0.071 LDR / STR post-indexé 32 bits\n";
    std::cout << "====================================\n";

    cpu.reset();

    cpu.writeRegister(0, 0xFFFFFFFF12345678ULL);
    cpu.writeRegister(1, 0x0800);

    // STR W0, [X1], #8
    cpuMemory.write32(0x0900, 0xB8008420);

    // LDR W2, [X1], #8
    cpuMemory.write32(0x0904, 0xB8408422);

    cpu.setPC(0x0900);

    bool strPost32 =
        cpu.step() &&
        cpuMemory.read32(0x0800) == 0x12345678 &&
        cpu.readRegister(1) == 0x0808;

    // On remet X1 à son adresse initiale avant le LDR
    cpu.writeRegister(1, 0x0800);

    bool ldrPost32 =
        cpu.step() &&
        cpu.readRegister(2) == 0x12345678ULL &&
        cpu.readRegister(1) == 0x0808;

    std::cout << "STR W0 [X1],#8 : "
            << (strPost32 ? "OK" : "FAIL") << "\n";

    std::cout << "LDR W2 [X1],#8 : "
            << (ldrPost32 ? "OK" : "FAIL") << "\n";


    // ============================================================
    // V0.072 - LDR / STR avec registre d'offset
    // ============================================================

    std::cout << "\nV0.072 LDR / STR registre offset\n";
    std::cout << "================================\n";

    cpu.reset();

    cpu.writeRegister(0, 0x1122334455667788ULL);
    cpu.writeRegister(1, 0x0800);
    cpu.writeRegister(2, 0x0010);

    // STR X0, [X1, X2]
    cpuMemory.write32(0x0A00, 0xF8226820);

    // LDR X3, [X1, X2]
    cpuMemory.write32(0x0A04, 0xF8626823);

    cpu.setPC(0x0A00);

    bool strRegisterOffset =
        cpu.step() &&
        cpuMemory.read64(0x0810) == 0x1122334455667788ULL;

    bool ldrRegisterOffset =
        cpu.step() &&
        cpu.readRegister(3) == 0x1122334455667788ULL;

    std::cout << "STR X0 [X1,X2] : "
            << (strRegisterOffset ? "OK" : "FAIL") << "\n";

    std::cout << "LDR X3 [X1,X2] : "
            << (ldrRegisterOffset ? "OK" : "FAIL") << "\n";

    std::cout << "\n# V0.073 LDRB / STRB\n";

    // STRB W0, [X1,#8]
    cpu.reset();
    cpu.writeRegister(0, 0x123456AB);
    cpu.writeRegister(1, 0x0800);

    cpuMemory.write8(0x0808, 0);

    cpuMemory.write32(0x0000, 0x39002020);
    cpu.setPC(0x0000);

    bool strbTest =
        cpu.step() &&
        cpuMemory.read8(0x0808) == 0xAB;

    std::cout << "STRB W0 [X1,#8] : "
            << (strbTest ? "OK" : "FAIL") << '\n';

    // LDRB W2, [X1,#8]
    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write8(0x0808, 0xCD);

    cpuMemory.write32(0x0000, 0x39402022);
    cpu.setPC(0x0000);

    bool ldrbTest =
        cpu.step() &&
        cpu.readRegister(2) == 0xCD;

    std::cout << "LDRB W2 [X1,#8] : "
            << (ldrbTest ? "OK" : "FAIL") << '\n';

    // ============================================================
    // V0.074 — LDRH / STRH
    // ============================================================

    cpu.reset();

    cpu.writeRegister(0, 0x1234);
    cpu.writeRegister(1, 0x100);

    cpuMemory.write32(0x0000, 0x79001020);
    cpu.setPC(0x0000);

    cpu.step();

    std::cout << "STRH W0 [X1,#8] : "
            << (cpuMemory.read16(0x108) == 0x1234 ? "OK" : "FAIL")
            << std::endl;

    cpu.reset();

    cpu.writeRegister(1, 0x100);
    cpuMemory.write16(0x108, 0xABCD);

    cpuMemory.write32(0x0000, 0x79401022);
    cpu.setPC(0x0000);

    cpu.step();

    std::cout << "LDRH W2 [X1,#8] : "
            << (cpu.readRegister(2) == 0xABCD ? "OK" : "FAIL")
            << std::endl;

    // ============================================================
    // V0.075 — LDRSB / LDRSH (chargements signés)
    // ============================================================

    std::cout << "\n# V0.075 LDRSB / LDRSH\n";

    // LDRSB X2, [X1,#8] : 0x80 doit être étendu avec le signe.
    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write8(0x0808, 0x80);
    cpuMemory.write32(0x0000, 0x39802022);
    cpu.setPC(0x0000);

    bool ldrsbNegative =
        cpu.step() &&
        cpu.readRegister(2) == 0xFFFFFFFFFFFFFF80ULL;

    std::cout << "LDRSB X2 [X1,#8] négatif : "
            << (ldrsbNegative ? "OK" : "FAIL") << '\n';

    // LDRSB X2, [X1,#8] : une valeur positive ne doit pas être altérée.
    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write8(0x0808, 0x7F);
    cpuMemory.write32(0x0000, 0x39802022);
    cpu.setPC(0x0000);

    bool ldrsbPositive =
        cpu.step() &&
        cpu.readRegister(2) == 0x7FULL;

    std::cout << "LDRSB X2 [X1,#8] positif : "
            << (ldrsbPositive ? "OK" : "FAIL") << '\n';

    // LDRSH X2, [X1,#8] : imm12=4 est multiplié par 2.
    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write16(0x0808, 0x8000);
    cpuMemory.write32(0x0000, 0x79801022);
    cpu.setPC(0x0000);

    bool ldrshNegative =
        cpu.step() &&
        cpu.readRegister(2) == 0xFFFFFFFFFFFF8000ULL;

    std::cout << "LDRSH X2 [X1,#8] négatif : "
            << (ldrshNegative ? "OK" : "FAIL") << '\n';

    // LDRSH X2, [X1,#8] : vérifie aussi le cas positif.
    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write16(0x0808, 0x7FFF);
    cpuMemory.write32(0x0000, 0x79801022);
    cpu.setPC(0x0000);

    bool ldrshPositive =
        cpu.step() &&
        cpu.readRegister(2) == 0x7FFFULL;

    std::cout << "LDRSH X2 [X1,#8] positif : "
            << (ldrshPositive ? "OK" : "FAIL") << '\n';

    // ============================================================
    // V0.076 — LDRSB / LDRSH vers W (extension de signe 32 bits)
    // ============================================================

    std::cout << "\n# V0.076 LDRSB / LDRSH W\n";

    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write8(0x0808, 0x80);
    cpuMemory.write32(0x0000, 0x39C02022); // LDRSB W2, [X1,#8]
    cpu.setPC(0x0000);
    bool ldrsbW = cpu.step() && cpu.readRegister(2) == 0xFFFFFF80ULL;

    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write16(0x0808, 0x8000);
    cpuMemory.write32(0x0000, 0x79C01022); // LDRSH W2, [X1,#8]
    cpu.setPC(0x0000);
    bool ldrshW = cpu.step() && cpu.readRegister(2) == 0xFFFF8000ULL;

    std::cout << "LDRSB W2 [X1,#8] : " << (ldrsbW ? "OK" : "FAIL") << '\n';
    std::cout << "LDRSH W2 [X1,#8] : " << (ldrshW ? "OK" : "FAIL") << '\n';

    // ============================================================
    // V0.077 — LDRSW (extension de signe 32 vers 64 bits)
    // ============================================================

    std::cout << "\n# V0.077 LDRSW\n";

    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write32(0x0808, 0x80000000);
    cpuMemory.write32(0x0000, 0xB9800822); // LDRSW X2, [X1,#8]
    cpu.setPC(0x0000);
    bool ldrsw = cpu.step() && cpu.readRegister(2) == 0xFFFFFFFF80000000ULL;

    std::cout << "LDRSW X2 [X1,#8] : " << (ldrsw ? "OK" : "FAIL") << '\n';

    // ============================================================
    // V0.078 / V0.079 — accès octet et demi-mot indexés
    // ============================================================

    std::cout << "\n# V0.078 / V0.079 accès indexés octet/demi-mot\n";

    cpu.reset();
    cpu.writeRegister(0, 0xAB);
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write32(0x0000, 0x38001C20); // STRB W0, [X1,#1]!
    cpu.setPC(0x0000);
    bool strbPre = cpu.step() && cpu.readRegister(1) == 0x0801 && cpuMemory.read8(0x0801) == 0xAB;

    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write8(0x0801, 0xCD);
    cpuMemory.write32(0x0000, 0x38401C22); // LDRB W2, [X1,#1]!
    cpu.setPC(0x0000);
    bool ldrbPre = cpu.step() && cpu.readRegister(1) == 0x0801 && cpu.readRegister(2) == 0xCD;

    cpu.reset();
    cpu.writeRegister(0, 0x1234);
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write32(0x0000, 0x78002420); // STRH W0, [X1],#2
    cpu.setPC(0x0000);
    bool strhPost = cpu.step() && cpu.readRegister(1) == 0x0802 && cpuMemory.read16(0x0800) == 0x1234;

    cpu.reset();
    cpu.writeRegister(1, 0x0800);
    cpuMemory.write16(0x0800, 0xABCD);
    cpuMemory.write32(0x0000, 0x78402422); // LDRH W2, [X1],#2
    cpu.setPC(0x0000);
    bool ldrhPost = cpu.step() && cpu.readRegister(1) == 0x0802 && cpu.readRegister(2) == 0xABCD;

    std::cout << "STRB W0 [X1,#1]! : " << (strbPre ? "OK" : "FAIL") << '\n';
    std::cout << "LDRB W2 [X1,#1]! : " << (ldrbPre ? "OK" : "FAIL") << '\n';
    std::cout << "STRH W0 [X1],#2  : " << (strhPost ? "OK" : "FAIL") << '\n';
    std::cout << "LDRH W2 [X1],#2  : " << (ldrhPost ? "OK" : "FAIL") << '\n';

    // ============================================================
    // V0.082 à V0.084 — adresses et branches relatives
    // ============================================================

    std::cout << "\n# V0.082 à V0.084 ADR / branches relatives\n";

    cpu.reset();
    cpuMemory.write32(0x0000, 0x10000082); // ADR X2, #16
    cpu.setPC(0x0000);
    bool adrTest = cpu.step() && cpu.readRegister(2) == 0x10;

    cpu.reset();
    cpuMemory.write32(0x0000, 0xB0000002); // ADRP X2, +1 page
    cpu.setPC(0x0000);
    bool adrpTest = cpu.step() && cpu.readRegister(2) == 0x1000;
    uint64_t adrpValue = cpu.readRegister(2);

    cpu.reset();
    cpuMemory.write32(0x0000, 0xB4000040); // CBZ X0, +8
    cpu.setPC(0x0000);
    bool cbzTest = cpu.step() && cpu.getPC() == 0x0008;

    cpu.reset();
    cpu.writeRegister(0, 1);
    cpuMemory.write32(0x0000, 0xB5000040); // CBNZ X0, +8
    cpu.setPC(0x0000);
    bool cbnzTest = cpu.step() && cpu.getPC() == 0x0008;

    cpu.reset();
    cpuMemory.write32(0x0000, 0x36000040); // TBZ W0, #0, +8
    cpu.setPC(0x0000);
    bool tbzTest = cpu.step() && cpu.getPC() == 0x0008;

    cpu.reset();
    cpu.writeRegister(0, 1);
    cpuMemory.write32(0x0000, 0x37000040); // TBNZ W0, #0, +8
    cpu.setPC(0x0000);
    bool tbnzTest = cpu.step() && cpu.getPC() == 0x0008;

    std::cout << "ADR X2, #16       : " << (adrTest ? "OK" : "FAIL") << '\n';
    std::cout << "ADRP X2, +1 page  : " << (adrpTest ? "OK" : "FAIL")
              << " (X2=" << std::hex << adrpValue << std::dec << ")\n";
    std::cout << "CBZ X0, +8        : " << (cbzTest ? "OK" : "FAIL") << '\n';
    std::cout << "CBNZ X0, +8       : " << (cbnzTest ? "OK" : "FAIL") << '\n';
    std::cout << "TBZ W0, #0, +8    : " << (tbzTest ? "OK" : "FAIL") << '\n';
    std::cout << "TBNZ W0, #0, +8   : " << (tbnzTest ? "OK" : "FAIL") << '\n';

    return 0;
}

int main()
{
    std::ostringstream output;
    auto* previous = std::cout.rdbuf(output.rdbuf());
    runLegacyTests();
    std::cout.rdbuf(previous);
    std::cout << output.str();
    return output.str().find("FAIL") == std::string::npos ? 0 : 1;
}

