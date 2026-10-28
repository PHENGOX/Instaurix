# Animated C client

Project-owned derivative of hle_graphics, compiled with Clang 18.1.6/Zig 0.13.0 for freestanding ARM64 O1. Rebuild from project root: `./tools/build_hle_animation.ps1`. Adds SleepThread and reuses ../hle_graphics/root.

It reads/checks sprite.txt, draws eight 160×96 frames, moves the pattern horizontally and changes stars. Between frames SleepThread waits 100,000,000 virtual ns; no sleep follows frame eight.

Success: **4,882,772 instructions, 8 frames, 700,000,000 virtual ns**. Four u64 fields at 0x12000: status=1, loaded=272, presented=8 and final-frame hash.

The host reference checks every pixel, timestamp and cumulative instruction count. See [viewer/sequence options](../../../docs/HLE_ANIMATION.md). This is development display without native GPU commands.

## Recorded artifact SHA-256

`AEA26DC51A3A52CF95132016692ED5E74BED3B518BE993E04EC81898AECFC5C8`

