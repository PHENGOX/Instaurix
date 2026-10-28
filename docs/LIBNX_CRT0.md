# Original libnx entry and RELRO protection

Milestone dated October 25, 2026. Original switch_crt0.s from libnx b156b4774fb2df7b93402169509266da7547fc4c is assembled **unchanged** in a reference NRO. It calls original dynamic.c, relocates, protects RELRO, saves SP and reaches __libnx_init.

## Executed path

The bench preserves original HOMEBREW, MOD0 and LNY0/LNY1/LNY2. The linker resolves MOD0 offsets. Loader validation checks file/GOT/RELRO bounds, alignment and LNY2 version 1. Empty RELRO, incomplete extensions and unknown versions fail.

1. Original crt0 receives the NRO environment/loader return.
2. __nx_dynamic clears BSS and applies RELATIVE relocation to the report.
3. SVC 2 makes two RELRO pages read-only.
4. SVC 3 locks those permissions.
5. Original initial SP and three __libnx_init arguments are saved/passed.
6. The bench explicitly stops at this boundary, then original __nx_exit restores SP and returns to the loader.

**This bench substitutes a probe for __libnx_init.** It records arguments and exits with boundary marker **0xC001 / 49,153**. main/exit/exception symbols fail if reached. The [next minimal target](LIBNX_INIT.md) separately runs actual initialization to main without default services.

The **32,768-byte file / 4,096-byte BSS** executes **12,432 instructions**. Report at **0x17000** records marker, X0=0xD00000, X1=UINT64_MAX, return=0xD01000 and SP=0x1000000. Guest code clears deliberately dirtied BSS, then saves SP at 0x18000. Relocation at 0x15000 points to the report.

Single-step tracing writes build/libnx-crt0-trace.csv: SVC 2 at PC 0x1016C (address 0x15000, size 0x2000, R), SVC 3 at 0x10174 (same range, mask/value 0x10), then local F001 at 0xD01000 with marker 0xC001. Tests verify parameters.

## Bounded protection services

SVC 2/3 apply only to validated homebrew RELRO, not general Horizon memory management.

- SVC 2 accepts None/R/RW, never execute; nonzero page-aligned size/address, entire range inside RELRO.
- SVC 3 accepts only MemAttr_IsPermissionLocked bit 4 as mask/value, with all pages already read-only. Unlocking is rejected.
- All pages are validated before changes; errors have no partial effects.
- Locked ranges reject further permission changes. QueryMemory exposes the attribute and splits/coalesces regions accordingly.
- Data, stack, heap, text and environment lie outside this authorization; RAM actually rejects writes.

Tests also cover invalid ranges/sizes/permissions, mixed ranges, unlock attempts, malformed extensions, failed-load preservation and seven-instruction equivalence. At this stage **53 Debug/Release suites passed**, and strict rebuilding reproduced the manifest hash.

```powershell
.\tools\build_libnx_crt0.ps1 -Python <python_path>
ctest --test-dir build -C Debug -R libnx_original_crt0_and_relro --output-on-failure
```

Zig 0.13.0 verifies pinned archive sources and recorded NRO hash. Original source/license/manifest are in tests/programs/libnx_crt0. --libnx-crt0 packaging preserves existing headers, generates no relocation tables and does not replace elf2nro. Base remains fixed at 0x10000.

## Toolchain research and subsequent work

No devkitA64 or devkitPro pacman was found locally. Official metadata then listed devkitA64 r30 and newlib 4.6.0.20260123-4. Requests to pkg.devkitpro.org returned HTTP 403 in that session; no global installer ran. Those tools were not installed/validated by this milestone. This bench uses Zig without newlib.

Later [official newlib sources compiled with Zig](LIBNX_INIT.md) enabled minimal original startup to main with no-service app hooks. Full devkitA64/default services remained separate, without fake success stubs. Historical Switch references do not prove native Switch 2 compatibility.

## Reference links

- [libnx switch_crt0.s](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/switch_crt0.s)
- [libnx dynamic.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/dynamic.c)
- [Official devkitA64 packages](https://github.com/devkitPro/pacman-packages/tree/master/devkita64)
