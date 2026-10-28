# Original GPU channel initialization and priority

Follow-up: [nvIoctl2 transport validation](GPU_IOCTL2_TRANSPORT.md) now decodes the actual three-buffer request. KickoffPb remains unsupported and no job is admitted; earlier transport statements below describe the preceding milestone.

The pinned libnx revision is `b156b4774fb2df7b93402169509266da7547fc4c`.
Unchanged `nvioctlChannel_SetPriority` sends write-only ioctl `0x4004480D` with
one u32 value. The SDK enumerates Low=50, Medium=100 and High=150. Its original
`nvGpuChannelCreate` configures priority after opening a channel, associating
Nvmap, binding the AS, allocating GPFIFO and graphics context, querying the error
event and enabling its notifier.

## Local priority policy

A typed channel in its owning initialized NV service needs its Nvmap association
before setting priority. No GPFIFO/context/event prerequisite is imposed on this
standalone ioctl, preserving use through the lower-level nvChannel wrapper.
Other device types remain UnknownCommand, missing/retired FDs return InvalidHandle
and unassociated channels return InvalidState. Values outside 50/100/150 return
InvalidArguments. Valid repeated calls are idempotent and may change the selected
priority. This defines the supported reference subset, not native scheduling.

New channels have no configured priority. The host-only gpuChannelPriority
diagnostic reports 0 for that local unconfigured state, a supported priority
after configuration, or no value for an invalid/unready identity. Zero is not
accepted as an ioctl priority and is not a claim about a hardware default.
Root and clone share state; distinct channels remain independent.

Write-only input may be read-only. A non-null receive descriptor is rejected.
Priority publishes only after a successful IPC reply, with no mutation of input.
Failed updates retain the previous value. Successful channel Close drops state;
failed Close retains it. Reused table slots start unconfigured. Failed reload
preserves configuration; exit/fault and fresh load remove it.

Priority is stored metadata. It does not change CPU scheduling, host thread
priority or GPU execution order: there are no submitted jobs or GPU scheduler.

## Complete original helper reference

Windows validation: **101/101 CTest suites pass in Debug (62.86 seconds) and
Release (14.68 seconds)**. All 34 recorded NRO hashes match; the preceding 33
binaries are unchanged. A strict second SDK build reproduces the new NRO hash.
Linux was not run for this milestone.

The new libnx_gpu_channel_init fixture links unchanged nvidia/gpu_channel.c.
Its project-owned harness initializes NV/AS/Nvmap, calls actual nvGpuChannelCreate
with Medium and High in two lifetimes, polls the returned unsignaled event,
then calls actual nvGpuChannelClose twice before caller-owned AS/Nvmap teardown.
The entire helper now returns success. No next unsupported operation is added
artificially: this fixture intentionally attempts no submission.

The original helper validates the combined channel path:

1. Open channel and SetNvmapFd.
2. BindChannel to the initialized AS.
3. AllocGpfifoEx2 for the supported local profile.
4. AllocObjCtx for the historical libnx class 0xB197.
5. QueryEvent for GPU error event 3.
6. Enable error notifier.
7. Set channel priority and return success.
8. Original Close disables the notifier, closes its copied event handle and
   channel; repeated Close is harmless.

Before support, the same NRO exits after 69,802 instructions with UnknownCommand
at the final priority call and original failure cleanup. After support it exits
after 70,276 with both helpers successful. Normal and seven-instruction slices
agree. The older low-level channel and notifier binaries remain unchanged and
now complete priority in 56,060 and 62,990 instructions respectively.

NRO SHA-256:
`10FC45C5AD553B0CCDA844A3F4DFA325A4FB994C0E5D2140FBCC1411F60334FC`.

An independent host fault-injection run replaces only the first transmitted
priority value with zero. The unchanged helper returns InvalidArguments,
disables its notifier, closes its event/channel and allows the next full
lifetime to succeed. This checks the SDK's actual error-unwind path; the NRO
itself is never modified.

Tests also cover all three priorities, unsupported values, typed endpoints,
immutable input, malformed extents/receive descriptors, protected initial and
update replies, state rollback, clone sharing, independent channels, failed
and successful Close, slot reuse, failed reload and exit/fault/fresh load.

## Scope and next work

This validates the **channel-initialization helper**, not complete GPU startup.
The fixture does not call nvGpuInit or validate control-GPU characteristics,
physical GPU allocation, hardware scheduling, command submission, method or
shader execution, host GPU rendering or native Switch 2 compatibility. The
historical class 0xB197 remains a reference profile, not a native Switch 2 class.

Next bounded milestone: audit and implement the nvIoctl2 submission transport
with explicit entry-buffer bounds, ownership and transactional failure rules.
Accepting transport alone must not be reported as executing commands.

## Report layout

gpu_as_report contains two 32-word blocks: 0 NV initialize, 1 AS Create, 2 AS FD,
3 Nvmap initialize, 4 Nvmap FD, 5 nvGpuChannelCreate result, 6 base initialized,
7 base FD, 8 fence ID, 9 fence value, 10 object identity, 11 event-active,
12 event read handle, 13 fence increment, 14 client entry count, 15 event poll,
16 base initialized after Close, 17 base FD after Close, 18 event-active after
Close, 19 event read handle after Close, 20 initialized after repeated Close,
21 AS FD after Close, 22 AS initialized flag, 23 Nvmap FD after Exit,
24 NV session after Exit, 25 requested priority; words 26 through 31 are reserved.
