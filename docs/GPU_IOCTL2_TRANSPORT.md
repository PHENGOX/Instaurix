# Original nvIoctl2 transport validation

Follow-up: [GPFIFO memory preflight](GPU_SUBMISSION_PREFLIGHT.md) now validates channel-owned mapped command ranges and creates temporary owned byte copies. Valid opaque input is still refused; the earlier transport-only outcome below is historical.

The pinned libnx revision is `b156b4774fb2df7b93402169509266da7547fc4c`.
Original nvIoctl2 sends NV service command 11 with two AutoSelect input buffers
and one AutoSelect output buffer. The inputs are the ioctl parameter structure
and a separate extra array; the output is the parameter receive range. The
unchanged nvioctlChannel_KickoffPb wrapper uses request 0xC018481B, a 24-byte
parameter structure, and an extra array of eight-byte GPFIFO descriptors.

## Supported packet profile

The actual SDK packet has HIPC header 0x01220004 / 0xC0B. Two null X descriptors
are at bytes 8 and 16, with indices zero and one. The main A descriptor is at 24,
extra A at 36, B receive descriptor at 48, CMIF at 64, command 11 at 72, FD at
80 and ioctl request at 84. The output pointer-size entry at 100 is zero and
the C descriptor at 104 is null. Non-null pointer-buffer layouts remain outside
this reference subset. Existing command-1 legacy/AutoSelect packets keep their
separate layouts and command IDs.

Ipc::NvIoctl now records command, extraSource and extraSize in addition to its
main source/destination/size. The plan is published only after all validation
succeeds. Any failure leaves every caller-provided plan field unchanged.

## Memory and dispatch rules

- The encoded ioctl size must match its main descriptors. Only write-only and
  read/write directions are supported, matching the existing command-1 subset.
  Write-only requires an entirely null B descriptor.
- Inputs must be readable RAM and the receive range writable RAM. Every page
  is checked. Buffer modes and unsupported message fields fail explicitly.
- Extra input is bounded by the 64 KiB transport limit. This is not the
  channel's queue-capacity limit. Empty extra input does not dereference its
  ignored address, including a non-null address transmitted with size zero.
- No buffer may overlap the IPC reply. Extra input cannot overlap the writable
  receive range. The main source/output may alias exactly, but not partially.
  Two input ranges may overlap when the receive range is separate.
- Driver initialization and FD ownership still apply. All command-11 methods
  currently return CMIF UnknownCommand after transport validation. They cannot
  fall through to command-1 setters or allocation methods.

This publishes validated ranges, not an immutable job snapshot. No descriptors
are queued, no GPU VA is dereferenced, no fence advances, and no submitted work
is acknowledged. Future asynchronous admission must snapshot input and retain
its backing explicitly. Method-specific count/flags validation is also future
work; generic transport does not interpret the parameter structure as commands.

## Original reference and version selection

Windows validation: **103/103 CTest suites pass in Debug (61.98 seconds) and
Release (15.41 seconds)**. All 35 recorded NRO hashes match; the preceding 34
binaries are unchanged. Linux was not run for this milestone.

libnx_gpu_ioctl2 calls original nvGpuChannelCreate, original AppendEntry and
original Kickoff. The default runtime's reported client HOS version is unknown
(zero), selecting the SDK's older SubmitGpfifo path. To exercise ioctl2, this
project-owned harness saves the version, temporarily calls hosversionSet with
4.0.0 around Kickoff only, then restores the saved value before cleanup. SDK
bodies and the general emulator profile remain unchanged. This override selects
a historical client branch; it is not a firmware or native Switch 2 claim.

AppendEntry serializes one deliberately synthetic, unmapped GPU VA
0x100000000 with command count one, producing descriptor 0x40100000000.
There is no executable command stream at that address. The original KickoffPb
packet carries one eight-byte entry, flags 4 and local fence input zero.

The same 417,792-byte NRO first exits after 71,180 instructions with kernel
UnsupportedFormat (0x5FF) at ioctl2. It now exits after 71,232 with a valid IPC
reply containing UnknownCommand (0x7FF). Both are failures to submit work; only
transport recognition has advanced. Original client entry count stays one,
descriptor and parameter/fence bytes are unchanged, and cleanup still succeeds.
Normal and seven-instruction slices agree. A strict second SDK build reproduces
the recorded NRO hash. Counts describe the observed path, not GPU performance.

NRO SHA-256:
`756F06EA210754F8A705CD39AB2579C98E0DA2C171EEDE4D1B1636DAE129B9DA`.

The parser suite covers actual layout fields, invalid directions, sizes/modes,
high address/size bits, RAM boundaries, all-page permissions, aliasing, empty and
maximum extra buffers, plan rollback and command-1 regression. Integration tests
capture the original SDK packet, verify root/clone handling, invalid FDs,
protected and overlapping memory, command isolation from notifier/priority
setters, failed replies, unchanged input/fence data and original cleanup.

The report retains the original channel-init words 0 through 25. Words 26
through 31 add AppendEntry result, descriptor, entry count before Kickoff,
Kickoff result, entry count after failure and fence increment after failure.

## Next bounded milestone

Validate KickoffPb-specific counts, flags and descriptor ranges against channel
state, then design an owned submission snapshot and lifetime rules. Execution,
GPU methods, shader translation, host rendering and native Switch 2 compatibility
remain unimplemented or unvalidated. A transport-level success must never be
reported as command execution.
