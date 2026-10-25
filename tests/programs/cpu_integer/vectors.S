.text
add w0, w1, #17
add w0, w1, #3, lsl #12
add w0, w1, w2, lsl #0
add w0, w1, w2, lsl #7
add w0, w1, w2, lsr #13
add w0, w1, w2, asr #17
add wzr, w1, w2
adds w0, w1, #17
adds w0, w1, #3, lsl #12
adds w0, w1, w2, lsl #0
adds w0, w1, w2, lsl #7
adds w0, w1, w2, lsr #13
adds w0, w1, w2, asr #17
adds wzr, w1, w2
sub w0, w1, #17
sub w0, w1, #3, lsl #12
sub w0, w1, w2, lsl #0
sub w0, w1, w2, lsl #7
sub w0, w1, w2, lsr #13
sub w0, w1, w2, asr #17
sub wzr, w1, w2
subs w0, w1, #17
subs w0, w1, #3, lsl #12
subs w0, w1, w2, lsl #0
subs w0, w1, w2, lsl #7
subs w0, w1, w2, lsr #13
subs w0, w1, w2, asr #17
subs wzr, w1, w2
adc w0, w1, w2
adcs w0, w1, w2
sbc w0, w1, w2
sbcs w0, w1, w2
and w0, w1, w2, lsl #0
and w0, w1, w2, lsl #7
and w0, w1, w2, lsr #13
and w0, w1, w2, asr #17
and w0, w1, w2, ror #5
bic w0, w1, w2, lsl #0
bic w0, w1, w2, lsl #7
bic w0, w1, w2, lsr #13
bic w0, w1, w2, asr #17
bic w0, w1, w2, ror #5
orr w0, w1, w2, lsl #0
orr w0, w1, w2, lsl #7
orr w0, w1, w2, lsr #13
orr w0, w1, w2, asr #17
orr w0, w1, w2, ror #5
orn w0, w1, w2, lsl #0
orn w0, w1, w2, lsl #7
orn w0, w1, w2, lsr #13
orn w0, w1, w2, asr #17
orn w0, w1, w2, ror #5
eor w0, w1, w2, lsl #0
eor w0, w1, w2, lsl #7
eor w0, w1, w2, lsr #13
eor w0, w1, w2, asr #17
eor w0, w1, w2, ror #5
eon w0, w1, w2, lsl #0
eon w0, w1, w2, lsl #7
eon w0, w1, w2, lsr #13
eon w0, w1, w2, asr #17
eon w0, w1, w2, ror #5
ands w0, w1, w2, lsl #0
ands w0, w1, w2, lsl #7
ands w0, w1, w2, lsr #13
ands w0, w1, w2, asr #17
ands w0, w1, w2, ror #5
bics w0, w1, w2, lsl #0
bics w0, w1, w2, lsl #7
bics w0, w1, w2, lsr #13
bics w0, w1, w2, asr #17
bics w0, w1, w2, ror #5
udiv w0, w1, w2
sdiv w0, w1, w2
lslv w0, w1, w2
lsrv w0, w1, w2
asrv w0, w1, w2
rorv w0, w1, w2
madd w0, w1, w2, w3
msub w0, w1, w2, w3
rbit w0, w1
rev16 w0, w1
rev w0, w1
clz w0, w1
cls w0, w1
sbfm w0, w1, #0, #7
sbfm w0, w1, #8, #15
sbfm w0, w1, #20, #7
sbfm w0, w1, #1, #30
sbfm w0, w1, #31, #31
ubfm w0, w1, #0, #7
ubfm w0, w1, #8, #15
ubfm w0, w1, #20, #7
ubfm w0, w1, #1, #30
ubfm w0, w1, #31, #31
bfm w0, w1, #0, #7
bfm w0, w1, #8, #15
bfm w0, w1, #20, #7
bfm w0, w1, #1, #30
bfm w0, w1, #31, #31
extr w0, w1, w2, #0
extr w0, w1, w2, #1
extr w0, w1, w2, #17
extr w0, w1, w2, #31
csel w0, w1, w2, eq
csinc w0, w1, w2, eq
csinv w0, w1, w2, eq
csneg w0, w1, w2, eq
csel w0, w1, w2, ne
csinc w0, w1, w2, ne
csinv w0, w1, w2, ne
csneg w0, w1, w2, ne
csel w0, w1, w2, hs
csinc w0, w1, w2, hs
csinv w0, w1, w2, hs
csneg w0, w1, w2, hs
csel w0, w1, w2, lo
csinc w0, w1, w2, lo
csinv w0, w1, w2, lo
csneg w0, w1, w2, lo
csel w0, w1, w2, mi
csinc w0, w1, w2, mi
csinv w0, w1, w2, mi
csneg w0, w1, w2, mi
csel w0, w1, w2, pl
csinc w0, w1, w2, pl
csinv w0, w1, w2, pl
csneg w0, w1, w2, pl
csel w0, w1, w2, vs
csinc w0, w1, w2, vs
csinv w0, w1, w2, vs
csneg w0, w1, w2, vs
csel w0, w1, w2, vc
csinc w0, w1, w2, vc
csinv w0, w1, w2, vc
csneg w0, w1, w2, vc
csel w0, w1, w2, hi
csinc w0, w1, w2, hi
csinv w0, w1, w2, hi
csneg w0, w1, w2, hi
csel w0, w1, w2, ls
csinc w0, w1, w2, ls
csinv w0, w1, w2, ls
csneg w0, w1, w2, ls
csel w0, w1, w2, ge
csinc w0, w1, w2, ge
csinv w0, w1, w2, ge
csneg w0, w1, w2, ge
csel w0, w1, w2, lt
csinc w0, w1, w2, lt
csinv w0, w1, w2, lt
csneg w0, w1, w2, lt
csel w0, w1, w2, gt
csinc w0, w1, w2, gt
csinv w0, w1, w2, gt
csneg w0, w1, w2, gt
csel w0, w1, w2, le
csinc w0, w1, w2, le
csinv w0, w1, w2, le
csneg w0, w1, w2, le
csel w0, w1, w2, al
csinc w0, w1, w2, al
csinv w0, w1, w2, al
csneg w0, w1, w2, al
csel w0, w1, w2, nv
csinc w0, w1, w2, nv
csinv w0, w1, w2, nv
csneg w0, w1, w2, nv
and w0, w1, #0xff
and w0, w1, #0x80000001
and w0, w1, #0x55555555
orr w0, w1, #0xff
orr w0, w1, #0x80000001
orr w0, w1, #0x55555555
eor w0, w1, #0xff
eor w0, w1, #0x80000001
eor w0, w1, #0x55555555
ands w0, w1, #0xff
ands w0, w1, #0x80000001
ands w0, w1, #0x55555555
add w0, wsp, #16
sub w0, wsp, #16
adds w0, wsp, #16
subs w0, wsp, #16
add wsp, w1, #16
add wsp, wsp, #16
sub wsp, w1, #16
sub wsp, wsp, #16
orr wsp, w1, #0xff
add w0, wsp, w2, uxtb #2
adds w0, wsp, w2, uxtb #2
sub w0, wsp, w2, uxtb #2
subs w0, wsp, w2, uxtb #2
add w0, wsp, w2, uxth #2
adds w0, wsp, w2, uxth #2
sub w0, wsp, w2, uxth #2
subs w0, wsp, w2, uxth #2
add w0, wsp, w2, uxtw #2
adds w0, wsp, w2, uxtw #2
sub w0, wsp, w2, uxtw #2
subs w0, wsp, w2, uxtw #2
add w0, wsp, w2, sxtb #2
adds w0, wsp, w2, sxtb #2
sub w0, wsp, w2, sxtb #2
subs w0, wsp, w2, sxtb #2
add w0, wsp, w2, sxth #2
adds w0, wsp, w2, sxth #2
sub w0, wsp, w2, sxth #2
subs w0, wsp, w2, sxth #2
add w0, wsp, w2, sxtw #2
adds w0, wsp, w2, sxtw #2
sub w0, wsp, w2, sxtw #2
subs w0, wsp, w2, sxtw #2
add x0, x1, #17
add x0, x1, #3, lsl #12
add x0, x1, x2, lsl #0
add x0, x1, x2, lsl #7
add x0, x1, x2, lsr #13
add x0, x1, x2, asr #17
add xzr, x1, x2
adds x0, x1, #17
adds x0, x1, #3, lsl #12
adds x0, x1, x2, lsl #0
adds x0, x1, x2, lsl #7
adds x0, x1, x2, lsr #13
adds x0, x1, x2, asr #17
adds xzr, x1, x2
sub x0, x1, #17
sub x0, x1, #3, lsl #12
sub x0, x1, x2, lsl #0
sub x0, x1, x2, lsl #7
sub x0, x1, x2, lsr #13
sub x0, x1, x2, asr #17
sub xzr, x1, x2
subs x0, x1, #17
subs x0, x1, #3, lsl #12
subs x0, x1, x2, lsl #0
subs x0, x1, x2, lsl #7
subs x0, x1, x2, lsr #13
subs x0, x1, x2, asr #17
subs xzr, x1, x2
adc x0, x1, x2
adcs x0, x1, x2
sbc x0, x1, x2
sbcs x0, x1, x2
and x0, x1, x2, lsl #0
and x0, x1, x2, lsl #7
and x0, x1, x2, lsr #13
and x0, x1, x2, asr #17
and x0, x1, x2, ror #5
bic x0, x1, x2, lsl #0
bic x0, x1, x2, lsl #7
bic x0, x1, x2, lsr #13
bic x0, x1, x2, asr #17
bic x0, x1, x2, ror #5
orr x0, x1, x2, lsl #0
orr x0, x1, x2, lsl #7
orr x0, x1, x2, lsr #13
orr x0, x1, x2, asr #17
orr x0, x1, x2, ror #5
orn x0, x1, x2, lsl #0
orn x0, x1, x2, lsl #7
orn x0, x1, x2, lsr #13
orn x0, x1, x2, asr #17
orn x0, x1, x2, ror #5
eor x0, x1, x2, lsl #0
eor x0, x1, x2, lsl #7
eor x0, x1, x2, lsr #13
eor x0, x1, x2, asr #17
eor x0, x1, x2, ror #5
eon x0, x1, x2, lsl #0
eon x0, x1, x2, lsl #7
eon x0, x1, x2, lsr #13
eon x0, x1, x2, asr #17
eon x0, x1, x2, ror #5
ands x0, x1, x2, lsl #0
ands x0, x1, x2, lsl #7
ands x0, x1, x2, lsr #13
ands x0, x1, x2, asr #17
ands x0, x1, x2, ror #5
bics x0, x1, x2, lsl #0
bics x0, x1, x2, lsl #7
bics x0, x1, x2, lsr #13
bics x0, x1, x2, asr #17
bics x0, x1, x2, ror #5
udiv x0, x1, x2
sdiv x0, x1, x2
lslv x0, x1, x2
lsrv x0, x1, x2
asrv x0, x1, x2
rorv x0, x1, x2
madd x0, x1, x2, x3
msub x0, x1, x2, x3
rbit x0, x1
rev16 x0, x1
rev x0, x1
clz x0, x1
cls x0, x1
sbfm x0, x1, #0, #7
sbfm x0, x1, #8, #15
sbfm x0, x1, #20, #7
sbfm x0, x1, #1, #30
sbfm x0, x1, #31, #31
ubfm x0, x1, #0, #7
ubfm x0, x1, #8, #15
ubfm x0, x1, #20, #7
ubfm x0, x1, #1, #30
ubfm x0, x1, #31, #31
bfm x0, x1, #0, #7
bfm x0, x1, #8, #15
bfm x0, x1, #20, #7
bfm x0, x1, #1, #30
bfm x0, x1, #31, #31
extr x0, x1, x2, #0
extr x0, x1, x2, #1
extr x0, x1, x2, #17
extr x0, x1, x2, #31
csel x0, x1, x2, eq
csinc x0, x1, x2, eq
csinv x0, x1, x2, eq
csneg x0, x1, x2, eq
csel x0, x1, x2, ne
csinc x0, x1, x2, ne
csinv x0, x1, x2, ne
csneg x0, x1, x2, ne
csel x0, x1, x2, hs
csinc x0, x1, x2, hs
csinv x0, x1, x2, hs
csneg x0, x1, x2, hs
csel x0, x1, x2, lo
csinc x0, x1, x2, lo
csinv x0, x1, x2, lo
csneg x0, x1, x2, lo
csel x0, x1, x2, mi
csinc x0, x1, x2, mi
csinv x0, x1, x2, mi
csneg x0, x1, x2, mi
csel x0, x1, x2, pl
csinc x0, x1, x2, pl
csinv x0, x1, x2, pl
csneg x0, x1, x2, pl
csel x0, x1, x2, vs
csinc x0, x1, x2, vs
csinv x0, x1, x2, vs
csneg x0, x1, x2, vs
csel x0, x1, x2, vc
csinc x0, x1, x2, vc
csinv x0, x1, x2, vc
csneg x0, x1, x2, vc
csel x0, x1, x2, hi
csinc x0, x1, x2, hi
csinv x0, x1, x2, hi
csneg x0, x1, x2, hi
csel x0, x1, x2, ls
csinc x0, x1, x2, ls
csinv x0, x1, x2, ls
csneg x0, x1, x2, ls
csel x0, x1, x2, ge
csinc x0, x1, x2, ge
csinv x0, x1, x2, ge
csneg x0, x1, x2, ge
csel x0, x1, x2, lt
csinc x0, x1, x2, lt
csinv x0, x1, x2, lt
csneg x0, x1, x2, lt
csel x0, x1, x2, gt
csinc x0, x1, x2, gt
csinv x0, x1, x2, gt
csneg x0, x1, x2, gt
csel x0, x1, x2, le
csinc x0, x1, x2, le
csinv x0, x1, x2, le
csneg x0, x1, x2, le
csel x0, x1, x2, al
csinc x0, x1, x2, al
csinv x0, x1, x2, al
csneg x0, x1, x2, al
csel x0, x1, x2, nv
csinc x0, x1, x2, nv
csinv x0, x1, x2, nv
csneg x0, x1, x2, nv
and x0, x1, #0xff
and x0, x1, #0x8000000000000001
and x0, x1, #0x5555555555555555
orr x0, x1, #0xff
orr x0, x1, #0x8000000000000001
orr x0, x1, #0x5555555555555555
eor x0, x1, #0xff
eor x0, x1, #0x8000000000000001
eor x0, x1, #0x5555555555555555
ands x0, x1, #0xff
ands x0, x1, #0x8000000000000001
ands x0, x1, #0x5555555555555555
add x0, sp, #16
sub x0, sp, #16
adds x0, sp, #16
subs x0, sp, #16
add sp, x1, #16
add sp, sp, #16
sub sp, x1, #16
sub sp, sp, #16
orr sp, x1, #0xff
add x0, sp, w2, uxtb #2
adds x0, sp, w2, uxtb #2
sub x0, sp, w2, uxtb #2
subs x0, sp, w2, uxtb #2
add x0, sp, w2, uxth #2
adds x0, sp, w2, uxth #2
sub x0, sp, w2, uxth #2
subs x0, sp, w2, uxth #2
add x0, sp, w2, uxtw #2
adds x0, sp, w2, uxtw #2
sub x0, sp, w2, uxtw #2
subs x0, sp, w2, uxtw #2
add x0, sp, w2, sxtb #2
adds x0, sp, w2, sxtb #2
sub x0, sp, w2, sxtb #2
subs x0, sp, w2, sxtb #2
add x0, sp, w2, sxth #2
adds x0, sp, w2, sxth #2
sub x0, sp, w2, sxth #2
subs x0, sp, w2, sxth #2
add x0, sp, w2, sxtw #2
adds x0, sp, w2, sxtw #2
sub x0, sp, w2, sxtw #2
subs x0, sp, w2, sxtw #2
smaddl x0, w1, w2, x3
smsubl x0, w1, w2, x3
umaddl x0, w1, w2, x3
umsubl x0, w1, w2, x3
smulh x0, x1, x2
umulh x0, x1, x2
rev32 x0, x1
sbfm x0, x1, #32, #63
sbfm x0, x1, #63, #63
sbfm x0, x1, #60, #7
sbfm x0, x1, #0, #63
ubfm x0, x1, #32, #63
ubfm x0, x1, #63, #63
ubfm x0, x1, #60, #7
ubfm x0, x1, #0, #63
bfm x0, x1, #32, #63
bfm x0, x1, #63, #63
bfm x0, x1, #60, #7
bfm x0, x1, #0, #63
extr x0, x1, x2, #32
extr x0, x1, x2, #63
b.eq .+16
b.eq .-16
b.ne .+16
b.ne .-16
b.hs .+16
b.hs .-16
b.lo .+16
b.lo .-16
b.mi .+16
b.mi .-16
b.pl .+16
b.pl .-16
b.vs .+16
b.vs .-16
b.vc .+16
b.vc .-16
b.hi .+16
b.hi .-16
b.ls .+16
b.ls .-16
b.ge .+16
b.ge .-16
b.lt .+16
b.lt .-16
b.gt .+16
b.gt .-16
b.le .+16
b.le .-16
b.al .+16
b.al .-16
b.nv .+16
b.nv .-16
b .+16
b .-16
bl .+16
bl .-16
cbz w1, .+16
cbz w1, .-16
cbz x1, .+16
cbz x1, .-16
cbnz w1, .+16
cbnz w1, .-16
cbnz x1, .+16
cbnz x1, .-16
tbz x1, #0, .+16
tbz x1, #31, .+16
tbz x1, #32, .+16
tbz x1, #63, .+16
tbnz x1, #0, .+16
tbnz x1, #31, .+16
tbnz x1, #32, .+16
tbnz x1, #63, .+16
adr x0, .+64
adr x0, .-64
.inst 0xb0000000
br x1
blr x1
ret
str x0, [x1, #8]
str x0, [x1, #-8]!
str x0, [x1], #8
str x0, [sp, #8]
str x0, [x1, w2, uxtw #0]
str x0, [x1, w2, uxtw #3]
str x0, [x1, w2, sxtw #0]
str x0, [x1, w2, sxtw #3]
str x0, [x1, x2, lsl #0]
str x0, [x1, x2, lsl #3]
str x0, [x1, x2, sxtx #0]
str x0, [x1, x2, sxtx #3]
ldr x0, [x1, #8]
ldr x0, [x1, #-8]!
ldr x0, [x1], #8
ldr x0, [sp, #8]
ldr x0, [x1, w2, uxtw #0]
ldr x0, [x1, w2, uxtw #3]
ldr x0, [x1, w2, sxtw #0]
ldr x0, [x1, w2, sxtw #3]
ldr x0, [x1, x2, lsl #0]
ldr x0, [x1, x2, lsl #3]
ldr x0, [x1, x2, sxtx #0]
ldr x0, [x1, x2, sxtx #3]
stur x0, [x1, #-7]
ldur x0, [x1, #-7]
str w0, [x1, #8]
str w0, [x1, #-8]!
str w0, [x1], #8
str w0, [sp, #8]
str w0, [x1, w2, uxtw #0]
str w0, [x1, w2, uxtw #2]
str w0, [x1, w2, sxtw #0]
str w0, [x1, w2, sxtw #2]
str w0, [x1, x2, lsl #0]
str w0, [x1, x2, lsl #2]
str w0, [x1, x2, sxtx #0]
str w0, [x1, x2, sxtx #2]
ldr w0, [x1, #8]
ldr w0, [x1, #-8]!
ldr w0, [x1], #8
ldr w0, [sp, #8]
ldr w0, [x1, w2, uxtw #0]
ldr w0, [x1, w2, uxtw #2]
ldr w0, [x1, w2, sxtw #0]
ldr w0, [x1, w2, sxtw #2]
ldr w0, [x1, x2, lsl #0]
ldr w0, [x1, x2, lsl #2]
ldr w0, [x1, x2, sxtx #0]
ldr w0, [x1, x2, sxtx #2]
stur w0, [x1, #-7]
ldur w0, [x1, #-7]
strb w0, [x1, #8]
strb w0, [x1, #-8]!
strb w0, [x1], #8
strb w0, [sp, #8]
strb w0, [x1, w2, uxtw #0]
strb w0, [x1, w2, uxtw #0]
strb w0, [x1, w2, sxtw #0]
strb w0, [x1, w2, sxtw #0]
strb w0, [x1, x2, lsl #0]
strb w0, [x1, x2, lsl #0]
strb w0, [x1, x2, sxtx #0]
strb w0, [x1, x2, sxtx #0]
ldrb w0, [x1, #8]
ldrb w0, [x1, #-8]!
ldrb w0, [x1], #8
ldrb w0, [sp, #8]
ldrb w0, [x1, w2, uxtw #0]
ldrb w0, [x1, w2, uxtw #0]
ldrb w0, [x1, w2, sxtw #0]
ldrb w0, [x1, w2, sxtw #0]
ldrb w0, [x1, x2, lsl #0]
ldrb w0, [x1, x2, lsl #0]
ldrb w0, [x1, x2, sxtx #0]
ldrb w0, [x1, x2, sxtx #0]
sturb w0, [x1, #-7]
ldurb w0, [x1, #-7]
strh w0, [x1, #8]
strh w0, [x1, #-8]!
strh w0, [x1], #8
strh w0, [sp, #8]
strh w0, [x1, w2, uxtw #0]
strh w0, [x1, w2, uxtw #1]
strh w0, [x1, w2, sxtw #0]
strh w0, [x1, w2, sxtw #1]
strh w0, [x1, x2, lsl #0]
strh w0, [x1, x2, lsl #1]
strh w0, [x1, x2, sxtx #0]
strh w0, [x1, x2, sxtx #1]
ldrh w0, [x1, #8]
ldrh w0, [x1, #-8]!
ldrh w0, [x1], #8
ldrh w0, [sp, #8]
ldrh w0, [x1, w2, uxtw #0]
ldrh w0, [x1, w2, uxtw #1]
ldrh w0, [x1, w2, sxtw #0]
ldrh w0, [x1, w2, sxtw #1]
ldrh w0, [x1, x2, lsl #0]
ldrh w0, [x1, x2, lsl #1]
ldrh w0, [x1, x2, sxtx #0]
ldrh w0, [x1, x2, sxtx #1]
sturh w0, [x1, #-7]
ldurh w0, [x1, #-7]
ldrsb w0, [x1, #8]
ldrsb w0, [x1, #-8]!
ldrsb w0, [x1], #8
ldrsb w0, [x1, w2, sxtw]
ldrsb x0, [x1, #8]
ldrsb x0, [x1, #-8]!
ldrsb x0, [x1], #8
ldrsb x0, [x1, w2, sxtw]
ldrsh w0, [x1, #8]
ldrsh w0, [x1, #-8]!
ldrsh w0, [x1], #8
ldrsh w0, [x1, w2, sxtw]
ldrsh x0, [x1, #8]
ldrsh x0, [x1, #-8]!
ldrsh x0, [x1], #8
ldrsh x0, [x1, w2, sxtw]
ldrsw x0, [x1, #8]
ldrsw x0, [x1, #-8]!
ldrsw x0, [x1], #8
ldrsw x0, [x1, w2, sxtw]
ldr w0, .
ldr x0, .
ldrsw x0, .
ccmp w1, w2, #0, eq
ccmp w1, #31, #0, eq
ccmp w1, w2, #7, ne
ccmp w1, #31, #7, ne
ccmp w1, w2, #14, hs
ccmp w1, #31, #14, hs
ccmp w1, w2, #5, lo
ccmp w1, #31, #5, lo
ccmp w1, w2, #12, mi
ccmp w1, #31, #12, mi
ccmp w1, w2, #3, pl
ccmp w1, #31, #3, pl
ccmp w1, w2, #10, vs
ccmp w1, #31, #10, vs
ccmp w1, w2, #1, vc
ccmp w1, #31, #1, vc
ccmp w1, w2, #8, hi
ccmp w1, #31, #8, hi
ccmp w1, w2, #15, ls
ccmp w1, #31, #15, ls
ccmp w1, w2, #6, ge
ccmp w1, #31, #6, ge
ccmp w1, w2, #13, lt
ccmp w1, #31, #13, lt
ccmp w1, w2, #4, gt
ccmp w1, #31, #4, gt
ccmp w1, w2, #11, le
ccmp w1, #31, #11, le
ccmp w1, w2, #2, al
ccmp w1, #31, #2, al
ccmp w1, w2, #9, nv
ccmp w1, #31, #9, nv
ccmp wzr, #0, #15, ne
ccmn w1, w2, #0, eq
ccmn w1, #31, #0, eq
ccmn w1, w2, #7, ne
ccmn w1, #31, #7, ne
ccmn w1, w2, #14, hs
ccmn w1, #31, #14, hs
ccmn w1, w2, #5, lo
ccmn w1, #31, #5, lo
ccmn w1, w2, #12, mi
ccmn w1, #31, #12, mi
ccmn w1, w2, #3, pl
ccmn w1, #31, #3, pl
ccmn w1, w2, #10, vs
ccmn w1, #31, #10, vs
ccmn w1, w2, #1, vc
ccmn w1, #31, #1, vc
ccmn w1, w2, #8, hi
ccmn w1, #31, #8, hi
ccmn w1, w2, #15, ls
ccmn w1, #31, #15, ls
ccmn w1, w2, #6, ge
ccmn w1, #31, #6, ge
ccmn w1, w2, #13, lt
ccmn w1, #31, #13, lt
ccmn w1, w2, #4, gt
ccmn w1, #31, #4, gt
ccmn w1, w2, #11, le
ccmn w1, #31, #11, le
ccmn w1, w2, #2, al
ccmn w1, #31, #2, al
ccmn w1, w2, #9, nv
ccmn w1, #31, #9, nv
ccmn wzr, #0, #15, ne
ccmp x1, x2, #0, eq
ccmp x1, #31, #0, eq
ccmp x1, x2, #7, ne
ccmp x1, #31, #7, ne
ccmp x1, x2, #14, hs
ccmp x1, #31, #14, hs
ccmp x1, x2, #5, lo
ccmp x1, #31, #5, lo
ccmp x1, x2, #12, mi
ccmp x1, #31, #12, mi
ccmp x1, x2, #3, pl
ccmp x1, #31, #3, pl
ccmp x1, x2, #10, vs
ccmp x1, #31, #10, vs
ccmp x1, x2, #1, vc
ccmp x1, #31, #1, vc
ccmp x1, x2, #8, hi
ccmp x1, #31, #8, hi
ccmp x1, x2, #15, ls
ccmp x1, #31, #15, ls
ccmp x1, x2, #6, ge
ccmp x1, #31, #6, ge
ccmp x1, x2, #13, lt
ccmp x1, #31, #13, lt
ccmp x1, x2, #4, gt
ccmp x1, #31, #4, gt
ccmp x1, x2, #11, le
ccmp x1, #31, #11, le
ccmp x1, x2, #2, al
ccmp x1, #31, #2, al
ccmp x1, x2, #9, nv
ccmp x1, #31, #9, nv
ccmp xzr, #0, #15, ne
ccmn x1, x2, #0, eq
ccmn x1, #31, #0, eq
ccmn x1, x2, #7, ne
ccmn x1, #31, #7, ne
ccmn x1, x2, #14, hs
ccmn x1, #31, #14, hs
ccmn x1, x2, #5, lo
ccmn x1, #31, #5, lo
ccmn x1, x2, #12, mi
ccmn x1, #31, #12, mi
ccmn x1, x2, #3, pl
ccmn x1, #31, #3, pl
ccmn x1, x2, #10, vs
ccmn x1, #31, #10, vs
ccmn x1, x2, #1, vc
ccmn x1, #31, #1, vc
ccmn x1, x2, #8, hi
ccmn x1, #31, #8, hi
ccmn x1, x2, #15, ls
ccmn x1, #31, #15, ls
ccmn x1, x2, #6, ge
ccmn x1, #31, #6, ge
ccmn x1, x2, #13, lt
ccmn x1, #31, #13, lt
ccmn x1, x2, #4, gt
ccmn x1, #31, #4, gt
ccmn x1, x2, #11, le
ccmn x1, #31, #11, le
ccmn x1, x2, #2, al
ccmn x1, #31, #2, al
ccmn x1, x2, #9, nv
ccmn x1, #31, #9, nv
ccmn xzr, #0, #15, ne
.inst 0x91800020
.inst 0x0b028020
.inst 0x0bc20020
.inst 0x53008020
.inst 0x13c00020
.inst 0x5ac00c20
.inst 0x52c00020
.inst 0x9b600c20
.inst 0x7a420030
.inst 0x7a420420
.inst 0x5a420020
