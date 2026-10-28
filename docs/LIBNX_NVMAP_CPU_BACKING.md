# Nvmap CPU backing

Alignment-policy update: [the original framebufferCreate reference](LIBNX_FRAMEBUFFER_CREATE_BOUNDARY.md)
now separates page-aligned CPU backing from a retained, bounded driver-alignment
request of up to 128 KiB. No GPU-address alignment is implemented by that change.

Follow-up: [the original nvMapCreate wrapper and GetId](LIBNX_NVMAP_EXPORT_ID.md)
now validate the next dependency identified by this milestone.

The local HLE reference profile now accepts original libnx Nvmap Alloc
(`0xC0200104`) for a previously created object on its owning device descriptor.
The request layout is taken from the pinned original
`nx/source/nvidia/ioctl/nvmap.c` already used by the fixture build.

This explicitly bounded policy accepts heapmask zero, linear kind zero, zero
padding, access flags 0 or 1 and a power-of-two alignment of at least 4 KiB.
The complete object must fit in the live process heap with the required RAM
permissions. An object can be bound once. Overlapping live object backings
are rejected across service instances. These restrictions are a local policy,
not validated native Switch 2 driver semantics.

Alloc records the guest address and flags without copying or changing bytes.
Its reply must be published before the object state is committed. Heap shrink
cannot discard live backing, and CreateTransferMemory cannot borrow it.
Free, device Close and last-service closure drop the binding; process teardown
also drops the NV service state. The CPU retains access to the backing.
The existing Free reply retains zero refcount and flags and the recorded size.

The unchanged original libnx Create/Alloc/Free client functions complete two
cycles in the existing project fixture, now executing 59,363 instructions.
Host replays exercise invalid object IDs, heapmask, flags, alignment, kind,
padding, heap bounds, protected replies, duplicate binding and transfer-memory
exclusion. The original CPU buffer contents remain unchanged.

No GPU address space, buffer registration, command submission or rendering is
implemented by this change. The pinned higher-level nvMapCreate wrapper calls
GetId after successful Alloc. Next, exercise that unchanged wrapper and capture
the GetId boundary before moving to buffer registration.

## Validation

All 79 CTest suites pass on Windows in Debug (157.55 s) and Release (31.53 s),
including the final backing-permission and transfer-memory exclusion checks.
All executable targets were rebuilt in both configurations. All 15 recorded
NRO hashes remain valid and the English README variants are identical.
The existing cycle fixture's NRO and original SDK source functions are unchanged;
its measured normal execution increases from 59,251 to 59,363 instructions.
Linux execution was not performed for this milestone.
