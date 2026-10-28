# Guest-generated linear GPU copy

Follow-up: the separate [pitched 2D guest reference](LIBNX_GUEST_GPU_COPY_2D.md)
now generates multiline copies and checks both rows and padding. This original
single-line fixture remains unchanged.

The project-owned `tests/programs/libnx_gpu_copy/harness.c` generates a B0B5
copy stream, submits it through unchanged pinned libnx wrappers, and checks
the resulting bytes inside the guest. The normal tests do not inject command
words, change registers or otherwise modify the running guest.

This is a synchronous historical HLE reference. It does not establish a native
Switch 2 command ABI, hardware cache behavior or GPU completion timing. No image,
shader or triangle is rendered. Ampere research classes remain non-executable.

## Workload

Each of two independently created channel/mapping lifetimes:

1. Initializes the original NV, Nvmap, address-space and GPU-channel wrappers.
2. Allocates and maps a 64 KiB buffer; fills a 64-byte source with a deterministic
   pattern and a separate destination with `0xCC`, plus adjacent guard bytes.
3. Constructs sixteen command words: B197 on subchannel 0, B0B5 on subchannel 4,
   zero 3D no-ops, source/destination registers, length 64, launch `0x186`, and
   a lifetime-specific SET_REFERENCE marker.
4. Flushes the command range using original `armDCacheFlush`, appends the entry
   and calls original Kickoff. The temporary client version 4.0.0 selection is
   restricted to Kickoff and immediately restored, as in earlier references.
5. Verifies every source/destination byte, both guards, and two FNV-1a hashes.
6. Generates a second stream with overlapping source/destination ranges. The
   local bounded profile refuses it. The guest verifies refusal and unchanged
   bytes, discards the rejected client entry, and closes all resources.

The inherited startup probe returns 77 on success. This harness normalizes that
sentinel and returns 0 for a successful copy check or 1 for failure. Horizon
ExitProcess has no status argument: the inherited guest `exit` wrapper records
the return value in `report[14]`, which the tests inspect separately from the
emulator's process-exit result.

The guest checks the destination after Kickoff specifically because this local
profile completes the transfer synchronously. A native hardware workload would
need a validated completion wait and CPU cache handling before reading GPU
output; this harness does not supply that proof. Overlap refusal is also a local
policy, not a claim about native hardware semantics.

## Validation

`libnx_guest_generated_copy_and_verification` runs the untouched NRO with large,
seven-instruction and one-instruction CPU slices. It checks guest reports and
independently recomputes expected hashes. The instruction observer reads actual
ioctl2 submissions and GPU-mapped command bytes, checks publication of bindings,
copy registers and the reference marker, and checks destination reservation
invalidation. Failed overlapping work preserves channel state and committed
submission count. The observer never writes guest state.

`libnx_guest_copy_detects_missing_transfer` is a separate negative test. Only
here, the host replaces the first LAUNCH_DMA word with DMA_NOP. Kickoff still
succeeds, but no first transfer occurs. The guest must detect the wrong
destination, record differing hashes and retain failure after the second
lifetime succeeds. Its exit wrapper must record 1. This deliberately perturbed
run is not counted as an untouched producer demonstration.

The existing mapped-copy transaction tests retain their wider rejection,
ownership, allocation-failure and lifecycle coverage. No production emulator
behavior was changed for this guest milestone.

The unmodified positive NRO completes in **85,571 ARM64 instructions** with each
of the three execution budgets. Its SHA-256 is
`9999487BB1B80084251977AE34817A8246077A79C672349DEF35231FA10CFC8A`.
Rebuilding the final source twice produced the same NRO. All 38 recorded NRO
hashes match their manifests, and all 38 pre-existing NRO files also match the
before-work snapshot (one existing NRO has no such manifest hash).

Windows validation: **121/121 CTest suites passed in Debug (104.68 seconds)
and Release (23.15 seconds)**. The isolated legacy virtual-memory test passed
with a measured Debug peak working set of 103.63 MiB. Linux was not executed
for this milestone. The deliberately suppressed-transfer run executes 85,954
instructions and records guest failure as expected.

## Reproduction and report

Build with `tools/build_libnx_gpu_copy.ps1 -Python <python executable>`.
`reference.json` records the NRO, harness and symbol hashes, pinned SDK source
hashes and instruction count. The shared SDK builder must run alone, without
tests or another SDK build. Included license files retain the SDK and public
NVIDIA notices. Packet-field provenance is linked from the manifest and the
[copy policy](GPU_MAXWELL_LINEAR_COPY.md).

There are two report blocks of 64 u64 words. Existing NV setup/cleanup fields
retain the earlier packet fixture layout. New fields:

| Words | Meaning |
| --- | --- |
| 39 / 40 / 41 | Source, destination and guard mismatch counts |
| 42 | Verified byte count |
| 43 / 44 | Source / destination FNV-1a 32-bit hashes |
| 45 | Sticky guest failure flag |
| 46 / 47 / 48 | Source GPU VA, destination GPU VA, command word count |
| 49 / 50 / 51 | Overlap append result, Kickoff result, retained entry count |
| 52 / 53 | Mismatches after refusal, count after explicit client discard |

The next bounded extension can address pitched two-dimensional copying after
its field encoding and row/range validation are established. Image layouts,
shaders, drawing, asynchronous synchronization and native Switch 2 validation
remain separate work.
