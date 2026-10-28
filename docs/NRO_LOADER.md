# Reference static NRO0 loader

Milestone dated October 25, 2026. NRO0 at offset 0x10 or the .nro extension selects validation before memory replacement. This is a bounded test subset, not a claim to load standard libnx homebrews or Switch 2 games.

## Accepted profile

- 0x80-byte header, version 0, zero flags and no additional header.
- Declared size exactly equals file size; no appended ASET/assets.
- Three segments with 4 KiB-aligned file offsets/sizes; text starts at zero and contains the header.
- No overlap, RAM overflow or stack/guard collision.
- Zero extended dynamic metadata; module identifier retained without treating it as trusted identity.

Local base/entry is **0x10000**, without ASLR. Text is R/X, rodata R, data/BSS R/W non-executable; gaps are inaccessible. BSS follows data and is zeroed. The protected 64 KiB stack remains at development RAM's top.

HLE heap/thread reservations retain prototype addresses and collision checks. The static sample uses the same local entry conventions as project ELF files, without implying a homebrew environment.

A nonzero MOD0 pointer must be aligned, file-backed and correctly signed. Segment loading marks requiresNroRuntime, but **Machine rejects execution by default**. The explicitly enabled [homebrew entry profile](NRO_BOOTSTRAP.md), --hle-reference --homebrew-entry, validates bounded MOD0/RELA/RELR and supplies an entry environment; it is not the entire libnx runtime.

Staged memory/image metadata preserve the old session on rejected headers, mappings or profiles. NRO magic is recognized even after renaming to .bin. NSO signatures and unsupported NCA/NSP/XCI containers never fall back to raw instructions.

## Reproducible sample

```powershell
.\tools\build_nro_client.ps1 -Python <python_path>
.\build\Debug\Instaurix.exe .\tests\programs\nro_client\program.nro 50000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

The C client reuses hle_client/work.c and its service/clone/copy/hash checks. A project-owned entry reserves the NRO header and branches to the program. The packager handles this fixed ELF profile without MOD0, relocations or assets; it does not replace elf2nro.

The file is **12,288 bytes** with another **12,288 bytes BSS**. It exits after **14,343 instructions**, with status=1, sum=55, copied=512 and hash=3505779141. Tests compare all seven report fields with the corresponding ELF.

Coverage includes permissions, BSS, stack, malformed sizes/offsets, overlap, truncation, unknown metadata, MOD0 reporting, magic detection and refusal of format fallback. See [external target audit](COMPATIBILITY_AUDIT.md).

## Reference links

- [switchbrew.org](https://switchbrew.org/wiki/NRO)
- [github.com](https://github.com/switchbrew/switch-tools/blob/master/src/elf2nro.c)
- [libnx switch_crt0.s](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/switch_crt0.s)
