# Homebrew NRO entry and MOD0 validation

Experimental profile dated October 25, 2026, enabled only by --hle-reference --homebrew-entry. It requires a bounded MOD0 NRO, not ELF or raw input. Earlier modes retain their entry conventions and reject MOD0.

## Supplied environment

| Element | Local value |
| --- | --- |
| X0 | 0xD00000, read-only non-executable ConfigEntry list |
| X1 | UINT64_MAX |
| X30 | 0xD01000, R/X return trampoline |
| SP | Top of the 64 KiB stack, 16-byte aligned |
| MainThreadHandle, key 1 | Actual process handle-table entry |
| OverrideHeap, key 3 | 2 MiB R/W heap at 0x200000 |
| EndOfList, key 0 | Terminator following these two entries |

Entries are 24 bytes. No firmware, system version or global SVC mask is invented. Environment/return reservations cannot overlap segments. Heap and QueryMemory state are actually initialized.

The main handle supports GetThreadId (SVC 0x25), wait and close, and becomes signaled on thread exit. StartThread on main returns InvalidState. The second thread retains distinct identity. GetThreadId does not implement pseudo-handles.

Return uses local SVC F001, accepted only at the trampoline PC from main in this profile, preserving X0. It is not an Horizon SVC. Test protocol F000 is incompatible with this profile.

## Pre-execution validation

Checks use file-backed ranges and memory regions, not merely RAM bounds:

- Read-only file-backed MOD0/tables; checked signed offsets; BSS must match NRO metadata.
- Exception bounds/reserved fields; bounded [LNY/RELRO extensions](LIBNX_CRT0.md), GOT/RELRO ranges and LNY2 version 1.
- Terminated dynamic table with unique tags; only DT_NULL, DT_RELA/RELASZ/RELAENT and DT_RELR/RELRSZ/RELRENT.
- Consistent entry sizes, aligned tables, 8-byte-aligned R/W targets and no duplicate destinations.
- RELA NONE or RELATIVE with zero symbol and signed addend bounded by local RAM.
- RELR direct address before bitmap, bounded progression/additions, and checked bitmap targets; initial values allow only positive non-overflowing additions.

**The loader does not apply relocations.** Guest libnx code applies them once. Machine stages loading, so rejection preserves earlier RAM/CPU/process state. This restrictive subset does not promise all current libnx NROs.

## Verified program

The project-owned entry/main in tests/programs/nro_bootstrap links **unchanged libnx dynamic.c**, revision b156b4774fb2df7b93402169509266da7547fc4c, with license, manifest and reconstruction script. The static packager reserves the header and this bench fills a fixed MOD0 pointer; it is not elf2nro.

The **24,576-byte file / 4,096-byte BSS** applies one RELA relocation, reaches main, checks environment entries, resolves GetThreadId, writes both heap ends and deliberately returns **42 in 32,968 instructions**. Tests dirty BSS after loading to prove guest libnx clears it. A separate RELR direct-address/bitmap variant is executed.

Coverage includes seven-instruction slices, permissions, malformed metadata, unknown relocation types/symbols, overflow, duplicate targets, missing RELR base, previous-session preservation, handle lifetime and F001 outside the trampoline. At this milestone **52 Debug/Release suites passed**, and strict rebuilding reproduced the manifest hash.

```powershell
.\tools\build_nro_bootstrap.ps1 -Python <python_path>
.\build\Debug\Instaurix.exe .\tests\programs\nro_bootstrap\program.nro 100000 --hle-reference --homebrew-entry
ctest --test-dir build -C Debug -R nro_homebrew_entry_and_validation --output-on-failure
```

The CLI reports normal guest exit code=42 but returns nonzero host status; the dedicated test explicitly expects 42.

This first bench does not use switch_crt0.s. Later stages add [original crt0/RELRO](LIBNX_CRT0.md) and [original minimal libnx initialization to main](LIBNX_INIT.md). External hello-world, default services and complete devkitA64 remain separate milestones. The project remains an emulator; historical Switch references do not establish native Switch 2 compatibility.
