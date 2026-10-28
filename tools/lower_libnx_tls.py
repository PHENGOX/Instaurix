"""Adapt Clang's direct AArch64 TLS reads to libnx's software thread pointer.

Only reference builds use this pass. The pinned upstream readtp.s changes x0
only; preserve x0/LR around it so this replacement has the MRS register effects.
Reject unexpected destinations instead of guessing a calling convention.
"""
import argparse
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('assembly', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
source = args.assembly.read_text()
pattern = re.compile(r'^\s*mrs\s+(x\d+),\s*TPIDR_EL0\s*(?://[^\n]*)?$', re.M)

def lower(match):
    register = match[1]
    if register not in {f'x{i}' for i in range(30)}:
        raise SystemExit('Unsupported TLS destination: ' + register)
    if register == 'x0':
        return '\tstr x30, [sp, #-16]!\n\tbl __aarch64_read_tp\n\tldr x30, [sp], #16'
    return ('\tstp x0, x30, [sp, #-16]!\n\tbl __aarch64_read_tp\n'
            f'\tmov {register}, x0\n\tldp x0, x30, [sp], #16')

output, count = pattern.subn(lower, source)
if not count:
    raise SystemExit('Expected at least one direct TLS read')
args.output.write_text(output)
print(f'libnx TLS accesses lowered: {count}')
