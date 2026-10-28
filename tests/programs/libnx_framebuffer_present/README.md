# Original framebuffer presentation reference

The project harness draws two 16x16 RGBA patterns through unchanged original
framebufferCreate/MakeLinear/Begin/End/Close. Original framebuffer.c converts
the linear pixels to GOB storage; original cache.s and the full buffer producer
remain linked. The emulator publishes two pixel-verified immutable snapshots.
Normal execution takes 150676 instructions. No GPU commands are executed.

Rebuild with tools/build_libnx_framebuffer_present.ps1 after preparing the
pinned Arm GNU toolchain. Full buffer_producer.c uses GCC; framebuffer.c and
the project harness use pinned Clang. NV communication memory is explicitly
limited to 64 KiB. See reference.json and adjacent licenses for provenance.

framebuffer_report fields 0..5 record creation and close; fields 6..12 record
MakeLinear result and each Begin pointer/stride and completed End call.
The unchanged creation-only fixture remains in ../libnx_framebuffer.
See ../../../docs/LIBNX_FRAMEBUFFER_PRESENTATION.md for scope and tests.
