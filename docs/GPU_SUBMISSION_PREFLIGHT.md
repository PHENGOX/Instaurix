# Bounded GPFIFO memory preflight

Follow-up: [historical FIFO packet decoding](GPU_PACKET_DECODER.md) now produces method traces from owned snapshots, including a documented SET_REFERENCE reference. No method executes and no job is admitted; earlier decoder boundaries below describe the previous milestone.

The original libnx AppendEntry implementation at revision
`b156b4774fb2df7b93402169509266da7547fc4c` stores its IOVA in a 64-bit descriptor
and ORs `num_cmds << 10` into its high word. The original Kickoff helper emits
flags 4 and zero input fence fields when no fence increment is requested.
This milestone supports only that observed local profile, not a complete native
GPU descriptor or synchronization ABI.

## Envelope, descriptor and memory validation

The driver handles only KickoffPb request 0xC018481B under nvIoctl2 command 11.
Other command-11 methods remain UnknownCommand and never fall through to ioctl1.
A channel needs its existing GPFIFO, graphics context, AS and Nvmap association.

Gpu::prepareKickoff validates the 24-byte parameter block, a nonzero count of at
most 2,048 descriptors, and an extra-array size exactly equal to count times 8.
Only flags 4 and zero input fence ID/value are supported. The parameter pointer
at byte 0 is ignored; the validated HIPC extra-input descriptor is authoritative.

The bounded descriptor interpretation uses the low 40 bits as GPU VA, bits
42 through 62 as word count and rejects control bits 40/41 and sync bit 63. This is deliberately a
restricted reference interpretation, not a hardware bitfield specification.
The word count must be nonzero, address four-byte aligned, and the byte range
inside the local GPU VA profile. Aggregate copied command bytes are limited to
64 KiB per preflight, independently of the descriptor-array transport limit.
All descriptor structure/aggregate checks precede command reads.

Each command range must fit within a single mapping in the channel's bound AS.
Its Nvmap object must belong to the channel's associated Nvmap device, not just
another device in the same NV service. Existing GPU-to-CPU translation then
validates current backing bounds and read permissions. No GPU VA is treated as
a CPU pointer. Adjacent independent mappings cannot be crossed by one range.

The pure helper stages owned descriptor and byte-vector copies and publishes
its output only after every read succeeds. Late failures preserve the previous
snapshot. Later CPU or descriptor-array changes cannot modify those copies.
Invalid envelopes/control fields return InvalidArguments, unreadable/unmapped
or foreign backing returns InvalidAddress, aggregate excess returns TransferLimit
and allocation failure returns OutOfResource.

## No submission admission or execution

A valid snapshot still returns UnknownCommand from the driver. No job is queued,
no fence advances, no error event signals and no persistent references pin the
mapping after the call. The temporary driver snapshot is discarded. Preflight
itself keeps command words opaque; the later packet-decoder stage is documented
separately and still does not execute methods, shaders or rendering.

These byte copies alone are not a complete asynchronous job: future admission
needs context and resource lifetime rules, including resources referenced by
decoded commands. Snapshot validation must not be reported as command execution.

## Original mapped reference and tests

Windows validation: **105/105 CTest suites pass in Debug (87.55 seconds) and
Release (25.79 seconds)**. All 36 recorded NRO hashes match; the preceding 35
binaries are unchanged. Linux was not run for this milestone.

libnx_gpu_preflight uses original full channel initialization, aligned CPU
allocation, nvMapCreate, nvAddressSpaceMap, AppendEntry and Kickoff. It maps a
64 KiB CPU buffer and sends two opaque words, 0x11223344 and 0x55667788, from its
first eight bytes. These words are test data, not a valid graphics program.
As in the prior transport fixture, only Kickoff temporarily selects client HOS
4.0.0 before restoring the saved version; SDK bodies and general profile stay
unchanged.

The mapped NRO exits after 77,624 instructions, with UnknownCommand after valid
memory preflight. Client entry count remains one and fence value remains zero.
Original Unmap and Nvmap cleanup succeed immediately after the refused call,
proving no job retention was introduced. Normal and seven-instruction slices
agree. A strict second SDK build reproduces the recorded NRO hash.

NRO SHA-256:
`7A0172F286C2DDA9D1EE96DA112CA2B9DEE4236F33EE36BA068E079121841A69`.

The preceding unmapped transport NRO remains unchanged. It now returns
InvalidAddress instead of UnknownCommand for its synthetic unmapped IOVA,
while its instruction count stays 71,232. Instruction count is not a measure
of this validation improvement.

Tests cover envelope sizes, flags/fences, descriptor count/control/alignment,
address boundaries, aggregate limits, immutable copies, late read failure and
complete output rollback. Integration tests inspect original mapped words via
GPU translation, CPU-write independence of a prepared copy, root/clone handling,
malformed counts and flags, CPU-pointer rejection, cross-mapping spans, current
read permissions, unchanged guest data and cleanup. An independently created
Nvmap device maps another object into the same AS: the channel correctly
rejects consuming it, and the object can be unmapped/freed after rejection.

## Next bounded milestone

Identify a minimal command format and reference stream from primary sources,
then implement a bounded decoder with verifiable behavior. The opaque marker
words in this fixture are not such a stream. GPU job admission, scheduling,
method execution, shaders, host rendering and native Switch 2 compatibility
remain separate work.

## Report additions

The fixture uses two 64-word blocks. Words 0 through 31 follow the transport
reference where applicable. Word 32 is CPU backing, 33 nvMapCreate result,
34 object ID, 35 Map result, 36 mapped GPU VA, 37 Unmap result, 38 Nvmap flag
after Close, and 39/40 preserved CPU words. Other unused words are reserved.
