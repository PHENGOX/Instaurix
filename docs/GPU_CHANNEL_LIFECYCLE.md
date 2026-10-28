# Reference GPU channel lifecycle

Follow-up: [original full channel initialization and priority](GPU_CHANNEL_INITIALIZATION.md) now validates nvGpuChannelCreate/Close. Submission transport is the next bounded milestone; earlier priority boundaries below describe previous snapshots.

Follow-up: [original channel/address-space binding](GPU_ADDRESS_SPACE_BINDING.md) now implements BindChannel. The next observed boundary is AllocGpfifoEx2; earlier next-step statements below describe the preceding milestone.

The pinned libnx revision is `b156b4774fb2df7b93402169509266da7547fc4c`.
Its unchanged `nvidia/channel.c` implements `nvChannelCreate` by opening the
requested device and calling `nvioctlChannel_SetNvmapFd` with `nvMapGetFd()`.
`nvidia/ioctl/nvchannel.c` encodes that call as `0x40044801`, a write-only
four-byte Nvmap descriptor. `nvChannelClose` closes the device and clears the
client object's initialized flag; repeated client-side Close sends no request.

## Bounded local policy

Only `/dev/nvhost-gpu` is a channel endpoint. It shares the existing per-service
32-descriptor quota and process-monotonic descriptor identities. SetNvmapFd
requires a live Nvmap device in the same NV service. A channel can bind once;
another valid binding returns AlreadyInitialized. Missing or wrongly typed
Nvmap descriptors return InvalidHandle. Other ioctls remain UnknownCommand.

Binding and endpoint publication occur only after the IPC reply write succeeds.
A bound channel prevents its Nvmap descriptor from closing. Channel Close drops
the binding only after a successful reply. Closing an unbound channel is valid.
There are no outstanding jobs, so process teardown can discard all channels.
These retention and duplicate-binding rules are explicit local policy; the
client source establishes request encoding and ordering, not full native driver
semantics.

The original `nvChannelSetPriority` call is deliberately the next unsupported
boundary. This milestone does not bind a channel to an address space, allocate
GPFIFO storage, allocate a graphics object, submit commands or execute shaders.
It does not establish native Switch 2 compatibility.

## Reference construction

`tools/build_libnx_gpu_channel.ps1` builds the unchanged SDK functions with a
project-owned harness. The harness runs two NV/Nvmap/channel lifetimes and
records creation, the priority boundary and cleanup. The legacy symbol name
`gpu_as_report` is reused for the existing symbol extraction option; it now
contains two 16-word channel report blocks.

The original `nvchannel.c` requires GNU variable-length structure members.
That translation unit uses the existing pinned Arm GNU 14.3.1 compiler;
the remaining translation units use Zig 0.13.0 / Clang 18.1.6. No SDK function
body is rewritten. Link-time section removal excludes unused submission code.

Report words per lifetime: 0 NV initialize result, 1 Nvmap initialize result,
2 Nvmap FD, 3 channel create result, 4 initialized flag, 5 channel FD,
6 priority result, 7 initialized flag after Close, 8 FD after Close,
9 initialized flag after repeated Close, 10 Nvmap FD after Exit,
11 NV session after Exit; words 12 through 15 are reserved.

## Measured progression

Windows validation: **95/95 CTest suites pass in Debug (59.59 seconds) and
Release (13.55 seconds)**. All 28 NRO hashes match their manifests. A strict
second build reproduces the new NRO hash. Linux was not run for this milestone.

The same 401,408-byte NRO first exited after 53,962 instructions with
FileNotFound at channel Open. With channel support it exits after 55,948
instructions, with successful Create and UnknownCommand at the original
SetPriority call. Full and seven-instruction slices agree. NRO SHA-256:
`310A886651D91D91F91E4162F41520479649AD505BA9E9A2204EF762BB65D197`.

The integration suite also probes root/clone ownership, typed binding targets,
duplicate binding, invalid transport extents, protected Open/SetNvmapFd/Close
replies, the shared descriptor quota, Nvmap close retention, stale channel FDs,
failed reload and process exit/fault followed by fresh load. The guest NRO is
unchanged during these host-injected checks.

Next bounded milestone: original address-space `BindChannel`, with endpoint
ownership and lifetime validation. Priority is the chosen standalone harness's
observed boundary, not a claim that it is the first operation in the higher-level
`nvGpuChannelCreate` path. That helper binds the address space and allocates
GPFIFO/object context before configuring later channel attributes.
