# Original nvmap device descriptors and first Create ioctl boundary

Original libnx nvMapInit now succeeds after NVIDIA initialization: nvOpen("/dev/nvmap") returns a tracked logical device descriptor, and original nvMapExit closes it through nvClose. These descriptors represent the local nvmap interface only; they are not host files, kernel handles, nvmap memory-object handles or allocated graphic buffers.

## Descriptor contract

Only the exact ten-byte path `/dev/nvmap` is supported, matching the original client's strlen-based input. Other paths return FileNotFound. Input must be one readable normal map-alias buffer, no raw payload and no receive buffer. Existing RAM/range/permission and IPC transport checks apply. The NVIDIA service must already be initialized and registered.

Each initialized service state owns a table of at most **32** descriptors. Root and tagged-clone endpoints share it. Descriptor IDs start at 1 and increase across the process's NVIDIA connections; zero and UINT32_MAX are reserved, and closed IDs are not reused in that process. Independent service objects have separate membership tables. Closing the last endpoint or retiring the process discards the table. Closing root alone leaves its descriptors usable through a surviving clone.

Open replies contain u32 fd and zero u32 NvError. Close command 2 accepts exactly one u32 fd and no buffers, returning one zero u32 NvError. Zero, foreign and already-closed IDs fail. Allocation/removal and ID advancement publish only after a complete IPC response write. Protected replies do not create hidden descriptors or prematurely close existing ones. Closing a device does not release the service's communication transfer memory; endpoint lifetime still controls that reference.

The preceding unchanged nvmap reference now executes **53,328 instructions** and completes both original device-open/close cycles. The original NV-only reference remains at 52,006 instructions. Their guest binaries are unchanged.

## First original allocation request

A separate libnx_nvmap_alloc reference additionally links original nvidia/ioctl/nvmap.c and calls original nvioctlNvmap_Create after nvMapInit. It requests a 4 KiB object through nvIoctl, command **0xC0080101**, using the descriptor returned by the real preceding Open call.

The captured original transport is NV command 1 with AutoSelect's null X/C fallback descriptors and active eight-byte A/B buffers. Input and output point to the same eight-byte stack structure (u32 size, u32 handle). This intentional alias is part of the next transport requirement and must not be treated like separate Binder parcel buffers.

The current generic NV buffer decoder rejects that AutoSelect shape explicitly with UnsupportedFormat. No output bytes or memory-object ID are published. Both Create calls preserve the project harness's 0xBADF00D output-handle sentinel. Original nvMapExit still closes the valid device descriptor, and NVIDIA/application cleanup completes normally. This target calls nvioctlNvmap_Create directly; nvMapCreate/Alloc/GetId, graphic-buffer memory and framebufferCreate are not executed.

The new guest executes **53,936 instructions**, with **401,408 file bytes** and **16,384 BSS bytes**. Base report: 0x70000; NV report: 0x711E0. SHA-256: `E2C2ABEE3BB34A740C1DE1D91FB3E6F0AA14596D62153AADCBCEC1E2889E09AE`. Original library sources and licenses are retained; source hashes and the explicit 64 KiB communication-memory configuration are recorded in reference.json.

## Validation scope

The NV integration suite captures original Open/Close calls, verifies fd/NvError layouts and compares continuous/seven-instruction execution. Replays cover wrong paths, protected source/reply memory, malformed payloads, the 32-descriptor quota, monotonic IDs, duplicate close and protected Close publication. Root and tagged clone alternately close descriptors. A root-first teardown run verifies that client registration and an opened descriptor survive through the clone, while communication memory stays protected until the last endpoint closes.

The allocation reference also captures both actual Create requests, checks the opened fd, request code, 4 KiB size and aliasing, and verifies unchanged ioctl input/output data and caller sentinels after failure. Existing transfer-memory fault/reload and kernel tests remain active.

## Next step

Add a scoped NV ioctl decoder supporting the actual aliased AutoSelect request and validating descriptor ownership, sizes and permissions. Then implement bounded nvmap Create/Free object lifetime before acknowledging successful object creation, followed by Alloc/GetId and memory binding. Device descriptors alone do not provide graphic buffers, fences, GPU submission, presentation or native Switch 2 compatibility.

## Final validation

All 78 CTest suites pass on Windows in Debug and Release after rebuilding the executables. Strict second reconstruction reproduces the new allocation-probe NRO hash. Ten guest binaries match their manifests; earlier guest binaries remain unchanged. The English README variants remain identical. Linux CI has not been executed for this milestone.

Follow-up: [unbound Create/Free objects](LIBNX_NVMAP_OBJECTS.md) supersedes the unsupported Create boundary recorded above.
