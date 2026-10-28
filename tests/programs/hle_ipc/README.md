# Two-thread IPC demonstration

Project-owned ARM64, assembled by Clang 18.1.6/Zig 0.13.0 without Nintendo files. Rebuild: `./tools/build_hle_ipc.ps1`, reusing hle_memory ELF layout.

Main opens emu:demo, builds HIPC/CMIF in its local region, computes 20+35 and creates a worker. Worker computes 44+55 through the same session in its own buffer. Main joins, checks results/buffer isolation, reads time and closes the session.

**116 instructions**. At 0x12000: main result 55 (+0), worker result 99 (+4), worker handle (+8), virtual time (+16), validation marker 1 (+24), or 99 on detected failure. This is a local descriptor-free demo subset, not a native console service. See [IPC](../../../docs/HLE_IPC.md).

## Recorded artifact SHA-256

`F5722EE5D1C337C258EBE3989BC962738508F75BE8F416E41FEA1560B29FC2DD`

