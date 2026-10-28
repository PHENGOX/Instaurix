# Guest-generated block-linear source conversion

The project-owned ARM64 harness in `tests/programs/libnx_gpu_copy_block`
constructs physical block-linear source data, programs the B0B5 source layout,
submits conversion through unchanged pinned libnx functions and verifies the
result inside the guest. The normal runs and their instruction observer never
write guest state. No production emulator behavior changes for this milestone.

This exercises the [restricted block-source policy](GPU_MAXWELL_BLOCK_SOURCE_COPY.md)
in Instaurix's synchronous historical Maxwell HLE interpreter. It does not
establish native Switch 2 compatibility, rendering or hardware completion/cache
semantics. Output is read after Kickoff under the explicit local synchronous
policy; native hardware would require validated completion and cache handling.

## Workload

Each case uses an independently initialized and closed channel and mapping:

| Case | Width (bytes) | Rows | Source GOBs | Source bytes | Destination pitch | Destination bytes checked |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 128 | 16 | 2 across, 2 down | 2048 | 144 | 2304 |
| 1 | 64 | 16 | 1 across, 2 down | 1024 | 80 | 1280 |

Both use Fermi 8-row GOBs, one GOB in block height/depth, complete source images,
depth 1, layer/origin zero and launch 0x306. The guest creates physical source
bytes by inverse traversal of 16-byte sectors. The pixel-byte pattern depends
on x, y, horizontal/vertical GOB position and lifetime. No color format is inferred.

It fills the entire pitched destination with 0xCC and sets guards before/after
the checked region. It then generates a 25-word stream containing separate
B197/B0B5 bindings, address/row registers, all six source surface registers,
launch, no-ops and a lifetime-specific SET_REFERENCE marker. Original libnx
mapping, cache, append and Kickoff functions are retained. The saved client
version is temporarily 4.0.0 only around Kickoff to select ioctl2, then restored.
The Nvmap allocation uses the existing raw NvKind_Pitch storage profile; source
layout is interpreted by the CE descriptor in this local interpreter. Native
PTE/kind behavior for this combination has not been validated.

The guest verifies every physical source byte, every converted destination
byte, all destination padding and both guards. It calculates an expected logical
row hash from the coordinate pattern, a hash of actual destination row bytes,
and a separate hash of physical source bytes. A second conversion deliberately
uses overlapping source/destination envelopes; the guest checks local refusal,
unchanged data and retained client entry, discards that entry and cleans up.

Failure is sticky across later successful work. The inherited startup sentinel
77 is normalized. The guest exit wrapper records main's return (0 or 1) in
report[14]; Horizon ExitProcess itself has no status argument.

## Independent validation

`libnx_guest_generated_block_conversion` runs the new NRO untouched with large,
seven-instruction and one-instruction CPU slices, with a two-million-instruction
total guard. The observer reads original ioctl2 submissions, GPU-mapped command
bytes, committed bindings/surface registers, reference state, submission count,
all source and destination bytes, guards and reservation versions.

The host constructs expected physical source data separately by raster traversal
and explicit bit placement, rather than calling the production layout helper or
copying the guest's inverse sector traversal. It independently calculates both
logical and physical hashes and exact mismatch counts from the known pattern.
Normal runs inject no commands and do not alter memory or registers.

Two separate negative tests perturb only the first case:

- `libnx_guest_block_detects_missing_conversion` replaces LAUNCH_DMA with DMA_NOP.
  Kickoff succeeds, but the guest must find the exact expected number of wrong
  row bytes and a wrong destination hash.
- `libnx_guest_block_detects_padding_corruption` changes one destination padding
  byte after successful conversion. Row hashes remain correct, but the guest
  must report exactly one padding mismatch and fail.

Both faults must remain recorded after the second case succeeds. These perturbed
runs are fault tests, not untouched-producer demonstrations. The original
synthetic conversion suite retains broader mapping, geometry and allocation
failure coverage. Earlier NROs and original SDK function bodies are unchanged.

The untouched positive NRO executes 552,272 ARM64 instructions with each CPU
slicing mode. Missing conversion and corrupted padding execute 572,671 and
552,283 instructions respectively and record guest return 1. Rebuilding twice
produced the same NRO SHA-256:
`C87E2E0C743D1BFC88F90C416030C45D19EB25FA9F9FE35053F3803832DAE640`.
All 40 recorded NRO hashes match their manifests; all 40 older NRO files in the
before-work snapshot remain unchanged. Linux was not executed here.

Windows validation: **127/127 CTest suites passed in Debug (112.67 seconds)
and Release (25.83 seconds)**. The isolated legacy test passed with a measured
Debug peak working set of 103.56 MiB.

## Reproduction and report

Rebuild with `tools/build_libnx_gpu_copy_block.ps1 -Python <python executable>`.
The shared SDK builder must run without another SDK build or emulator/test run.
The manifest records source/symbol/NRO hashes and instruction counts. Included
SDK/newlib/libgloss and NVIDIA notices retain their original terms.

Two 64-word report blocks follow the earlier GPU guest fixtures. Differences:

| Word | Meaning |
| --- | --- |
| 39 / 40 / 41 | Physical source / converted row / padding-and-guard mismatches |
| 42 | Converted byte count |
| 43 / 44 | Expected logical row hash / actual destination row hash |
| 52 | Source and destination mismatches after rejected overlap |
| 54 / 55 | Byte width / row count |
| 56 | Physical source byte count, not pitch |
| 57 / 58 | Destination pitch / verified padding byte count |
| 59 | Actual physical source hash |
| 60 / 61 | Checked destination byte count / source block-size register |

Display integration, larger block heights, partial images, reverse conversion,
shaders and drawing remain separate work. A useful next bounded step is a
guest-visible conversion-to-display path with explicit layout and presentation
checks, rather than inferring rendering from successful byte conversion.
