# Original libnx bounded buffer registration

The project harness creates a 16x16 linear RGBA NvGraphicBuffer with 4 KiB
CPU backing through original nvMapCreate, then calls original
nwindowConfigureBuffer. The complete unchanged buffer_producer.c is compiled
with pinned Arm GNU GCC 14.3.1. The original client uses legacy A/B Binder
transport, not AutoSelect; host replays additionally cover AutoSelect.

Registration succeeds and sets slot 0. Availability now signals that the registered slot is free. Original nwindowReleaseBuffers disconnects before
nvMapClose releases the backing; a second Free verifies it is gone.
This cleanup order is a change to the project harness, not the SDK source.
No rendered frame or native Switch 2 compatibility is demonstrated.

Normal execution is 121,614 instructions. The parcel remains 476 bytes,
including a 364-byte flattened object and 81 native integers.
Report fields 13..15 record ReleaseBuffers result, slots after release and
duplicate Free result. Other fields are listed in reference.json.

Run tools/prepare_arm_gnu_reference.ps1, then tools/build_libnx_register.ps1
with Python. Normal host tests use the included NRO and require no toolchain
download. See docs/LIBNX_BUFFER_SLOTS.md for the supported local profile.