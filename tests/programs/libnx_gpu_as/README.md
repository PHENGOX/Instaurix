# Original GPU address-space lifecycle reference

The project harness calls unchanged original nvAddressSpaceCreate/Close twice
through original NV service initialization and cleanup. It requests a local
64 KiB page profile and retains the existing small-heap 64 KiB communication
memory override. Both lifetimes succeed, use distinct descriptors and close
normally. Execution takes 54014 instructions with no produced frame. The same
NRO previously stopped at Open with FileNotFound in 52674 instructions.

Rebuild with tools/build_libnx_gpu_as.ps1. Original nvidia/address_space.c and
nvidia/ioctl/nvhost-as-gpu.c are compiled unchanged under the pinned libnx
license. The shared startup/newlib/libgloss configuration follows the earlier
reference fixtures. See reference.json and adjacent licenses for provenance.

gpu_as_report has two 16-entry lifetime blocks: NV initialize result, Create
result, initialized state, descriptor and page size, then Close/repeated Close
and NV service cleanup. No GPU VA allocations, mappings, channels, commands,
shaders or native Switch 2 compatibility are validated.
See ../../../docs/LIBNX_GPU_ADDRESS_SPACE.md for scope and negative tests.
