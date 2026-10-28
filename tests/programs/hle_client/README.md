# Compiled C HLE client

Original work.c/start.S, compiled by Clang 18.1.6/Zig 0.13.0 for freestanding ARM64 O1, without libc, libnx or Nintendo SDK. work.generated.s records generated instructions. Rebuild: `./tools/build_hle_client.ps1` from project root; ELF layout follows hle_memory.

The client opens/clones SM, registers shared state, obtains/clones emu:demo, queries pointer size, computes 55, copies/checks 512 bytes, reads time and closes sessions. Assembly wrappers do not contain validation logic.

Seven u64 fields at 0x12000: status+0, sum+8, copied+16, hash+24, pointer_size+32, time+40, flags+48. Success: **1,55,512,3505779141,0,injected time,7** after **14,342 instructions**. Intermediate status identifies failure phase. See [details](../../../docs/HLE_CLIENT.md).

## Recorded artifact SHA-256

`33C49812E24B7BB1835C49B267E7FD8C75842AD01C74A1ACC411F1283070BFCC`

