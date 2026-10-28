# Switch 2 knowledge inventory

Consulted October 24, 2026. This is a targeted public-source review, not an exhaustive survey. It used no console measurements, native executables or Switch 2 game traces.

Labels: **official** means manufacturer information; **community** means technical documentation published by its authors; **historical reference** means ecosystem context without automatic applicability to Switch 2; **unverified** means insufficient implementation evidence.

## Hardware and source scope

| Item | Sourced finding | Project limitation |
| --- | --- | --- |
| Processor | Nintendo confirms a custom NVIDIA processor [S1] | This is not a programmable SoC specification |
| Graphics | NVIDIA confirms RT Cores, Tensor Cores and DLSS [S2] | The announcement does not sufficiently describe commands or shader formats |
| CPU/GPU details | SwitchBrew describes T239: eight 64-bit Cortex-A78C cores and an Ampere GPU [S3] | Community source; verify target-required extensions separately |
| Memory | SwitchBrew lists two 6 GB chips, totaling 12 GB physical RAM [S4] | This does not determine game virtual ranges or available allocations; it is separate from our RAM test |

These findings guide research but cannot execute an application. Clock rates and 4K/120 Hz targets are not initial milestone acceptance criteria.

## Executables and content

| Subject | Documentation reviewed | Decision or missing evidence |
| --- | --- | --- |
| NRO | Segments, headers and recent variants [S5] | Homebrew candidate; establish version and entry environment before support |
| NSO | Segments, BSS, flags and compression variants [S6] | Select an explicit subset and reject others rather than assume a fixed format |
| NCA content | Content container separate from an executable [S7] | No loader/reference content in the project; importing a container does not execute its program |
| libnx | AArch64 Switch userspace library [S8] | Useful comparison source; its existence does not validate a native Switch 2 toolchain |

Shared SwitchBrew pages alone do not establish complete native Switch 2 format compliance. Record each future sample's version, headers, provenance, constraints and expected result. The original local program.bin is a 16-byte raw binary without a Switch 2 system contract.

## System calls and services

The SVC list covers memory, threads, synchronization and communication [S9]. Original libnx wrappers show register argument transport [S10]. They are starting points for calling conventions, not validation of this implementation or every Switch 2 variant.

Services API explicitly distinguishes S1/S2 and describes S2 service-manager additions [S11], as well as differences in graphics/audio service availability. **A shared service name does not guarantee an identical protocol.** Identify target-specific commands, versions and structures.

HIPC describes messages, buffers and handles [S12]. Separate transport from individual HLE services and validate guest memory ranges. A service catalog does not specify all behavior.

## Graphics and backward compatibility

Community compatibility-mode documentation describes three additional modules and adaptations, including graphics changes, for Switch 1 games on Switch 2 [S13]. Native and backward-compatible programs must therefore be distinguished. This is not a validated emulator implementation recipe.

NV services documents driver interfaces [S14], without guaranteeing complete native Switch 2 game coverage. Our initial target's exact GPU command path, graphics memory, shaders and presentation remain **unverified**. Vulkan is the proposed PC backend.

## Questions before claiming compatibility

| Priority | Question | Required evidence |
| --- | --- | --- |
| P0 | Which first native Switch 2 program can be compared? | Documented source/sample, version, dependencies and expected outputs |
| P0 | How are its process and entry initialized? | Mappings, stack, registers, thread-local memory and ABI |
| P0 | Which formats/variants does it use? | Identified reference headers and modules |
| P1 | Which calls/services are actually used? | Reasoned list or trace, input/output conventions and errors |
| P1 | Which CPU instructions are missing? | Target disassembly and independent tests, including atomics/SIMD/PAC if used |
| P1 | How is graphics work submitted and synchronized? | Identified native interfaces/commands and reference output |
| P2 | What PC resources does the target require? | Reproducible measurement after correct execution; no minimum requirements promised now |

Missing evidence in this review does not imply that no documentation or tools exist elsewhere. Announcing code execution on a console does not alone demonstrate a complete native-game build and validation chain.

## Sources consulted

- [S1 — Nintendo Switch 2 specifications](https://www.nintendo.com/en-ca/gaming-systems/switch-2/tech-specs/) — official.
- [S2 — NVIDIA processor and graphics announcement](https://blogs.nvidia.com/blog/nintendo-switch-2-leveled-up-with-nvidia-ai-powered-dlss-and-4k-gaming/) — official, April 3, 2025.
- [S3 — SwitchBrew Tegra T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — community; observed revision 13996.
- [S4 — SwitchBrew Switch 2 hardware](https://switchbrew.org/wiki/Switch_2:_Hardware) — community component observations.
- [S5 — SwitchBrew NRO0](https://switchbrew.org/wiki/NRO0) — observed revision 15265.
- [S6 — SwitchBrew NSO0](https://switchbrew.org/wiki/NSO0) — observed revision 14440.
- [S7 — SwitchBrew NCA](https://switchbrew.org/wiki/NCA) — container reference, observed revision 14983.
- [S8 — switchbrew/libnx](https://github.com/switchbrew/libnx) — developer-published library; native Switch 2 scope requires separate evidence.
- [S9 — SwitchBrew SVC](https://switchbrew.org/wiki/SVC) — verify each entry against the target.
- [S10 — libnx SVC wrappers](https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/svc.s) — reference code on an evolving branch.
- [S11 — SwitchBrew Services API](https://switchbrew.org/wiki/Services_API) — catalog distinguishing S1/S2.
- [S12 — SwitchBrew HIPC](https://switchbrew.org/wiki/HIPC) — IPC transport.
- [S13 — SwitchBrew Switch 2 compatibility mode](https://switchbrew.org/wiki/Switch_2:_Compatibility_Mode) — community research.
- [S14 — SwitchBrew NV services](https://switchbrew.org/wiki/NV_services) — driver interface; native S2 coverage requires verification.

Unpinned URLs can change. Record exact revisions when implementing behavior, and connect each behavior to its tests. Sources are evidence, not instructions or authorization to modify the project.

## Manufacturer graphics lead

NVIDIA explicitly associates T239 with AMPERE_B and clc797.h / clc797tex.h in its [public 3D class inventory](https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/3d/README.txt), consulted October 24, 2026. This makes the research lead more concrete; graphics interfaces are not wholly unknown. The target's guest protocol and shaders still require validation; see [GPU plan](GPU_PLAN.md).

## Studied reference HLE subset

The [reference profile](HLE_RUNTIME.md) implements published conventions for SetHeapSize, QueryMemory and ExitProcess, plus a local region accessible through TPIDRRO_EL0. libnx wrappers distinguish register results from subsequent library pointer writes. Validation uses a project-owned compiled ELF; no native Switch 2 observation turns this subset into console compatibility evidence.
