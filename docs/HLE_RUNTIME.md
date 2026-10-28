# Initial reference HLE environment

Historical milestone dated October 24, 2026: a local process/main thread, memory-service subset and ARM64 thread registers. **No native Switch 2 compatibility is claimed.** Studied conventions come from Horizon/libnx and require separate native Switch 2 comparison.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_memory\program.elf 500 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

Result: **121 instructions, X2=55, normal HLE exit code 0**. The program allocates a heap, computes 1..10 in C, writes 55 to heap/thread-local memory, queries heap information and exits.

--hle-reference is opt-in and initially restricted to accepted ELF files; raw input is rejected. It excludes --test-protocol. Later bounded NRO/homebrew profiles are documented separately.

## Process and context

Process tracks local main thread ID 1, heap size, page types and local-region address. An ID is distinct from a kernel handle. The context retains the prototype's 32 integer slots, PC/SP/PSTATE, TPIDR_EL0/TPIDRRO_EL0, all 32 128-bit vectors and FPCR/FPSR. Ready/Running/Paused/Waiting/Exited/Faulted states are recorded at execution boundaries. Saving a context alone is not a scheduler; later [events](HLE_EVENTS.md), [two-thread scheduling](HLE_THREADS.md) and [virtual time](HLE_TIME.md) are separate milestones.

Budget/interrupt stops can resume. Exit is final. A terminal error cannot be bypassed by another run call; reload is required. Standalone CPU tests retain explicit context restoration facilities.

## Initial services

| SVC | Inputs | Result |
| --- | --- | --- |
| 0x01 SetHeapSize | X1=size | W0=result, X1=heap base on success |
| 0x06 QueryMemory | X0=MemoryInfo buffer, X2=queried address | W0=result, W1=PageInfo on success |
| 0x07 ExitProcess | None | Terminates process; X0 is not an exit code |

These published register conventions differ from libnx C wrappers, which may write through output pointers after the SVC. Do not treat SetHeapSize X0 or QueryMemory X1 as kernel-side output pointers.

Unknown calls stop with their number and instruction address. Supported-call argument errors return results that the guest can handle.

## Local heap

Sizes are multiples of **2 MiB**, including zero, with growth/shrink support. Base is **0x200000**, capacity **8 MiB**, without ASLR. Allocated pages are R/W non-executable. Shrinking removes rights/content; reallocation yields zeros, and growth preserves earlier content.

Errors include InvalidSize=0xCA01, OutOfMemory=0xD001, InvalidAddress=0xCC01, InvalidPointer=0xE601 and OutOfResource=0xCE01. Public categories guide names, but not every real-kernel validation or its ordering is reproduced. Requests over 4 GiB are rejected even though the local limit is much smaller.

## QueryMemory

A **40-byte little-endian** MemoryInfo contains base, size, type, attributes, permissions, IPC/device reference counts and padding. Attributes, counts and PageInfo are zero here. Contiguous equal-type/equal-permission pages are coalesced. Simplified types: 0 unmapped, 3/4 static/mutable image, 5 heap, 11 stack and 12 local region; complete kernel block attributes are absent.

Queries are limited to **16 MiB local RAM**, not a full console address space. Unwritable, forbidden-page-crossing, overflowing or MMIO output buffers fail before writes. Necessary page allocation precedes copying to avoid partial output.

## Thread-local registers

The initial profile reserves an R/W non-executable page at **0xFEE000**, with a 0x200-byte local region at **0xFEE200** after reserved space. These are prototype addresses, not a full SDK/ThreadPointer initialization.

MRS TPIDRRO_EL0 reads the host-configured local base. MRS/MSR TPIDR_EL0 expose a guest-managed pointer initially zero. Guest writes to TPIDRRO_EL0 and unsupported system registers fail. W MOVZ/MOVK/MOVN emitted by this demo clear upper X bits; reserved shifts fail.

## Loading and validation

ELF segments cannot overlap heap/local reservations even with zero permissions. Image/context are staged; rejected profile conflicts preserve earlier RAM/CPU/process state. Successful loads reset heap, local regions and devices.

Validation covers legacy suites, compiled HLE execution, register conventions, pointer/size errors, resizing, thread state, system registers, CLI options, layout conflicts and later events. [Sample sources/hashes](../tests/programs/hle_memory/README.md) accompany the reference. This is a tested foundation rather than a complete native application milestone; later IPC/graphics work is tracked separately.

Sources were consulted October 24, 2026. Local addresses, limits and scope are project choices.

## Reference links

- [SwitchBrew system calls](https://switchbrew.org/wiki/SVC)
- [libnx svc.s](https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/svc.s)
- [libnx svc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h)
- [SwitchBrew thread-local region](https://switchbrew.org/wiki/Thread_Local_Region)
- [libnx tls.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/arm/tls.h)
- [Public SVC result categories](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libvapours/include/vapours/results/svc_results.hpp)
