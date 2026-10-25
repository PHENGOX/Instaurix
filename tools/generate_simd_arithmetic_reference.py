"""Independent saturation, variable shift and widening arithmetic fixtures."""
import argparse
import generate_fp_reference as g
g.V=[]
g.OUT=g.ROOT/'tests/programs/cpu_simd_arithmetic'
def add(asm,bits,kind): g.add(asm,'simd-extra',False,element_bits=bits,category=kind,samples=128)
for shape,bits in [('8b',8),('16b',8),('4h',16),('8h',16),('2s',32),('4s',32),('2d',64)]:
    for op in ['sqadd','uqadd','sqsub','uqsub','sshl','ushl','srshl','urshl','sqshl','uqshl','sqrshl','uqrshl']:
        for dst in [0,1,2]: add(f'{op} v{dst}.{shape}, v1.{shape}, v2.{shape}',bits,'shift' if 'shl' in op else 'saturate')
    for op in ['sqabs','sqneg']:
        for dst in [0,1]: add(f'{op} v{dst}.{shape}, v1.{shape}',bits,'saturate')
for scalar,bits in [('b',8),('h',16),('s',32),('d',64)]:
    for op in ['sqadd','uqadd','sqsub','uqsub','sshl','ushl','srshl','urshl','sqshl','uqshl','sqrshl','uqrshl']:
        # Non-saturating scalar shifts only exist for 64-bit elements.
        if bits!=64 and op in ['sshl','ushl','srshl','urshl']: continue
        add(f'{op} {scalar}0, {scalar}1, {scalar}2',bits,'shift' if 'shl' in op else 'saturate')
    for op in ['sqabs','sqneg']: add(f'{op} {scalar}0, {scalar}1',bits,'saturate')
for src,dst,bits in [('8b','8h',8),('4h','4s',16),('2s','2d',32)]:
    high={'8b':'16b','4h':'8h','2s':'4s'}[src]
    for op in ['smull','umull','smlal','umlal','smlsl','umlsl']:
        for upper in [False,True]:
            for rd in [0,1,2]: add(f'{op}{"2" if upper else ""} v{rd}.{dst}, v1.{high if upper else src}, v2.{high if upper else src}',bits,'widen')
for src,dst,high,bits in [('8h','8b','16b',16),('4s','4h','8h',32),('2d','2s','4s',64)]:
    for op in ['sqxtn','uqxtn','sqxtun']:
        for upper in [False,True]:
            for rd in [0,1]: add(f'{op}{"2" if upper else ""} v{rd}.{high if upper else dst}, v1.{src}',bits,'narrow')
    scalarSrc={'8h':'h','4s':'s','2d':'d'}[src]
    scalarDst={'8b':'b','4h':'h','2s':'s'}[dst]
    for op in ['sqxtn','uqxtn','sqxtun']: add(f'{op} {scalarDst}0, {scalarSrc}1',bits,'narrow')

def setup(uc,regs,qregs,v,sample,rng):
    from unicorn.arm64_const import UC_ARM64_REG_FPCR,UC_ARM64_REG_FPSR
    bits=v['element_bits']; mask=(1<<bits)-1; sign=1<<(bits-1)
    edges=[0,1,mask,sign,sign-1,sign+1,mask-1,2,3,7,15,127,128,255,256,257]
    if v['category']=='narrow':
        bound=1<<(bits//2-1)
        edges=[0,1,bound-1,bound,bound+1,bound*2-1,bound*2,mask,sign,sign-1,
               (-bound)&mask,(-bound-1)&mask,(-bound+1)&mask,((-bound*2)&mask),17,mask-17]
    shifts=[-128,-65,-64,-bits-1,-bits,-bits+1,-1,0,1,bits-1,bits,bits+1,63,64,65,127]
    uc.reg_write(UC_ARM64_REG_FPCR,0x01000000 if sample&2 else 0)
    uc.reg_write(UC_ARM64_REG_FPSR,0x0800009F if sample&1 else 0x12)
    packed=[0,0]
    for lane in range(128//bits):
        a=edges[(sample//2+lane*3)%len(edges)] if sample<96 else rng.getrandbits(bits)
        b=edges[(sample//4+lane*7)%len(edges)] if sample<96 else rng.getrandbits(bits)
        if v['category']=='shift':
            # Only the low signed byte determines the shift; other bits are noise.
            b=(rng.getrandbits(bits)&~255)|(shifts[(sample//2+lane*5)%len(shifts)]&255)
        packed[0]|=(a&mask)<<(lane*bits); packed[1]|=(b&mask)<<(lane*bits)
    uc.reg_write(qregs[1],packed[0]); uc.reg_write(qregs[2],packed[1])

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--oracle',action='store_true'); args=parser.parse_args()
    g.OUT.mkdir(parents=True,exist_ok=True)
    if args.oracle: g.CUSTOM_SETUP=setup; g.oracle()
    else: (g.OUT/'vectors.S').write_text('.text\n'+''.join(v['asm']+'\n' for v in g.V))
