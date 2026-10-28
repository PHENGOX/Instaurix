# C file-reading client

Project-owned freestanding ARM64 O1, Clang 18.1.6/Zig 0.13.0. Rebuild: `./tools/build_hle_files.ps1`. Entry/SVC wrappers reuse hle_client/start.S; ELF layout follows hle_memory.

root contains only sample.txt, **30 bytes including final LF**. Through SM/emu:fs, the client opens it, checks size/contents/FNV-1a, clones its session and checks partial/EOF reads, closing intermediary sessions.

From this directory with a valid executable path: `Instaurix program.elf 50000 --hle-reference --read-root root`.

Six u64 report fields at 0x12000: status,size,read,hash,partial,eof. Success: **1,30,30,530586646,9,0** in **4,006 instructions**. Normal exit alone is insufficient; tests inspect the report. See [scope](../../../docs/HLE_FILES.md).

## Recorded artifact SHA-256

`BE2CE87106895290C85AEE8EDB0CC84ED5BFE209C84DFE4ACA7F9635138BD0A4`

