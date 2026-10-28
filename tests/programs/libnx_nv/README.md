# Original libnx NVIDIA initialization reference

Separate project harness calling original nvInitialize and nvExit twice after original default application/FSdev initialization. The harness overrides weak __nx_nv_transfermem_size to 64 KiB for the local 2 MiB heap. Original NV/tmem/allocator function bodies are unchanged.

Initialization now succeeds: the service retains transfer-memory ownership after sender handle close, original tagged clone and client registration complete, and original cleanup releases the final endpoint before waiting for restored memory permissions and freeing the source allocation. Device opens/ioctls, nvmap-backed buffers and GPU rendering remain unsupported.

Build with `tools/build_libnx_nv.ps1 -Python <python_path>`. Source hashes, report fields, explicit configuration and expected unchanged NRO hash are recorded in reference.json. Original libnx/newlib/libgloss licenses accompany the fixture. This reference does not call framebufferCreate.

Run CTest `libnx_original_nv_transfer_memory`; the independent kernel suite is `transfer_memory_ownership_and_lifetime`. See [current initialization and lifetime](../../../docs/LIBNX_NV_INITIALIZATION.md). The separate libnx_nvmap fixture captures the next actual device-open boundary.