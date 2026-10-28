# Padded and multiple-block framebuffer geometry

Follow-up: [one, two and three buffers](LIBNX_FRAMEBUFFER_BUFFERS.md) adds local fair reuse. The same original binaries now execute 152895 instructions (16x16) and 378310 (37x137); measurements below describe their earlier reuse policy.

The bounded CPU framebuffer profile now accepts block-linear RGBA dimensions
from 1 through 256 in each axis, with the existing fixed block-height log2 4,
kind 0xfe and two surfaces per Nvmap object. The previous 16x16 linear profile
is unchanged. This range is a local policy, not a native hardware limit or a
claim of exhaustive compatibility for every size within it.

Validation rejects zero or excessive dimensions before calculating sizes.
Pitch is align_up(width*4,64), padded height is align_up(height,128), and
surface size is pitch*padded_height. Only offsets zero and surface_size are
accepted in an allocated object exactly twice that size. Width, height,
stride, pitch, plane extent and flattened metadata must agree. Unused planes
and padding remain zero. A layer cannot mix dimensions or linear/block-linear
layouts. Dequeue dimensions must match its registered surfaces; Queue replies
report the retained visible dimensions. Unsupported formats/heights remain
explicit errors. The maximum permitted object is 512 KiB.

## Independent original-client validation

The new libnx_framebuffer_wide fixture preserves the existing 16x16 fixture
and uses unchanged pinned framebuffer.c, cache.s and the full producer. Its
project harness requests 37x137 RGBA, draws two patterns through original
MakeLinear/Begin/End and closes normally. This requires a 192-byte pitch,
256 padded rows, 49152 bytes per surface and a 98304-byte map. Width spans
three 64-byte blocks; height crosses the 128-row block boundary and ends
partway through a GOB. All 5069 visible pixels of each snapshot are compared
against the original linear colors. Padding never becomes visible pixels.
Normal and seven-instruction slices both execute 376091 original instructions.

The generalized GOB address includes vertical block rows and horizontal blocks:

```
(y/128)*(pitch/64)*8192 + (byte_x/64)*8192 + ((y%128)/8)*512
+ ((byte_x%64)/32)*256 + ((y%8)/2)*64
+ ((byte_x%32)/16)*32 + (y%2)*16 + byte_x%16
```

Original SDK software produces the GOB bytes independently of this address
calculation. Host replays separately acquire the second slot while slot zero
is owned, fill its extent with 42, then check every queued pixel. The original
guest produces two frames; this extra replay is an isolated host test action.
The tests also cover protected replies, unreadable backing, duplicate Queue,
invalid dimensions and mixing width 38 with an existing width-37 surface even
though both would have the same padded pitch and extent. Original close and
the 107-handle event quota remain intact.

## Reproduction and scope

Rebuild with tools/build_libnx_framebuffer_wide.ps1 and the pinned toolchain.
The manifest and adjacent licenses record provenance. Other fixture binaries
are unchanged. The shared host test accepts --wide and an optional PNG path:

```powershell
.\build\Debug\libnx_framebuffer_tests.exe .\tests\programs\libnx_framebuffer_wide\program.nro .\tests\programs\libnx_framebuffer_wide\symbols.txt --wide .\build\framebuffer-wide.png
```

The exported 37x137 snapshot was visually inspected as well as pixel-checked.
The consumer remains synchronous CPU software, with empty fences and immutable
RGB snapshots. No GPU command submission, shaders, hardware synchronization,
native Switch 2 program or commercial game has been validated. Larger display
resolutions require further memory/format/client validation.

## Validation results

All 88 CTest suites pass on Windows in Debug (59.32 s) and Release (12.83 s).
Logs: build/framebuffer-wide-ctest-debug.log and
build/framebuffer-wide-ctest-release.log. Linux was not run locally.
Strict reconstruction reproduces the 499712-byte new NRO SHA-256:
F694317F153746DA2D7300144DDA9DFD90C9381F22323B99298E58D509A3A6EB.
All 23 recorded NRO hashes match; the earlier 22 binaries are unchanged.
The new harness and symbol hashes match their manifest; English README variants
are identical. The exported build/framebuffer-wide.png snapshot was inspected.
