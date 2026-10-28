# Instaurix 0.86

Previously named Switch2Emu. The current executable is `Instaurix.exe` on Windows.

## A note from the author

Hey everyone,

First of all, I'm sorry for the misunderstanding surrounding my project. I'm still new to all of this, and I honestly didn't expect it to get this much attention.

Instaurix is an experimental project that I'm working on for fun and to learn more about emulation and low-level programming. I'm not a professional developer, and I'm still learning as I go, with the help of AI tools.

I want to make it clear that this is **not a working emulator yet**, and I'm not claiming that it can run any commercial games. It's a work in progress, and there's still a huge amount of work to do.

The project is **open source**, so anyone is free to explore the code, learn from it, or reuse anything they find useful, in accordance with the project's license. If any part of my work can help someone else's project, feel free to use it!

I never intended to mislead anyone or create unrealistic expectations. I was simply sharing what I've been working on, and I genuinely didn't expect it to reach this many people.

I'm sorry again for the confusion and for any inconvenience I've caused. Thank you to everyone who took the time to check out the project, share feedback, or offer constructive criticism.

I'm just a beginner trying to learn, experiment, and build something interesting. ❤️

## Current technical status

I am developing this emulator with the help of artificial intelligence for coding, testing, and technical analysis.

Project documentation, source comments, tools, and runtime messages are maintained in English.

An experimental C++20 emulator aiming to eventually run Nintendo Switch 2 games on PC. Current development focuses on the ARM64 foundations and HLE system services. **No commercial games or native Switch 2 compatibility have been validated yet.**

A C++20 ARM64 machine prototype featuring partial integer/FP/SIMD CPU support, a reference HLE runtime, and bounded support for static ELF/NRO formats. It does not run Switch 2 games.

Latest memory milestone: [sparse reservation tracking](docs/SPARSE_RESERVATION_TRACKING.md). Reservation counters now allocate per written page; the unchanged 12 GiB virtual-memory legacy test measured approximately 104 MiB peak working set instead of 6.05 GiB on the Windows development machine.

Latest diagnostics milestone: [automatic Windows test reports](docs/HOST_DIAGNOSTICS.md). CTest records native exit status, output and sampled host memory, including failures that previously had no captured output. The legacy virtual-memory test remains isolated.

GPU milestone: [guest-generated block-source conversion](docs/LIBNX_GUEST_GPU_BLOCK_COPY.md). A project-owned ARM64 guest now builds physical Fermi GOB data and B0B5 conversion commands through unchanged libnx functions, then verifies source bytes, converted rows, padding, guards and logical/physical hashes. Normal runs have no host command injection; independent host checks and separate missing-conversion/padding-corruption tests corroborate the guest verifier. This remains synchronous historical Maxwell HLE. Reverse conversion, general surface layouts, hardware completion/cache behavior, rendering, shaders and native Switch 2 compatibility remain unvalidated.

## Build and test (PowerShell)

The repository includes the small programs and reference data needed for testing. Downloaded tools, caches, and build directories are excluded. Zig, Python, Unicorn and, for references using GNU extensions, Arm GNU GCC are used to rebuild references; they are not required to compile the C++ core and run the supplied tests. The libnx validations concern the original Switch ecosystem and do not demonstrate Switch 2 compatibility. Latest validation: **127 CTest suites passing in Debug and Release on Windows**.

**Git LFS is required to retrieve three large CPU reference files.** After cloning, run `git lfs install` and then `git lfs pull` before testing. GitHub ZIP downloads may contain LFS pointers instead of the actual data. ELF/NRO test programs remain included normally.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

CMake 3.20 or later and a C++20 compiler are required. On Windows, the Visual Studio C++ build tools are suitable.

## License and contributions

Instaurix's own code is distributed under **GNU GPL version 3 only** (`GPL-3.0-only`); see [LICENSE](LICENSE). Third-party components and references retain their original licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

See [CONTRIBUTING.md](CONTRIBUTING.md) for the English-language policy and checks. Contributions are welcome: provide a reproducible case, describe limitations, and respect the original licenses. Contributions to the project's own code are submitted under GPL-3.0-only. Do not attach games, firmware, console keys, or confidential files to contributions.

An independent project with no affiliation with Nintendo. To prepare the GitHub repository, see the [publication guide](docs/GITHUB_PUBLICATION.md).

## Run the supplied program

```powershell
.\build\Debug\Instaurix.exe .\program.bin 4
```

Expected result: 4 instructions, X0=5, X1=7, and X2=12. The program contains MOVZ, MOVZ, ADD, and RET. RET returns to X30, initially zero; the four-instruction budget therefore stops execution before it starts over.

The first argument is a raw ARM64 binary file loaded at address 0. Its size must be nonzero, a multiple of 4, and no greater than 16 MiB. The second argument is a strict instruction budget (100000 by default). Reaching the budget is an intentional stop, not proof that the program has finished. A file, instruction, or bus error produces a nonzero exit code.

## Project structure

- `src/cpu.*`: step-by-step ARM64 interpretation; all accesses go through Bus.
- `src/memory.*`: RAM, with 4 KiB pages allocated on demand.
- `src/bus.*`: RAM and MMIO devices; accesses crossing a region boundary are rejected.
- `src/machine.*`: component assembly, loading, and execution loop.
- `src/main.cpp`: command-line interface.
- `tests/legacy_tests.cpp`: historical checks; the process fails if a FAIL line appears.
- `tests/core_tests.cpp`: regression checks, 5,334 logical masks, and CPU/bus/timer/IRQ integration.

## Development machine

These addresses are specific to the prototype and do not represent the Switch 2 motherboard.

| Address | Function |
| --- | --- |
| 0x00000000 to 0x00FFFFFF | 16 MiB RAM; program at 0, initial stack at 0x01000000 |
| 0x10000000 | Test device: ID +0, CONTROL +4, STATUS +8, DATA +12 (32-bit) |
| 0x10001000 | IRQ: PENDING +0 (write bits to acknowledge), ENABLE +4, ACTIVE +8 (32-bit) |
| 0x10002000 | Timer: COUNTER +0, PERIOD +8 (64-bit), CONTROL +16 (32-bit, bit 0 enables it) |

Each successfully executed instruction advances the timer by one tick. The timer triggers IRQ 4. An active, enabled IRQ returns control from the execution loop to the host program, which can acknowledge it and resume execution. ARM64 exception entry, vectors, privilege levels, and ERET remain to be implemented. The timer does not model real time or the cycles of a physical processor.

## Limitations and next steps

The ARM64 interpreter remains partial: MMU translation, architectural exception handling and parallel multicore execution are not implemented. Historical HLE references cover applet/HID services, software framebuffer presentation and a bounded CPU-interpreted Maxwell copy engine. A general guest GPU rendering pipeline, shaders, audio backend, commercial-game loading and native Switch 2 compatibility remain unvalidated. Tests cover the documented reference cases rather than full ARM64 or console compatibility.

Chosen direction: a hybrid application emulator with HLE system services. First milestone completed: a compiled C program with a stack, a verifiable result, and explicit termination. Static ARM64 ELF loading and R/W/X memory permissions are now available for test programs. Full hardware exceptions are no longer the default next task. `Machine::load` replaces RAM and resets the CPU and devices after a successful load; a failed load preserves the previous session.

## Switch 2 strategy

- [Architecture, milestones, and next task](docs/STRATEGY.md)
- [Knowledge inventory and public sources](docs/SWITCH2_RESEARCH.md)

These documents distinguish the ARM64 prototype from native Switch 2 interfaces that still need validation. The long-term goal is playable games on Windows; no game compatibility is currently claimed.

## Historical development milestones

The sections below record earlier implementation stages. Their counts and former limitations are historical snapshots; the current status and next task are summarized above and in the linked GPU reports.

## First C program and stack instructions

Integer register pairs LDP/STP are implemented for W/X registers, with signed offsets, pre-indexing, and post-indexing. SIMD variants and MMIO pairs remain unsupported.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\stack_sum\program.bin 500 --test-protocol
```

This compiled demonstration computes 55 in memory and terminates normally through a local test protocol. The integration test also checks stack restoration. This protocol does not represent a Switch 2 ABI and remains disabled by default.

- [Program sources, reproduction, and limitations](tests/programs/stack_sum/README.md)
- [GPU technical plan](docs/GPU_PLAN.md)

## ARM64 ELF programs and memory protection

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\elf_sum\program.elf 500 --test-protocol
```

The reference ELF program computes **55** in **94 instructions**, with R/X code, R constants, R/W data, and a non-executable stack. **All 11 CTest tests pass**, including the rejection of 102 invalid ELF variants. Earlier raw binaries remain available and retain their original R/W/X memory behavior.

- [ELF loader, memory permissions, and restrictions](docs/ELF_LOADER.md)
- [ELF program sources and reproduction](tests/programs/elf_sum/README.md)

## First reference HLE profile

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_memory\program.elf 500 --hle-reference
```

This profile adds a process and a main thread, thread-local registers, followed by SetHeapSize, QueryMemory, and ExitProcess according to public reference conventions. The C demonstration produces **55 in 121 instructions**; **all 15 CTest tests pass**.

This mode is explicitly enabled, restricted to ELF files, and separate from the previous test protocol. Its memory layout is local, and its native Switch 2 compatibility remains to be validated.

- [Services, context, limitations, and sources](docs/HLE_RUNTIME.md)
- [HLE program and reproduction](tests/programs/hle_memory/README.md)

## Integer CPU audit and consolidation

The CPU now handles common W/X integer calculations: arithmetic with flags and carry, multiplication/division, shifts and rotations, bitfields, and conditional selections. Scalar memory access and condition decoding have been reviewed.

Validation of the current integer corpus: **41,304 independent comparisons** across **775 variants**, now including CCMP/CCMN. A 32/64-bit C program produces the same results in the emulator and in native execution, using 303 guest instructions. The complete suite now contains 24 CTest tests with the milestones below.

- [CPU audit, coverage, and limitations](docs/CPU_AUDIT.md)
- [Calculation program and reference reproduction](tests/programs/cpu_integer/README.md)

The CPU is not finished: FP/SIMD remains partial, and several other instruction families still need development. Adding HLE and IPC services is deferred to prioritize these foundations.

## Atomics and exclusive accesses milestone

LDXR/STXR, their acquire/release variants, exclusive pairs, LDAR/STLR, and the main LSE operations (including CAS and SWP) are available. Reservation tracking detects writes within the same RAM granule, including writes from the host or another CPU executed sequentially. CLREX and common barriers are handled. The model remains sequential and does not yet provide parallel multicore execution.

Validation of this milestone: **19 CTest suites**, including a new suite using **206 independently assembled instructions**. See [coverage, tests, and limitations](docs/CPU_ATOMICS.md).

## First FP/SIMD milestone

The 32 vector registers, FPCR, and FPSR are saved in the thread context. The CPU executes an initial set of S/D floating-point calculations, comparisons, moves, B/H/S/D/Q memory accesses, and integer vector operations. Subnormal calculations and FZ are now supported by a software engine; trapped exceptions and certain FPCR modes remain rejected.

Validation: **24 CTest suites passing**, with **96,400 general FP/SIMD cases** and **64,896 additional subnormal/FZ cases**. The same C float/double calculation produces identical bits on the host and emulator; the test executable also combines SIMD operations, integer/floating-point and S/D conversions, and fused multiply-add. It finishes in **343 instructions**.

Conversions cover signed/unsigned W/X integers and all five rounding modes for conversions to integers, with saturation and error flags. FMADD/FMSUB/FNMADD/FNMSUB S/D use a single rounding step. Unsupported FPCR modes remain explicitly rejected; subnormals and FZ are available.

Integer SIMD now includes multiplication/accumulation, signed and unsigned comparisons, minimum/maximum operations, reductions, interleaving and extraction, element order reversal, bit counting, and immediate shifts. A chain of these operations and CCMP/CCMN is checked over 112 bytes against a separate scalar reference.

Floating-point vectors 2S/4S/2D support FADD/FSUB/FMUL/FDIV/FSQRT, fused FMLA/FMLS, and FABS/FNEG. They share scalar rounding and NaN handling. An error in one element cancels the entire instruction. A second 112-byte chain is compared bit for bit with native calculations. The software engine also handles arithmetic subnormals and FZ.

Three extensions add pairwise floating-point reductions and minimum/maximum operations on 4S, vector integer/floating-point conversions, FRINT instructions, and scalar/vector minimum/maximum operations. A 128-byte chain checks their integration. NM variants, signed zeros, and the flag difference between FRINTI and FRINTX are checked.

Multiple-structure transfers LD1/ST1 through LD4/ST4, TBL/TBX table operations, DUP/INS element copies, and SMOV are available. A 144-byte chain checks deinterleaving, calculations, table lookup, and reinterleaving. Incomplete accesses are rejected before any side effects, and writes invalidate exclusive reservations.

- [CPU coverage, limitations, and next steps](docs/CPU_FP_SIMD.md)
- [Program and reproduction](tests/programs/cpu_fp/README.md)

The [software floating-point engine](docs/CPU_SOFTFLOAT.md) operates on bit patterns using exact integers: it handles subnormals, tininess before rounding, FZ, and UFC/IXC/IDC flags without relying on the PC's floating-point calculations.

The [SIMD arithmetic milestone](docs/CPU_SIMD_ARITHMETIC.md) adds saturation/QC, narrowing, variable shifts, and widening multiplication: **61,056 additional independent comparisons**. The next priority is a wider range of compiled C/C++ programs before moving on to multiple threads.

## Contributors

Thanks to [@dougchansan](https://github.com/dougchansan) for the Linux atomic-test compilation fix and the Windows/Linux CI workflow, adapted from [PR #1](https://github.com/dougchansan/switch2emu/pull/1).
