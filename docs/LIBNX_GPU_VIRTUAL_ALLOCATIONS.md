# GPU virtual address reservations

Implementation follow-up: [Nvmap-backed GPU mappings](LIBNX_GPU_MAPPINGS.md) now adds bounded automatic/fixed associations, metadata translation and backing-retention guards. Command consumption, channels and native Switch 2 validation remain separate work; earlier boundary statements below are historical.

The original libnx address-space client now executes AllocSpace and FreeSpace,
including its fixed-address wrapper, through the existing typed GPU endpoint.
These operations reserve virtual ranges only. They do not allocate CPU pages,
physical GPU memory, Nvmap mappings, channels, command buffers or frames.

## Separate bounded address model

src/gpu_va.hpp defines Gpu::VirtualAddress as a distinct type and a standalone
address-space registry. The local region begins at 0x100000000 (4 GiB), outside
the 16 MiB CPU RAM model, and spans 64 MiB. Each initialized address-space
descriptor owns an independent namespace with at most 32 live ranges. The
page size remains 64 KiB. These limits and addresses are explicit reference
policy choices, not native GPU capabilities or a Switch 2 address map.

Automatic allocation uses deterministic first fit and honors power-of-two
alignment from one page through the region capacity. Fixed requests require
page alignment, an in-range extent and no overlap. Sparse or unknown flags,
nonzero padding, invalid page sizes and zero/excessive page counts fail before
publication. Page arithmetic is widened before multiplication. Bounds checks
avoid end-address overflow. The entire region may be reserved as metadata
without creating physical backing.

Free requires the exact reservation base and size. Partial, interior, unknown
and duplicate frees fail. Freed holes and record slots may be reused. GPU
address numbers are not permanent object identities: a number can refer to a
new reservation after reuse, or to independent ranges in different address
spaces. Descriptor ownership determines the namespace.

## Driver integration and transaction boundaries

AllocSpace is 0xC0184102 (24 bytes): pages, page size, flags, zero padding and
an alignment-or-offset union. On success only its final u64 becomes the GPU
address. FreeSpace is 0xC0104103 (16 bytes): address, pages and page size.
Both require an initialized AddressSpace endpoint and the validated R/W ioctl
transport. Other endpoint types remain isolated. Mapping requests remain
unsupported, and no GPU virtual address is cast to a CPU pointer.

Registry snapshots are immutable in published service state. Each operation
stages an independent copy, its output bytes and the IPC reply; the new state
is committed only after all reply writes succeed. Failed allocation or Free
replies therefore leave both ranges and caller bytes unchanged. Cloned NV
sessions share the owning descriptor's registry. Distinct descriptors can
reserve identical GPU numbers independently. Close discards the descriptor's
unmapped reservations. Exit/fault and successful reload discard old records;
failed reload preserves them. Future Nvmap mappings must add backing retention
and mapping-aware teardown rules before extending this policy.

## Same original guest before and after

The new libnx_gpu_va harness links unchanged pinned address_space.c and
nvhost-as-gpu.c. It runs two independent service/address-space lifetimes.
Each performs rounded allocation, a second range, exact and duplicate free,
hole reuse, fixed allocation/overlap/free, invalid requests, and Close with
one live unmapped reservation.

| Measurement | Before allocation support | Current |
| --- | --- | --- |
| Instructions | 54733 | 63991 |
| First original AllocSpace | UnknownCommand | Success |
| Allocation output | Unchanged UINT64_MAX | 0x100000000 |
| Original AllocSpace requests | 2 | 14 |
| Original FreeSpace requests | 0 | 12 |
| Frames | 0 | 0 |

The same unchanged NRO was measured on both emulator states. Normal and
seven-instruction execution slices produce the same current count and reports.
The original client rounds 0x10001 bytes to two pages; the next allocation is
at base+0x20000. After freeing the first range, a one-page allocation reuses
base. A live reservation left at Close does not survive the next namespace.

## Validation

The standalone registry suite checks alignment, fragmentation/hole reuse,
copy independence, exact free, fixed overlap, overflow/out-of-range addresses,
independent namespaces, the full 64 MiB region and the 32-record quota.
Original-client host replays additionally check invalid encoded fields,
unsupported sparse flags, type/descriptor validation, unwritable output,
protected IPC replies, quota rollback, root/tagged-clone sharing, independent
descriptors, Close with live ranges, failed load, exit/fault and fresh reload.
The host-driven probes are separate from original instruction metrics.
The earlier lifecycle test also verifies the newly supported allocation;
its original guest still performs only Create/Close and is unchanged.

Rebuild with tools/build_libnx_gpu_va.ps1 and the pinned toolchain. Provenance,
source/harness/symbol hashes and the explicit local limits are recorded in
../tests/programs/libnx_gpu_va/reference.json. No SDK function body is patched.

## Next boundary

Add original Nvmap-backed MapBufferEx/UnmapBuffer calls, validate the relation
between virtual reservations and backing ranges, retain backing until unmap,
and prevent premature destruction. Command decoding, shaders and native
Switch 2 compatibility remain separate unvalidated work.

## Validation results

All 92 CTest suites pass on Windows in Debug (98.13 s) and Release (29.83 s).
Logs: build/gpu-va-ctest-debug.log and build/gpu-va-ctest-release.log.
Linux was not run locally. Strict reconstruction reproduces the 401408-byte
new NRO SHA-256:
CBACDCC17019860D11A38E0A1D6B0B29C54F340E8DDBD7750ABCCD97E5914946.
All 26 recorded NRO hashes match; the earlier 25 binaries are unchanged.
The new harness/symbol hashes match their manifest. Both English README
variants match. Baseline/current instruction measurements use the same guest
binary; host negative probes are separate from those measurements.
