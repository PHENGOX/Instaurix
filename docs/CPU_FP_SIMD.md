# Floating-point and Advanced SIMD milestones

Historical status dated October 24, 2026. These milestones add instructions useful to compiled programs and their architectural state. They do not implement complete FP/Advanced SIMD or validate Switch 2 compatibility.

## CPU and thread state

V0–V31 are 128-bit registers represented by two 64-bit integer halves. B/H/S/D/Q views share bits. Supported scalar writes clear upper bits; INS changes only the selected lane. V31 is an ordinary vector register, distinct from XZR/SP.

CPU::Context includes all V registers, FPCR and FPSR. Reset and successful loading clear them; capture/restore, pause/exit/error checkpoints and failed loads preserve them. MRS/MSR exposes local masks FPCR `0x07C89F00` and FPSR `0x0800009F`. Other bits are RAZ/WI; newer extensions are not claimed.

## Instruction coverage

| Family | Implemented subset |
| --- | --- |
| Scalar S/D FP | FADD/FSUB/FMUL/FDIV/FSQRT; single-rounding FMADD/FMSUB/FNMADD/FNMSUB |
| Vector 2S/4S/2D FP | FADD/FSUB/FMUL/FDIV/FSQRT, fused FMLA/FMLS, FABS/FNEG |
| Conversion | W/X SCVTF/UCVTF to S/D; S/D FCVTNS/NU, AS/AU, PS/PU, MS/MU, ZS/ZU to W/X; S↔D FCVT |
| Vector conversion | SCVTF/UCVTF and FCVTNS/NU, AS/AU, PS/PU, MS/MU, ZS/ZU on 32/64-bit 2S/4S/2D elements |
| Rounding | Scalar S/D and vector 2S/4S/2D FRINTN/A/P/M/Z/I/X |
| Extrema | Scalar/vector FMAX/FMIN and FMAXNM/FMINNM |
| FP reductions | Vector/scalar FADDP, FMAXP/FMINP/FMAXNMP/FMINNMP; 4S FMAXV/FMINV/FMAXNMV/FMINNMV |
| Moves/signs | S/D register and immediate FMOV, W↔S and X↔D FMOV, FABS/FNEG |
| Compare/select | Register/zero FCMP/FCMPE, NZCV-based FCSEL |
| Vector integers | ADD/SUB: 8/16/32-bit lanes in 64/128-bit vectors and 64-bit lanes in 128-bit vectors; modulo-width overflow |
| Integer products/extrema | MUL/MLA/MLS, SMAX/SMIN/UMAX/UMIN, 8/16/32-bit lanes in 64/128-bit vectors |
| Additional arithmetic | SQADD/UQADD/SQSUB/UQSUB/SQABS/SQNEG; SQXTN/UQXTN/SQXTUN; SSHL/USHL and rounded/saturating forms; SMULL/UMULL and SMLAL/UMLAL/SMLSL/UMLSL, including 2 variants |
| Vector comparisons | CMEQ, CMGT/CMGE, CMHI/CMHS and CMTST; 8/16/32-bit lanes in 64/128-bit vectors and 64-bit lanes in 128-bit vectors |
| Integer reductions | Vector ADDP; ADDV, SMAXV/SMINV/UMAXV/UMINV on 8B/16B/4H/8H/4S |
| Permutations/bits | ZIP1/2, UZP1/2, TRN1/2, byte EXT, 1–4-register TBL/TBX, valid REV16/32/64, byte CNT/NOT |
| Immediate shifts | SHL, SSHR/USHR, including full-element-width right shifts |
| Vector logic | 8B/16B AND/BIC/ORR/ORN/EOR/BSL/BIT/BIF |
| Lane copies | GPR/lane DUP, GPR UMOV/SMOV, GPR/lane INS |
| Memory | B/H/S/D/Q LDR/STR and LDUR/STUR; immediate/register/pre/post offsets; S/D/Q literals; S/D/Q LDP/STP offset/pre/post |
| Structured memory | LD1/ST1 with 1–4 registers; interleaved LD2/ST2, LD3/ST3, LD4/ST4; multiple-structure forms without offset or with immediate/register post-index |

## Memory, tables and lanes

Transfers are RAM-only and validate the complete range and permissions before effects. Single, pair and structure stores use transactional copies and invalidate exclusive reservations. Integer base and vector destination may share a register number because their banks differ. Non-temporal pairs, single-lane structures and replicated LDnR are not added.

LD1/ST1 transfer consecutive blocks. LD2/3/4 and ST2/3/4 deinterleave/interleave 8/16/32/64-bit elements. 1D forms are restricted to LD1/ST1. Register lists wrap from V31 to V0. A 64-bit load clears each register's upper half. Post-index uses total transfer size or the original offset-register value, committing only after success. Permission, MMIO, bounds, SP-alignment and address-overflow errors have no partial effects.

TBL/TBX use consecutive-register tables of 16–64 bytes, wrapping V31→V0. TBL returns zero for out-of-range indices; TBX preserves the corresponding old destination byte. Table/index/destination overlap is supported through captured inputs. 8B forms clear upper halves. SMOV sign-extends B/H to W/X or S to X; W writes clear upper X bits. INS updates only its lane.

## Floating-point behavior

The [software engine](CPU_SOFTFLOAT.md) uses no host FP arithmetic. Four FPCR rounding modes, DN and FZ are supported, including subnormals, signaling/quiet NaNs and priority, signed zeros and cumulative FPSR IOC/DZC/OFC/UFC/IXC/IDC. FCMP/FCMPE change NZCV; other covered operations preserve integer flags. Unsupported encoding/mode errors leave no partial state and do not modify the host FP environment.

Integer/FP conversions operate on bits without out-of-range C++ conversion. SCVTF/UCVTF use FPCR rounding. FP-to-integer instructions encode nearest-even, nearest-ties-away, toward positive/negative infinity or toward zero. Overflow/infinity saturates; NaN yields zero. Invalid conversions set IOC without adding IXC; valid inexact conversions set IXC. FP-to-integer conversions also handle subnormal inputs without enabling FZ. W writes clear upper X bits even for negative results.

S/D FCVT preserves NaN signs and representable payload bits according to DN, signals signaling NaNs, and handles subnormals, underflow, rounding and FZ. Scaled fixed-point conversions are absent.

Fused operations compute exact product-plus-add before one rounding, including instruction-specific signs, three NaN operands and zero-times-infinity with a NaN addend. Tests distinguish fused from intermediate rounding and check cancellation of a product that would overflow alone.

Vector arithmetic shares scalar helpers per element. FMLA/FMLS capture the initial destination accumulator and all sources, allowing aliases. FPSR effects accumulate locally; destination and flags commit only if every element succeeds. 2S forms clear upper 64 bits and ignore inactive lanes. FABS/FNEG operate on bits rather than arithmetic restrictions.

FADDP combines adjacent elements in architectural order, without C++ reassociation. Pairwise/4S extrema preserve combination order and NaN behavior. Failed reductions commit no flags from earlier pairs. Vector conversion shares scalar signedness, rounding, saturation and flags while preserving element width; it does not add S/D widening/narrowing FCVTL/FCVTN.

FRINTN uses nearest-even; FRINTA uses ties-away; P/M/Z use positive/negative infinity or zero. FRINTI/X follow FPCR.RMode, but only FRINTX adds IXC when a numeric result changes. They preserve signed zeros and handle signaling NaNs and supported subnormal inputs through bit calculations. These rounding instructions alone do not resolve arithmetic underflow.

FMAX/FMIN propagate NaNs with signaling priority. NM variants select the numeric operand when only one input is a quiet NaN; signaling NaNs produce IOC and quiet NaN output. DN controls payload preservation. Extrema distinguish -0/+0, select subnormals with FZ=0, and flush inputs with IDC when FZ=1.

Earlier subnormal/underflow/FZ rejection has been removed. Tininess is detected before rounding; exact and inexact subnormals differ, and FZ applies to inputs/results. Trapped exceptions, FZ16/AHP and newer Alternate Handling remain rejected. [Saturation and widening arithmetic](CPU_SIMD_ARITHMETIC.md) changes cumulative QC without disturbing other flags. FP16, fixed-point conversion, FCVTL/FCVTN and other families remain absent.

## Validation

- **1,491 variants, 96,400 independent cases**, encoded by Clang 18.1.6/Zig 0.13.0 with Unicorn 2.1.4 expectations, seed `0xF0642026`. Previous 983 variants remain; 508 structure/table/lane variants add 10,944 cases, including circular registers, base updates, out-of-table indices, sign extension and overlap. Existing heterogeneous lanes, boundaries, four FPCR modes, DN, NaNs and operand/accumulator aliases remain.
- cpu_subnormal adds **64,896 cases / 129 variants**; cpu_simd_arithmetic adds **61,056 cases / 477 variants**.
- Comparisons include X0–X30, SP/NZCV, all 32 V registers, FPCR/FPSR and 512 bytes of RAM, with V31/XZR, infinities, signed zeros, inexact/overflow and memory paths.
- Host FP environment preservation is tested with different host rounding and FTZ/DAZ enabled.
- reference-layout.txt records instruction/block lengths outside input-generation loops. The C++ reader checks layout and sample indices before CPU comparison. This guard was added after detecting and fixing reuse of an index variable in the generator.
- Direct tests cover context/reset, process pauses/exit/errors, reloads, accepted/rejected subnormal modes, invalid encodings, RAM bounds, permissions, pairs and MMIO.
- A shared native/ARM64 float/double C calculation yields bits `0x40B8B8B0` and `0xC026C80000000000`. The ELF retains earlier chains and adds **192 bytes** for saturation, narrowing, variable shifts, widening products and QC snapshots. It exits in **343 instructions** (308 before this addition).
- The milestone had **24 Debug/Release CTest suites passing with MSVC**, retaining integer and atomic tests.

Unicorn is independent software, not target hardware. A finite corpus is not exhaustive compliance. tests/programs/cpu_fp/reference.json records instructions, seed, versions and SHA-256.

## Reproduction

```powershell
./tools/build_fp_reference.ps1 -Python /path/to/python.exe
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Regeneration needs Unicorn 2.1.4 in tools/python-deps or the selected Python environment, and Zig 0.13.0 (override with -Zig). Supplied artifacts suffice for normal tests. Scripts call Clang entry points directly and use build/ caches. -SamplesOnly recompiles the ELF without regenerating independent data.

The next priority at this historical stage was broader compiled C/C++ programs, native comparison and measured missing instructions before multithread scheduling; IPC was deferred during that consolidation. Current priorities are in README.

## Reference links

- [Arm A64 instruction reference](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab)
- [Microsoft floating-point behavior](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170)
- [Microsoft floating-point environment preservation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/feholdexcept2?view=msvc-170)
- [Unicorn independent reference engine](https://github.com/unicorn-engine/unicorn)
