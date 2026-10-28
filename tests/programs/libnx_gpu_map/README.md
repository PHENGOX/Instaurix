# Original GPU Nvmap mapping reference

This separate project harness runs unchanged original Nvmap and GPU address-space
wrappers for two lifetimes. Each creates 128 KiB CPU backing, maps aliases,
checks backing retention, unmaps, maps explicit reservations and a second-page
subrange, and leaves one mapping for AS Close cleanup. CPU marker bytes remain
unchanged. Normal execution takes 73754 instructions with no produced frame.
Before support the same NRO stopped at its first MapBufferEx in each lifetime,
in 60814 instructions.

Rebuild with tools/build_libnx_gpu_map.ps1. Pinned address_space.c,
nvhost-as-gpu.c, map.c and nvmap.c are unchanged. The 64 KiB page profile and
64 KiB NV communication override are separate explicit local settings.
reference.json records source/harness/symbol hashes and two 64-entry report
blocks. See adjacent licenses and ../../../docs/LIBNX_GPU_MAPPINGS.md.

This validates mappings and metadata translation, not GPU reads/writes,
commands, shaders or native Switch 2 program execution.
