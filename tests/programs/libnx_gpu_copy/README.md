# Guest-generated B0B5 copy reference

Project-owned producer and verifier using unchanged pinned libnx runtime,
Nvmap, address-space and channel functions. Two independent lifetimes each copy
64 bytes and verify all bytes, guards and hashes, then check local overlap
refusal. See [the workload and report layout](../../../docs/LIBNX_GUEST_GPU_COPY.md).

This targets the synchronous historical HLE profile. Native GPU completion,
cache coherence, Switch 2 compatibility and rendering are not demonstrated.
The inherited startup sentinel 77 is normalized; main returns 0 on success or
1 on failure, recorded by the inherited guest exit wrapper in report[14].

Rebuild with `tools/build_libnx_gpu_copy.ps1 -Python <python executable>`.
The shared SDK builder must run alone. The manifest records reproducible hashes;
included licenses cover the unchanged SDK sources and public NVIDIA references.
