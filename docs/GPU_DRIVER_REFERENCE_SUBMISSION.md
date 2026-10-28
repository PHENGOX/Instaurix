# Synchronous reference submissions through the NV channel

Follow-up: [four-byte semaphore releases](GPU_MEMORY_RELEASE.md) now extend this profile with transactional mapped-memory stores. Reference-only restrictions and validation counts below record the earlier milestone.

The explicit `--hle-reference --homebrew-entry` profile now connects the bounded FIFO reference interpreter to the historical `KickoffPb` ioctl2 path. The accepted work is limited to subchannel-zero SET_REFERENCE and DMA_NOP. This is a local synchronous HLE policy, not a validated native Switch 2 GPU implementation.

## Request and reply contract

Existing preflight still requires a live initialized typed channel, its Nvmap association, a bound address space, FIFO and graphics context, owned readable mappings, at most 2,048 descriptors and 64 KiB of commands. Only flags `4` and zero input fence fields are accepted. Waits, increment requests, sync controls and other packet/method profiles remain explicit errors.

The pinned original libnx [gpu_channel.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/nvidia/gpu_channel.c) sends flags `4` when `fence_incr` is zero. Its raw Kickoff helper passes a temporary zero-ID fence. Original [nvchannel.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/nvidia/ioctl/nvchannel.c) supplies the 24-byte in/out structure and extra descriptor buffer. These sources establish the observed client ordering and layout, not the complete native driver policy.

The local interpreter decodes the owned snapshot and stages all reference writes. The driver then copies the selected channel's FIFO state and increments a host-only submission count. Its ioctl output copies the input structure, including the unchanged zero fence. The IPC reply and ioctl output are written together through `Memory::writeRanges`. Only after both writes succeed is the channel's staged state published. No allocating operation follows that publication.

There is no queued or asynchronous work. Command snapshots and temporary state are released before returning to guest execution. The host diagnostic count means a synchronous supported stream was committed; it is not a syncpoint or GPU completion fence. SET_REFERENCE does not increment the allocation fence, and the error event is not signaled for successful work or ordinary refused requests. No invented guest query is added.

## Ownership and errors

State belongs to the channel's FIFO, not a process-wide register. Cloned NV sessions observe the same channel state; independent channels remain independent. Channel close resets its FIFO. New/reused slots and successful reloads start fresh. Failed reloads preserve the live process. Exit and fault retire NV channel state through the existing cleanup path.

Malformed packets return InvalidArguments; unsupported methods/subchannels/control forms return UnknownCommand. Mapping ownership, bounds and permissions retain their preflight errors. Exhausted submission counters return OutOfResource. Any rejected request or failed reply publication leaves the prior register and count unchanged. Completed work retains no mapping, so original Unmap/Free cleanup is immediately valid.

## Original fixture and regression coverage

The existing NRO is unchanged, SHA-256:
`69EF1BD8220547A996EBDDF497AB57060E87DE57BF69F3E13A0867F7A8B00D3B`.

Its two original Kickoff calls now succeed and libnx clears `num_entries`. The original client keeps its allocated fence IDs and zero values, closes both channels, unmaps/frees command backing, and exits without rendering. The uninstrumented run takes 77,756 guest instructions, compared with 77,630 at the earlier decoder-only boundary. This difference reflects taking the success path, not a performance measurement. Seven-instruction slices give the same result.

Integration tests inspect the actual committed reference and count, changed command payloads, root/clone behavior, simultaneous independent channels, closed descriptors and reused slots, late unsupported/malformed streams, unsupported synchronization flags, protected IPC/ioctl outputs, failed reload, successful live reload, exit and fault.

`gpu_submission_allocation_rollback` injects successive production allocation failures through command capture, decode, staged channel copy, output construction and both reply tracking rows. Each failure preserves the channel state and both guest reply ranges; the first complete attempt commits exactly once. In MSVC iterator-debug builds, allocations matching the internal container-proxy size are exempt: those diagnostic proxies can allocate in noexcept constructors. This bounds the injection claim; it does not promise recovery from every possible C++ runtime allocation failure. The initial overly broad injection terminated the test process at approximately 13 MiB working set; automatic diagnostics captured it. That test setup issue does not establish the cause of the older legacy failure.

## Next GPU milestone

Establish the encoding and semantics of a small memory-writing GPU operation from primary evidence, then validate its guest-visible bytes and its reply transaction. Synchronization increments/waits require a separate explicit policy and tests. Shaders, a Vulkan backend, drawing and native Switch 2 compatibility remain unimplemented.

## Validation

Windows: **111/111 CTest suites passed in Debug (50.54 seconds) and Release (12.42 seconds)**. The allocation suite exercises nine failed production allocation boundaries per channel lifetime, including both reply tracking allocations, before the first successful commit. All 37 recorded NRO SHA-256 values match. No guest binary or SDK source was changed or rebuilt. Linux was not executed for this milestone.
