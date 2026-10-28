# Two HLE threads demonstration

Project-owned ARM64, Clang 18.1.6/Zig 0.13.0, without Nintendo files. Rebuild: `./tools/build_hle_threads.ps1`, reusing hle_memory ELF placement.

Main creates event/worker and waits for the event then worker exit. Worker writes 42, signals, loops, writes 55 and exits; main checks 55 and exits. Ordinary path: **367 instructions**.

R/W data at 0x12000 stores event handles (+0/+4), worker handle (+8), result (+12), then integer/TLS markers. Initially zero +96/+100 let host tests request a busy-loop main or a worker that also waits on the event before calculating.

Integer/vector/TLS markers and separate stacks check context preservation. C++ also checks FPCR/FPSR, guards and scheduling equivalence under one-instruction slices. See [scheduler limits](../../../docs/HLE_THREADS.md).

## Recorded artifact SHA-256

`B8969BED87ADBE2D2C5BAE75491E9BFE8EF68EB359F2820CBC7F621EB9532CEB`

