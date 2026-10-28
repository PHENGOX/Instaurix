# Guest-generated pitched 2D reference

Project-owned ARM64 producer/verifier using unchanged pinned libnx functions.
Two independent cases generate multiline B0B5 commands, check copied rows,
source bytes, untouched destination bytes, guards and row hashes, then verify
local overlap refusal and cleanup. See [the workload and report layout](../../../docs/LIBNX_GUEST_GPU_COPY_2D.md).

This is a synchronous historical HLE reference. It does not establish native
Switch 2 compatibility, hardware synchronization/cache behavior or rendering.
Rebuild with `tools/build_libnx_gpu_copy_2d.ps1 -Python <python executable>`;
run the shared SDK builder alone. Provenance and hashes are in reference.json;
included notices cover the original SDK and public NVIDIA reference material.
