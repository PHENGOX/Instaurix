# SIMD saturation, variable shifts and widening multiplication

Milestone dated October 24, 2026. Operations include valid vector and corresponding architectural scalar forms.

| Group | Coverage |
| --- | --- |
| Saturation | SQADD/UQADD, SQSUB/UQSUB, SQABS/SQNEG; 8/16/32/64-bit elements and scalar B/H/S/D |
| Saturating narrowing | SQXTN/UQXTN/SQXTUN, scalar/vector and 2 variants; 16/32/64-bit sources to 8/16/32-bit results |
| Variable shifts | SSHL/USHL, SRSHL/URSHL, SQSHL/UQSHL, SQRSHL/UQRSHL; valid scalar/vector forms |
| Widening multiplication | SMULL/UMULL, SMLAL/UMLAL, SMLSL/UMLSL and 2 variants; 8/16/32-bit sources to 16/32/64-bit results |

## Semantics

Saturation clamps to the element's signed/unsigned range and sets cumulative **FPSR.QC, bit 27** if any element saturates. Later non-saturating instructions do not clear it; MSR FPSR can. NZCV and other FPSR bits are preserved.

Variable shifts use only each shift element's **signed low byte**. Positive values shift left; negative values shift right, sign-extending signed forms. Equal/greater-than-width shifts and rounding avoid undefined C++ shifts. R forms round right shifts independently of FPCR.RMode; Q forms saturate left shifts and set QC when needed.

Widening multiplication selects the lower/upper source half according to suffix 2 and always produces 128 bits. Accumulate/subtract operations wrap modulo destination width, without saturation or QC changes. Unsigned C++ arithmetic avoids signed-overflow undefined behavior.

Plain narrowing writes the lower half and clears the upper half; 2 variants preserve the lower half and replace the upper half. Sources and old destination are captured first, supporting register aliases. Reserved encodings, .1D vectors and non-architectural widening/narrowing widths fail without partial effects.

## Validation

**61,056 cases over 477 variants**, encoded by Clang 18.1.6/Zig 0.13.0 with Unicorn 2.1.4 expectations. Comparisons include complete integer/vector contexts, SP/NZCV, FPCR/FPSR and memory: boundaries, aliases, shifts -128..127, noise in ignored shift bits, initial QC states and random data.

The separate corpus reuses the general FP/SIMD format and per-variant provenance checks. The combined ELF chains signed/unsigned saturation, rounded shifts, low/high narrowing, widening products and accumulations. A separate scalar reference checks **192 bytes**, including QC snapshots and clearing. The full demo exits in **343 instructions**. At this milestone **24 Debug/Release CTest suites passed**, retaining previous corpora. These finite comparisons do not establish every Advanced SIMD extension or Switch 2 hardware compatibility.

```powershell
./tools/build_simd_arithmetic_reference.ps1 -Python /path/to/python.exe
./tools/build_fp_reference.ps1 -Python /path/to/python.exe -SamplesOnly
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Next: broader optimized C/C++ vector, buffer and atomic programs, compared with native results; add encountered missing instructions before a multithread scheduler. Decide on a JIT from performance measurements.

## Reference links

- [Arm A64 instruction reference](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab)
- [Arm AArch64 QC flag](https://documentation-service.arm.com/static/5f3e9263b13d4764d4613b69?token=)
- [Unicorn independent reference engine](https://github.com/unicorn-engine/unicorn)
