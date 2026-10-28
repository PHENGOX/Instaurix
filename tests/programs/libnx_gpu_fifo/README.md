# Original libnx GPFIFO allocation reference

See [the policy and measurements](../../../docs/GPU_GPFIFO_ALLOCATION.md).
The project-owned harness runs two original NV/AS/Nvmap/channel lifetimes,
allocates a bounded GPFIFO and attempts original AllocObjCtx. Graphics-object
allocation remains unsupported. No commands are submitted or executed.

Rebuild with `tools/build_libnx_gpu_fifo.ps1 -Python <python executable>`.
The original nvchannel.c translation unit needs the pinned Arm GNU 14.3.1
compiler. Other source units use the existing Zig/Clang reference builder.
SDK sources remain unchanged; license texts and reference.json record provenance.

The legacy gpu_as_report symbol uses two 32-word blocks. Words 0 through 16
match the BindChannel fixture's report layout; word 17 is AllocObjCtx's result,
18 is its project-owned output variable, and 19 through 31 are reserved.
