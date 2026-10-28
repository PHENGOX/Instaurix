# Guest-generated pitched 2D copy

The project-owned ARM64 harness in `tests/programs/libnx_gpu_copy_2d` generates
its own B197/B0B5 bindings and multiline copy commands, submits them with
unchanged pinned libnx functions and verifies the result inside the guest.
The normal test and its instruction observer do not write guest memory or CPU
registers. No production emulator behavior changes for this milestone.

This exercises the [bounded pitched-copy policy](GPU_MAXWELL_PITCHED_COPY.md),
using the CPU-side synchronous Maxwell HLE interpreter. Native Switch 2
compatibility, GPU rendering and native completion/cache semantics remain
unvalidated. The harness checks output after Kickoff specifically because this
local profile completes copies synchronously. A real hardware workload would
require validated completion waits and CPU cache handling.

## Workload and results

Each case creates and closes its own channel, address space and Nvmap buffer:

| Case | Width (bytes) | Rows | Source pitch | Destination pitch | Copied bytes |
| --- | --- | --- | --- | --- | --- |
| 0 | 7 | 3 | 13 | 64 | 21 |
| 1 | 17 | 4 | 31 | 48 | 68 |

The guest fills a 256-byte source with a lifetime-specific pattern, initializes
256 destination bytes to 0xCC and sets two adjacent guards. It constructs an
18-word command stream using the documented eight address/geometry registers
and launch 0x386. Original libnx cache, mapping, append and Kickoff functions are
retained. As in earlier fixtures, the saved client version is temporarily set
to 4.0.0 only around Kickoff to select ioctl2, then restored.

After submission, the guest verifies all source bytes, every copied destination
byte, all untouched destination bytes and both guards. It computes FNV-1a hashes
over packed row data, separately using the source and destination pitches. It
then generates overlapping work, verifies local refusal and unchanged data,
discards the rejected client entry and cleans up through original SDK functions.
Overlap refusal is a local policy, not a hardware-semantics claim.

The inherited startup sentinel 77 is normalized. The harness returns 0 on
success, otherwise 1, retaining failure across later successful work. The guest
exit wrapper records this in report[14]; Horizon ExitProcess itself has no
status argument.

## Independent checks and deliberate faults

`libnx_guest_generated_2d_copy` runs the original new NRO with large, seven- and
one-instruction CPU slices. The observer reads actual ioctl2 descriptors and
GPU-mapped command bytes, compares their fields with independently specified
geometry and checks committed class bindings, row registers, reference marker,
submission count, destination bytes and reservation changes. The host independently
recomputes expected row hashes and checks all guest report fields and cleanup.
No command injection occurs in these normal runs.

Two separately registered negative tests deliberately perturb the first case:

- `libnx_guest_2d_detects_missing_transfer` replaces only LAUNCH_DMA with DMA_NOP.
  Submission succeeds, but the guest must report wrong row bytes and hashes.
- `libnx_guest_2d_detects_padding_corruption` changes one padding byte after the
  first successful transfer. Row bytes and hashes remain correct, but the guest
  must report padding corruption and fail.

Both must retain failure after the second case succeeds and record return 1.
These perturbed runs are fault tests, not untouched-producer demonstrations.

The positive NRO completes in 108,040 ARM64 instructions in each CPU slicing
mode. The missing-transfer and padding-corruption runs execute 108,165 and
108,045 instructions respectively, recording guest failure as expected.
Two SDK builds produced the same new NRO SHA-256:
`480049ACAB04C245E23671E188695611E7DD6DC675D2A9CE8182D24612770EE9`.
All 39 recorded NRO hashes match their manifests; all 39 older NRO files in
the before-work snapshot remain unchanged. Linux was not executed here.

Windows validation: **124/124 CTest suites passed in Debug (106.54 seconds)
and Release (24.41 seconds)**. The isolated legacy virtual-memory test passed
with a Debug peak working set of 103.64 MiB.

## Provenance, rebuilding and report

Rebuild with `tools/build_libnx_gpu_copy_2d.ps1 -Python <python executable>`.
The shared SDK builder must run without another SDK build or emulator/test run.
The manifest records pinned SDK sources, source/symbol/NRO hashes, byte size and
the measured instruction count. Included libnx, newlib, libgloss and NVIDIA
notices are retained. Earlier NROs and original SDK function bodies are unchanged.

The two 64-word report blocks retain the [linear guest layout](LIBNX_GUEST_GPU_COPY.md).
Here word 41 also counts incorrect untouched destination bytes, word 42 is
width times rows, and words 43/44 hash packed row data. New geometry fields:

| Word | Meaning |
| --- | --- |
| 54 | Row width in bytes |
| 55 | Row count |
| 56 / 57 | Source / destination pitch in bytes |
| 58 | Number of untouched destination bytes verified, excluding the two guards |

Image-layout transfers and eventual rendering remain separate milestones. The
next bounded step is to identify and test the required block-linear copy fields
and address transformation before expanding the admitted launch forms.
