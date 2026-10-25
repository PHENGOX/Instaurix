.text
.global _start
_start:
    stp x29, x30, [sp, #-16]!
    add x29, sp, #0
    adrp x0, seed_a
    ldr x0, [x0, :lo12:seed_a]
    adrp x1, seed_b
    ldr x1, [x1, :lo12:seed_b]
    bl work64
    adrp x1, observed64
    str x0, [x1, :lo12:observed64]
    adrp x0, seed_c
    ldr w0, [x0, :lo12:seed_c]
    adrp x1, seed_d
    ldr w1, [x1, :lo12:seed_d]
    bl work32
    adrp x1, observed32
    str w0, [x1, :lo12:observed32]
    adrp x1, observed64
    ldr x2, [x1, :lo12:observed64]
    adrp x1, observed32
    ldr w3, [x1, :lo12:observed32]
    ldp x29, x30, [sp], #16
    movz x0, #0
    svc #0xf000
