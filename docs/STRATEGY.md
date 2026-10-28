# Switch 2 strategy

Working decision dated October 24, 2026. The goal is a PC emulator eventually capable of running Switch 2 games. This document describes the chosen direction and does not claim current game compatibility. Historical milestone counts below describe their original scope; see [README](../README.md) for the latest complete validation.

## Current priority

The [compatibility audit](COMPATIBILITY_AUDIT.md) selects public switch-examples/libnx hello-world as an external historical Switch reference, not a native Switch 2 target. [Static NRO0 loading](NRO_LOADER.md), the homebrew entry contract, MOD0 and bounded relocations are available. The original filesystem client initializes three sessions, and its [TLS path](LIBNX_TLS.md) is validated in the reference build. The separate [fsdev reference](LIBNX_FSDEV.md) now mounts the configured SD device and reaches project main with original cleanup. [Native file opening/reading](LIBNX_NATIVE_FILES.md) is now tested through original libnx functions. The [standard guest libc/fsdev path](LIBNX_POSIX_FILES.md) now passes open/read/close checks. The [original VI client](LIBNX_VI.md) now opens a logical Default display; the next observed window-path blocker is SelfController CreateManagedDisplayLayer, before native presentation. [SD object opening](LIBNX_SD_ROOT.md) is available; native file methods and HID samples remain to be implemented. Default startup reaches main in the separate fsdev reference; the earlier libnx_app fixture keeps its explicit dependency stop.

CPU extensions follow measured blockers. Demos using emu:* are not equivalent to an unmodified external application. Once startup works, identify the required graphics interfaces before implementing the guest GPU path. No playable-game date is established.

## Chosen architecture

Initial host: Windows x64, C++20 and CMake. The hybrid approach executes ARM64 code, reproduces system services with HLE, and proposes Vulkan for host graphics. Vulkan is a PC backend choice, not a claim about the console's application interfaces.

HLE reproduces observable service results and effects, including errors, events and synchronization. Returning success for every unknown service is not an implementation. This focuses work on application requirements without making complete console OS boot a prerequisite. Performance and compatibility still require measurement.

| Layer | Direction | Implemented foundation |
| --- | --- | --- |
| CPU | Reference interpreter, later ARM64-to-x64 dynamic translation if measurements justify it | Partial integer, FP/SIMD and atomic interpretation, checked against independent corpora |
| Process memory | Virtual addresses, mappings and R/W/X permissions | Paged RAM and permissions; no MMU translation |
| Execution | Process context, stack, thread-local memory and explicit termination | Static ELF/NRO0, guarded stacks, integer/FP/SIMD/TLS contexts, two logical threads, bounded libnx bootstrap |
| HLE kernel | System calls, objects, handles, events and threads | Memory/exit services, typed handles, events, thread exit, immediate/infinite/finite waits and host-driven virtual time |
| HLE services | IPC transport and target-driven services | Partial HIPC/CMIF, A/B buffers, bounded sm:, sessions, emu:demo and read-only emu:fs; original libnx clients under development |
| GPU | Guest commands, shaders, synchronization and Vulkan | Not implemented |
| Development graphics | Inspect pixels produced by ARM64 programs | emu:disp, timestamped RGB/PNG sequences and Windows window; no guest GPU commands |
| Audio/input/files | Separate guest interfaces from Windows facilities | Keyboard-backed emu:pad and read-only snapshot emu:fs; native interfaces remain partial, audio absent |
| UI | Library, settings, controls and logs after initial targets | Command-line launcher |

The current machine's MMIO addresses, timer and 16 MiB RAM are a development bench, not the Switch 2 memory model. Keep these tests while developing a separate application environment.

## Priority changes

Full privileged exceptions and ERET are no longer the default next task. Application HLE first needs to process a program's system calls and resume at the correct PC. Model full hardware only when a target demonstrates that need.

Keep the interpreter for diagnosis. Defer a JIT until a reproducible target and performance profile exist. Add SIMD, atomics and floating point as required by observed programs; do not assume the current subset suffices for games.

## Milestones and acceptance criteria

| Milestone | Deliverable | Evidence |
| --- | --- | --- |
| J0 — strategy | This document and a sourced inventory | Separate facts, assumptions and blockers |
| J1 — minimal application | ARM64 program with sources, memory result and explicit exit | Deterministic result, distinct exit/budget/error states, negative tests |
| J2 — structured loading | Segments and permissions for a documented format | Correct sections/BSS; reject truncation, invalid ranges and unknown variants |
| J3 — HLE environment | Required system calls, thread context and IPC | Verify arguments, results, handles and buffers; identify unsupported calls |
| J4 — native Switch 2 target | Explicitly identified native program or trace | Compare observable behavior against a versioned reference |
| J5 — graphics and interaction | Understood guest graphics chain, then input and audio | Image from guest commands, verified synchronization and interaction |
| J6 — first game | One identified title and version | Reproducible loading, menu and playable sequence; measured limitations and performance |

J1–J3 build infrastructure, not proof of Switch 2 compatibility. J4 requires a console-specific reference; a Switch 1 application running in Switch 2 compatibility mode does not qualify.

## Initial J1 scope, now verified

Use a small freestanding ARM64 target containing a function call, loop, stack accesses, known memory result and explicit exit. Keep sources and exact build options with the artifact. Check available tools before installing a toolchain.

Execution results distinguish ordinary instructions, system requests, normal exit and errors. Supported requests resume at the correct PC. A local exit mechanism must be named a test protocol, not a Switch 2 ABI. Add only necessary instructions, initially stack register pairs, and test range overflow and unknown requests. The original program.bin returns to address zero and stops only through its budget.

Before selecting native formats, study NRO and NSO versions, startup and reference data separately. ELF is useful for compiled tests; it is not a Switch 2 game loader.

## Transition conditions and blockers

- Without a verifiable interface contract and reference, continue generic foundations and document the unknown rather than inventing a console ABI.
- Record each target's origin, version, dependencies and expected result. No commercial title is currently selected or validated.
- Host-generated graphics validate the backend, not guest commands or shaders.
- A service existing on both consoles does not establish identical commands or structures.
- Revisit assumptions contradicted by evidence. Do not promise game dates before identifying a native target and its dependencies.

Full firmware boot, cycle accuracy, every physical peripheral, an elaborate UI, online networking and JIT optimization are not J1 prerequisites. Advanced graphics remain deferred until a target needs them; their absence may restrict compatibility.

## Historical progress

- **J1, October 24:** [C program and ARM64 entry](../tests/programs/stack_sum/README.md), integer W/X LDP/STP, restored stack, sum 1..10 = 55 in memory and explicit test-protocol exit. Eight CTest suites passed. No Switch 2 ABI validation.
- **J2:** [Static ELF64 AArch64 loader](ELF_LOADER.md), segment placement, zeroed BSS, validated entry, R/W non-executable stack and guard page. CPU permissions are checked across pages. ELF C program: 55 in 94 instructions; 11 suites passed, including rejection of 102 invalid ELF variants. Native formats and console startup were outside that milestone.
- **Early J3:** [Reference HLE profile](HLE_RUNTIME.md), integer thread context, local registers, SetHeapSize, QueryMemory and ExitProcess. C program: 55 in 121 instructions; 15 suites passed. Handles, scheduling, events, IPC and native Switch 2 services were still future work at that stage.
- **Integer consolidation:** [CPU audit](CPU_AUDIT.md) added decoder corrections and instruction families without new HLE services. Initial validation: 18 suites, 32,832 independent ARM64 comparisons and a native-versus-guest C calculation. IPC was deferred during consolidation.
- **Atomics:** [Exclusive/atomic milestone](CPU_ATOMICS.md), single/pair exclusives, LDAR/STLR, major LSE operations, CLREX and barriers. Reservations track RAM writes in 16-byte granules. Nineteen suites included 206 assembled instructions. Execution remained sequential; host-thread synchronization and a multicore memory model were not implemented.
- **FP/SIMD:** [Initial coverage](CPU_FP_SIMD.md), [software floating point](CPU_SOFTFLOAT.md) and [SIMD arithmetic](CPU_SIMD_ARITHMETIC.md) added scalar/vector arithmetic, conversions, fused operations, extrema, rounding, structured transfers, tables, element copies, subnormals, tininess before rounding, FZ, saturation/QC, narrowing, variable shifts and widening multiplication. Validation: 24 suites, 41,304 integer cases, 96,400 general FP/SIMD cases, 64,896 subnormal/FZ cases and 61,056 arithmetic SIMD comparisons. A 343-instruction program combined these chains. The CPU remained partial; broader compiled C/C++ programs and measured missing instructions guided subsequent work.

## References

[Public-source knowledge inventory](SWITCH2_RESEARCH.md), [compatibility audit](COMPATIBILITY_AUDIT.md), and [GPU plan](GPU_PLAN.md). Architecture decisions are project choices, not official console specifications.
