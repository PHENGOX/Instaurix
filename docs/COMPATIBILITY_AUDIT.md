# Audit: moving from demos to an external application

Historical audit dated October 25, 2026, based on repository code and public sources. Project-specific emu:* clients do not measure compatibility with a complete SDK or native Switch 2 software. See [README](../README.md) for subsequent milestones.

## Selected external target

**switchbrew/switch-examples, graphics/printing/hello-world/source/main.c**, with libnx startup. The repository provides Switch examples for devkitA64/libnx. This is a historical Switch reference, not native Switch 2 evidence.

The source examined on master calls console output, controller configuration, an applet loop and display updates, without emu:* services. Success must be demonstrated without replacing these dependencies with demo interfaces.

**The complete hello-world target had not been compiled or executed in this audit.** No third-party NRO or Nintendo SDK was imported. Pinned public revisions: switch-examples `669786898205b7beb25ff1731e72982e6d0397d3`, libnx `b156b4774fb2df7b93402169509266da7547fc4c`. A [first unchanged libnx component](LIBNX_REFERENCE.md) executes separately. A complete pinned devkitA64/newlib toolchain is still needed for the original hello-world.

## Identified gaps

| Layer | Existing implementation at audit time | Target requirement/blocker |
| --- | --- | --- |
| Format | ELF ET_EXEC and bounded static NRO0 | libnx NRO MOD0 startup and linking |
| Entry | Zeroed registers, bench-specific stack/TLS | Homebrew entry environment, loader return and main-thread handle |
| Linking | Fixed addresses, no host dynamic linker | MOD0/GOT/relocations and libnx's own dynamic startup |
| Memory | 16 MiB, bounded local heap, simplified QueryMemory | Compare libnx allocation/information requests against a reference |
| Runtime | Original SVC wrappers, freestanding C without libc | libnx/newlib setup, thread context and exit |
| Services | Partial sm:, emu:demo/fs/disp/pad | Actual console, applet and controller services |
| Graphics | CPU RGB sent to emu:disp | Client-library display path, services and synchronization |
| Input | Local five-button mask | libnx controller interfaces and structures |

switch_crt0 performs dynamic linking and initialization before main. init.c contains heap choices and service dependencies. Source inspection identifies obligations; it does not provide an execution trace or the exact first SVC reached by a compiled program.

## Concrete progress at this stage

The loader accepts a bounded [static NRO0 profile](NRO_LOADER.md). The existing project C client was packaged with separate BSS, executes **14,343 instructions**, and produces the same report as its ELF version. It is a project-owned sample, not the external hello-world.

MOD0 NROs are rejected by default. An [experimental entry profile](NRO_BOOTSTRAP.md) supplies an environment, main handle and bounded relocation validation. Its bench executes unchanged libnx dynamic startup with a project-owned entry point. Unknown variants and content formats never silently fall back to raw loading.

## Work order and acceptance

1. Pin public revisions, toolchain and target binary hash.
2. Describe and test homebrew entry: environment, memory, TLS, main handle and exit.
3. Validate MOD0 and required relocations using reproducible references; reject unknown types.
4. Run a minimal libnx target to its first stop, record arguments and fix that blocker. A non-graphical intermediate target may precede the full console.
5. For hello-world, obtain expected text, a stable loop and exit through the intended button, without changing its source to emu:*.

At the time of this audit, progress included [NRO entry](NRO_BOOTSTRAP.md), [original crt0/RELRO](LIBNX_CRT0.md), [minimal initialization to main](LIBNX_INIT.md), [original SM client](LIBNX_SM.md), [APM](LIBNX_APP.md), and [appletOE domain interfaces](LIBNX_APPLET_DOMAINS.md). The next blocker then was GetAppletResourceUserId and application state. Later work is tracked in README. The reference used a newlib subset compiled with Zig; a complete devkitA64 toolchain and further services remained outside its scope. CPU additions follow encountered instructions.

A native Switch 2 milestone requires a distinct target, origin, version and verifiable comparison. A historical Switch reference is insufficient.

## Sources consulted October 25, 2026

- [switch-examples](https://github.com/switchbrew/switch-examples) and [hello-world](https://github.com/switchbrew/switch-examples/blob/master/graphics/printing/hello-world/source/main.c).
- [libnx switch_crt0.s](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/switch_crt0.s): entry and pre-main startup.
- [libnx init.c](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/init.c): runtime/heap initialization.
- [SwitchBrew NRO](https://switchbrew.org/wiki/NRO) and [switch-tools elf2nro.c](https://github.com/switchbrew/switch-tools/blob/master/src/elf2nro.c): format and conversion.

Versions and forms not checked against implementation remain compatibility assumptions rather than supported features.
