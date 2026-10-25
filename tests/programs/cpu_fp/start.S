.text
.global _start
_start:
    adrp x0, result
    add x0, x0, :lo12:result
    bl fp_work
    // Exercise integer vectors in the same executable as scalar FP.
    adrp x1, lanes
    add x1, x1, :lo12:lanes
    ldp q0, q1, [x1]
    add v2.4s, v0.4s, v1.4s
    eor v2.16b, v2.16b, v0.16b
    str q2, [x1, #32]
    adrp x0, conversions
    add x0, x0, :lo12:conversions
    adrp x1, signed_seed
    ldr x1, [x1, :lo12:signed_seed]
    movz x2, #3
    movk x2, #0x100, lsl #16
    bl conversion_work
    adrp x1, lanes
    add x1, x1, :lo12:lanes
    ldp q0, q1, [x1]
    mul v2.4s, v0.4s, v1.4s
    mla v2.4s, v1.4s, v0.4s
    zip1 v3.4s, v2.4s, v0.4s
    zip2 v4.4s, v2.4s, v0.4s
    uzp1 v5.4s, v3.4s, v4.4s
    sshr v6.4s, v5.4s, #1
    cmgt v7.4s, v6.4s, v0.4s
    cnt v8.16b, v7.16b
    addv b9, v8.16b
    addp v10.4s, v5.4s, v0.4s
    addv s11, v10.4s
    ext v12.16b, v3.16b, v4.16b, #8
    rev32 v13.16b, v12.16b
    adrp x1, simd_result
    add x1, x1, :lo12:simd_result
    stp q5, q6, [x1]
    stp q7, q8, [x1, #32]
    str s9, [x1, #64]
    str s11, [x1, #68]
    stp q12, q13, [x1, #80]
    umov w2, v9.b[0]
    mov w5, #128
    cmp w2, #0
    ccmp w2, w5, #0, ne
    cset w3, lo
    ccmn w2, #1, #15, eq
    cset w4, vs
    stp w3, w4, [x1, #72]
    adrp x1, float_vectors
    add x1, x1, :lo12:float_vectors
    ldp q0, q1, [x1]
    ldr q2, [x1, #32]
    fadd v3.4s, v0.4s, v1.4s
    fmul v4.4s, v0.4s, v1.4s
    fmla v2.4s, v0.4s, v1.4s
    fdiv v5.4s, v2.4s, v1.4s
    fsqrt v6.4s, v4.4s
    fneg v7.4s, v3.4s
    fabs v8.4s, v7.4s
    adrp x1, fp_vector_result
    add x1, x1, :lo12:fp_vector_result
    stp q3, q4, [x1]
    stp q2, q5, [x1, #32]
    stp q6, q7, [x1, #64]
    str q8, [x1, #96]
    adrp x1, float_vectors
    add x1, x1, :lo12:float_vectors
    ldr q2, [x1, #32]
    faddp v3.4s, v0.4s, v1.4s
    faddp s4, v3.2s
    fmaxv s5, v0.4s
    frintn v6.4s, v0.4s
    frintm v7.4s, v2.4s
    fcvtns v9.4s, v0.4s
    scvtf v10.4s, v9.4s
    fmaxnm v11.4s, v2.4s, v1.4s
    fmin v12.4s, v2.4s, v0.4s
    frinta s13, s0
    fmax s14, s0, s1
    adrp x1, extension_result
    add x1, x1, :lo12:extension_result
    str q3, [x1]
    stp s4, s5, [x1, #16]
    stp s13, s14, [x1, #24]
    stp q6, q7, [x1, #32]
    stp q9, q10, [x1, #64]
    stp q11, q12, [x1, #96]
    adrp x1, interleaved_data
    add x1, x1, :lo12:interleaved_data
    ld2 {v0.4s, v1.4s}, [x1], #32
    ldr q4, [x1]
    add v2.4s, v0.4s, v1.4s
    dup v3.4s, v0.s[3]
    tbl v5.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    orr v6.16b, v3.16b, v3.16b
    tbx v6.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    orr v7.16b, v0.16b, v0.16b
    ins v7.s[3], v1.s[0]
    smov x2, v1.b[7]
    smov w3, v1.h[1]
    adrp x5, structure_result
    add x5, x5, :lo12:structure_result
    st3 {v0.4s, v1.4s, v2.4s}, [x5], #48
    stp q5, q6, [x5]
    stp q3, q7, [x5, #32]
    stp x2, x3, [x5, #64]
    str x1, [x5, #80]
    str x5, [x5, #88]
    adrp x1, tiny_inputs
    add x1, x1, :lo12:tiny_inputs
    ldp s0, s1, [x1]
    ldp s2, s3, [x1, #8]
    ldp d4, d5, [x1, #16]
    ldp d6, d7, [x1, #32]
    adrp x2, tiny_result
    add x2, x2, :lo12:tiny_result
    msr fpsr, xzr
    fmul s8, s0, s1
    mrs x3, fpsr
    str s8, [x2]
    str w3, [x2, #4]
    msr fpsr, xzr
    fmul s9, s2, s3
    mrs x3, fpsr
    str s9, [x2, #8]
    str w3, [x2, #12]
    msr fpsr, xzr
    fmul d10, d4, d5
    mrs x3, fpsr
    stp d10, d10, [x2, #16]
    str x3, [x2, #24]
    msr fpsr, xzr
    fmul d11, d6, d7
    mrs x3, fpsr
    str d11, [x2, #32]
    str x3, [x2, #40]
    mov x4, #0x1000000
    msr fpcr, x4
    msr fpsr, xzr
    fmul s12, s0, s1
    mrs x3, fpsr
    str s12, [x2, #48]
    str w3, [x2, #52]
    msr fpsr, xzr
    fadd s13, s2, s1
    mrs x3, fpsr
    str s13, [x2, #56]
    str w3, [x2, #60]
    msr fpsr, xzr
    fmul d14, d4, d5
    mrs x3, fpsr
    str d14, [x2, #64]
    str x3, [x2, #72]
    msr fpcr, xzr
    msr fpsr, xzr
    dup v16.4s, v2.s[0]
    dup v17.4s, v3.s[0]
    fmul v18.4s, v16.4s, v17.4s
    mrs x3, fpsr
    str q18, [x2, #80]
    str x3, [x2, #96]
    msr fpsr, xzr
    fcvt s19, d4
    mrs x3, fpsr
    str s19, [x2, #104]
    str w3, [x2, #108]
    adrp x1, simd_math_inputs
    add x1, x1, :lo12:simd_math_inputs
    ldp q0, q1, [x1]
    ldr q2, [x1, #32]
    msr fpsr, xzr
    sqadd v3.4s, v0.4s, v1.4s
    sqshl v4.4s, v3.4s, v2.4s
    sqrshl v5.4s, v3.4s, v2.4s
    sqxtn v6.4h, v4.4s
    sqxtn2 v6.8h, v5.4s
    sqxtun v7.4h, v5.4s
    uqadd v8.4s, v0.4s, v1.4s
    ushl v9.4s, v8.4s, v2.4s
    smull v10.2d, v0.2s, v1.2s
    smull2 v11.2d, v0.4s, v1.4s
    smlal2 v11.2d, v0.4s, v1.4s
    umlsl v10.2d, v0.2s, v1.2s
    mrs x3, fpsr
    msr fpsr, xzr
    sshl v12.4s, v0.4s, v2.4s
    mrs x4, fpsr
    dup v14.4s, v0.s[1]
    sqneg s13, s14
    mrs x5, fpsr
    adrp x1, simd_math_result
    add x1, x1, :lo12:simd_math_result
    stp q3, q4, [x1]
    stp q5, q6, [x1, #32]
    stp q7, q8, [x1, #64]
    stp q9, q10, [x1, #96]
    str q11, [x1, #128]
    stp x3, x4, [x1, #144]
    str q12, [x1, #160]
    str s13, [x1, #176]
    str w5, [x1, #180]
    mov x0, #0
    svc #0xf000
.data
.balign 16
.global result
result:
    .zero 16
.global lanes
lanes:
    .word 1, 0xffffffff, 0x80000000, 17
    .word 2, 3, 0x80000001, 23
    .zero 16
.global conversions
conversions:
    .zero 48
signed_seed:
    .xword 0xffdfffffffffffff
.balign 16
.global simd_result
simd_result:
    .zero 112
.global float_vectors
float_vectors:
    .float 1.0001220703125, 1.5, 4.0, 9.0
    .float 0.9998779296875, 2.0, 0.5, 3.0
    .float -1.0, 0.25, -2.0, -1.5
.global fp_vector_result
fp_vector_result:
    .zero 112
.global extension_result
extension_result:
    .zero 128
.global interleaved_data
interleaved_data:
    .word 1, 0x80000065, 2, 0xffffffff, 3, 103, 4, 104
    .byte 0,15,16,31,32,47,48,63,64,255,1,17,33,49,7,8
.global structure_result
structure_result:
    .zero 144
.global tiny_inputs
tiny_inputs:
    .word 0x00800000, 0x3f000000, 1, 0x3fc00000
    .xword 0x0010000000000000, 0x3fe0000000000000, 1, 0x3ff8000000000000
.global tiny_result
tiny_result:
    .zero 128
.global simd_math_inputs
simd_math_inputs:
    .word 0x7fffffff, 0x80000000, 100, 0xffffff9c
    .word 1, 0xffffffff, 1001, 0xfffffc17
    .word 1, 1, 0xffffffff, 0xffffffff
.global simd_math_result
simd_math_result:
    .zero 192
