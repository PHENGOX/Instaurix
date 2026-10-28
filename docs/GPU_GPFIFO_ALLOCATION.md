# Bounded reference GPFIFO allocation

Follow-up: [historical graphics-context allocation](GPU_GRAPHICS_CONTEXT.md) now implements the observed AllocObjCtx profile. The GPU error-event query is the next unsupported boundary; earlier context-allocation statements below describe the previous milestone.

The pinned libnx revision is `b156b4774fb2df7b93402169509266da7547fc4c`.
Its unchanged `nvioctlChannel_AllocGpfifoEx2` sends ioctl `0xC020481A` with a
32-byte input/output structure: entry count at byte 0, flags at 4, unknown input
at 8, output fence ID/value at 12/16, and three unknown inputs at 20/24/28.
`nvGpuChannelCreate` requests 0x800 entries, flags 1 and all unknown inputs zero,
after BindChannel and before AllocObjCtx. This observed client profile defines
the supported subset; it is not a hardware capacity or ABI claim for Switch 2.

## Local allocation model

A channel needs both its Nvmap and initialized address-space associations.
The accepted request has exactly 2,048 entries, flags 1 and zero unknown inputs.
Other input combinations return InvalidArguments. Output fence words are not
validated as inputs. A second allocation on the channel returns AlreadyInitialized.
Allocation on other device types remains UnknownCommand.

Each successful allocation owns zero-initialized host storage for 2,048 64-bit
entry slots (16 KiB). At most eight channels per NV service can own a queue;
root and cloned endpoints share that quota. This limits queue storage to 128 KiB
per service. No guest memory, GPU VA reservation or physical GPU buffer is
implicitly created. The slots cannot yet be submitted to or consumed.

The returned fence has a process-monotonic, nonzero **local allocation identity**
and value zero. It is not a native syncpoint, kernel handle, completion event or
evidence of executed work. No wait/signal behavior is added. Identities are not
recycled on Close and allocation refuses identity exhaustion. Fresh process
load resets the identity namespace.

Queue storage, quota and fence identity are published only after the IPC reply
and output buffer write succeed together. Failures leave all output bytes and
driver state unchanged. A successful channel Close drops the storage and frees
quota; a failed reply cannot drop it. Existing channel retention guards keep
the AS and Nvmap descriptors alive. Exit/fault releases all service state;
failed reload preserves live allocations.

## Original reference and tests

Windows validation: **97/97 CTest suites pass in Debug (65.63 seconds) and
Release (16.80 seconds)**. All 30 recorded NRO hashes match, and the previous
29 binaries are unchanged. Linux was not run for this milestone.

`libnx_gpu_fifo` runs two original NV/AS/Nvmap/channel/binding lifetimes. The
unchanged AllocGpfifoEx2 call is followed by original AllocObjCtx requesting the
historical libnx 3D class 0xB197, flags zero. AllocObjCtx remains UnknownCommand
and publishes no object. That historical class does not establish the Switch 2
GPU class or shader ABI.

The same 401,408-byte NRO first exits after 58,734 instructions with allocation
unsupported. After support it exits after 59,610, with successful allocation and
the explicit graphics-object boundary. Normal and seven-instruction slices
agree. A strict second SDK build reproduces the recorded NRO SHA-256.

NRO SHA-256:
`229C516F51CB0D4091A1893C491CCE3E1A7B8B85DF2AA68AF7B057D670DA325B`.

The host integration suite covers prerequisite associations, device types,
every supported input field, duplicate allocation, receive extent and write
permissions, protected reply rollback, unchanged non-output bytes, monotonic
identities, eight simultaneous queues, quota retention after failed Close,
successful cleanup, root/clone sharing, failed reload and exit/fault/fresh load.
The earlier BindChannel fixture is unchanged; it now completes its final
AllocGpfifoEx2 call (58,926 instructions), and its assertions follow that progress.

## Next bounded milestone

Audit and implement the required graphics-object context lifecycle, keeping the
historical reference class distinct from any validated native Switch 2 class.
Command submission, queue consumption, synchronization, method decoding,
shaders and host GPU rendering remain separate work. No game compatibility or
native Switch 2 execution is demonstrated by queue allocation.
