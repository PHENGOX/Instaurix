# Interactive C client

Project-owned derivative of hle_animation, Clang 18.1.6/Zig 0.13.0 freestanding ARM64 O1. Rebuild: `./tools/build_hle_interactive.ps1`. Entry/SleepThread reuse hle_animation/start.S; resources come from hle_graphics/root.

Opens emu:fs/emu:disp/emu:pad through SM, loads a sprite and reads buttons before each frame. Movement uses four-pixel steps, cancels opposite directions and clamps the sprite to the image. Escape exits before presentation; without input the initial position remains.

Seven u64 fields at 0x12000: status,loaded,presented,hash,x,y,max_frames, initially **99,0,0,0,48,16,60**. Tests change max_frames; instruction count/hash depend on input. See [launch/limits](../../../docs/HLE_INPUT.md).

## Recorded artifact SHA-256

`69949E93E913C07DEF4AEFA3937127983FA9EACEAFD99E00C6B5907A84E5120B`

