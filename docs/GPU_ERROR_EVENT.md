# Reference GPU error-event query

Follow-up: [GPU error-notifier configuration](GPU_ERROR_NOTIFIER.md) now implements the observed enable/disable profile. SetPriority is the next unsupported boundary; earlier notifier statements below describe the previous milestone.

The pinned libnx revision is `b156b4774fb2df7b93402169509266da7547fc4c`.
Original `nvQueryEvent` sends NV service command 4 with two input u32 values:
the device FD and event ID. It expects a copied kernel handle and a separate
u32 NvError. `nvGpuChannelCreate` queries GPU error-notifier event 3 after
AllocObjCtx, before SetErrorNotifier. The unchanged wrapper loads the result as
a remote, autoclear event with no writable endpoint.

## Bounded local policy

The service must be initialized and registered. The FD must identify a channel
owned by that service, and this reference profile requires its allocated
graphics context. Missing or wrong-type FDs return InvalidHandle; missing
context returns InvalidState. Only event ID 3 is supported; other IDs and
malformed payloads return InvalidArguments. These prerequisites and error codes
are local policy, not proof of the complete native driver contract.

Each channel lazily owns one initially unsignaled event. Repeated root or cloned
NV-session queries return distinct read-only handles referencing that event.
Successful replies contain one copied handle, CMIF success and zero NvError.
The channel reference and handle are published only after the reply write
succeeds. Failed replies consume no kernel identity or quota slot. Handles use
the existing process-wide 128-slot kernel handle table and normal CloseHandle.

Closing a copied handle leaves other copies and the channel reference intact.
Successful channel Close drops its owner reference; previously copied handles
remain valid and unsignaled until individually closed. A failed Close retains
the channel owner and permits further queries. Existing copies do not retain
channel/AS/Nvmap resources. Queries on a retired FD fail. Exit/fault drops GPU
event handle copies along with driver state; fresh load resets identities.

Zero-time waits time out. The event is driver-managed: guest SignalEvent is
rejected and ClearEvent/ResetSignal return InvalidState in this bounded profile.
No error delivery, automatic reset after a real GPU error or completion signal
is implemented. Future error delivery must define the signaling/reset policy
before enabling such behavior. There is no synthetic signal to bypass startup.

## Original reference and measurements

Windows validation: **99/99 CTest suites pass in Debug (71.55 seconds) and
Release (16.47 seconds)**. All 32 recorded NRO hashes match; the previous 31
binaries are unchanged. Linux was not run for this milestone.

The project-owned `libnx_gpu_event` harness runs two original channel lifetimes,
queries the event, attempts original SetErrorNotifier enable and cleanup-disable,
polls it through original eventWait with timeout zero, then closes it through
original eventClose before channel teardown. Both notifier calls still return
UnknownCommand. No commands execute and the higher-level nvGpuChannelCreate
helper is not yet validated through its full initialization sequence.

The same 409,600-byte NRO first exits after 60,144 instructions with QueryEvent
unsupported. It now exits after 62,002 instructions with the query successful,
notifier configuration unsupported and the poll timing out. Normal and
seven-instruction slices agree. A strict second SDK build reproduces its hash.
The preceding context reference binary remains unchanged and now completes its
QueryEvent and eventClose calls in 60,320 instructions.

NRO SHA-256:
`B38A30F0D6C2CE57D8906C4F6C8B370B9E9BD6D91A0CBA041FBCEC5C3B89F533`.

Host probes cover malformed requests, descriptor types, unsupported event IDs,
context prerequisites, copied-handle response shape, repeated root/clone queries,
independent closing, forbidden guest signaling/clearing, unsignaled waits,
protected replies, monotonic identities, quota exhaustion/refill, failed channel
Close, surviving copies after channel Close, failed reload and exit/fault/fresh
load. No test injects a GPU error or fake completion.

Report words 0 through 21 follow the graphics-context reference. Words 22 and
23 record notifier enable/disable, 24 the zero-time wait, 25 event-active state
after Close, and 26 its read handle after Close. Words 27 through 31 are reserved.

## Next bounded milestone

Implement the observed SetErrorNotifier enable/disable state and lifetime rules,
then record the next original initialization boundary. Priority, command
submission, queue consumption, synchronization, methods, shaders, host GPU
rendering and native Switch 2 compatibility remain separate work.
