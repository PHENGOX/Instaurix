"""Assemble separately with Clang; execute with Unicorn, never with our CPU."""
import argparse, hashlib, json, pathlib, random, struct, sys
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/programs/cpu_fp'
MASK = (1 << 64)-1
V = []
CUSTOM_SETUP = None
def add(asm, kind='bits', wide=False, **extra): V.append(dict(asm=asm, kind=kind, wide=wide, **extra))
for reg in ['s','d']:
    wide = reg == 'd'
    for op in ['fadd','fsub','fmul','fdiv']:
        add(f'{op} {reg}0, {reg}1, {reg}2','arithmetic',wide)
        add(f'{op} {reg}1, {reg}1, {reg}2','arithmetic',wide)
    add(f'fsqrt {reg}0, {reg}1','sqrt',wide)
    for op in ['fmov','fabs','fneg']: add(f'{op} {reg}0, {reg}1','scalarbits',wide)
    for imm in ['1.0','-2.0','0.125','31.0']: add(f'fmov {reg}0, #{imm}')
    for op in ['fcmp','fcmpe']:
        add(f'{op} {reg}1, {reg}2','compare',wide)
        add(f'{op} {reg}1, #0.0','compare',wide)
    for condition in ['eq','ne','gt','le','mi','pl','vs','vc','al','nv']:
        add(f'fcsel {reg}0, {reg}1, {reg}2, {condition}','scalarbits',wide)
for asm in ['fmov w0, s1','fmov s0, w1','fmov x0, d1','fmov d0, x1',
            'fmov wzr, s1','fmov s31, wzr','fmov xzr, d1','fmov d31, xzr',
            'mrs x0, fpcr','mrs x0, fpsr','msr fpcr, x0','msr fpsr, x0',
            'msr fpcr, xzr','msr fpsr, xzr']:
    add(asm,'system' if 'msr' in asm or 'mrs' in asm else 'bits')
for shape in ['8b','16b','4h','8h','2s','4s','2d']:
    for op in ['add','sub']:
        add(f'{op} v0.{shape}, v1.{shape}, v2.{shape}')
        add(f'{op} v1.{shape}, v1.{shape}, v2.{shape}')
for shape in ['8b','16b']:
    for op in ['and','bic','orr','orn','eor','bsl','bit','bif']:
        add(f'{op} v0.{shape}, v1.{shape}, v2.{shape}')
for shape in ['8b','16b','4h','8h','2s','4s','2d']:
    add(f'dup v0.{shape}, {"x1" if shape == "2d" else "w1"}')
for shape, lane in [('b',15),('h',7),('s',3),('d',1)]:
    add(f'umov {"x0" if shape == "d" else "w0"}, v1.{shape}[{lane}]')
    add(f'ins v0.{shape}[{lane}], {"x1" if shape == "d" else "w1"}')
for reg,width in [('b',1),('h',2),('s',4),('d',8),('q',16)]:
    for op in ['ldr','str']:
        add(f'{op} {reg}0, [x1, #{2*width}]','memory')
        add(f'{op} {reg}0, [x1, #-16]!','memory')
        add(f'{op} {reg}0, [x1], #16','memory')
        add(f'{op} {reg}0, [x1, x2]','memory')
        add(f'{op} {reg}0, [x1, w2, sxtw #{width.bit_length()-1}]','memory')
        add(f'{op} {reg}0, [sp, #{width}]','memory')
        add(f'{op[:-1]}ur {reg}0, [x1, #-7]','memory')
for reg,width in [('s',4),('d',8),('q',16)]:
    for op in ['ldp','stp']:
        add(f'{op} {reg}0, {reg}2, [x1, #{width*2}]','memory')
        add(f'{op} {reg}0, {reg}2, [x1, #-32]!','memory')
        add(f'{op} {reg}0, {reg}2, [x1], #32','memory')
    # Literal displacement 8 is assembled without an ELF relocation.
    add(f'ldr {reg}0, .+8','literal')
add('ldr q1, [x1], #16','memory') # register banks may share an index
for reg in ['s','d']:
    wide = reg == 'd'
    for gp in ['w','x']:
        for op in ['scvtf','ucvtf']:
            add(f'{op} {reg}0, {gp}1','itof',wide)
        for op in ['fcvtns','fcvtnu','fcvtas','fcvtau','fcvtps','fcvtpu','fcvtms','fcvtmu','fcvtzs','fcvtzu']:
            add(f'{op} {gp}0, {reg}1','ftoi',wide)
    for op in ['fmadd','fmsub','fnmadd','fnmsub']:
        for dst in [0,1,3]: add(f'{op} {reg}{dst}, {reg}1, {reg}2, {reg}3','fused',wide)
add('fcvt d0, s1','precision',False)
add('fcvt s0, d1','precision',True)
add('ucvtf d31, xzr','itof',True)
add('fcvtzs xzr, d31','ftoi-zr',True)
for shape in ['8b','16b','4h','8h','2s','4s','2d']:
    ops=['cmeq','cmgt','cmge','cmhi','cmhs','cmtst','addp']
    if shape!='2d': ops += ['mul','mla','mls','smax','smin','umax','umin']
    for op in ops:
        for dst in [0,1]: add(f'{op} v{dst}.{shape}, v1.{shape}, v2.{shape}','simd-integer')
    for op in ['zip1','zip2','uzp1','uzp2','trn1','trn2']:
        for dst in [0,1]: add(f'{op} v{dst}.{shape}, v1.{shape}, v2.{shape}','simd-integer')
    bits={'b':8,'h':16,'s':32,'d':64}[shape[-1]]
    for op, amounts in [('shl',[0,1,bits-1]),('sshr',[1,bits]),('ushr',[1,bits])]:
        for amount in amounts: add(f'{op} v0.{shape}, v1.{shape}, #{amount}','simd-integer')
for shape,scalar in [('8b','b'),('16b','b'),('4h','h'),('8h','h'),('4s','s')]:
    for op in ['addv','smaxv','sminv','umaxv','uminv']:
        add(f'{op} {scalar}0, v1.{shape}','simd-integer')
for shape,block in [('8b',16),('16b',16),('8b',32),('16b',32),('4h',32),('8h',32),
                    ('8b',64),('16b',64),('4h',64),('8h',64),('2s',64),('4s',64)]:
    add(f'rev{block} v0.{shape}, v1.{shape}','simd-integer')
for shape in ['8b','16b']:
    for op in ['cnt','not']: add(f'{op} v0.{shape}, v1.{shape}','simd-integer')
    for offset in ([0,1,7] if shape=='8b' else [0,1,7,15]):
        for dst in [0,1]: add(f'ext v{dst}.{shape}, v1.{shape}, v2.{shape}, #{offset}','simd-integer')

for shape in ['2s','4s','2d']:
    for op in ['fadd','fsub','fmul','fdiv','fmla','fmls']:
        for dst in [0,1,2]: add(f'{op} v{dst}.{shape}, v1.{shape}, v2.{shape}','vector-fp',shape=='2d')
    for op in ['fsqrt','fabs','fneg']:
        for dst in [0,1]: add(f'{op} v{dst}.{shape}, v1.{shape}','vector-fp',shape=='2d')

for shape in ['2s','4s','2d']:
    for op in ['scvtf','ucvtf','fcvtns','fcvtnu','fcvtas','fcvtau','fcvtps','fcvtpu','fcvtms','fcvtmu','fcvtzs','fcvtzu']:
        for dst in [0,1]: add(f'{op} v{dst}.{shape}, v1.{shape}','ext-itof' if op in ['scvtf','ucvtf'] else 'ext-ftoi',shape=='2d')
    for op in ['frintn','frinta','frintp','frintm','frintz','frinti','frintx']:
        for dst in [0,1]: add(f'{op} v{dst}.{shape}, v1.{shape}','ext-round',shape=='2d')
    for op in ['fmax','fmin','fmaxnm','fminnm','faddp','fmaxp','fminp','fmaxnmp','fminnmp']:
        for dst in [0,1]: add(f'{op} v{dst}.{shape}, v1.{shape}, v2.{shape}','ext-pair' if op.endswith('p') else 'ext-minmax',shape=='2d')
for reg,shape in [('s','2s'),('d','2d')]:
    for op in ['faddp','fmaxp','fminp','fmaxnmp','fminnmp']:
        for dst in [0,1]: add(f'{op} {reg}{dst}, v1.{shape}','ext-pair',reg=='d')
    for op in ['fmax','fmin','fmaxnm','fminnm']:
        for dst in [0,1]: add(f'{op} {reg}{dst}, {reg}1, {reg}2','ext-minmax',reg=='d')
    for op in ['frintn','frinta','frintp','frintm','frintz','frinti','frintx']:
        for dst in [0,1]: add(f'{op} {reg}{dst}, {reg}1','ext-round',reg=='d')
for op in ['fmaxv','fminv','fmaxnmv','fminnmv']:
    for dst in [0,1]: add(f'{op} s{dst}, v1.4s','ext-reduce')

for shape in ['8b','16b','4h','8h','2s','4s','1d','2d']:
    registerBytes=8 if shape in ['8b','4h','2s','1d'] else 16
    for structures in range(1,5):
        if structures>1 and shape=='1d': continue
        for count in (range(1,5) if structures==1 else [structures]):
            registers=', '.join(f'v{i}.{shape}' for i in range(count))
            for op in [f'ld{structures}',f'st{structures}']:
                for address in ['[x1]',f'[x1], #{count*registerBytes}','[x1], x2']:
                    add(f'{op} {{ {registers} }}, {address}','memory')
for count in range(1,5):
    registers=', '.join(f'v{(31+i)%32}.16b' for i in range(count))
    for op in ['ld1','st1']:
        add(f'{op} {{ {registers} }}, [sp], #{count*16}','memory')
for structures in range(2,5):
    registers=', '.join(f'v{(31+i)%32}.8h' for i in range(structures))
    for op in [f'ld{structures}',f'st{structures}']:
        add(f'{op} {{ {registers} }}, [x1], x1','memory')
for shape in ['8b','16b']:
    for op in ['tbl','tbx']:
        for count in range(1,5):
            for start in [1,31]:
                registers=', '.join(f'v{(start+i)%32}.16b' for i in range(count))
                for dst in [0,2,start]:
                    add(f'{op} v{dst}.{shape}, {{ {registers} }}, v2.{shape}','simd-table')
for shape,laneType,last in [('8b','b',15),('16b','b',15),('4h','h',7),('8h','h',7),('2s','s',3),('4s','s',3),('2d','d',1)]:
    for index in [0,last]:
        for dst in [0,1]: add(f'dup v{dst}.{shape}, v1.{laneType}[{index}]','simd-integer')
for laneType,last in [('b',15),('h',7),('s',3),('d',1)]:
    for source in [0,last]:
        for target in [0,last]:
            for dst in [0,1]: add(f'ins v{dst}.{laneType}[{target}], v1.{laneType}[{source}]','simd-integer')
for gp in ['w','x']:
    for laneType,last in [('b',15),('h',7),('s',3)]:
        if gp=='w' and laneType=='s': continue
        for index in [0,last]:
            for dst in [f'{gp}0',f'{gp}zr']: add(f'smov {dst}, v1.{laneType}[{index}]','simd-integer')

def state(uc, regs, qregs):
    values = [uc.reg_read(r) for r in regs]
    from unicorn.arm64_const import UC_ARM64_REG_SP, UC_ARM64_REG_NZCV, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR
    data = struct.pack('<31QQQII',*values,uc.reg_read(UC_ARM64_REG_SP),uc.reg_read(UC_ARM64_REG_NZCV),
                       uc.reg_read(UC_ARM64_REG_FPCR),uc.reg_read(UC_ARM64_REG_FPSR))
    return data + b''.join(uc.reg_read(r).to_bytes(16,'little') for r in qregs)

def iterationsFor(v):
    if 'samples' in v: return v['samples']
    kind=v['kind']
    return 160 if kind.startswith('ext-') else 256 if kind=='fused' else 192 if kind in ['itof','ftoi','ftoi-zr','precision','vector-fp'] else 80 if kind in ['arithmetic','sqrt','compare'] else 32 if kind in ['simd-integer','simd-table'] else 16

def layout():
    # Describe block boundaries independently of the mutable input-generation loops.
    code=(OUT/'vectors.bin').read_bytes()
    if len(code)!=len(V)*4: raise RuntimeError('Layout instruction count mismatch')
    rows=[f'{word[0]:08x} {iterationsFor(v)}' for word,v in zip(struct.iter_unpack('<I',code),V)]
    header=f'FP_LAYOUT_1 {len(V)} {sum(iterationsFor(v) for v in V)}\n'
    (OUT/'reference-layout.txt').write_text(header+'\n'.join(rows)+'\n')

def oracle():
    sys.path.insert(0,str(ROOT/'tools/python-deps'))
    import unicorn
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
    from unicorn.arm64_const import (UC_ARM64_REG_X0,UC_ARM64_REG_X29,UC_ARM64_REG_X30,
        UC_ARM64_REG_Q0,UC_ARM64_REG_SP,UC_ARM64_REG_NZCV,UC_ARM64_REG_FPCR,UC_ARM64_REG_FPSR,
        UC_ARM64_REG_CPACR_EL1)
    regs=[UC_ARM64_REG_X0+i for i in range(29)]+[UC_ARM64_REG_X29,UC_ARM64_REG_X30]
    qregs=[UC_ARM64_REG_Q0+i for i in range(32)]
    uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM); uc.mem_map(0,0x10000)
    uc.reg_write(UC_ARM64_REG_CPACR_EL1,3<<20)
    code=(OUT/'vectors.bin').read_bytes()
    if len(code) != len(V)*4: raise RuntimeError('Instruction count mismatch')
    layout()
    rng=random.Random(0xF0642026)
    count=0
    with (OUT/'reference.bin').open('wb') as out:
        out.write(b'FPREF001'+struct.pack('<I',0))
        for variantIndex,v in enumerate(V):
            instruction=code[variantIndex*4:variantIndex*4+4]
            kind=v['kind']; wide=v['wide']
            arithmetic=kind in ['arithmetic','sqrt']
            iterations=iterationsFor(v)
            for sample in range(iterations):
                for r in regs: uc.reg_write(r,rng.getrandbits(64))
                for r in qregs: uc.reg_write(r,rng.getrandbits(128))
                uc.reg_write(UC_ARM64_REG_SP,0x4100)
                uc.reg_write(UC_ARM64_REG_NZCV,(sample%16)<<28)
                uc.reg_write(UC_ARM64_REG_FPCR,((sample%4)<<22) | (((sample//4)%2)<<25))
                uc.reg_write(UC_ARM64_REG_FPSR,(sample%2)*0x08000012)
                if kind in ['arithmetic','sqrt','compare','scalarbits']:
                    def bits(x): return int.from_bytes(struct.pack('<d' if wide else '<f',x),'little')
                    exp=0x7FF0000000000000 if wide else 0x7F800000
                    quiet=1<< (51 if wide else 22)
                    sign=1<< (63 if wide else 31)
                    pairs=[(bits(1.5),bits(2.25)),(bits(-0.0),bits(0.0)),(bits(-4.0),bits(2.0)),
                           (bits(1.0),bits(3.0)),(exp,exp),(exp|sign,exp),
                           (bits(0.0),bits(0.0)),(bits(1.0),bits(0.0)),
                           (exp|quiet|123,bits(2.0)),(bits(1.0),exp|12),
                           (exp|quiet|3,exp|9),(exp|17,exp|quiet|20),
                           (bits(1.0),bits(2.0**(-53 if wide else -24))),
                           (exp-1,bits(2.0)),(bits(-1.0),bits(0.0)),(bits(4.0),bits(-2.0))]
                    # Two full passes of boundary pairs, then finite randomized values.
                    if sample<64: a,b=pairs[sample//4]
                    else:
                        a=bits(rng.uniform(-1000,1000)); b=bits(rng.uniform(0.5,1000))
                    if kind=='sqrt': b=0
                    if kind=='compare' and sample>=64:
                        a,b=rng.choice([(1,2),(sign|1,1),(exp-1,exp),(sign,0)])
                    uc.reg_write(qregs[1],(rng.getrandbits(64)<<64)|a)
                    uc.reg_write(qregs[2],(rng.getrandbits(64)<<64)|b)
                if kind=='system': uc.reg_write(regs[0],0x02C00000 if sample&1 else 0)
                if kind=='simd-integer' and sample<9:
                    patterns=[(0,0),(255,1),(128,127),(127,128),(255,255),(85,170),(0,255),(1,1)]
                    if sample<8:
                        av,bv=patterns[sample]
                        a=int.from_bytes(bytes([av])*16,'little'); b=int.from_bytes(bytes([bv])*16,'little')
                    else:
                        a=int.from_bytes(bytes(range(16)),'little'); b=int.from_bytes(bytes(range(31,15,-1)),'little')
                    uc.reg_write(qregs[1],a); uc.reg_write(qregs[2],b)
                if kind=='simd-table':
                    # Exercise every boundary, both upper halves, and out-of-range bytes.
                    for reg in range(32): uc.reg_write(qregs[reg],int.from_bytes(bytes((reg*17+i)&255 for i in range(16)),'little'))
                    edges=[0,7,8,15,16,31,32,47,48,63,64,127,128,255,1,62]
                    indices=bytes(edges[(i+sample)%16] if sample<16 else rng.randrange(256) for i in range(16))
                    uc.reg_write(qregs[2],int.from_bytes(indices,'little'))
                if kind in ['itof','ftoi','ftoi-zr','fused','precision']:
                    def bits(x): return int.from_bytes(struct.pack('<d' if wide else '<f',x),'little')
                    exp=0x7FF0000000000000 if wide else 0x7F800000
                    quiet=1<<(51 if wide else 22); sign=1<<(63 if wide else 31)
                    if kind=='itof':
                        edges=[0,1,MASK,MASK-1,1<<63,(1<<63)-1,(1<<32)-1,1<<31,(1<<31)-1,
                               (1<<24)-1,(1<<24)+1,(1<<24)+3,(1<<53)-1,(1<<53)+1,(1<<53)+3,
                               (1<<64)-(1<<40)-1]
                        value=edges[(sample//8)%len(edges)] if sample<128 else rng.getrandbits(64)
                        uc.reg_write(regs[1],value)
                    elif kind in ['ftoi','ftoi-zr']:
                        edges=[bits(x) for x in [0.0,-0.0,0.5,-0.5,1.5,-1.5,2.5,-2.5,0.1,-0.1,3.75,-3.75,
                            2.0**31,-2.0**31,2.0**32,-2.0**32,2.0**63,-2.0**63,2.0**64,-2.0**64]]
                        edges += [exp,exp|sign,exp|quiet|123,exp|1,1,sign|1,exp-1,(exp-1)|sign]
                        value=edges[(sample//4)%len(edges)] if sample<112 else bits(rng.uniform(-1e10,1e10))
                        if sample>=160:
                            # Adjacent representable values at each integer-width boundary.
                            boundary=[bits(2.0**31),bits(2.0**32),bits(2.0**63),bits(2.0**64)][(sample//8)%4]
                            value=(boundary + [-1,0,1][(sample//4)%3]) | (sign if sample&4 else 0)
                        uc.reg_write(qregs[31] if kind=='ftoi-zr' else qregs[1],(rng.getrandbits(64)<<64)|value)
                    elif kind=='precision':
                        edges=[bits(x) for x in [0.0,-0.0,1.0,-2.5,1.0+2.0**-24,1.0+2.0**-23,2.0**100,2.0**-100]]
                        edges += [exp,exp|sign,exp|quiet|123,exp|1,exp|sign|quiet|0x123456,exp|sign|17]
                        if not wide: edges += [1,0x7FFFFF,0x80000001]
                        value=edges[(sample//8)%len(edges)] if sample<136 else bits(rng.uniform(-1e20,1e20))
                        uc.reg_write(qregs[1],(rng.getrandbits(64)<<64)|value)
                    else:
                        # This first triple separates a fused result from rounded multiply+add.
                        delta=2.0**(-27 if wide else -13)
                        triples=[(bits(1+delta),bits(1-delta),bits(-1.0)),
                            (bits(1.5),bits(2.0),bits(4.0)),(bits(-1.5),bits(2.0),bits(4.0)),
                            (bits(-0.0),bits(1.0),bits(0.0)),(bits(0.0),bits(1.0),bits(-0.0)),
                            (exp,bits(0.0),bits(1.0)),(exp,bits(0.0),exp|quiet|17),
                            (exp,bits(0.0),exp|17),(exp,bits(1.0),exp|sign),
                            (exp|quiet|11,exp|quiet|12,exp|quiet|13),
                            (exp|17,exp|18,exp|19),(exp|quiet|1,exp|2,exp|quiet|3),
                            (exp|1,exp|quiet|2,exp|quiet|3),
                            (exp-1,bits(2.0),(exp-1)|sign),
                            (bits(1.0),bits(2.0**(-53 if wide else -24)),bits(1.0)),
                            (bits(-4.0),bits(2.0),bits(8.0)),
                            (exp|sign|quiet|5,bits(2.0),bits(3.0)),
                            (bits(1.0),exp|sign|quiet|6,bits(3.0)),
                            (bits(1.0),bits(2.0),exp|sign|quiet|7),
                            (exp|sign|1,exp|2,exp|quiet|3),
                            (exp|1,exp|sign|2,exp|3),
                            (exp|quiet|1,exp|sign|quiet|2,exp|sign|3),
                            (bits(-0.0),bits(-0.0),bits(-0.0)),
                            (exp|sign,bits(-0.0),exp|sign|quiet|8)]
                        if sample<192: a,b,c=triples[(sample//8)%len(triples)]
                        else: a,b,c=(bits(rng.uniform(-1000,1000)) for _ in range(3))
                        for idx,value in [(1,a),(2,b),(3,c)]:
                            uc.reg_write(qregs[idx],(rng.getrandbits(64)<<64)|value)
                if kind=='vector-fp':
                    def bits(x): return int.from_bytes(struct.pack('<d' if wide else '<f',x),'little')
                    exponent=0x7FF0000000000000 if wide else 0x7F800000
                    quiet=1<<(51 if wide else 22); sign=1<<(63 if wide else 31)
                    delta=2.0**(-27 if wide else -13)
                    triples=[(bits(1+delta),bits(1-delta),bits(-1.0)),
                        (bits(1.5),bits(2.25),bits(4.0)),(bits(-4.0),bits(2.0),bits(8.0)),
                        (bits(0.0),bits(-0.0),bits(0.0)),(bits(-0.0),bits(1.0),bits(-0.0)),
                        (exponent,bits(0.0),exponent|quiet|17),
                        (exponent,bits(1.0),exponent|sign),
                        (exponent|quiet|11,exponent|quiet|12,exponent|quiet|13),
                        (exponent|17,exponent|18,exponent|19),
                        (exponent|quiet|1,exponent|2,exponent|quiet|3),
                        (exponent|1,exponent|quiet|2,exponent|quiet|3),
                        (exponent-1,bits(2.0),(exponent-1)|sign),
                        (bits(1.0),bits(2.0**(-53 if wide else -24)),bits(1.0)),
                        (bits(1.0),bits(3.0),bits(-1.0)),
                        (exponent|sign|quiet|5,bits(2.0),exponent|sign|7),
                        (bits(0.0),exponent,exponent|quiet|1)]
                    width=64 if wide else 32
                    packed=[0,0,0]
                    for lane in range(128//width):
                        a,b,c=triples[(sample//8+lane*3)%len(triples)] if sample<128 else (bits(rng.uniform(-1000,1000)) for _ in range(3))
                        for i,value in enumerate([c,a,b]): packed[i] |= value<<(lane*width)
                    for i,value in enumerate(packed): uc.reg_write(qregs[i],value)
                if kind.startswith('ext-'):
                    def bits(x): return int.from_bytes(struct.pack('<d' if wide else '<f',x),'little')
                    exponent=0x7FF0000000000000 if wide else 0x7F800000
                    quiet=1<<(51 if wide else 22); sign=1<<(63 if wide else 31)
                    width=64 if wide else 32; mask=(1<<width)-1
                    packed=[0,0]
                    integerEdges=[0,1,mask,mask-1,sign,sign-1,2**24+1,2**24+3,2**32-1,
                                  2**31,2**53+1,2**53+3,2**63-1,2**63,2**64-1,17]
                    floatEdges=[bits(x) for x in [0.0,-0.0,0.5,-0.5,1.5,-1.5,2.5,-2.5,0.1,-0.1,
                        2.0**31,-2.0**31,2.0**32,2.0**63,-2.0**63,2.0**64]]
                    floatEdges += [exponent|quiet|3,exponent|sign|7,exponent,exponent|sign,1,sign|1,
                        bits(2.0**31)-1,bits(2.0**63)-1,bits(2.0**64)-1,exponent-1]
                    pairs=[(bits(1.5),bits(2.5)),(bits(-4),bits(2)),(0,sign),(sign,0),
                        (exponent,exponent|sign),(exponent|sign,exponent),
                        (exponent|quiet|1,bits(2)),(bits(1),exponent|quiet|2),
                        (exponent|quiet|1,exponent|quiet|2),(exponent|3,exponent|quiet|4),
                        (exponent|quiet|5,exponent|6),(exponent|7,exponent|8),
                        (exponent|sign|quiet|11,exponent|sign|12),(bits(-1.5),bits(-2.5)),
                        (1,sign|1),(exponent-1,bits(1)),(sign|1,0),(1,0),
                        (exponent|quiet|1,1),(exponent|2,sign|1)]
                    if kind=='ext-pair' and v['asm'].startswith('faddp'):
                        pairs=[pair for pair in pairs if all((value&exponent)!=0 or (value&(sign-1))==0 for value in pair)]
                    for lane in range(128//width):
                        edgeIndex=sample//8+lane*5
                        if kind=='ext-itof':
                            a=integerEdges[edgeIndex%len(integerEdges)] if sample<128 else rng.getrandbits(width)
                            b=rng.getrandbits(width)
                        elif kind in ['ext-ftoi','ext-round']:
                            # Four cases per boundary exercise every FPCR rounding mode.
                            a=floatEdges[(sample//4+lane*5)%len(floatEdges)] if sample<128 else bits(rng.uniform(-1e10,1e10))
                            b=bits(1.0)
                        else: a,b=pairs[edgeIndex%len(pairs)]
                        packed[0] |= (a&mask)<<(lane*width); packed[1] |= (b&mask)<<(lane*width)
                    uc.reg_write(qregs[1],packed[0]); uc.reg_write(qregs[2],packed[1])
                if CUSTOM_SETUP is not None: CUSTOM_SETUP(uc,regs,qregs,v,sample,rng)
                data=bytes((i*37+sample*13)&255 for i in range(512))
                uc.mem_write(0x4000,data)
                if kind=='memory':
                    uc.reg_write(regs[1],0x4100)
                    uc.reg_write(regs[2],MASK-7 if 'sxtw' in v['asm'] and sample&1 else 8)
                uc.mem_write(0x1000,instruction+bytes.fromhex('1f2003d5')+data[:32])
                uc.ctl_remove_cache(0x1000,0x1040)
                before=state(uc,regs,qregs)
                try: uc.emu_start(0x1000,0x1004,count=1)
                except Exception as e: raise RuntimeError(f'{variantIndex}: {v}, sample {sample}') from e
                out.write(instruction+struct.pack('<I',sample)+before+state(uc,regs,qregs)+bytes(uc.mem_read(0x4000,512)))
                # For literal loads the fixed code-adjacent bytes are reconstructible.
                count+=1
        out.seek(8); out.write(struct.pack('<I',count))
    manifest=dict(assembler='Clang 18.1.6 via Zig 0.13.0',oracle='Unicorn '+unicorn.__version__,
                  seed='0xF0642026',variants=len(V),cases=count,
                  sha256=hashlib.sha256((OUT/'reference.bin').read_bytes()).hexdigest(),instructions=V)
    (OUT/'reference.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{len(V)} variants, {count} independent cases')

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--oracle',action='store_true'); parser.add_argument('--layout',action='store_true'); args=parser.parse_args()
    OUT.mkdir(exist_ok=True,parents=True)
    if args.layout: layout()
    elif args.oracle: oracle()
    else: (OUT/'vectors.S').write_text('.text\n'+''.join(v['asm']+'\n' for v in V))
