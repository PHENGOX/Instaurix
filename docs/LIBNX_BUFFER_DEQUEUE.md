# CPU-only Dequeue, RequestBuffer and Cancel

Follow-up: [immediate CPU QueueBuffer presentation](LIBNX_CPU_PRESENTATION.md)
now implements the bounded empty-fence queue path. QueueBuffer being unsupported
below records the earlier acquisition/cancellation milestone.

The bounded 16x16 linear RGBA profile now implements Binder RequestBuffer (1),
DequeueBuffer (3) and CancelBuffer (8). Each registered slot is either free or
acquired. Dequeue selects the lowest free slot, RequestBuffer requires an
acquired slot, and Cancel returns it to the free set. Duplicate cancellation,
unregistered slots and unsupported geometry or nonempty fences fail explicitly.
An acquired slot cannot be unregistered, and Disconnect requires cancellation
first. Existing Nvmap backing-lifetime protections still apply.

The service-owned availability event now reflects whether the connected layer
has a free registered slot. Reply publication precedes ownership and event
changes. Registration and Cancel can wake waiters; acquisition of the last
free slot clears availability. A raw Dequeue with no free slot returns native
WouldBlock (-11) in an otherwise successful Binder reply without changing state.
This is a local immediate CPU-only policy, not a complete native queue model.

Dequeue returns a 72-byte parcel containing the slot and a complete empty
36-byte NvMultiFence. No GPU work exists, so no syncpoint completion is invented.
RequestBuffer returns a 396-byte parcel containing the same 364-byte flattened
descriptor accepted at registration. Tests compare every byte against the
original GCC producer input. Cancel returns an empty 16-byte parcel and
accepts only an empty fence.

The separate libnx_dequeue harness uses unchanged original nwindowDequeueBuffer
and nwindowCancelBuffer functions. It acquires/cancels slot zero twice, requests
its metadata once through the SDK cache, and verifies availability before and
after ownership changes. The SDK nwindow path passes a null output pointer to
bqRequestBuffer; libnx's unfinished non-null-output branch is not exercised.
Original fence sources are linked for the wrapper's conditional wait branch;
nonempty hardware fences are not exercised or supported.

The new fixture executes 134,129 instructions and produces no frame. Earlier
fixture binaries remain unchanged. The registration-only fixture now measures
121,614 instructions because its availability wait succeeds immediately.

Host replays verify malformed access packets, publication failure, exhaustion,
descriptor identity, cached requests, duplicate Cancel, owned-slot removal and
Disconnect rejection. QueueBuffer (7) remains an explicit unsupported command.
Next, define QueueBuffer ownership and presentation semantics before accepting
submission; GPU address spaces, shaders and native Switch 2 compatibility remain
unvalidated.

## Validation

All 83 CTest suites pass on Windows in Debug (59.20 s) and Release (13.57 s).
The core and executable targets were rebuilt for both configurations. Normal
and seven-instruction guest slicing produce the same cycle and instruction
count. Strict reconstruction reproduces the 495,616-byte fixture NRO SHA-256:
9351F0782466C21274666407DE8D415E0DE50EEE3414C10C6C18D5F3C076EC8D.
All 19 recorded NRO hashes match their manifests. The earlier 18 NRO binaries
remain unchanged; only the registration-only runtime measurements are updated.
The English README variants match. Linux execution was not performed.
