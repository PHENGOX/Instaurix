.text
.global _start
_start:
    stp x29, x30, [sp, #-16]!
    add x29, sp, #0
    adrp x1, zero_probe
    ldr x4, [x1, :lo12:zero_probe]
    cbnz x4, fail
    bl compute_sum
    adrp x1, output
    ldr x2, [x1, :lo12:output]
    adrp x3, expected
    ldr x3, [x3, :lo12:expected]
    cmp x2, x3
    b.ne fail
    ldp x29, x30, [sp], #16
    movz x0, #0
    svc #0xf000
fail:
    movz x0, #1
    svc #0xf000
