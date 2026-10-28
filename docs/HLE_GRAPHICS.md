# First development frame

Milestone dated October 25, 2026. An ARM64 C client reads a pattern through emu:fs, computes pixels in guest memory and presents them to local **emu:disp** through SM. The host captures/exports PNG. This validates a development framebuffer; guest GPU commands, shaders and Vulkan are absent.

| Element | Local contract |
| --- | --- |
| Service | 8-byte name emu:disp |
| Command | 0 Present |
| CMIF parameters | u32 width, u32 height |
| Input | One Normal Send descriptor with contiguous RGB8 pixels |
| Layout | Top-to-bottom rows, left-to-right pixels, R/G/B without alpha/padding |
| Reply | SFCO u64 presented byte count |

Dimensions are 1–256; width×height×3 must be at most 64 KiB and exactly match Send size. Receive buffers/extra parameters fail. Common validation checks readable RAM, bounds, mode and no MMIO.

Pixels are copied to a new immutable host object and published only after the full reply reaches RAM. Invalid input/unwritable replies preserve previous frame/count; later guest edits do not change captured pixels.

process().frame() returns the last image; frameCount() counts successful presentations. Frames survive session close/process exit. Successful loads reset both; failed loads preserve them. Existing session clone/close controls apply.

## PNG export

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_graphics\program.elf 1000000 --hle-reference --read-root .\tests\programs\hle_graphics\root --frame-out .\build\first-frame.png
```

--frame-out explicitly requests post-execution export, requires HLE and an actual presentation, and accepts no output path from the guest. An insufficient pre-presentation budget produces an export error, not a fabricated image.

The encoder writes RGB8 PNG with uncompressed DEFLATE blocks, CRC and Adler-32, without external graphics libraries. PNG export alone is not a window or realtime presentation loop.

## Demo and validation

sprite.txt contains 16 lines of 16 characters plus LF: **272 bytes**. Guest C validates it, draws a starfield and expands the pattern in 4×4 blocks at the center of a **160×96** image. Every pixel is computed by ARM64 code.

**614,588 instructions**; four u64 report fields at 0x12000: status=1, loaded=272, presented=46080, hash=4044961488. Frame count=1.

Tests compare every pixel against independent host calculations, the hash and 127-instruction slicing; also invalid dimensions/sizes, MMIO, failed replies, immutable captures, frame replacement and reload. Pillow independently decoded demo and two-DEFLATE-block PNG outputs.

[Animation](HLE_ANIMATION.md) followed, then [keyboard input](HLE_INPUT.md). Development graphics remain separate from native Switch 2 GPU decoding.
