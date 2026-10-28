# Compiled ARM64 program: stack and sum

Project development target combining start.S and sum.c, without firmware or a native Switch 2 executable contract.

Entry saves X29/X30 with STP on a 16-byte-aligned stack, calls Clang 18.1.6/Zig 0.13.0 sum_to(10), and executes a real C loop/stack accesses to compute **55**. It writes a u64 result at **0x1000**, restores stack/X29/X30, and invokes **SVC F000** with exit code in X0, here zero.

This is an explicitly enabled **local test protocol**, not Switch 2 ABI. Unknown SVCs fail; F000 fails without opt-in. The handled SVC counts toward **85 instructions**. The C function targets n=10, not n=0 behavior.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\stack_sum\program.bin 500 --test-protocol
ctest --test-dir build -C Debug --output-on-failure
```

CLI reports normal exit 0; cpu_pairs_and_compiled_program also verifies memory=55 and restored SP. Budget exhaustion is a distinct stop.

## Rebuilding

Reference binaries are included so normal tests need no download/cross-compiler. Portable Windows x64 Zig 0.13.0 archive SHA-256:

`d859994725ef9402381e557c60bb57497215682e355204d754ee3df75ee3c158`

This was checked against the official download index. Local tools are under tools/, without system PATH changes.

```powershell
.\tools\build_arm64_samples.ps1
# Alternative compatible installation:
.\tools\build_arm64_samples.ps1 -Zig 'C:\path\zig.exe'
```

Scripts record aarch64-freestanding-none, C without standard libraries, link.ld at zero, and .text extraction. sum.generated.s is retained; intermediate ELF stays in build/. This raw target loads only extracted bytes. The same tool independently assembles tests/programs/pair_vectors.S; pair tests do not rely on a CPU-specific assembler.

| Artifact | SHA-256 |
| --- | --- |
| stack_sum/program.bin | 0695fd1292e450249977dd7b2ab3b8f15fef7d2f3f062845e4efa4b85b21a5f5 |
| pair_vectors.bin | 8dbeddf8423500de87363a2fa51064f23983abcd2319ac1bc46d295f858bc4f7 |

## CPU scope

W/X integer LDP/STP and LDPSW support signed offset, pre/post-index. Tests cover extreme offsets, cross-page transfers, ZR, SP, bounds and unsupported overlaps. MMIO pairs fail before access. SIMD pairs have a separate decoder; non-temporal integer pairs are absent. [External libnx](../../../docs/LIBNX_REFERENCE.md) motivated LDPSW.

CPU::stepDetailed distinguishes execution/SVC/error. Historical step() returns true only for ordinary executed instructions, false on SVC without setting CPU error. Service loops must use the detailed result. Finite cases do not establish complete ARM64 compliance.

## Reference links

- [Zig 0.13.0 Windows x64 archive](https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip)
- [Official Zig download index](https://ziglang.org/download/index.json)
- [Arm A64 load/store reference](https://documentation-service.arm.com/static/5f3fa899428f7a6b3328fd44)
- [Zig 0.13.0 release notes](https://ziglang.org/download/0.13.0/release-notes.html)
