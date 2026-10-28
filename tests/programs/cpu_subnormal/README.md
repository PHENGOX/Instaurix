# Subnormal and FZ references

**64,896 cases over 129 variants**, assembled by Clang 18.1.6 and executed with Unicorn 2.1.4. See [software floating-point report](../../../docs/CPU_SOFTFLOAT.md).

vectors.S/vectors.bin contain encodings. reference.bin contains inputs, expected context/flags/RAM in FPREF001 format. reference-layout.txt records instruction/block sizes checked by C++; reference.json retains variants/provenance/seed/SHA-256.

Regenerate: `tools/build_subnormal_reference.ps1 -Python <python.exe>`. Ordinary CTest requires neither Python nor Unicorn. The cpu_fp ELF adds a tiny-value/flag chain compared against explicit constants.
