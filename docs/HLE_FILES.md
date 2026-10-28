# Read-only files

Milestone dated October 25, 2026. Local **emu:fs**, acquired through sm:, opens files from a host-selected directory. File sessions expose size and positional reads. This prototype interface does not reproduce fsp-srv or validate native Switch 2 compatibility.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_files\program.elf 50000 --hle-reference --read-root .\tests\programs\hle_files\root
```

## Explicit immutable snapshot

Without --read-root, emu:fs returns FilesDisabled. Mounting requires reference HLE. The directory is read before ELF loading into an immutable snapshot: guest requests only search this snapshot, never open PC paths. Later host file edits/deletion do not affect the running session.

Limits: **32 files, 4 MiB per file, 8 MiB total**, no subdirectories. Names are 1–64 ASCII letters/digits/dots/hyphens/underscores, compared exactly and case-sensitively. Empty/long names, separators, colons, . and .. are rejected.

Only regular files are imported. Windows opening rejects reparse points/directories and checks the actual opened handle's parent; junctions/links are not followed as guest files. The explicit root is resolved at mount time. POSIX uses no-follow opening. Link tests run when the system permits their creation.

Rejected mounts preserve configuration. Machine::mountReadOnlyRoot prepares the next successful load, never replaces a live process's files. Reload reuses the configured snapshot without rereading the directory. Guest creation/write/deletion are not exposed.

## Local interface

| Object/command | Input | Reply |
| --- | --- | --- |
| emu:fs / 0 OpenFile | One Send buffer with name plus final NUL; no inline parameters | Move file handle |
| File / 1 GetSize | No buffers/parameters | u64 size |
| File / 0 Read | Receive buffer; inline u64 offset and length | Read bytes and SFCO u64 count |

Names require a single final NUL within declared size. Positional reads have no shared cursor; clones see the same immutable bytes. Closing emu:fs leaves acquired files valid; closing an original file leaves its clone valid.

Length must fit Receive capacity, bounded to 64 KiB. At/beyond EOF returns zero without writes. Crossing EOF returns remaining bytes only, preserving unused capacity. Unsigned offsets are checked without overflowing additions.

Common IPC checks enforce RAM rights, page boundaries, no MMIO and no command-buffer overlap. Data/reply use grouped transactional writes; invalid descriptors produce no partial read or success reply. Module-511 results: FilesDisabled=0x17FF, InvalidPath=0x19FF, FileNotFound=0x1BFF. Unknown operations, including writes, return UnknownCommand. Service errors use SFCO; envelope/memory errors use transport status. Handles publish only after complete reply writes.

## Demo and tests

Clang 18.1.6/Zig 0.13.0 compiles hle_files/work.c. It opens sample.txt via SM/emu:fs, closes intermediary sessions, checks size/full contents, clones the file, checks partial and EOF reads.

**4,006 instructions**; six u64 fields at 0x12000: status=1, size=30, read=30, hash=530586646, partial=9, eof=0. The fixture is exactly `Switch2Emu read-only fixture.` followed by LF.

Tests cover seven-instruction slicing, persistence after host deletion, disabled mounts, unsafe/missing names, EOF, forbidden outputs, subdirectories, quotas and available link cases. Only test-owned temporary directories under build are created/removed.

[Graphics using file resources](HLE_GRAPHICS.md) and [animation](HLE_ANIMATION.md) followed. Native GPU/application formats and game filesystem services remain separate work.
