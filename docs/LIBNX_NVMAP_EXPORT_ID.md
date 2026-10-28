# Original nvMapCreate wrapper and GetId

Follow-up: [the control-device prerequisite](LIBNX_NVHOST_CONTROL.md) covers
the nvFence initialization that framebufferCreate performs before allocation.

The reference profile implements `GetId` (`0xC008010E`): the original libnx
eight-byte request contains an output ID at offset 0 and an input object handle
at offset 4. The same registered NVIDIA service and owning live device
descriptor checks apply as for Alloc and Free.

The local policy requires a bound object and returns its process-monotonic
object ID as a stable export ID. This deliberately limited identity mapping
is not evidence of native Switch 2 driver semantics. It does not implement
remote import, FromId or shared-object reference counting. An unbound object
returns InvalidState; a missing, wrong-device or retired object returns
InvalidHandle. Repeated queries and cloned endpoints return the same ID.
Errors do not publish output, and reply/output writes use the existing atomic
publication path. Querying an ID does not consume another object ID.

The separate `libnx_nvmap_wrapper` project harness calls the unchanged original
`nvMapCreate` and `nvMapClose` functions twice, in independent service lifetimes.
This exercises Create, Alloc, GetId and Free through the SDK wrapper, with
CPU-cacheable linear backing. It checks stable IDs, unchanged CPU bytes, cleared
wrapper state and rejection of a repeated Free. The original cache assembly
is linked because the wrapper references it, but this fixture does not execute
the non-cacheable cache-maintenance branch.

The lower-level cycle fixture remains unchanged. Neither fixture registers a
graphic buffer, maps a GPU address, submits GPU commands or renders a frame.
Next, choose an original buffer-registration client and record the first
unsupported dependency; remote Nvmap import and Param may also be required
depending on that client's actual calls.

## Validation

All 80 CTest suites pass on Windows in Debug (48.00 s) and Release (13.27 s).
All executable targets were relinked against the updated core in both configurations.
Strict reconstruction reproduces the new wrapper NRO SHA-256
CDC1CEE89DC9913B01C602C808F55DD67D620AE8C2275905AF94A5067E924FD7.
The fixture executes 60,250 normal guest instructions. All 16 recorded NRO
hashes match their manifests; the earlier fixture binaries remain unchanged.
The two English README variants match. Linux execution was not performed
for this milestone.
