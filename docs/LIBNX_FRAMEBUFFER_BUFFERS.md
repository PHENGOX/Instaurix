# One, two and three framebuffer buffers

The original libnx framebuffer reference now validates one, two and three
buffers in a single guest run. Each configuration renders twice through all
its slots using unchanged original Create/MakeLinear/Begin/End/Close functions.
The guest produces twelve immutable pixel-verified 16x16 frames in 340060
instructions, with the same count under seven-instruction slices.

## Bounded allocation and ownership

A block-linear Nvmap object may contain one, two or three complete surfaces.
Its size must be a nonzero integral multiple of the validated surface extent,
with count <=3. Plane offsets must be aligned to the extent, point to one of
those three possible positions and fit wholly within the actual object.
Two slots in a layer cannot alias the same block-linear object/offset. The
existing width/height/layout compatibility and readable backing checks remain.
The 16x16 linear profile keeps its previous behavior and lifetime rules.
With dimensions capped at 256 per axis the largest block-linear object is
768 KiB. No graphics memory addresses or GPU commands are allocated here.

## Explicit local fair reuse policy

The immediate CPU consumer no longer always reacquires slot zero for
block-linear buffers. Successful presentation advances a per-layer search
cursor after that slot. Dequeue scans registered unowned slots from the cursor,
wrapping at 64. Thus the tested configurations reuse 0; 0,1; and 0,1,2.
Dequeue never returns an owned slot. Cancel frees an acquired slot without
presenting or advancing the cursor. Exhaustion returns native WouldBlock.
Cursor, acquisition, replies and snapshot publication commit transactionally;
failed replies and duplicate Queue calls cannot advance reuse order.
Disconnect resets the cursor and releases descriptors. This is a local
headless CPU-consumer policy, not a claim of native BufferQueue scheduling,
vsync or Switch 2 semantics. Linear references retain lowest-slot selection.

## Original client lifetimes

The harness draws each frame's colors from x, y, frame number and buffer count.
All visible RGB bytes are checked at each Frame stop. It records original
cur_slot values, expected masks, completed frames and cleanup. Original Close
and repeated Close retire each map; the first snapshot survives all subsequent
writes and destruction. The later original 107-handle event quota remains intact.

Between configurations the project harness closes and recreates its NWindow
through original nwindowClose/nwindowCreateFromLayer. This follows the pinned
client lifecycle: nwindowReleaseBuffers resets format to zero, while
nwindowConfigureBuffer initializes format only from the ~0 sentinel set by
nwindowCreate. Reusing that disconnected instance directly would serialize
format zero. No SDK function is patched and unsupported format zero remains
rejected by the emulator. Each framebuffer/NWindow lifecycle is explicit.

A second host-instrumented run captures the actual three Nvmap allocations,
six registrations and twelve Queue requests. It tests registration past the
map end, duplicate extent aliases, protected reply rollback, acquiring all
slots, WouldBlock, cancelling the extra acquired slots and rejecting duplicate
Queue. These host probes do not create extra frames. The unchanged guest
binaries for the older 16x16 and 37x137 presentation references also succeed.

Fair reuse makes those original clients request the second slot's metadata:
their current instruction counts are 152895 and 378310 respectively, versus
150676 and 376091 before this policy. Their NRO hashes are unchanged; the
older docs preserve historical measurements and link to this update.

## Reproduction and next GPU milestone

Rebuild the separate libnx_framebuffer_buffers fixture with
 tools/build_libnx_framebuffer_buffers.ps1 and the pinned tools. Report fields
are recorded in its manifest: three 16-entry blocks, followed by reserved
entries. All SDK bodies and the complete original producer remain unchanged.
The host test is libnx_framebuffer_buffers_tests. GPU execution is not part
of these tests. See [the GPU submission audit](GPU_SUBMISSION_AUDIT.md) for
the source-backed address-space/channel path and its explicit native S2 gaps.

## Validation results

All 89 CTest suites pass on Windows in Debug (55.45 s) and Release (12.77 s).
Logs: build/framebuffer-buffers-ctest-debug.log and
build/framebuffer-buffers-ctest-release.log. Linux was not run locally.
Strict reconstruction reproduces the 499712-byte new NRO SHA-256:
6845557CAAA7B4F985AD6926DF91E2B5CDA2B9758CC5378F91CE86E4D2B7E7FF.
All 24 recorded NRO hashes match; the earlier 23 binaries are unchanged.
The new harness/symbol hashes and eight audited GPU source hashes match their
recorded values. Both English README variants match. GPU audit findings are
source inspection and an implementation plan, not executed GPU validation.
