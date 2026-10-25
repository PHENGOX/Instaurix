"""Generate independent MOVI byte-mask results using Unicorn 2.1.4."""
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent / "python-deps"))
import unicorn
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn.arm64_const import UC_ARM64_REG_Q0
if unicorn.__version__ != "2.1.4":
    raise SystemExit("Expected Unicorn 2.1.4")
output = bytearray(b"MOVI0001" + struct.pack("<I", 1024))
engine = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
engine.mem_map(0x10000, 4096)
for q in range(2):
    for immediate in range(256):
        for rd in (0, 31):
            instruction = 0x2F00E400 | (q << 30) | ((immediate >> 5) << 16) | ((immediate & 31) << 5) | rd
            engine.mem_write(0x10000, struct.pack("<I", instruction))
            engine.ctl_remove_cache(0x10000, 0x10004)
            engine.reg_write(UC_ARM64_REG_Q0 + rd, (1 << 128) - 1)
            engine.emu_start(0x10000, 0x10004, count=1)
            result = engine.reg_read(UC_ARM64_REG_Q0 + rd)
            output += struct.pack("<IQQ", instruction, result & ((1 << 64) - 1), result >> 64)
destination = Path(__file__).resolve().parents[1] / "tests/programs/movi_reference.bin"
destination.write_bytes(output)
print(f"{len(output)} bytes; 1024 independent cases")
