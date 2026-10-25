.text
.global _start
_start:
    movz x21, #1, lsl #16
    movk x21, #0x2000
    svc #0x45
    cbnz w0, fail
    str w1, [x21]
    str w2, [x21, #4]
    adr x1, worker
    mov x2, x21
    movz x3, #0xC1, lsl #16
    mov w4, #32
    mov w5, #0
    svc #8
    cbnz w0, fail
    str w1, [x21, #8]
    mov w0, w1
    svc #9
    cbnz w0, fail
    add x1, x21, #4
    mov w2, #1
    ldr x3, [x21, #32]
    svc #0x18
    str w0, [x21, #16]
    str x1, [x21, #24]
    svc #0x1E
    str x0, [x21, #48]
    add x1, x21, #8
    mov w2, #1
    mov x3, #-1
    svc #0x18
    cbnz w0, fail
    svc #0x1E
    str x0, [x21, #56]
    mov w0, #55
    str w0, [x21, #12]
    svc #0xA
worker:
    mov x21, x0
    ldr x0, [x21, #40]
    svc #0xB
    ldr w0, [x21]
    svc #0x11
    mov x0, #500
    svc #0xB
    svc #0xA
fail:
    mov w0, #99
    str w0, [x21, #12]
    svc #7
.data
.space 32
.quad 2000
.quad 1000
.space 16
