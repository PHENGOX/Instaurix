# Original framebufferCreate reference

The harness calls unchanged original framebufferCreate for 16x16 RGBA and
two buffers. It initializes NV, Nvmap and the fence client, allocates 16 KiB
CPU backing, then registers two 8 KiB block-linear surfaces at offsets 0
and 8192. Creation succeeds; original framebufferClose releases resources.
Normal execution is 125,136 instructions with no produced frame. The prior
allocation-only milestone took 121,329 instructions and rejected registration.

The guest NRO remains unchanged. Rebuild with tools/build_libnx_framebuffer.ps1
after preparing the pinned Arm GNU toolchain. Complete original producer code
uses GCC; framebuffer.c uses the pinned Clang configuration. The explicit NV
communication-memory override is 64 KiB rather than the SDK default of 8 MiB.

framebuffer_report fields 0..5 contain creation result, initialized state,
configured slot mask, connected state, whether backing is null before close,
and close completion. Original framebufferBegin/End, block-linear presentation,
GPU commands and hardware synchronization are not exercised here.
See reference.json and adjacent licenses for provenance.
