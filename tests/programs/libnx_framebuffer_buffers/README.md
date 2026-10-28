# Original framebuffer buffer-count reference

The harness runs counts 1, 2 and 3 sequentially. Each original framebuffer
lifetime renders two full cycles of 16x16 RGBA frames, using a distinct pattern
for its count and frame index. Twelve snapshots and recorded cur_slot sequences
are checked. Normal execution takes 340060 instructions. Original NWindow
instances are closed/recreated between configurations; SDK bodies are unchanged.

Rebuild with tools/build_libnx_framebuffer_buffers.ps1. The original full producer,
framebuffer.c, cache.s, 64 KiB NV communication-memory override, toolchains and
third-party licenses follow ../libnx_framebuffer_present. See reference.json
for hashes, exact report-field layout and provenance.

See ../../../docs/LIBNX_FRAMEBUFFER_BUFFERS.md for the local fair reuse policy,
transactional failure tests and GPU scope. No GPU command or native Switch 2
execution is validated by this CPU software-display reference.
