# Original libnx BindChannel reference

See [the binding policy and report layout](../../../docs/GPU_ADDRESS_SPACE_BINDING.md).
Two original NV/AS/Nvmap/channel lifetimes call BindChannel, then attempt
AllocGpfifoEx2. The latter is deliberately unsupported; no commands execute.

Rebuild with `tools/build_libnx_gpu_bind.ps1 -Python <python executable>`.
The original nvchannel.c translation unit additionally requires the pinned
Arm GNU 14.3.1 compiler. SDK sources remain unchanged. Original initialization
uses the existing default application reference and a 64 KiB NV transfer-memory
override. Included license texts and reference.json record provenance.
