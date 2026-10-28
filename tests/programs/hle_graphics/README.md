# ARM64 C graphics client

Project-owned Clang 18.1.6/Zig 0.13.0 freestanding O1 source. Rebuild: `./tools/build_hle_graphics.ps1`. Entry/wrappers reuse hle_client/start.S; ELF layout follows hle_memory.

Through SM it opens emu:fs/emu:disp, reads/checks **272 bytes** from root/sprite.txt, creates a **160×96 RGB** starfield/center pattern, hashes/presents it through IPC and closes sessions.

Four u64 report fields at 0x12000: status,loaded,presented,hash. Success: **1,272,46080,4044961488**, **614,588 instructions**. Exit status alone does not validate the report. See [export/limits](../../../docs/HLE_GRAPHICS.md). This local framebuffer submits no native GPU commands.

## Recorded artifact SHA-256

`8CB689A7E5F5701B004163E44316E25B3CE348823A95A6A628EDCC28DA9D4A9C`

