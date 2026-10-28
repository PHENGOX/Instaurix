# Original GPU address-space lifecycle

Implementation follow-up: [Nvmap-backed GPU mappings](LIBNX_GPU_MAPPINGS.md) now adds bounded automatic/fixed associations, metadata translation and backing-retention guards. Command consumption, channels and native Switch 2 validation remain separate work; earlier boundary statements below are historical.

Implementation follow-up: [GPU virtual reservations](LIBNX_GPU_VIRTUAL_ALLOCATIONS.md) now supports bounded original AllocSpace/FreeSpace calls. Nvmap mappings, channels and GPU command execution remain unsupported; older boundary statements below describe prior milestones.

The libnx_gpu_as reference now executes unchanged original nvAddressSpaceCreate
and nvAddressSpaceClose twice. Its project harness explicitly requests a
64 KiB page profile and a 64 KiB NV communication buffer for the small heap.
These are local reference choices, not validated native Switch 2 capabilities.
No GPU virtual addresses, memory mappings, channels, commands or frames are
created by this milestone.

## Measured original-client progress

The same unchanged 397312-byte NRO was run before and after support:

| Measurement | Previous emulator | Current emulator |
| --- | --- | --- |
| Instructions | 52674 | 54014 |
| Address-space Open | FileNotFound (0x1BFF) | Success |
| Original InitializeEx calls | 0 | 2 |
| Original Create result | FileNotFound | Success |
| Original Close/repeated Close | Failure cleanup | Normal cleanup |
| Frames | 0 | 0 |

The fixture performs two independent NV service lifetimes, gets distinct local
descriptors 1 and 2, and retires them through the original Close routine.
Normal and seven-instruction slices produce identical counts and reports.
Baseline mode was measured before the production change; it is historical,
not an expected result for the current implementation.

## Implemented profile

NV descriptors now carry explicit Map, Control or AddressSpace types.
/dev/nvhost-as-gpu joins the existing common 32-descriptor quota. IDs remain
process-monotonic. A typed address-space descriptor is initially uninitialized.
InitializeEx is request 0x40284109, exactly 40 input bytes: flags=1, as_fd=0,
big_page_size=65536, reserved=0 and three zero u64 fields. Other flags/page
sizes/reserved values fail. Reinitialization returns AlreadyInitialized.

The NV ioctl transport now supports input-only requests (direction 1) as well
as the existing input/output direction 3. The input length must match the
encoded size and be readable RAM outside IPC reply storage. Input-only
requests require a completely null receive descriptor. No output pointer or
writable input memory is required. Legacy and AutoSelect transport parsing
share these validations; the original fixture uses AutoSelect.

Initialization state and page metadata are staged until the IPC reply write
succeeds. Descriptor Close also stages retirement; failed replies preserve
state. Closing a descriptor clears its type and initialization metadata.
Session clones share the owning driver's descriptors and initialization.
Nvmap/control operations cannot initialize an address-space descriptor, and
address-space initialization cannot run on a Map endpoint. Unknown operations
before initialization return NotInitialized; after initialization they return
UnknownCommand. AS AllocSpace remains explicitly unsupported and leaves
caller bytes unchanged. These are local result/lifetime policies, not a full
Horizon or native Switch 2 driver implementation.

Process exit, fault and fresh load discard these records through the existing
NV service lifetime. Failed load preserves the current state. There are no
external GPU objects or mappings to release yet. Future mapping/channel work
must add its own ownership and teardown rules.

## Validation coverage

The original-client test covers successful Create/Close and repeated Close,
normal and short execution slices, distinct lifetimes and zero frames.
Host replays exercise every InitializeEx field, invalid descriptor IDs,
malformed input/output descriptors, IPC overlap, unreadable and read-only
input, the shared quota, slot reuse, root/tagged-clone sharing, device-type
isolation, duplicate initialization, protected Open/Initialize/Close replies,
stale descriptors and unsupported AS allocation without data publication.
Separate live-process checks cover failed load, successful fresh load, exit,
fault and restoration of borrowed NV communication memory. Legacy input-only
parsing is tested directly; the original guest executes AutoSelect transport.

The source hash manifest records unchanged original address_space.c and
nvhost-as-gpu.c. The project-owned harness and symbol table are hashed too.
Rebuild with tools/build_libnx_gpu_as.ps1 and the pinned Zig/Python references.
No downloaded toolchain or original SDK source body is modified by this target.

## Next implementation boundary

Add bounded GPU virtual allocation and release through original
nvAddressSpaceAlloc/Free. Keep GPU addresses in a separate registry/type,
validate page arithmetic, alignment, overflow and overlap, stage results until
reply publication and enforce quotas/lifetime cleanup. Only after that should
Nvmap-backed mappings and channel binding be added. This milestone validates
GPU-driver infrastructure, not command execution, shaders or rendering.

## Validation results

All 90 CTest suites pass on Windows in Debug (69.27 s) and Release (13.05 s).
Logs: build/gpu-as-ctest-debug.log and build/gpu-as-ctest-release.log.
Linux was not run locally. Strict reconstruction reproduces the new 397312-byte
NRO SHA-256:
8D95CD432A41AE32DB2F3137FBCE970F7FD47946449183CC9D0CF36597171C03.
All 25 recorded NRO hashes match; the earlier 24 binaries are unchanged.
The new harness and symbol hashes match their manifest, and both English
README variants match. The baseline and updated original-client runs used the
same guest binary; host negative probes are separate from instruction metrics.
