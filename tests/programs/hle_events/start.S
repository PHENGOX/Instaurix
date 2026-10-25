.text
.global _start
_start:
    movz x21, #1, lsl #16
    movk x21, #0x2000
    svc #0x45
    cbnz w0, fail
    mov w19, w1
    mov w20, w2
    str w19, [x21]
    str w20, [x21, #4]
    add x1, x21, #4
    mov w2, #1
    mov x3, #-1
    svc #0x18
    cbnz w0, fail
    cbnz w1, fail
    mov w0, w20
    svc #0x17
    cbnz w0, fail
    mov w0, w19
    svc #0x16
    cbnz w0, fail
    mov w0, w20
    svc #0x16
    cbnz w0, fail
    mov w0, #55
    str w0, [x21, #8]
    svc #7
fail:
    mov w0, #99
    str w0, [x21, #8]
    svc #7
.data
.space 16
