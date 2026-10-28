# Finite-wait demonstration

Project-owned ARM64 ELF assembled by Clang 18.1.6/Zig 0.13.0, without Nintendo files. Rebuild from root: `./tools/build_hle_time.ps1`; hle_memory ELF layout.

Main creates event/worker, waits for event then worker exit. Worker sleeps **1,000 ns**, signals, sleeps **500 ns** and exits. CLI explicitly advances idle virtual time. Result: **43 instructions, 1,500 virtual ns, value 55 at 0x1200C**.

Data at 0x12000: handles +0/+4/+8, final result +12, wait result +16, post-wait X1 +24, main timeout +32 (2,000 ns), worker sleep +40 (1,000 ns), ticks +48/+56. Changing main timeout to 500 ns yields TimedOut without preventing normal completion after join. hle_time_tests covers full execution/time boundaries; see [virtual time](../../../docs/HLE_TIME.md).

## Recorded artifact SHA-256

`0C2800208D3DEE4A33EFB29699C506ACF88BBAA554FA7B3367C73F290BA553FE`

