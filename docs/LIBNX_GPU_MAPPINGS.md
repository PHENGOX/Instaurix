# GPU virtual mappings to Nvmap backing

Follow-up: [synchronous GPU memory access](GPU_MEMORY_ACCESS.md) now consumes these live mappings for bounded host reads and writes. Earlier statements about absent memory consumers describe the mapping-only milestone.

The original libnx client now maps CPU-backed Nvmap objects into the bounded
GPU virtual address registry. Automatic aliases, fixed mappings, a second-page
subrange and unmapping work through unchanged original SDK wrappers. The
registry translates a covered GPU byte range to an object ID and backing
offset. No GPU command consumer or physical GPU allocation is implemented.

## Bounded map profile

MapBufferEx is request 0xC0284106 with 40 bytes: flags, kind, Nvmap handle,
page size, buffer offset, mapping size and input/output GPU address. Supported
flags are 0/4 for automatic and 1/5 for fixed, with bit 2 treated as a cache
hint in the existing coherent local model. Only pitch kind 0 and explicit
64 KiB pages are supported. Modify/sparse modes, other kinds and default-page
selection remain unsupported. The object must belong to the same NV driver
service, be allocated with writable CPU backing, and remain live.

Offsets and extents must be page-aligned and fit the object. Size zero selects
the remainder after the buffer offset. The selected backing range must be
readable/writable CPU RAM; unrelated object pages need not be accessible.
Automatic mapping requires input address zero and allocates an owned virtual
reservation. Fixed mapping requires an exact existing reservation with the
same base and size; this profile does not split larger reservations. Overlap,
unknown handles and invalid bounds fail without publishing state or addresses.

UnmapBuffer is request 0xC0084105 with an eight-byte base address. It removes
one exact mapping. An automatic mapping also releases its owned reservation;
a fixed mapping leaves the explicit reservation for a later FreeSpace call.
FreeSpace rejects mapped reservations. Multiple GPU addresses may reference
the same Nvmap object, and removing one alias does not release other references.
The existing local 4 GiB base, 64 MiB virtual region and 32-record quotas remain.

## Translation and backing lifetime

Mappings retain object identity, object-relative offset, byte extent and
reservation ownership in immutable published registry snapshots. Translation
accepts only a completely covered byte range, never wraps an address, and
returns the object ID/offset rather than a CPU pointer. Driver map validation
connects that identity to the owning service's bound Nvmap object. A future
GPU memory consumer must resolve through that owner and validate backing
access; no command fetch or GPU read/write is performed by this milestone.

The Nvmap lifetime guard now includes GPU mappings as well as framebuffer
slots. Nvmap Free and device Close reject mapped objects. One cloned kernel
NV endpoint may close while another retains the service; the last cannot
discard live mapped backing. Existing bound-memory heap/transfer guards remain.
Address-space descriptor Close cascades unmapped/mapped metadata teardown and
its reservations, allowing later Nvmap cleanup. Failed Close replies preserve
those references. This close policy assumes no channels/jobs exist; future
pending jobs require explicit ownership rules before enabling submission.

Map, Unmap, reservation state and reply bytes are staged together and committed
only after successful reply writes. Failed Map does not pin an object or
consume a virtual range. Failed Unmap cannot make a reservation freeable.
Exit/fault and fresh load drop the old service/association state; failed load
preserves it. GPU/CPU addresses remain separate types and namespaces.

## Original-client measurements

The same unchanged libnx_gpu_map NRO was run before and after support:

| Measurement | Before mapping support | Current |
| --- | --- | --- |
| Instructions | 60814 | 73754 |
| First original MapBufferEx | UnknownCommand | Success |
| Original MapBufferEx requests | 2 | 10 |
| Original UnmapBuffer requests | 0 | 10 |
| Frames | 0 | 0 |

The project harness performs two full address-space/Nvmap lifetimes using
128 KiB CPU backing per object. It checks automatic aliases, blocked Nvmap
Free, removing one alias, duplicate Unmap, fixed reservations blocked from
Free while mapped, explicit second-page subranges, and AS Close with one
live mapping. Three original CPU markers remain unchanged. The original
Nvmap close succeeds after AS teardown. Normal and seven-instruction slices
produce the same current reports and instruction count.

## Tests and reproduction

The standalone registry suite checks object/offset translation, boundary bytes,
rejected cross-boundary/zero/overflow reads, aliases, automatic versus fixed
ownership, mapped Free rejection, copy independence and mapping quotas.
Original-client host replays check request fields, backing permissions, quota,
root/tagged-clone sharing, protected Map/Unmap/AS Close replies, a separate
object freed after failed Map, backing-retention guards and source bytes.
Additional live-mapping probes exercise last-kernel-endpoint protection,
failed reload, exit, fault and fresh reload. These host replays are separate
from the original instruction measurements and do not execute GPU commands.

Rebuild the separate reference with tools/build_libnx_gpu_map.ps1. Unchanged
pinned libnx address_space.c, nvhost-as-gpu.c, map.c and nvmap.c implement the
client path; exact source/harness/symbol hashes are recorded in the fixture's
reference.json. No original SDK function body is patched.

Next, connect a bounded GPU memory-access path to these translations and
prepare the original channel/address-space binding lifecycle. Command formats,
shader execution, synchronization and native Switch 2 compatibility still
require separate evidence and implementation.

## Validation results

All 94 CTest suites pass on Windows in Debug (47.63 s) and Release (12.23 s).
Logs: build/gpu-map-ctest-debug.log and build/gpu-map-ctest-release.log.
Linux was not run locally. Strict reconstruction reproduces the 401408-byte
new NRO SHA-256:
673D2A4211CDA2F454894C00D0093F4C3590563C384F04FA16729F8D8BC7E3E6.
All 27 recorded NRO hashes match; the earlier 26 binaries are unchanged.
The new harness and symbol hashes match the manifest. Both English README
variants match. Original baseline/current measurements use the same guest
binary; host failure/teardown probes are separate from those counts.
