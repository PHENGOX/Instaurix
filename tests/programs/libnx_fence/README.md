# Original libnx fence-device initialization

The project harness calls unchanged nvFenceInit/nvFenceExit functions twice
with independent NVIDIA service lifetimes. nvFenceInit opens /dev/nvhost-ctrl;
cleanup closes the descriptor and resets its local value to UINT32_MAX.
No event is registered and no fence is signaled or waited on.

Rebuild with tools/build_libnx_fence.ps1. The explicit communication-memory
override is 64 KiB; the full SDK default is not validated. Source hashes and
licenses accompany this fixture. fence_report fields 0..4 and 8..12 contain
nvInitialize result, nvFenceInit result, control fd, post-fence-exit fd and
post-nv-exit session. Normal execution is 54,115 instructions.
This is a prerequisite probe based on framebufferCreate's source ordering,
not execution of framebufferCreate and not a registered graphic buffer.