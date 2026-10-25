.text
.global _start
_start:
    stp x29, x30, [sp, #-32]!
    stp x19, x20, [sp, #16]
    add x29, sp, #0
    movz x1, #0x20, lsl #16
    svc #1
    cbnz x0, fail
    mov x19, x1
    ldr x4, [x19]
    cbnz x4, fail
    mrs x20, tpidrro_el0
    ldr x4, [x20]
    cbnz x4, fail
    mov x0, x19
    mov x1, x20
    bl fill_heap
    adrp x0, memory_info
    add x0, x0, :lo12:memory_info
    mov x2, x19
    svc #6
    cbnz x0, fail
    adrp x3, memory_info
    add x3, x3, :lo12:memory_info
    ldr w4, [x3, #16]
    movz x5, #5
    cmp x4, x5
    b.ne fail
    ldr w4, [x3, #24]
    movz x5, #3
    cmp x4, x5
    b.ne fail
    ldr x2, [x19]
    adrp x3, expected
    ldr x3, [x3, :lo12:expected]
    cmp x2, x3
    b.ne fail
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #32
    movz x0, #123
    svc #7
fail:
    svc #0xf001
.bss
.p2align 3
.global memory_info
memory_info:
    .skip 40
