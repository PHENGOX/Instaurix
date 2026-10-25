"""Independent ARM64 reference fixtures: Clang assembly + Unicorn execution."""
import argparse, hashlib, json, pathlib, random, struct, sys
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/programs/cpu_integer'
CODE, DATA = 0x1000, 0x4000
MASK = (1 << 64) - 1
V = []
def add(asm, kind='integer'): V.append({'asm': asm, 'kind': kind})
for reg in ['w', 'x']:
    for op in ['add','adds','sub','subs']:
        add(f'{op} {reg}0, {reg}1, #17')
        add(f'{op} {reg}0, {reg}1, #3, lsl #12')
        for shift in ['lsl #0','lsl #7','lsr #13','asr #17']:
            add(f'{op} {reg}0, {reg}1, {reg}2, {shift}')
        add(f'{op} {reg}zr, {reg}1, {reg}2')
    for op in ['adc','adcs','sbc','sbcs']:
        add(f'{op} {reg}0, {reg}1, {reg}2')
    for op in ['and','bic','orr','orn','eor','eon','ands','bics']:
        for shift in ['lsl #0','lsl #7','lsr #13','asr #17','ror #5']:
            add(f'{op} {reg}0, {reg}1, {reg}2, {shift}')
    for op in ['udiv','sdiv','lslv','lsrv','asrv','rorv']:
        add(f'{op} {reg}0, {reg}1, {reg}2')
    for op in ['madd','msub']:
        add(f'{op} {reg}0, {reg}1, {reg}2, {reg}3')
    for op in ['rbit','rev16','rev','clz','cls']:
        add(f'{op} {reg}0, {reg}1')
    for op in ['sbfm','ubfm','bfm']:
        for r,s in [(0,7),(8,15),(20,7),(1,30),(31,31)]:
            add(f'{op} {reg}0, {reg}1, #{r}, #{s}')
    for amount in [0,1,17,31]: add(f'extr {reg}0, {reg}1, {reg}2, #{amount}')
    for condition in ['eq','ne','hs','lo','mi','pl','vs','vc','hi','ls','ge','lt','gt','le','al','nv']:
        for op in ['csel','csinc','csinv','csneg']:
            add(f'{op} {reg}0, {reg}1, {reg}2, {condition}', 'condition')
    for op in ['and','orr','eor','ands']:
        for imm in ['0xff','0x80000001','0x55555555']:
            # Replicated 32-bit mask is valid for either width.
            encoded_imm = ('0x8000000000000001' if imm == '0x80000001' else '0x5555555555555555' if imm == '0x55555555' else imm) if reg == 'x' else imm
            add(f'{op} {reg}0, {reg}1, #{encoded_imm}')
    sp = 'sp' if reg == 'x' else 'wsp'
    for op in ['add','sub','adds','subs']:
        add(f'{op} {reg}0, {sp}, #16')
    for op in ['add','sub']:
        add(f'{op} {sp}, {reg}1, #16')
        add(f'{op} {sp}, {sp}, #16')
    add(f'orr {sp}, {reg}1, #0xff')
    for extend in ['uxtb','uxth','uxtw','sxtb','sxth','sxtw']:
        for op in ['add','adds','sub','subs']:
            add(f'{op} {reg}0, {sp}, w2, {extend} #2')
for op in ['smaddl','smsubl','umaddl','umsubl']:
    add(f'{op} x0, w1, w2, x3')
for op in ['smulh','umulh']: add(f'{op} x0, x1, x2')
add('rev32 x0, x1')
for op in ['sbfm','ubfm','bfm']:
    for r,s in [(32,63),(63,63),(60,7),(0,63)]: add(f'{op} x0, x1, #{r}, #{s}')
for amount in [32,63]: add(f'extr x0, x1, x2, #{amount}')
for condition in ['eq','ne','hs','lo','mi','pl','vs','vc','hi','ls','ge','lt','gt','le','al','nv']:
    for direction in ['+16','-16']: add(f'b.{condition} .{direction}', 'condition')
for op in ['b','bl']: add(f'{op} .+16'); add(f'{op} .-16')
for op in ['cbz','cbnz']:
    for reg in ['w','x']: add(f'{op} {reg}1, .+16'); add(f'{op} {reg}1, .-16')
for op in ['tbz','tbnz']:
    for bit in [0,31,32,63]: add(f'{op} x1, #{bit}, .+16')
add('adr x0, .+64'); add('adr x0, .-64')
add('.inst 0xb0000000') # ADRP X0,+1 page, no relocation required
for op in ['br','blr']: add(f'{op} x1','indirect')
add('ret','indirect')
for suffix, reg in [('', 'x'),('', 'w'),('b','w'),('h','w')]:
    for op in ['str','ldr']:
        add(f'{op}{suffix} {reg}0, [x1, #8]', 'memory')
        for mode in ['[x1, #-8]!','[x1], #8','[sp, #8]']:
            add(f'{op}{suffix} {reg}0, {mode}', 'memory')
        for extend in ['uxtw','sxtw','lsl','sxtx']:
            index = 'w2' if extend in ['uxtw','sxtw'] else 'x2'
            for scaled in [False,True]:
                scale = 0 if suffix=='b' else 1 if suffix=='h' else 2 if reg=='w' else 3
                add(f'{op}{suffix} {reg}0, [x1, {index}, {extend} #{scale if scaled else 0}]', f'memory-{extend}')
    for op in ['stur','ldur']: add(f'{op}{suffix} {reg}0, [x1, #-7]', 'memory')
for op in ['ldrsb','ldrsh','ldrsw']:
    for reg in (['x'] if op=='ldrsw' else ['w','x']):
        add(f'{op} {reg}0, [x1, #8]', 'memory')
        add(f'{op} {reg}0, [x1, #-8]!', 'memory')
        add(f'{op} {reg}0, [x1], #8', 'memory')
        add(f'{op} {reg}0, [x1, w2, sxtw]', 'memory-sxtw')
for op,reg in [('ldr','w'),('ldr','x'),('ldrsw','x')]: add(f'{op} {reg}0, .', 'integer')
# A few architecturally unallocated encodings: oracle must reject them too.
for reg in ['w','x']:
    for op in ['ccmp','ccmn']:
        for conditionIndex,condition in enumerate(['eq','ne','hs','lo','mi','pl','vs','vc','hi','ls','ge','lt','gt','le','al','nv']):
            for operand in [f'{reg}2','#31']:
                add(f'{op} {reg}1, {operand}, #{(conditionIndex*7)%16}, {condition}','conditional-compare')
        add(f'{op} {reg}zr, #0, #15, ne','conditional-compare')
for inst in [0x91800020,0x0B028020,0x0BC20020,0x53008020,0x13C00020,0x5AC00C20,0x52C00020,0x9B600C20,
             0x7A420030,0x7A420420,0x5A420020]:
    add(f'.inst 0x{inst:08x}','invalid')


def emit():
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'vectors.S').write_text('.text\n'+'\n'.join(v['asm'] for v in V)+'\n',encoding='utf-8')
    (OUT/'manifest.json').write_text(json.dumps(V,indent=2)+'\n',encoding='utf-8')
    print(f'Assembly variants: {len(V)}')


def oracle():
    sys.path.insert(0,str(ROOT/'tools/python-deps'))
    import unicorn
    from unicorn import Uc, UcError, UC_ARCH_ARM64, UC_MODE_ARM
    from unicorn.arm64_const import UC_ARM64_REG_X0,UC_ARM64_REG_X29,UC_ARM64_REG_X30,UC_ARM64_REG_SP,UC_ARM64_REG_PC,UC_ARM64_REG_NZCV
    regs = [UC_ARM64_REG_X0+i for i in range(29)]+[UC_ARM64_REG_X29,UC_ARM64_REG_X30]
    uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM); uc.mem_map(0,0x10000)
    code=(OUT/'vectors.bin').read_bytes()
    if len(code)!=4*len(V): raise RuntimeError('Assembly fixture length mismatch')
    rng=random.Random(0xA6402026)
    edges=[0,1,MASK,1<<63,(1<<63)-1,0xffffffff,0x80000000,0x7fffffff,0x100000000,31,32,63,64,7,8,16]
    pairs=[(0,0),(0,1),(MASK,1),(MASK,MASK),(1<<63,MASK),((1<<63)-1,1),(1<<63,1<<63),
           (0x7fffffff,1),(0x80000000,0xffffffff),(0xffffffff,1),(0, MASK),(1, MASK),
           (31,32),(32,31),(64,63),(63,64)]
    data=bytes((i*37+11)&255 for i in range(512))
    count=0
    with (OUT/'reference.bin').open('wb') as output:
        output.write(b'A64REF01'+struct.pack('<I',0))
        for index,v in enumerate(V):
            kind=v['kind']; memory=kind.startswith('memory')
            iterations=16 if kind=='condition' else 8 if kind=='invalid' else 64
            instruction=code[index*4:index*4+4]
            for sample in range(iterations):
                values=[rng.getrandbits(64) for _ in regs]
                if sample<32:
                    a,b=pairs[sample%len(pairs)]; values[1]=a; values[2]=b; values[3]=edges[sample%len(edges)]
                flags=(sample%16)<<28; sp=0x6100
                if memory:
                    values[1]=0x4100; sp=0x4100
                    if kind in ['memory-sxtw','memory-sxtx']:
                        values[2] = (0xfffffff8 if kind.endswith('sxtw') else MASK-7) if sample&1 else 8
                    elif kind=='memory-uxtw': values[2]=0xDEADBEEF00000008
                    else: values[2]=8
                    uc.mem_write(DATA,data)
                if kind=='indirect': values[1]=0x1100; values[30]=0x1100
                for reg,value in zip(regs,values): uc.reg_write(reg,value)
                uc.reg_write(UC_ARM64_REG_SP,sp); uc.reg_write(UC_ARM64_REG_NZCV,flags)
                uc.mem_write(CODE,instruction+struct.pack('<I',0xD503201F))
                uc.ctl_remove_cache(CODE,CODE+8)
                invalid=False
                try: uc.emu_start(CODE,CODE+4,count=1)
                except UcError as error:
                    if kind!='invalid': raise RuntimeError(f'Oracle rejected variant {index} {v}: {error}') from error
                    invalid=True
                if kind=='invalid' and not invalid: raise RuntimeError(f'Expected unallocated instruction was accepted: {v}')
                expected=values if invalid else [uc.reg_read(reg) for reg in regs]
                expected_sp=sp if invalid else uc.reg_read(UC_ARM64_REG_SP)
                expected_flags=flags if invalid else uc.reg_read(UC_ARM64_REG_NZCV)
                expected_pc=CODE if invalid else uc.reg_read(UC_ARM64_REG_PC)
                output.write(struct.pack('<II',int.from_bytes(instruction,'little'),int(memory)|(int(invalid)<<1)))
                output.write(struct.pack('<31Q',*values)+struct.pack('<QQ',sp,flags))
                output.write(struct.pack('<31Q',*expected)+struct.pack('<QQQ',expected_sp,expected_flags,expected_pc))
                if memory: output.write(bytes(uc.mem_read(DATA,512)))
                count+=1
        output.seek(8); output.write(struct.pack('<I',count))
    summary={'generator':'Clang 18.1.6 (Zig 0.13.0) + Unicorn '+unicorn.__version__, 'seed':'0xA6402026',
             'variants':len(V),'cases':count,'reference_sha256':hashlib.sha256((OUT/'reference.bin').read_bytes()).hexdigest()}
    (OUT/'reference.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary))

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--oracle',action='store_true'); args=parser.parse_args()
    oracle() if args.oracle else emit()

