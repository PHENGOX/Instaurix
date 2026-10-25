"""Package our fixed-address static ELF fixture as an uncompressed NRO0.

Not a replacement for elf2nro: no relocations are generated and appended assets
are unsupported. --libnx-crt0 preserves existing MOD0 and HOMEBREW header bytes.
"""
import argparse
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("elf", type=Path)
parser.add_argument("nro", type=Path)
parser.add_argument("--libnx-crt0", action="store_true", help="Preserve the pinned upstream crt0 MOD0 pointer and HOMEBREW marker")
args = parser.parse_args()
data = args.elf.read_bytes()
if data[:6] != b"\x7fELF\x02\x01":
    raise SystemExit("Expected ELF64 little-endian")
u16 = lambda at: struct.unpack_from("<H", data, at)[0]
u64 = lambda at: struct.unpack_from("<Q", data, at)[0]
if u16(16) != 2 or u16(18) != 183 or u64(24) != 0x10000:
    raise SystemExit("Expected our ET_EXEC AArch64 fixture at 0x10000")
segments = {}
for index in range(u16(56)):
    at = u64(32) + index * u16(54)
    kind, flags = struct.unpack_from("<II", data, at)
    if kind != 1:
        continue
    offset, address, _, file_size, memory_size, _ = struct.unpack_from("<6Q", data, at+8)
    if flags not in (5, 4, 6) or flags in segments or address < 0x10000 or file_size > memory_size:
        raise SystemExit("Unsupported fixture segment")
    if offset > len(data) or file_size > len(data)-offset or address % 4096:
        raise SystemExit("Invalid fixture segment range")
    segments[flags] = (address-0x10000, data[offset:offset+file_size], memory_size)
if set(segments) != {5, 4, 6}:
    raise SystemExit("Expected text/rodata/data")
align = lambda value: (value+4095) & ~4095
parts = [(offset, payload, align(len(payload)), size) for offset, payload, size in (segments[5], segments[4], segments[6])]
if parts[0][0] or any(parts[i][0]+parts[i][2] > parts[i+1][0] for i in range(2)):
    raise SystemExit("Overlapping NRO fixture segments")
size = parts[2][0] + parts[2][2]
output = bytearray(size)
for offset, payload, _, _ in parts:
    output[offset:offset+len(payload)] = payload
if args.libnx_crt0:
    module = struct.unpack_from("<I", output, 4)[0]
    if output[8:16] != b"HOMEBREW" or output[16:0x80] != bytes(0x70) or module < 0x80 or output[module:module+4] != b"MOD0":
        raise SystemExit("Expected the upstream libnx crt0 header and MOD0")
elif output[4:0x80] != bytes(0x7c):
    raise SystemExit("Fixture must reserve a zeroed NRO header with no MOD0")
output[0x10:0x14] = b"NRO0"
struct.pack_into("<I", output, 0x18, size)
for index, (offset, _, padded, _) in enumerate(parts):
    struct.pack_into("<II", output, 0x20+index*8, offset, padded)
bss = max(0, align(parts[2][3])-parts[2][2])
struct.pack_into("<I", output, 0x38, bss)
args.nro.write_bytes(output)
print(f"Static NRO fixture: {size} bytes, BSS={bss}, base=0x10000")
