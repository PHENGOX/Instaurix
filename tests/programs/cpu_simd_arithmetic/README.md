# Arithmetic SIMD references

**61,056 cases over 477 variants** for saturation, narrowing, variable shifts and widening multiplication. Encodings: Clang 18.1.6/Zig 0.13.0. Expectations: Unicorn 2.1.4.

reference.bin uses shared FPREF001; reference-layout.txt checks block provenance; reference.json records seed/versions/variants/SHA-256. vectors.S/vectors.bin retain encodings.

Regenerate: `tools/build_simd_arithmetic_reference.ps1 -Python <python.exe>`. CTest reads supplied artifacts without generation dependencies. See [coverage/limits](../../../docs/CPU_SIMD_ARITHMETIC.md). The cpu_fp ELF chains these families and compares results/QC against a separate reference.
