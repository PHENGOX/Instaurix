# Original NVIDIA client and scoped transfer memory

The separate libnx_nv reference runs original `nvInitialize`, original tmem.c and original newlib aligned allocation/cleanup. This isolates the first NVIDIA initialization prerequisite used by framebufferCreate. It does not execute framebufferCreate itself, initialize nvmap, allocate graphic buffers or execute GPU commands.

## Explicit reference configuration

The SDK's weak `__nx_nv_transfermem_size` defaults to 8 MiB. The local homebrew environment currently supplies a 2 MiB heap, so this project harness explicitly overrides that configuration variable to **64 KiB**. Original SDK function bodies remain unchanged. This is a small-memory reference configuration, not validation of the default NVIDIA allocation size or native console memory layout.

Original nv.c, tmem.c, runtime/alloc.c, newlib aligned_alloc.c and malignr.c are compiled from verified pinned sources. The latter uses the same original dlmalloc implementation and _LIBC build configuration as the earlier allocator reference. Source hashes and original library licenses accompany the fixture.

## Kernel transfer-memory subset

SVC 0x15 now creates a uniquely owned transfer-memory handle in the explicit homebrew-reference profile. Its source must be a nonempty page-aligned range of currently allocated writable heap pages, with no existing memory attributes or overlapping borrow. Only owner permission Perm_None is supported. Invalid inputs, unsupported permissions and exhausted kernel-handle quota fail before any permission/handle mutation; failed creation returns output handle zero under this local ABI.

Successful creation removes guest read/write access and marks the source heap pages with the local borrowed attribute. QueryMemory reports the existing heap type, borrowed attribute and current no-access permission. Source bytes are preserved. The range cannot be discarded by heap shrink while its handle exists. The existing handle quota bounds the number of objects. General transfer-memory mapping, imports, duplicate handles and nonzero owner-permission variants remain unsupported.

Closing the sole transfer-memory handle restores heap read/write access and removes the borrowed attribute. Process fault/exit also retire outstanding transfers. The memory reference used for terminal cleanup is bound on the first live SVC after transactional loading, not to the temporary Memory object used to stage a load. Failed load, fault and reload behavior is tested through Machine as well as direct kernel ABI calls. Successful load replaces the previous address space and process context.

## NVIDIA endpoint and observed boundary

Homebrew-reference SM lookup now recognizes only application service `nvdrv`, producing a typed session. System/applet/factory aliases remain absent. Initialize command 3's actual packet is decoded: two copied handles (current-process pseudo handle and a real transfer-memory handle), four-byte matching memory size, no PID and the original plain CMIF layout. Invalid handle types, wrong size and unsupported copied-handle layouts fail explicitly.

A structurally valid Initialize still returns **UnknownCommand**. The service does not retain or map the transfer-memory object and does not claim successful initialization. This is intentional: original nv.c closes its sending handle immediately after the request. Implementing success requires service-owned copied references that keep the source borrowed until service cleanup; merely accepting the packet would violate memory lifetime. Driver clone/client-PID setup, device opens and ioctls are not reached by this fixture.

The original client makes two attempts. Each allocates aligned memory, creates transfer memory, sends the actual Initialize packet, receives the explicit service error, closes the transfer handle, observes restored source access through original cleanup and frees the allocation. It closes its NVIDIA service session after failure and returns normally through application cleanup. No unsupported success is substituted.

## Measurements and tests

The guest executes **51,360 instructions**, with **376,832 file bytes** and **16,384 BSS bytes**. Base report is 0x6A000; NV report is 0x6B1D8. NRO SHA-256: `187C46FFC15D117F36F4DC53B99B836BBE229C1E09FF291664D34F7406761C8A`.

`transfer_memory_ownership_and_lifetime` checks alignment/range/permission rejection, source protection and QueryMemory metadata, overlapping borrow, blocked heap shrink, byte preservation, close/double-close, quota exhaustion without hidden borrow and fault cleanup.

`libnx_original_nv_transfer_memory` compares continuous and seven-instruction runs, captures both original create/Initialize/close sequences, validates the copied handles/size, replays malformed/protected requests and checks source restoration before SDK free. Another run fails a load while source pages are borrowed, faults the guest, checks restoration and reloads the original program successfully. Earlier guest binaries remain separate and unchanged.

## Next step

Implement service-retained transfer-memory references and their release across NVIDIA session clones/cleanup before returning success from Initialize. Then follow original SetClientPID and device-open requests toward nvmap. Buffer slots, NVIDIA memory mappings, fences, framebuffer creation and native Switch 2 graphics remain unvalidated.

## Final validation

All 76 CTest suites pass on Windows in Debug and Release after rebuilding every executable. Strict second reconstruction reproduces the NV NRO hash. Eight guest binaries match their manifests; the earlier binaries remain unchanged. All 37 checked NV/newlib source-file hashes match the cached verified sources. English README variants remain identical. Linux CI has not been executed for this milestone.

Follow-up: [original NVIDIA initialization](LIBNX_NV_INITIALIZATION.md) now retains service-owned transfer references, supports tagged clone/client registration and completes nvInitialize. The unchanged NV guest now executes 52,006 instructions; original nvMapInit in a separate fixture reaches unsupported /dev/nvmap Open.
