# IPC buffer demonstration

Project-owned ARM64 ELF, Clang 18.1.6/Zig 0.13.0, without Nintendo files. Rebuild: `./tools/build_hle_ipc_buffers.ps1`; ELF layout follows hle_memory.

Fills **512 bytes** at 0x12100 with two repetitions of 0..255, asks emu:demo to copy them to 0x12400 through Send/Receive, then checks every byte. Stores count **512 at 0x12000**, success marker **1 at 0x12008** (99 on failure), closes and exits.

**5,677 instructions**. Request: HIPC header 0x01100004, RawCount=8, two 12-byte descriptors, SFCI at +32, command 2. See [limits/tests](../../../docs/HLE_IPC_BUFFERS.md).

## Recorded artifact SHA-256

`7472F0E085C8D4564B701140813D597CAFBD4AA75B4AB55CD5E9480905DF527F`

