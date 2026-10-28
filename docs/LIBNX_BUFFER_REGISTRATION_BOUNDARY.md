# Unchanged original buffer-registration client

Historical boundary: [bounded slot registration](LIBNX_BUFFER_SLOTS.md) now
implements the captured profile. The actual original transport is legacy A/B;
the earlier AutoSelect description below was incorrect. Both transports are
covered by the later implementation's tests. The later project harness also
releases its slots before closing the map.

The Windows x86_64 Arm GNU Toolchain 14.3.Rel1 compiler resolves the GCC-only
variable-length structure member in the pinned libnx buffer_producer.c. The
complete original source is now compiled unchanged for libnx_register; all
other fixture objects retain the existing Zig/Clang build. Earlier fixtures
still use their recorded source prefix and their binaries are unchanged.

Run tools/prepare_arm_gnu_reference.ps1, then tools/build_libnx_register.ps1
with the Python argument used by the other reference builds. Preparation
downloads a portable official Arm archive into ignored tools directories,
checks its pinned SHA-256 before extraction and checks the compiler version.
There is no system installation or PATH change. The archive URL, official
checksum URL and digest are recorded in tools/arm-gnu-reference.json.
Normal host builds and CI tests use the included fixture and need no GCC.

The project harness creates a 16x16 linear RGBA native buffer descriptor and
binds its 4 KiB CPU backing with the original nvMapCreate wrapper. It then
calls original nwindowConfigureBuffer, which invokes original
bqSetPreallocatedBuffer. This is not execution of framebufferCreate or a
native Switch 2 program.

The captured AutoSelect Binder transaction targets producer 0x1000, code 14,
slot 0. Its input parcel is 476 bytes and its destination capacity is 1024.
The flattened object is 364 bytes: 40 bytes of GraphicBuffer metadata followed
by 81 native-handle integers. The native NvGraphicBuffer is 336 bytes including
its 12-byte NativeHandle header. Tests check dimensions, stride, format,
magic values, export ID and serialized byte counts against the original call.

The emulator still returns UnknownCommand for registration. The reply parcel
stays unchanged, slots_configured remains zero, the availability event stays
unsignaled and CPU buffer bytes remain intact. Original cleanup releases the
map and all window resources. No graphic buffer is accepted or presented.
Normal execution is 121,155 instructions with 107 event-handle quota copies.

Next, implement bounded registration with complete parcel/descriptor checks,
Nvmap ID ownership, backing-size/offset validation and transactional slot
publication. Buffer ownership must retain or reject backing consistently when
the map handle, device, session or layer closes; returning success without
that lifetime model would be premature. Queue/dequeue and presentation remain
separate subsequent work.

## Validation

All 82 CTest suites pass on Windows in Debug (55.12 s) and Release (11.65 s).
The changed host test was rebuilt in both configurations; the emulator core
is unchanged by this compiler/reference milestone.
Strict second reconstruction reproduces the new 487,424-byte NRO SHA-256
6B1CE49B9AE4A6CA3238C5125792912369AC4A3E0D223FAABD6A9513725DEB36.
All 18 recorded NRO hashes match their manifests. Earlier fixture binaries
remain unchanged. The English README variants match. Linux execution and
non-Windows reference rebuilding were not performed for this milestone.
