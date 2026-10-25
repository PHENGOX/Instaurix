"""Additional independent corpus for gradual underflow and FPCR.FZ."""
import argparse, pathlib, struct
import generate_fp_reference as g
g.V=[]
g.OUT=g.ROOT/'tests/programs/cpu_subnormal'
def add(asm,wide=False,kind='arithmetic',samples=640):
    g.add(asm,'tiny',wide,category=kind,samples=samples)
for reg in ['s','d']:
    wide=reg=='d'
    for op in ['fadd','fsub','fmul','fdiv']:
        for dst in [0,1]: add(f'{op} {reg}{dst}, {reg}1, {reg}2',wide)
    add(f'fsqrt {reg}0, {reg}1',wide)
    for op in ['fmadd','fmsub','fnmadd','fnmsub']:
        for dst in [0,3]: add(f'{op} {reg}{dst}, {reg}1, {reg}2, {reg}3',wide)
    for op in ['fcmp','fcmpe']:
        add(f'{op} {reg}1, {reg}2',wide,'other',256)
    for op in ['fcvtzs','fcvtzu']:
        add(f'{op} x0, {reg}1',wide,'other',256)
    for op in ['frintn','frintx','frinti','fmax','fmin','fmaxnm','fminnm']:
        operands=f'{reg}0, {reg}1'+(f', {reg}2' if op.startswith(('fmax','fmin')) else '')
        add(f'{op} {operands}',wide,'other',256)
add('fcvt s0, d1',True,'precision',640)
add('fcvt d0, s1',False,'precision',640)
for shape in ['2s','4s','2d']:
    wide=shape=='2d'
    for op in ['fadd','fsub','fmul','fdiv','fmla','fmls','faddp']:
        for dst in [0,1]: add(f'{op} v{dst}.{shape}, v1.{shape}, v2.{shape}',wide)
    add(f'fsqrt v0.{shape}, v1.{shape}',wide)
    for op in ['frintx','frinti','fcvtzs','fcvtzu']:
        add(f'{op} v0.{shape}, v1.{shape}',wide,'other',256)
    for op in ['fmax','fmin','fmaxnm','fminnm']:
        add(f'{op} v0.{shape}, v1.{shape}, v2.{shape}',wide,'other',256)
for reg,shape in [('s','2s'),('d','2d')]: add(f'faddp {reg}0, v1.{shape}',reg=='d')

def setup(uc,regs,qregs,v,sample,rng):
    from unicorn.arm64_const import UC_ARM64_REG_FPCR,UC_ARM64_REG_FPSR
    wide=v['wide']; fraction=52 if wide else 23
    exp=0x7FF0000000000000 if wide else 0x7F800000
    sign=1<<(63 if wide else 31); quiet=1<<(51 if wide else 22)
    normal=1<<fraction; one=(1023 if wide else 127)<<fraction
    def bits(value): return int.from_bytes(struct.pack('<d' if wide else '<f',value),'little')
    pairs=[(1,bits(1.5),0),(normal,bits(0.5),0),(1,bits(0.5),0),
        (normal-1,one,0),(normal+1,one-2,0),(normal+1,one-1,0),
        (normal,normal-1,1),(1,bits(2.0),sign|normal),(exp-1,1,sign|one),
        (exp-1,exp-1,1),(1,1,exp-1),(sign|1,bits(1.5),0),
        (sign|normal,bits(0.5),sign),(normal,sign|normal,0),
        (normal-1,1,sign|normal),(normal,one,sign|(normal-1)),
        (0,sign,sign),(sign,one,0),(exp,1,exp|quiet|17),
        (1,exp,exp|quiet|19),(exp|3,sign|1,0),
        (exp|quiet|5,sign|1,exp|7),(1,normal-1,normal),
        (normal-1,bits(3.0),sign|normal),(normal*2-1,bits(0.5),0),
        (2,bits(0.5),0),(3,bits(0.5),0),(sign|3,bits(0.5),0),
        (one,sign|1,0),(sign|one,1,0),(normal,normal,normal),(1,normal,1)]
    control=((sample%4)<<22)|(((sample//4)%2)<<24)|(((sample//8)%2)<<25)
    uc.reg_write(UC_ARM64_REG_FPCR,control)
    uc.reg_write(UC_ARM64_REG_FPSR,0x08000000 if sample&1 else 0)
    width=64 if wide else 32
    packed=[0,0,0]
    for lane in range(128//width):
        if sample<512:
            a,b,c=pairs[(sample//16+lane*7)%len(pairs)]
        else: a,b,c=(rng.getrandbits(width) for _ in range(3))
        if v['category']=='precision' and wide:
            edges=[0x380FFFFFE0000000,0x3810000000000000,0x36A0000000000000,
                   0x3690000000000000,0x36A8000000000000,0x0010000000000000,1,
                   0x380FFFFFF0000000,0x3810000020000000,0x47EFFFFFFFFFFFFF,
                   0xB80FFFFFE0000000,0xB6A8000000000000,0x3FF0000010000000]
            if sample<512: a=edges[(sample//16+lane)%len(edges)]
        for index,value in enumerate([c,a,b]): packed[index]|=value<<(lane*width)
    for index,value in [(0,packed[0]),(1,packed[1]),(2,packed[2]),(3,packed[0])]: uc.reg_write(qregs[index],value)

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--oracle',action='store_true'); args=parser.parse_args()
    g.OUT.mkdir(parents=True,exist_ok=True)
    if args.oracle:
        g.CUSTOM_SETUP=setup; g.oracle()
    else: (g.OUT/'vectors.S').write_text('.text\n'+''.join(v['asm']+'\n' for v in g.V))
