# Original NVIDIA initialization and retained transfer-memory references

Original libnx nvInitialize now completes successfully in the explicit 64 KiB transfer-memory profile. Initialize command 3, original tagged session cloning and SetClientPID command 8 are implemented as bounded HLE behavior. This establishes client/session lifetime; it does not initialize a physical GPU or provide NVIDIA device descriptors, mappings or ioctls.

## Shared ownership

Transfer memory is now a shared kernel object tracked by a bounded process registry independently of the guest handle table. A guest handle and an initialized NVIDIA service may each retain it. Closing the sender's handle does not restore heap access while the service owns a reference. Cloned sessions share the service state, so either root-first or clone-first close order keeps the source protected until the last kernel endpoint closes. A CMIF close alone does not release its still-open kernel handle; original serviceClose performs both operations.

The final owner release restores heap read/write permissions and clears borrowed attributes without changing source bytes. Heap shrink consults the registry, including objects whose sender handles have already closed. Fault/exit retire both guest handles and NVIDIA service references before restoring remaining source ranges. Transactional loading still avoids references to temporary staged Memory objects.

## Protocol subset

Initialize accepts the previously validated two copied handles and matching size. Only after the complete reply is written does it publish the retained transfer object and initialized state. Repeated Initialize returns AlreadyInitialized; invalid handles, wrong size, malformed packets or protected replies publish no ownership change.

The original client then closes its transfer handle and requests control command 4 with tag 1 (serviceCloneEx). This tagged form is scoped to NVIDIA sessions and shares initialized state. Unsupported tags and protected replies cannot create hidden endpoints. Existing ordinary clone behavior remains available through its previous command.

SetClientPID accepts the original SendPid/plain-CMIF packet containing the current process's applet resource user ID. It requires initialized service state. Guest PID placeholder bytes are not trusted as a process identity. Invalid resource IDs and protected replies do not register a client. Successful registration is shared by service clones and enables subsequent device-command dispatch.

Device Open still returns explicit UnknownCommand after transport/state validation. No fd, memory mapping or successful ioctl is fabricated. Mapping/import and nonzero owner-permission transfer-memory variants remain unsupported. The local configuration remains 64 KiB within the 2 MiB heap, rather than the original SDK's 8 MiB transfer-memory default.

## Original client milestones

The previous libnx_nv guest is unchanged and now completes two successful initialization/cleanup cycles in **52,006 instructions**. It acquires a root endpoint, creates transfer memory, initializes the service, closes its sending handle, clones the endpoint, registers its applet identity and returns success. Original nvExit closes clone/root endpoints, waits for restored source permissions and frees its original aligned allocation.

A separate libnx_nvmap reference additionally links original nvidia/map.c and calls nvMapInit. Both attempts reach the actual **nvOpen("/dev/nvmap")** request and return UnknownCommand, leaving the fd sentinel at UINT32_MAX. Original NVIDIA cleanup still completes. This is the next observed device-service boundary; no nvmap object is created.

The new guest executes **52,746 instructions**, with **393,216 file bytes** and **16,384 BSS bytes**. Base report is 0x6E000; NV report is 0x6F1E0. NRO SHA-256: `C16F45D045AE601462852A943B142D3029255ECA485AD64978913A6F6ED7F11C`. Source hashes and original licenses accompany the fixture. Its project harness calls NV/nvmap APIs directly; framebufferCreate and a complete external application are not executed.

## Validation scope

The NV integration suite compares continuous/seven-instruction execution and captures actual Initialize, tagged clone, registration and cleanup packets. It checks duplicate initialization, malformed/copied handles, wrong size/identity/tag, protected replies and state publication. The source stays inaccessible after sender close and after closing one endpoint, then becomes writable after the last endpoint closes, before original SDK cleanup/free.

A second run closes root before clone and verifies deferred restoration and blocked heap shrink without a sender handle. Another faults a process holding service-only transfer references, verifies restoration, then reloads and reruns the original client. Existing direct transfer-memory ABI tests remain active. The new nvmap test also verifies both real /dev/nvmap path requests and absence of fabricated descriptors.

## Next step

Implement bounded /dev/nvmap descriptors and their close/clone/process lifetime, then follow the original nvmap Create/Alloc/GetId ioctls. Kernel transfer memory is communication memory; it is not a substitute for nvmap-backed graphic-buffer allocation. Fences, buffer registration/submission, presentation and native Switch 2 compatibility remain unvalidated.

## Final validation

All 77 CTest suites pass on Windows in Debug and Release after rebuilding the executables. Strict second reconstruction reproduces the new nvmap NRO hash. Nine guest binaries match their manifests, including the unchanged original NV reference. English README variants remain identical. Linux CI has not been executed for this milestone.

Follow-up: [nvmap device descriptors](LIBNX_NVMAP_DESCRIPTORS.md) now supports original /dev/nvmap Open/Close. The next observed boundary is the original Nvmap Create aliased AutoSelect ioctl; no memory object is created yet.
