# Original GPU virtual allocation reference

This separate project harness extends the unchanged original address-space
client with Alloc/AllocFixed/Free calls. It checks rounded allocation sizes,
separate ranges, exact and duplicate free, hole reuse, fixed overlap and Close
with an unmapped live reservation across two lifetimes. Execution takes 63991
instructions and produces no frame. Before allocation support the same guest
stopped at its first AllocSpace in each lifetime, in 54733 instructions.

Rebuild with tools/build_libnx_gpu_va.ps1. Original address_space.c and
nvhost-as-gpu.c remain unchanged under the adjacent libnx license. The local
page size and NV communication-memory override are both explicitly 64 KiB;
they are distinct settings. This is not a native Switch 2 capability profile.

The gpu_as_report array has two 32-entry lifetime blocks. reference.json records
all field names, toolchain/source hashes and the 4 GiB base, 64 MiB virtual
region and 32-reservation local limit. No CPU/GPU backing or mappings are
created. See ../../../docs/LIBNX_GPU_VIRTUAL_ALLOCATIONS.md for validation,
transactional publication and remaining scope.
