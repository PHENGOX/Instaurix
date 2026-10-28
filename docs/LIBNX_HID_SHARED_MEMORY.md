# Original HID client and shared memory

Historical milestone after [applet initialization](LIBNX_OPERATION_MODE.md), followed by [time/UTC](LIBNX_TIME_CLOCKS.md). The reference links original libnx services/hid.c, kernel/shmem.c and kernel/random.c at b156b4774fb2df7b93402169509266da7547fc4c.

## Executed path

The original client looks up hid, sends CreateAppletResource with SendPid/AppletResourceUserId, receives a resource session, obtains a copied shared-memory handle, and uses original shmem/virtmem to choose an address and MapSharedMemory.

Resource identity is checked against the simulated process; guest PID bytes are not host identity. HID is homebrew-profile-only. HID sessions/resources/memory handles are typed; memory is neither an awaitable event nor an IPC session.

Resource/handle publication occurs only after complete replies. Invalid format/arguments/identity/quota consume no IDs. Independent resources and clones within one process share one memory object.

## Bounded shared memory

The object is **0x40000 / 256 KiB**, initially zero. RAM mappings directly reference shared backing pages, not a map-time copy. Memory tests verify backing updates through two read-only aliases.

SVC 0x13 requires a valid memory handle, page-aligned free address, full object size and Read permission, outside image/runtime reservations. Reads work; writes/execute fail. QueryMemory reports SharedMemory type **6**, R.

Only **one simultaneous mapping per HID object** is supported, regardless of handle count. SVC 0x14 requires exact address/size; errors retain mappings. Valid unmap frees virtual pages, permitting remap through another handle/resource. Closing a handle does not unmap; closing a parent session leaves children alive. Copied handles use the shared **128-slot** quota. Failed loads preserve mappings; successful loads replace RAM/resources.

Memory remains neutral: no connected controllers, button samples, sensors or HID timestamps. This validates initialization/transport, not controller reading or every HID ring structure.

## Original allocator and reference build

virtmem uses original libnx PRNG. Four GetInfo RandomEntropy calls (ID 11, INVALID_HANDLE, subindices 0..3) return fixed test words for reproducible addresses, not hardware/cryptographic entropy. Observed mapping is **0xD93000**.

Original HID uses GCC atomic loads on ordinary shared fields, including structs. [compat_hid_atomics.h](../tests/programs/libnx_app/compat_hid_atomics.h), injected only for hid.c, adapts Clang using generic __atomic_load with requested memory ordering. It is not a fake HID success stub. Upstream sources remain unchanged; this is not full devkitA64 installation.

Removing hidInitialize/hidExit traps requires a rebuilt NRO: SHA-256 `C4FCA5BFB745F80EA1616B003FAA89DE695B5F33DBA321A98B2B901ED37BA79F`, reproduced strictly. Source/shim hashes are in reference.json.

## Result and tests

Original applet/HID initialization completes. Focused startup: **11,123 instructions / 32 IPC exchanges**, then unlinked timeInitialize marker **0xE012 / 57362**, not a native HID error. Main/constructors remain unreached. Initially unfocused: wait after **7,186**, then **5,240 additional instructions**. Rebuilding changed counts relative to earlier milestones.

libnx_hid_resources_and_shared_memory checks original SDK flow, identity/formats, copied handles, transactional publication, permissions/reservations/QueryMemory, invalid mapping, closure/remap, independent sessions/clones, quota/reload, shared backing and deterministic entropy. **63 Debug/Release suites passed**, with reproducible NRO hash.

Next was original time initialization and required clock reads. Modern time shared memory, actual HID samples and filesystem startup remain distinct. This historical Switch reference establishes no native Switch 2, GPU or game compatibility.
