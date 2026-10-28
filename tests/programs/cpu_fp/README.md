# FP/SIMD references and program

See [coverage and limits](../../../docs/CPU_FP_SIMD.md).

- vectors.S/vectors.bin: **1,491 variants**, assembled by Clang 18.1.6.
- reference.bin: **96,400 expected executions** from Unicorn 2.1.4.
- reference.json: provenance, seed, SHA-256 and variants.
- reference-layout.txt: instruction/sample count per variant, checked by C++ to catch encoding/input misalignment.
- work.c: shared native/guest float/double calculation. Volatile products prevent fusion in the first calculation; the second uses conversion and explicit fused multiply-add.
- work.generated.s: compiler assembly.
- start.S/link.ld/program.elf: **343-instruction** local ELF. Earlier SIMD/subnormal/flag chains remain; **192 bytes** add saturation, narrowing, variable shifts, widening products and QC snapshots. This is not native Switch 2 homebrew.

Regenerate from project root: `tools/build_fp_reference.ps1 -Python <python.exe>`. No firmware, keys or game code is required.

Format: `FPREF001`, little-endian u32 count; each case contains u32 instruction/sample index, initial/expected state and 512 expected RAM bytes. State holds 31 u64 GPRs, u64 SP/NZCV, u32 FPCR/FPSR and 32 128-bit V registers. Initial PC=0x1000, expected PC=0x1004. RAM at 0x4000 is `(i*37 + sample*13) mod 256`; literal reads use its first 32 bytes at 0x1008.
