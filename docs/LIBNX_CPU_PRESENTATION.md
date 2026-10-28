# Immediate CPU-only QueueBuffer presentation

Follow-up: [original libnx framebuffer presentation](LIBNX_FRAMEBUFFER_PRESENTATION.md) adds the bounded block-linear software path; the linear profile below remains supported.

Follow-up: [the original framebufferCreate boundary](LIBNX_FRAMEBUFFER_CREATE_BOUNDARY.md)
now completes the SDK allocation and both block-linear registrations. That creation-only milestone did not yet present block-linear buffers.

The bounded native-window profile now accepts Binder QueueBuffer (7) for an
acquired registered slot. Its input is the original 84-byte BqBufferInput in a
192-byte parcel. Only the tested zero timestamp, automatic-timestamp flag,
crop, scaling, transforms, reserved fields and empty fence are accepted; the
swap interval must be 1. The profile remains 16x16 linear RGBA with 4 KiB
CPU-cacheable Nvmap backing. Other metadata/layouts fail explicitly.

Queue validates ownership and readable backing, copies the 256 RGBA pixels to
an immutable 16x16 RGB Frame, discards alpha and stages a 36-byte reply with
16x16 geometry, zero transform hint, zero pending buffers and successful status.
Reply writes must succeed before the slot is released, availability changes,
the snapshot is exposed or the frame counter advances. Failed and duplicate
queues preserve the existing snapshot, count and ownership.

This is an immediate local CPU consumer. Swap-interval timing, vsync, native
compositing, GPU queues, shaders and hardware fences are not emulated. A Frame
is exposed to the existing host presentation/export API; creating it does not
establish native Switch 2 GPU compatibility or prove an onscreen backend.

The separate libnx_present harness uses unchanged original nwindowDequeueBuffer
and nwindowQueueBuffer functions. It fills a gradient and its complement,
presents each once, overwrites the backing and releases its buffers normally.
Both images remain independent of subsequent writes and backing cleanup.
Normal execution is 143,267 instructions and produces exactly two frames.

Tests check every final RGB pixel, reply fields, malformed metadata, unreadable
backing, protected reply memory, no partial publication, duplicate queues,
legacy and AutoSelect transport, availability and SDK metadata caching.
Frame-stop/resume returns once per original queue without replaying its SVC.
The CLI test accepts an optional final PNG path after --present; normal CTest
runs do not write a PNG. The reference image is a 16x16 CPU test pattern.

Next, exercise the actual framebufferCreate path and identify its alignment,
block-linear surface and fence dependencies. Existing manual linear-buffer
support is not full framebufferCreate or commercial-game compatibility.

## Validation

All 84 CTest suites pass on Windows in Debug (58.08 s) and Release (13.41 s).
The core and executable targets were rebuilt in both configurations. Normal
and seven-instruction slicing produce the same original guest execution.
Strict reconstruction reproduces the 495,616-byte presentation NRO SHA-256:
B5FC6D6332CFB0859DB55470FE1C3313E2066017332D7AB05BC9FD7914FDA6C7.
All 20 recorded NRO hashes match; the earlier 19 fixture binaries are unchanged.
The optional PNG export was executed and visually inspected. This verifies an
image artifact, not a visible native-window or hardware GPU backend.
The English README variants match. Linux execution was not performed.
