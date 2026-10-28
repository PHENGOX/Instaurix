# Guest-generated block-source conversion

Project-owned ARM64 producer/verifier using unchanged pinned libnx functions.
Two complete Fermi one-GOB-height sources are generated in physical sector order,
converted to pitched rows and checked byte by byte, including padding, guards,
logical/physical hashes, overlap refusal and cleanup.

See [the workload and report layout](../../../docs/LIBNX_GUEST_GPU_BLOCK_COPY.md).
This is a synchronous historical HLE reference, without validated native Switch
2 compatibility, hardware cache/completion semantics or rendering.

Rebuild with `tools/build_libnx_gpu_copy_block.ps1 -Python <python executable>`.
Run the shared SDK builder alone. The manifest records hashes and provenance;
included notices cover the original SDK and NVIDIA reference material.
