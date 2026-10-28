# Original libnx nvmap Create request reference

Separate project harness with original NV/nvmap initialization, original device Open/Close and original nvioctlNvmap_Create from nvidia/ioctl/nvmap.c. It reaches the first actual eight-byte aliased AutoSelect ioctl request. The decoder now returns a distinct unbound object ID on each Create. Device Close reclaims its objects. No backing memory or graphic buffer is created. The current reference executes 54,104 instructions.

Rebuild with `tools/build_libnx_nvmap_alloc.ps1 -Python <python_path>`. Source hashes, report fields, required NRO hash and the explicit 64 KiB communication-memory configuration are recorded in reference.json. Original library licenses accompany the fixture. Run CTest `libnx_original_nvmap_create_boundary`.

See [device descriptors and allocation boundary](../../../docs/LIBNX_NVMAP_DESCRIPTORS.md).
