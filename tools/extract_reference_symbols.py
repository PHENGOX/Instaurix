"""Record selected symbols of our static ELF so tests can call original libc.

These are reference-build test entry points, not an emulator symbol resolver.
"""
import argparse
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('elf', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--fsdev', action='store_true')
parser.add_argument('--posix', action='store_true')
parser.add_argument('--vi', action='store_true')
parser.add_argument('--queue', action='store_true')
parser.add_argument('--window', action='store_true')
parser.add_argument('--nv', action='store_true')
parser.add_argument('--nv-cycle', action='store_true')
parser.add_argument('--fence', action='store_true')
parser.add_argument('--gpu-as', action='store_true')
parser.add_argument('--register', action='store_true')
parser.add_argument('--dequeue', action='store_true')
parser.add_argument('--present', action='store_true')
parser.add_argument('--framebuffer', action='store_true')
args = parser.parse_args()
data = args.elf.read_bytes()
if data[:6] != b'\x7fELF\x02\x01' or len(data) < 64:
    raise SystemExit('Expected ELF64 little endian')
offset = struct.unpack_from('<Q', data, 40)[0]
stride, count = struct.unpack_from('<HH', data, 58)
if stride != 64 or offset + stride * count > len(data):
    raise SystemExit('Invalid section table')
sections = [struct.unpack_from('<IIQQQQIIQQ', data, offset + i * stride) for i in range(count)]
selected = {'setenv', 'getenv', 'unsetenv', 'malloc', 'free', 'realloc', '_sbrk_r',
            'environ', 'fake_heap_start', 'fake_heap_end', 'report', '_impure_ptr',
            '__env_recursive_mutex', '__malloc_recursive_mutex', 'g_fsSessionMgr', '__tls_start', '__tls_end', '__tdata_lma', '__tdata_lma_end',
            'referenceTlsReadSeed', 'referenceTlsReadZero', 'referenceTlsWrite', 'fsOpenSdCardFileSystem'}
if args.fsdev:
    selected.update({'fsFsOpenFile', 'fsFileRead', 'fsFileGetSize', 'fsFileClose', 'main', 'FindDevice', 'GetDeviceOpTab', 'fsdevGetDeviceFileSystem', 'fsdevMountSdmc', 'fsdevUnmountDevice'})
if args.posix:
    selected.update({'posix_report'})
if args.vi:
    selected.update({'vi_report'})
if args.queue:
    selected.update({'queue_report'})
if args.window:
    selected.update({'window_report'})
if args.nv:
    selected.update({'nv_report'})
if args.nv_cycle:
    selected.update({'map_report'})
if args.fence:
    selected.update({'fence_report'})
if args.gpu_as:
    selected.update({'gpu_as_report'})
if args.register:
    selected.update({'registration_report'})
if args.dequeue:
    selected.update({'dequeue_report'})
if args.present:
    selected.update({'present_report'})
if args.framebuffer:
    selected.update({'framebuffer_report'})
found = {}
for section in sections:
    if section[1] != 2:
        continue
    if section[9] != 24 or section[5] % 24 or section[4] + section[5] > len(data) or section[6] >= count:
        raise SystemExit('Invalid symbol table')
    strings = sections[section[6]]
    if strings[4] + strings[5] > len(data):
        raise SystemExit('Invalid string table')
    names = data[strings[4]:strings[4] + strings[5]]
    for at in range(section[4], section[4] + section[5], 24):
        name, info, other, index, value, size = struct.unpack_from('<IBBHQQ', data, at)
        if not index or name >= len(names):
            continue
        end = names.find(b'\0', name)
        if end < 0:
            raise SystemExit('Unterminated symbol name')
        symbol = names[name:end].decode('utf-8')
        if symbol in selected:
            found[symbol] = value
missing = selected - found.keys()
if missing:
    raise SystemExit('Missing reference symbols: ' + ', '.join(sorted(missing)))
args.output.write_text(''.join(f'{name} 0x{value:x}\n' for name, value in sorted(found.items())), encoding='utf-8')
print(f'Reference symbols: {len(found)}')
