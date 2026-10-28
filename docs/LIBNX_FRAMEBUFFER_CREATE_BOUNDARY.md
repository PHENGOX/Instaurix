# Original framebufferCreate success

Follow-up: [original framebuffer presentation](LIBNX_FRAMEBUFFER_PRESENTATION.md) now adds the bounded pixel-conversion path. The creation-only measurements below remain unchanged.

The unchanged original libnx framebufferCreate now succeeds for the bounded
16x16 RGBA, two-buffer reference. It allocates one 16 KiB Nvmap object and
registers two 8 KiB block-linear surfaces at offsets 0 and 8192. Original
framebufferClose disconnects the queue, releases the map and backing, and
leaves the later window/event-quota probes intact. No guest NRO was changed.

## Supported descriptor profile

Registration accepts the previous 4 KiB linear profile and this additional
profile: one 16x16 RGBA plane, 64-byte pitch, layout 3, kind 0xfe,
block-height log2 4, 8192-byte surface extent and offset 0 or 8192 in an
allocated, writable 16384-byte Nvmap object. Other planes, padding and unused
fields must be zero. This is a bounded local reference policy, not general
Horizon or native Switch 2 compatibility.

Each slot retains an immutable heap-allocated copy of the complete original
364-byte flattened descriptor. RequestBuffer returns that copy, including the
surface offset. Staged layer copies share immutable records; failed reply
writes cannot publish registrations. Removal, disconnect, exit and reload use
the existing slot/map lifetime rules. Descriptor storage is allocated only
for occupied slots, avoiding large inline arrays in every layer.

Block-linear QueueBuffer remains explicitly rejected with InvalidArguments,
without publishing a frame or releasing the acquired slot. Pixel conversion
and original framebufferBegin/End execution are the next milestone. Creation
alone does not execute GPU commands or produce an image. The previously
verified linear CPU presentation remains available.

## Progress with the same guest binary

| Measurement | Before alignment fix | Allocation only | Current |
| --- | --- | --- | --- |
| Guest instructions | 117,230 | 121,329 | 125,136 |
| Nvmap Alloc result | InvalidArguments | Success | Success |
| Registration requests | 0 | 1 | 2 |
| framebufferCreate result | InvalidArguments | InvalidArguments | Success |
| Configured slot mask before close | 0 | 0 | 3 |
| Frames produced | 0 | 0 | 0 |

The CPU pointer remains page-aligned independently of the retained 128 KiB
driver alignment request. This does not allocate GPU addresses or establish
physical hardware alignment guarantees.

## Validation

The focused reference runs normally and in seven-instruction slices, with the
same instruction count. It checks both actual registration requests, separate
offsets, original close, the unchanged 107-handle event quota, malformed
offset/kind/block-height/size/padding rejection, protected IPC reply rollback,
and byte-for-byte RequestBuffer descriptor preservation for both surfaces.
The RequestBuffer preservation checks use captured original parcels against
the host helper; original framebufferBegin is not executed by this fixture.

The unchanged 487424-byte NRO SHA-256 is
A34D33832FB1743FC5B64636D027DCE81294A7589EA83894DA4C3BA833F933B8.
All 85 CTest suites pass on Windows in Debug (58.11 s) and Release (12.54 s). All 21 recorded NRO hashes match; README variants match. Linux execution is not claimed. See the build/blocklinear-ctest-*.log files
for the full Windows regression results of this milestone.
