# Control-device prerequisite for framebuffer initialization

Follow-up: [the unchanged original registration client](LIBNX_BUFFER_REGISTRATION_BOUNDARY.md)
now resolves the compiler limitation discussed below and captures Binder code 14.

The pinned original framebufferCreate implementation initializes NV, Nvmap and
nvFence before allocating backing or calling nwindowConfigureBuffer. The
original nvFence initializer opens /dev/nvhost-ctrl. This milestone implements
that descriptor prerequisite, not graphic-buffer registration.

The explicit reference profile accepts exact /dev/nvmap and /dev/nvhost-ctrl
paths. Both use the same service-scoped quota of 32 descriptors and the same
process-monotonic ID sequence. Each descriptor records its device type.
Existing cloned endpoints share that service state. Close clears the ID and
type; opening and closing publish state only after their replies succeed.
Nvmap ioctls sent to a control descriptor return an explicit unsupported error
without allocating a map object or modifying the caller's ioctl data.

The separate libnx_fence harness calls unchanged nvFenceInit/nvFenceExit twice
with independent NVIDIA service lifetimes. The descriptor resets to UINT32_MAX
after cleanup; NV communication memory is released through the existing service
cleanup. This prerequisite probe does not execute framebufferCreate itself.
Its normal execution is 54,115 instructions, with no rendered frame.

Host replays check mixed-device quota exhaustion, failed Open publication,
protected Close, stale descriptor rejection, cleanup and device-type isolation.
The original library source bodies are unchanged. Root libnx ISC notices and
the fixture manifest cover nvidia/fence.c and nvidia/ioctl/nvhost-ctrl.c.

No control ioctl, event registration, query-event operation, syncpoint wait or
fence signaling is implemented. A successfully initialized fence client does
not demonstrate functioning GPU synchronization.

Next, exercise the actual framebuffer/buffer-registration path. The pinned
bqSetPreallocatedBuffer function uses a GCC-only variable-length struct member;
the current Clang reference build deliberately excludes that function. A
compatible reference compiler or an explicitly labeled ABI comparison is
needed before claiming an unchanged original registration client runs.

## Validation

All 81 CTest suites pass on Windows in Debug (55.80 s) and Release (12.98 s).
All executable targets were rebuilt in both configurations. Strict second
reconstruction reproduces the fence fixture NRO SHA-256
8CB53950BF74A80239394C12DE93B6749BABFF260D1506387F2DFD788291811A.
All 17 recorded NRO hashes match; earlier fixture binaries remain unchanged.
The English README variants match. Linux execution was not performed for
this milestone.
