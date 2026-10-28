# Original framebuffer padded-geometry reference

The project harness requests 37x137 RGBA instead of the previous 16x16 case.
Unchanged original framebufferCreate/MakeLinear/Begin/End/Close produce two
pixel-verified frames. Pitch is 192 bytes; surfaces are 49152 bytes at offsets
0 and 49152 in one 98304-byte Nvmap object. Normal execution is 376091
instructions. Horizontal and vertical block transitions and partial final
rows/columns are covered. No GPU command pipeline is executed.

Rebuild with tools/build_libnx_framebuffer_wide.ps1 after preparing the pinned
toolchains. Original sources, explicit 64 KiB communication-memory override,
report fields and licenses follow ../libnx_framebuffer_present; the earlier
fixture remains unchanged. See reference.json for hashes and recorded values.
See ../../../docs/LIBNX_FRAMEBUFFER_GEOMETRY.md for validation and limitations.
