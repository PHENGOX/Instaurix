# Software floating point and subnormals

Status dated October 24, 2026. Implemented scalar/vector binary32/binary64 arithmetic uses no host floating-point operations. This replaces earlier rejection of subnormal operands/results and FPCR.FZ.

## Representation and arithmetic

src/cpu_softfloat.cpp uses fixed 4096-bit integers in 128 32-bit words, without dynamic allocation. Finite operands are decomposed into sign, integer significand and power of two.

- Addition/subtraction aligns significands exactly, then adds or subtracts.
- Multiplication computes exact significand products, up to 106 bits for binary64.
- FMA computes the exact product and accumulator sum before one rounding; maximum alignment requires at most 3172 bit positions.
- Division retains enough quotient guard bits and a sticky remainder.
- Square root uses integer bit-pair extraction and a sticky remainder.
- S/D conversion shares this exact representation and final rounding.

Common NaN, infinity, signed-zero and control handling precedes finite calculations. Signaling NaN priority and fused-operation rules are retained. No third-party floating-point implementation is incorporated.

## Rounding and flags

All four FPCR.RMode settings and Default NaN are supported. Cumulative FPSR IOC, DZC, OFC, UFC, IXC and IDC flags combine across vector elements. In the profile without Alternate Handling:

- With FZ=0, an exact subnormal result sets neither UFC nor IXC.
- An inexact tiny result sets UFC and IXC. Tininess is detected **before rounding**, even if rounding yields the smallest normal value.
- FZ=1 replaces subnormal inputs with same-sign zero and sets IDC.
- With FZ=1, a nonzero tiny result becomes same-sign zero and sets UFC; flushing does not additionally set IXC.
- Exact cancellations and signed zeros respect rounding mode and operand signs.

FZ applies to applicable arithmetic, comparison, conversion, extrema and rounding operations. FMOV, FABS/FNEG and data moves retain their own bit rules. The engine does not modify the host floating-point environment, so host environment save/restore is unnecessary.

## Validation

The existing **96,400 cases over 1,491 variants** remain. A separate corpus adds **64,896 cases over 129 variants** covering S/D, fused/vector/pairwise arithmetic, comparisons, conversions, extrema and rounding. Inputs include both FZ states, DN, four rounding modes, normal/subnormal boundaries, ties, extreme products/alignment, random values, NaNs, infinities and signed zeros.

Clang 18.1.6/Zig 0.13.0 produces encodings; Unicorn 2.1.4 generates expected bits and flags. The reader compares the complete exposed context and reference-layout.txt variant boundaries. A host-environment test uses different rounding with FTZ/DAZ enabled to detect accidental host arithmetic dependence.

The ELF demo checks 128 bytes of explicit result/flag expectations: exact/inexact S/D subnormal products, input/output FZ, tiny vector arithmetic and D-to-S underflow. It originally executed 308 instructions, and now executes **343** with [later SIMD additions](CPU_SIMD_ARITHMETIC.md). At that stage **24 CTest suites passed in Debug and Release with MSVC**. Finite comparisons do not replace target-hardware or exhaustive ARM64 validation.

```powershell
./tools/build_subnormal_reference.ps1 -Python /path/to/python.exe
./tools/build_fp_reference.ps1 -Python /path/to/python.exe -SamplesOnly
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The tests/programs/cpu_subnormal generator reuses the FP/SIMD format with specialized inputs. Manifests retain versions, seed, variants and reference hashes.

## Limits

Trapped floating-point exceptions are rejected with diagnostics. FP16, FPCR.FZ16/AHP, Alternate Handling, fixed-point conversion and further SIMD families remain absent. This is a correctness reference without game-performance or multicore promises. Saturation, variable shifts and widening products have since been added; broader compiled C/C++ targets are next.

## Reference links

- [Arm floating-point exceptions and tininess](https://support.arm.com/documentation/dui0349/c/floating-point-support/ieee-754-arithmetic/exceptions)
- [Arm floating-point operation pseudocode](https://support.arm.com/documentation/ddi0487/mc/-Part-E-The-AArch32-Application-Level-Architecture/-Chapter-E1-The-AArch32-Application-Level-Programmers--Model/-E1-3-Advanced-SIMD-and-floating-point-instructions/-E1-3-11-Pseudocode-description-of-floating-point-operations?lang=en)
- [Unicorn independent reference engine](https://github.com/unicorn-engine/unicorn)
