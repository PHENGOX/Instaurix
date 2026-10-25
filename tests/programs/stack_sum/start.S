.text
.global _start
_start:
    stp x29, x30, [sp, #-16]!
    add x29, sp, #0
    movz x0, #10
    bl sum_to
    movz x1, #0x1000
    str x0, [x1]
    ldp x29, x30, [sp], #16
    movz x0, #0
    svc #0xf000
