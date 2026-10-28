# CPU audit: initial ARM64 integer milestone

Verified October 24, 2026. This milestone prioritized CPU consolidation and added no HLE/IPC services. Counts below describe that historical milestone; current validation is in README.

## Results

The initial milestone had 18 CTest suites and 32,832 independent comparisons over 640 variants. The expanded integer corpus contains **41,304 comparisons over 775 variants**, adding W/X CCMP/CCMN, immediate/register forms and invalid encodings. With subsequent FP/SIMD, subnormal, arithmetic SIMD and atomic milestones, the suite then contained 24 tests.

A mixed 32/64-bit C calculation matches native x64 execution: 64-bit result `0x78335ef4bc270ff5`, 32-bit result `0xc5473408`, **303 guest instructions**, restored stack and explicit exit. This validates an integer subset, not complete ARM64 or Switch 2 compatibility.

## Audit findings and corrections

| Area | Earlier issue | Verified change |
| --- | --- | --- |
| Immediate ADD/SUB | Missing SUBS and incorrect WSP/SP versus ZR distinctions | Shared 32/64-bit handling; encoding-dependent SP operands and ZR flag-setting destinations |
| Carry arithmetic | Missing ADC/SBC | Common carry/overflow calculation preserving other PSTATE bits |
| Shifts | Register decoders accepted only simple forms | Validated amounts, signed/logical shifts and rotations |
| Logical immediates | Destination 31 always ignored | SP for non-flag forms; ANDS/TST set N/Z and clear C/V |
| Conditions | Separate branch blocks | Shared evaluator for all 16 conditions, branches and selections |
| PC-relative addresses | Unnecessary signed additions | Relative displacement without C++ signed overflow |
| Scalar memory | Incomplete extension/scaling and duplicated width handling | Common size, extension, offset and base-update decoding |
| Unknown encodings | Overly broad masks | Explicit rejection of reserved or unmodeled combinations |
| Historical SP tests | Two XZR-labeled checks only preserved X0 | Correct labels and actual SP checks after immediate ADD/SUB |

Code is split across cpu.cpp (execution/main decode), cpu_integer.cpp, cpu_load_store.cpp and cpu_pair.cpp. Replaced blocks were removed rather than retained as competing paths.

## Covered families

| Family | Implemented and exercised forms |
| --- | --- |
| Arithmetic | Immediate/shifted/extended ADD/ADDS/SUB/SUBS; ADC/ADCS/SBC/SBCS; applicable CMP/CMN/NEG aliases |
| Logic | Shifted-register AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS; immediate AND/ORR/EOR/ANDS |
| Shifts/bitfields | LSLV/LSRV/ASRV/RORV; SBFM/BFM/UBFM and shift/extract/insert/extend aliases; EXTR/ROR |
| Multiplication | MADD/MSUB, MUL/MNEG; signed/unsigned long forms; SMULH/UMULH |
| Division | W/X UDIV/SDIV, including zero divisor and signed minimum divided by -1 |
| Bits/bytes | RBIT, REV16, width-appropriate REV32/REV, CLZ and CLS |
| Conditions | CSEL/CSINC/CSINV/CSNEG; B.cond and existing branches |
| Conditional comparison | W/X CCMP/CCMN, register or 5-bit immediate; all 16 conditions and initial NZCV combinations; immediate fallback flags |
| Scalar memory | LDR/STR and signed forms, LDUR/STUR, immediate and UXTW/SXTW/LSL/SXTX register offsets, pre/post-index, W/X/SW literals |

A family name does not imply all similarly named ARM extensions. Shift amounts and reserved combinations are checked. W writes clear upper X bits. ZR/SP interpretation depends on the instruction form.

## Independent validation

Clang 18.1.6 through Zig 0.13.0 produces encodings. Unicorn 2.1.4 independently generates expected ARM64 results. Scripts, assembly and readable manifests accompany artifacts.

Cases compare X0–X30, SP, NZCV and next PC; memory cases also compare a 512-byte window. Other local PSTATE bits must remain unchanged. Selected invalid encodings must fail without register changes. Inputs cover boundaries, carry, signs, shifts, flag combinations and deterministic data seeded with `0xA6402026`.

The native-versus-guest unsigned C calculation separately checks instruction chains, loops and memory results. Unicorn can itself have limitations or bugs; a finite corpus is not full ISA or target-hardware validation. See [reproduction guide](../tests/programs/cpu_integer/README.md).

## Limits and next work

[Atomics/exclusives](CPU_ATOMICS.md) use a sequential model, without parallel multicore execution. [FP/SIMD](CPU_FP_SIMD.md) and its saved context remain partial. CCMP/CCMN are available, while further integer families remain absent. Unprivileged memory forms, PRFM and non-temporal pairs are unsupported; B/H/S/D/Q scalar/vector memory and S/D/Q pairs belong to the FP/SIMD milestone.

SP memory bases require 16-byte alignment. Unmodeled writeback overlaps are rejected. Local transfers reject numeric address overflow rather than model every full-system addressing rule; differential cases use valid local addresses. There is no JIT, complete MMU, privileged exception handler or native Switch 2 startup.

Continue validating implemented families, expanding floating-point/vector coverage and compiled targets; instruction counts alone cannot establish a finished CPU.

## Reference links

- [Arm A64 instruction-set overview](https://developer.arm.com/-/media/Files/pdf/graphics-and-multimedia/ARMv8_InstructionSetOverview.pdf)
- [Arm A64 load/store reference](https://documentation-service.arm.com/static/5f3fa899428f7a6b3328fd44)
- [Unicorn independent reference engine](https://github.com/unicorn-engine/unicorn)
- [Unicorn 2.1.4 release](https://pypi.org/project/unicorn/2.1.4/)
