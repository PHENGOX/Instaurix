# Reference channel/address-space binding

Follow-up: [bounded GPFIFO allocation](GPU_GPFIFO_ALLOCATION.md) now supports the observed AllocGpfifoEx2 profile. AllocObjCtx is the next unsupported boundary; earlier allocation statements below describe the preceding milestone.

The unchanged libnx function `nvioctlNvhostAsGpu_BindChannel` at revision
`b156b4774fb2df7b93402169509266da7547fc4c` sends ioctl `0x40044101` to an
address-space descriptor with one four-byte channel descriptor as input and no
receive buffer. The higher-level `nvGpuChannelCreate` calls this after channel
creation and before `AllocGpfifoEx2` and graphics-object allocation.

## Local reference policy

The address space must be initialized. The target must be a live channel in the
same NV service and already associated with a live Nvmap descriptor. Missing or
wrongly typed channel descriptors return InvalidHandle; a channel without Nvmap
returns InvalidState. Repeating a binding, including to another initialized
address space, returns AlreadyInitialized. Multiple channels may share one AS.

The published relationship stores the owning service's AS descriptor identity,
not a CPU address or copied reservation table. Publication occurs only after the
IPC reply succeeds. Clones share the service and relationship. A channel retains
its AS and Nvmap descriptors until its successful Close. Closing an AS with any
bound channel returns InvalidState, preserving its reservations and mappings.
Closing one channel does not release retention held by another. There is no
unbind operation or channel migration in this profile.

These lifetime and duplicate-binding rules are deliberately bounded local policy;
the source establishes client request layout and ordering, not complete native
driver semantics. There are no outstanding GPU jobs. Process exit/fault drops
all driver state; failed reload preserves it and fresh load resets identities.

## Original client reference

Windows validation: **96/96 CTest suites pass in Debug (64.51 seconds) and
Release (14.89 seconds)**. All 29 recorded NRO hashes match. A strict second
build reproduces the new NRO hash; the previous 28 NROs are unchanged. Linux
was not run for this milestone.

`libnx_gpu_bind` runs two original NV/AS/Nvmap/channel lifetimes, calls the
original BindChannel and then attempts the original AllocGpfifoEx2 with 0x800
entries and flags 1. GPFIFO allocation is an explicit UnknownCommand boundary;
no fence, command storage or executable job is published. The harness closes
the channel before its AS and Nvmap descriptor.

The same 401,408-byte NRO exits after 57,910 instructions before support
(UnknownCommand at BindChannel), and 58,724 after support (UnknownCommand at
AllocGpfifoEx2). Normal and seven-instruction slices agree. The fixture uses
unchanged SDK bodies; only the project-owned harness defines the bounded test.
The build uses the same Clang/Arm GNU split as the prior channel reference.

NRO SHA-256:
`CC2B494D27AD6A1D5F5DC81E71D960C3F678956A70C0C50BD57712A1891AF292`.

Host probes test malformed transport, missing and wrong-type descriptors,
uninitialized AS, channel-before-Nvmap rejection, duplicate/cross-AS binding,
root/clone state, protected binding and Close replies, shared-AS retention,
stale descriptors, failed reload, exit/fault and fresh-load cleanup.

The legacy `gpu_as_report` symbol contains two 32-word blocks. Each records:
0 NV initialization, 1 AS Create, 2 AS FD, 3 Nvmap initialization, 4 Nvmap FD,
5 channel Create, 6 channel FD, 7 BindChannel, 8 AllocGpfifoEx2,
9 fence ID, 10 fence value, 11 channel FD after Close, 12 channel flag,
13 AS FD after Close, 14 AS flag, 15 Nvmap FD after Exit, 16 NV session after
Exit. Words 17 through 31 are reserved.

## Next boundary

Audit and implement bounded GPFIFO allocation/lifetime before accepting any
submission. Allocation is not execution: queue consumption, fences, command
decoding, shaders, a host GPU backend and native Switch 2 compatibility remain
unimplemented or unvalidated.
