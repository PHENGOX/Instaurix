# Bounded native buffer registration

Follow-up: [CPU-only Dequeue/RequestBuffer/Cancel](LIBNX_BUFFER_DEQUEUE.md)
now provides ownership transitions and free-slot availability. The unsignaled
availability policy and lack of dequeue described below record the earlier
registration-only milestone.

SetPreallocatedBuffer (Binder code 14) now registers or removes bounded slots
on an opened, strongly referenced and connected producer. Both legacy A/B and
AutoSelect transport are supported; the original SDK fixture uses legacy A/B.
This corrects the earlier boundary document's transport description.

The local profile accepts exactly the captured 16x16 linear RGBA descriptor:
stride 16 pixels, pitch 64 bytes, a single plane at offset zero, a 4 KiB native
size, and a live CPU-cacheable writable 4 KiB Nvmap object. The parcel header,
interface token and padding, flattened length, magic values, integer count,
format and every unused native word are checked. Other profiles fail explicitly.
The exported ID must belong to an active local NVIDIA service endpoint.

Each layer has 64 slots storing process-local Nvmap IDs. Duplicate insertion
fails; a null descriptor removes an occupied slot and an empty removal fails.
Reply publication precedes slot mutation. The success parcel is an empty
16-byte ParcelHeader. Disconnect atomically drops all slot references.

This reference uses rejection rather than native driver reference counting:
Free and device Close return InvalidState while a registered slot references
their object. IPC session Close is rejected while that service has registered
objects. Kernel CloseHandle may close one cloned endpoint but cannot remove
the final endpoint retaining registered backing. ReleaseBuffers/Disconnect
must precede the final Nvmap cleanup. Process fault and exit drop the slots
and services through normal teardown. These are local policies, not validated
Horizon or native Switch 2 ownership semantics.

The project-owned fixture harness now calls original nwindowReleaseBuffers
before original nvMapClose, then verifies a second Free fails. Its NRO changes;
the original SDK source bodies, full GCC producer compilation and other fixture
binaries are unchanged. The client registers slot 0, releases it and exits
normally in 121,616 instructions.

Host replays cover malformed dimensions, integer count, plane offset, pitch,
color and IDs; protected replies; readonly backing; all 64 slots; duplicates;
null removal; both transports; blocked Free/device/session closure; failed
Disconnect; last-endpoint retention; failed reload; process fault and fresh
reload. The original cleanup successfully frees the map after Disconnect.

Registration does not implement dequeue readiness: the availability event
remains unsignaled. No GPU address space, command submission or rendering is
added. Next, capture the original dequeue/request-buffer calls and define their
slot states and fence requirements before accepting them.

## Validation

All 82 CTest suites pass on Windows in Debug (56.64 s) and Release (13.33 s),
after rebuilding the core and executable targets in both configurations.
The registration fixture and original SDK client execute successfully under
both normal and seven-instruction slicing. Strict reconstruction reproduces
the updated NRO SHA-256:
65E26C0E03A7998D45A074B5652CE45DFBCC875D84F34E5A92BCD36F094F8700.
All 18 recorded NRO hashes match their manifests; libnx_register is the one
updated project harness and the other 17 recorded fixture binaries are unchanged.
English README variants match. Linux execution was not performed.
