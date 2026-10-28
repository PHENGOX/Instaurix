# Bounded historical graphics-context allocation

Follow-up: [GPU error-event query](GPU_ERROR_EVENT.md) now supports the original event-3 request and copied read handles. SetErrorNotifier is the next unsupported boundary; earlier event-query statements below describe the preceding milestone.

The pinned libnx revision is `b156b4774fb2df7b93402169509266da7547fc4c`.
Its unchanged `nvioctlChannel_AllocObjCtx` sends ioctl `0xC0104809` with 16
input/output bytes: class at byte 0, flags at 4 and output object identity at 8.
The original wrapper initializes the output field to 0xDEADBEEF; this is not an
input requirement. `nvGpuChannelCreate` requests historical class 0xB197 and
flags zero after GPFIFO allocation, before querying GPU error event 3.

## Local reference policy

Only a channel with an allocated GPFIFO can allocate a context. Other channel
states return InvalidState. Other device types remain UnknownCommand. The
supported profile is class 0xB197 and flags zero; other classes, including
0xC797, and nonzero flags return InvalidArguments. Accepting the historical
class does not validate its methods, shaders or native Switch 2 class semantics.

Each channel owns at most one context containing a class number and a nonzero,
process-monotonic 64-bit local object identity. A valid repeated allocation
returns AlreadyInitialized. Identities have their own namespace, are never
recycled on Close, and refuse exhaustion rather than wrapping. Since allocation
requires GPFIFO, the existing eight-queue-per-NV-service quota also bounds live
contexts. Multiple channels sharing an AS still own independent contexts.

The runtime stages the 16-byte output and context metadata, publishes both only
after the complete IPC reply reaches RAM, and advances the identity counter
only on publication. The eight input bytes are preserved. Invalid requests,
inaccessible output and failed replies preserve existing state and output.

A successful channel Close drops the context alongside its GPFIFO and AS/Nvmap
associations. Failed Close replies retain it. Root and cloned endpoints share
the owning service. Failed reload preserves context state; exit/fault releases
it and fresh load resets identities. An explicit FreeObjCtx operation remains
unsupported, consistent with the pinned wrapper's comment; channel Close is
the only supported per-channel release path.

This is context bookkeeping, not a 3D engine, command interpreter, shader
compiler, host graphics object or GPU rendering implementation.

## Original reference and progression

Windows validation: **98/98 CTest suites pass in Debug (74.64 seconds) and
Release (19.21 seconds)**. All 31 recorded NRO hashes match; the previous 30
binaries are unchanged. A strict second SDK build reproduces the new NRO hash.
Linux was not run for this milestone.

The new `libnx_gpu_context` reference calls unchanged original NV, AS, Nvmap,
channel, BindChannel, AllocGpfifoEx2 and AllocObjCtx functions. After context
allocation it calls original `nvQueryEvent` for NvEventId_Gpu_ErrorNotifier (3).
This next boundary returns UnknownCommand and publishes no event handle. The
project-owned harness observes the failure and closes its resources normally.
It does not claim successful completion of the entire nvGpuChannelCreate helper.

The same 409,600-byte NRO exits after 59,640 instructions before support
(AllocObjCtx unsupported), and 60,134 after support (error-event query
unsupported). Normal and seven-instruction slices agree. The existing GPFIFO
reference NRO remains unchanged; it now completes AllocObjCtx in 59,728
instructions, with its assertions updated to verify the local identities.

NRO SHA-256:
`6D779E6D1DBD0BFB0B7C08AE5D02FB512B660A083926ECCB67AA384FB1F2FA1F`.

The new integration suite checks original request encoding, prerequisite queue,
unsupported classes/flags, device types, input/output extents and permissions,
protected reply rollback, complete output preservation on failure, unchanged
input bytes on success, identity publication, root/clone sharing, simultaneous
independent contexts, failed/successful Close, stale descriptors, failed reload,
exit/fault and fresh-load identity reset.

Report blocks reuse the 32-word GPFIFO report layout. Word 17 is AllocObjCtx's
result, 18 its object identity, 19 the error-event query result, 20 event-active
state and 21 the event read handle. Words 22 through 31 are reserved.

## Next bounded milestone

Implement the observed GPU error-event query with explicit handle ownership and
channel/event lifetimes. No fake completion or error signal should bypass the
event model. Error notifier configuration, priority, command submission, queue
consumption, synchronization, methods, shaders and native Switch 2 validation
remain separate work.
