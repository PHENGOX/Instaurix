# Original libnx NVIDIA/nvmap device reference

This separate project harness executes original nvInitialize, nvMapInit, nvMapGetFd, nvMapExit and nvExit under the explicit 64 KiB transfer-memory profile. Device Open now returns tracked /dev/nvmap descriptors, shared across NVIDIA session clones. Original cleanup closes them and releases communication memory normally. No nvmap memory object, graphic buffer or rendered frame is created by this target.

Rebuild with `tools/build_libnx_nvmap.ps1 -Python <python_path>`. Original licenses, source hashes, report fields and the required unchanged NRO hash accompany the fixture. Run CTest `libnx_original_nvmap_device_boundary`.

The separate libnx_nvmap_alloc fixture captures the first original Create ioctl. See [device descriptors and current limits](../../../docs/LIBNX_NVMAP_DESCRIPTORS.md).