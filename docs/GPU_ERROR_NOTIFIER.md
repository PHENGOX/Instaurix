# Reference GPU error-notifier configuration

Follow-up: [original full channel initialization and priority](GPU_CHANNEL_INITIALIZATION.md) now validates nvGpuChannelCreate/Close. Submission transport is the next bounded milestone; earlier priority boundaries below describe previous snapshots.

The pinned libnx revision is `b156b4774fb2df7b93402169509266da7547fc4c`.
Original `nvioctlChannel_SetErrorNotifier` sends ioctl `0xC018480C` with a
24-byte input/output structure: u64 offset at byte 0, u64 size at 8, u32 enable
at 16 and u32 padding at 20. The wrapper sets offset, size and padding to zero.
`nvGpuChannelCreate` enables it after querying error event 3, before priority;
`nvGpuChannelClose` disables it before closing the event and channel.

## Bounded local policy

Only a channel owned by the initialized NV service can configure a notifier.
This reference profile requires an allocated graphics context and a queried
error-event owner. Missing prerequisites return InvalidState. Other device
types remain UnknownCommand and retired/missing FDs return InvalidHandle.
Offset, size and padding must be zero, and enable must be exactly 0 or 1.
Other values return InvalidArguments. These restrictions and prerequisite rules
define the local supported subset, not full native driver behavior.

Each channel starts disabled. Valid enable and disable calls are idempotent.
Root and cloned NV endpoints share the state; distinct channels remain
independent even when sharing an address space. Successful replies copy all 24
request bytes unchanged to the validated receive buffer, including when input
and output are disjoint. State changes publish only after the full reply and
data write succeed. Invalid requests and reply failures preserve prior state.

The host-only `Process::gpuErrorNotifierEnabled(endpoint, descriptor)` diagnostic
returns an optional bool for a live channel with the context/event prerequisites.
It returns no value for absent, foreign, closed or uninitialized identities or
after process exit/fault. This supports inspection and state/rollback tests;
it does not add a fabricated guest query command.

Successful channel Close drops notifier state with its context, queue and event
owner; Close is allowed while enabled because no jobs exist. Failed Close keeps
the enabled state. A new channel reusing a table slot starts disabled. Existing
copied error-event handles retain their prior lifetime policy and stay unsignaled.
Failed reload preserves live state; exit/fault drops it and fresh load resets it.

Enabling or disabling does not signal, clear or generate an error. No notifier
memory mapping, GPU error delivery, command completion or native synchronization
semantics are claimed. Those require a future job/error backend and a defined
delivery/reset contract.

## Original reference and progression

Windows validation: **100/100 CTest suites pass in Debug (66.82 seconds) and
Release (16.03 seconds)**. All 33 recorded NRO hashes match; the preceding 32
binaries are unchanged. A strict second SDK build reproduces the new NRO hash.
Linux was not run for this milestone.

`libnx_gpu_notifier` runs two original channel lifetimes, enables the notifier,
attempts original medium priority, disables it for cleanup, polls the error
event with zero timeout and closes resources. Priority remains UnknownCommand,
while the poll returns TimedOut. This is a bounded harness, not proof that the
entire nvGpuChannelCreate helper has completed.

The same NRO exits after 62,006 instructions before support (notifier enable
and disable unsupported), and 62,878 after support (priority unsupported).
Normal and seven-instruction slices agree. A strict second SDK build reproduces
the recorded hash. The preceding event reference binary stays unchanged and
now completes enable/disable in 62,226 instructions.

NRO SHA-256:
`7FFE1632D24059CE7AF01CF7F8BA692C2C07E9F9A49C910421585C1EF9B2429E`.

Host tests cover prerequisite context and event, every request field, strict
boolean values, device types, invalid input/output extents and permissions,
separate receive buffers, complete byte preservation, protected reply rollback,
idempotence, root/clone sharing, independent channels, failed and successful
Close while enabled, slot reuse, unsignaled events, failed reload, exit/fault
and fresh-load state. Only project-owned harnesses/tests change; SDK bodies
remain unchanged.

The report uses the prior event fixture's two 32-word blocks. Word 27 adds the
original SetPriority result; words 28 through 31 remain reserved.

## Next bounded milestone

Implement the observed channel priority configuration and its validation, then
check the full original channel-initialization helper before attempting command
submission. No GPU commands, methods or shaders execute; host GPU rendering and
native Switch 2 compatibility remain unvalidated.
