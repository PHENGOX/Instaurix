# Bounded Maxwell pitched 2D copy

Follow-up: [one-GOB-height block-linear source conversion](GPU_MAXWELL_BLOCK_SOURCE_COPY.md)
adds a restricted source layout. Earlier block-linear refusal statements below
describe the pitched-only milestone.

Follow-up: a [guest-generated 2D producer](LIBNX_GUEST_GPU_COPY_2D.md) now generates
and verifies two rectangular workloads through unchanged libnx functions without
normal-run host injection. Statements below about the next producer proof record
the earlier interpreter milestone.

The B0B5 handler now admits one pitch-to-pitch rectangle per submission,
extending the [linear copy policy](GPU_MAXWELL_LINEAR_COPY.md). Execution remains
CPU-interpreted synchronous HLE. This does not demonstrate drawing, pixel
conversion, block-linear DMA layouts or a native Switch 2 interface.

## Primary evidence

The pinned [NVIDIA B0B5 header](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/dma-copy/clb0b5.h)
defines PITCH_IN (0x410), PITCH_OUT (0x414), LINE_LENGTH_IN (0x418), LINE_COUNT
(0x41C) and MULTI_LINE_ENABLE (launch bit 9). The historical
[deko3d producer](https://raw.githubusercontent.com/devkitPro/deko3d/master/source/maxwell/gpu_transfer.cpp),
`BlitCopyEngine`, programs separate pitches for linear surfaces and passes
byte width and height to LineLengthIn/LineCount. The producer URL is mutable
reference evidence; the NVIDIA revision is pinned. No deko3d code is incorporated.
Existing NVIDIA notices are retained with the GPU fixtures.

The admitted launch is exactly 0x386: the earlier 0x186 plus bit 9. All eight
address/geometry registers must be initialized. Addresses remain 40-bit virtual
addresses; width and pitch are bytes, count is rows. No pixel format is inferred.
Single-line 0x186 still needs only its original five registers and ignores saved
pitch/count values. Both modes capture geometry at launch, retain registers
after success and clear the bank on rebinding.

## Local policy and publication

- Positive width and height; each pitch must be at least the row width.
- At most 64 rows and 64 KiB of actual data per submission.
- Each envelope, `(rows - 1) * pitch + width`, is at most 64 KiB and fits the
  40-bit address space. These are local limits, not hardware limits.
- Entire envelopes must be disjoint in GPU VA and resolved CPU backing. This
  conservatively also refuses interleaved padding that would have disjoint row
  bytes.
- Each envelope, including gaps, must fit one live mapping owned by the channel
  and pass RAM/bus permissions: source read, destination write. Inaccessible
  padding or mapping holes are refused although padding is not transferred.
- Neither envelope may overlap IPC or ioctl replies. Multiple copies, copies
  mixed with semaphore stores, remap, physical addresses and other launches
  remain explicit errors. Ampere research classes remain non-executable.

The handler prepares geometry without accessing RAM. The driver validates both
envelopes and captures source rows into one packed vector. It prepares one
destination WriteRange per row, alongside the two replies. The fixed transaction
array accommodates all 64 rows. Memory::writeRanges validates and preallocates
every destination/reply page and tracking row before changing bytes or versions.
Channel geometry, bindings, reference state and submission count publish only
after that transaction succeeds.

A bad last row or late allocation failure therefore cannot leave earlier rows
modified. Padding bytes are not rewritten and untouched reservation granules
retain their versions. No asynchronous job or invented completion event is added.

## Validation boundary

The existing copy-plan suite checks separate pitches, count persistence,
single-line reuse after multiline setup, immutable captured geometry, missing
setup, zero dimensions, short pitches, arithmetic/address limits, exact resource
limits, overlap and full output-plan rollback.

The mapped transport suite injects synthetic 2D words at the unchanged original
libnx ioctl2 boundary. It checks every row and padding byte with distinct strides,
including an exact 64-row transaction alongside both replies,
padding reservation preservation, inaccessible final rows, a final row beyond
the mapping, protected replies and a reply collision in a later row. A separate
failure sweep spans three cold destination pages, checking all row bytes and
versions, channel geometry, submission count and both replies before eventual
exactly-once success. Earlier single-line allocation coverage is retained.
MSVC Debug retains the existing iterator-proxy-size exemption; Release does not.

The [guest-generated producer](LIBNX_GUEST_GPU_COPY.md) remains single-line,
with its unchanged NRO and instruction count. This milestone does not claim an
untouched guest-generated 2D producer. That is the next bounded proof before
image-layout operations and drawing. Linux has not been executed here.

Windows validation: **121/121 CTest suites passed in Debug (107.56 seconds) and
Release (27.44 seconds)**. The existing suites were extended rather than adding
duplicate CTest registrations. All 38 recorded NRO hashes match; all 38 older
NRO files in the previous snapshot remain unchanged as well. The unchanged
single-line guest producer still completes in 85,571 ARM64 instructions.
The isolated legacy test passed with a Debug peak working set of 103.64 MiB.
