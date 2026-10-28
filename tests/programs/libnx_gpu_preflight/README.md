# Original mapped GPU command-memory reference

See [the bounded preflight policy](../../../docs/GPU_SUBMISSION_PREFLIGHT.md).
The project-owned harness uses original Nvmap/AS mapping, full GPU channel
initialization, AppendEntry and Kickoff. Its two words are opaque markers, not
an executable graphics program. Valid preflight still refuses job admission.

Only around Kickoff, client HOS 4.0.0 selects the original ioctl2 branch and
the saved version is restored afterward. The general emulator profile and all
SDK bodies remain unchanged. Rebuild with tools/build_libnx_gpu_preflight.ps1
-Python <python executable>; the existing pinned Arm GNU compiler is required
for original nvchannel.c. License texts and reference.json record provenance.

gpu_as_report uses two 64-word blocks. The policy documents its original
transport fields and additional backing/map/cleanup markers.
