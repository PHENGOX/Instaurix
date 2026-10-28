# Restricted block-linear source conversion

Follow-up: a separate [guest-generated conversion reference](LIBNX_GUEST_GPU_BLOCK_COPY.md)
now generates source GOB data, conversion commands and verification through
unchanged libnx functions without normal-run host injection. Earlier next-proof
statements below describe the interpreter-only milestone.

The B0B5 interpreter now accepts a first block-linear-source to pitched-destination
copy form. It converts raw image bytes through the existing owned-mapping and
atomic reply/memory path. Execution remains a CPU-side synchronous historical
Maxwell HLE reference. No rendering, shader execution or native Switch 2 ABI is
established, and this is not general block-linear surface support.

## Primary evidence

The pinned [NVIDIA B0B5 header](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/dma-copy/clb0b5.h)
defines the source layout launch bit, Fermi 8-row GOB selection, and the six
source geometry methods at 0x728 through 0x73C. The selected block-size value
0x1000 means one GOB in block width, height and depth with Fermi 8 GOB rows.
Existing NVIDIA notices are retained in the GPU fixture directories.

The unchanged pinned libnx source
`tools/reference-sources/libnx-b156b4774fb2df7b93402169509266da7547fc4c/nx/source/display/framebuffer.c`,
functions `_convertGobTo16Bx2` and `_convertToBlocklinear`, provides the
historical 16Bx2 sector order and GOB traversal reference. Its included ISC
notice is retained with the existing libnx fixtures. Tests construct physical
sectors in inverse order independently of the implementation's address helper.
The existing display path handles a different, fixed sixteen-GOB block height;
that setting is not reused for this one-GOB-height DMA profile.

The pinned local framebuffer source SHA-256 is
`13F829E0F5AA2015211575374AE94A25DA6B5C2A35AB4299703860D824296347`.

## Admitted form and explicit local limits

- Launch exactly 0x306: non-pipelined, flush, source block-linear, destination
  pitched, multiline, no remap/semaphore/physical-address modes.
- All six source geometry methods must be programmed. Source block size is
  exactly 0x1000, depth is 1, layer and origin are zero.
- The transfer covers the complete source image: source width equals row byte
  width and source height equals row count. Width is a positive multiple of
  64 bytes and height a positive multiple of 8 rows. No pixel format is inferred.
- Source GPU address is 512-byte aligned. Destination pitch is at least width.
  Source pitch is ignored in this mode; it need not be initialized.
- Existing local bounds apply: at most 64 rows, 64 KiB of data and 64 KiB for
  each validated envelope. Addresses and their complete extents fit 40 bits.
- The entire tiled source and pitched destination envelope each fit one live
  mapping owned by the channel, with the required RAM/bus permissions.
- Both envelopes are disjoint in GPU VA and CPU backing and avoid reply ranges.

Quarter-GOB blocks, Tesla 4-row GOBs, larger block heights/depths, partial images,
origins, layers, reverse pitched-to-block conversion, block-to-block copying,
remap and additional launch variants remain explicit errors. These restrictions
and resource bounds are local policy, not hardware limitations.

For a byte coordinate x,y, the one-GOB-height layout uses:

```
gob = (y / 8) * (widthBytes / 64) + x / 64
offset = gob * 512 + ((x % 64) / 32) * 256
       + ((y % 8) / 2) * 64 + ((x % 32) / 16) * 32
       + (y % 2) * 16 + x % 16
```

Full image dimensions ensure this is a permutation of exactly width times
height source bytes. The driver validates that full source extent, captures
converted rows in one packed owned buffer and uses the existing per-row atomic
write transaction. Destination row padding is never rewritten. Geometry is
captured at launch; later register changes cannot change the prepared copy.
Registers persist after success and clear on rebinding as before.

## Validation and boundary

The copy-plan suite checks required configuration, captured/persistent geometry,
alignment, exact limits, unsupported layouts/depth/layer/origin/partial images,
reverse-direction refusal and full plan rollback. An exhaustive 128-byte by
16-row address permutation and known sector/GOB boundary offsets are checked.

The mapped test builds a 128-byte by 16-row image using physical sector traversal,
crossing both horizontal and vertical GOB boundaries. It checks all 2,048 source
bytes, all converted destination bytes, row padding, adjacent guards, source
reservations and clone reuse. Invalid whole-source extents, reply collision in
the last row, unreadable source, an inaccessible final destination row and late
unknown controls must not partially write earlier rows.

The retained allocation-failure sweep now separately covers pitched rows and
conversion into eight cold destination pages. Every failure checks all row bytes
and reservation versions, geometry/submission state and both replies before
eventual exactly-once success. The existing MSVC Debug iterator-proxy-size
exemption remains; Release has no such exemption.

These conversion commands are synthetic host-injected test data at the original
libnx submission boundary. The unchanged [2D guest producer](LIBNX_GUEST_GPU_COPY_2D.md)
still uses pitched sources. A guest-generated block-linear conversion is the
next bounded proof; display integration and drawing remain separate work.

Windows validation: **124/124 CTest suites passed in Debug (112.79 seconds)
and Release (26.01 seconds)**. Existing suites were extended without duplicate
CTest registrations. All 39 recorded NRO hashes match their manifests. The
unchanged guest-generated pitched workload still executes 108,040 instructions.
The isolated legacy test passed at a Debug peak working set of 103.63 MiB.
Linux was not executed for this milestone. The conversion sweep reaches 33
allocation failures in MSVC Debug before complete success for each channel
lifetime; the retained pitched-row sweep reaches 22.
