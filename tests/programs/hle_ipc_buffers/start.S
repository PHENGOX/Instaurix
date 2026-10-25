.text
.global _start
_start:
    movz x21, #1, lsl #16
    movk x21, #0x2000
    adr x1, port
    svc #0x1F
    cbnz w0, fail
    mov w19, w1
    mrs x20, tpidrro_el0
    add x22, x21, #0x100
    add x23, x21, #0x400
    mov x0, #0
fill:
    strb w0, [x22, x0]
    add x0, x0, #1
    cmp x0, #512
    b.ne fill
    movz w0, #4
    movk w0, #0x110, lsl #16
    str w0, [x20]
    mov w0, #8
    str w0, [x20, #4]
    mov w0, #512
    str w0, [x20, #8]
    str w22, [x20, #12]
    str wzr, [x20, #16]
    str w0, [x20, #20]
    str w23, [x20, #24]
    str wzr, [x20, #28]
    movz w0, #0x4653
    movk w0, #0x4943, lsl #16
    str w0, [x20, #32]
    str wzr, [x20, #36]
    mov w0, #2
    str w0, [x20, #40]
    str wzr, [x20, #44]
    mov w0, w19
    svc #0x21
    cbnz w0, fail
    ldr w0, [x20, #24]
    cbnz w0, fail
    ldr x0, [x20, #32]
    cmp x0, #512
    b.ne fail
    str x0, [x21]
    mov x0, #0
verify:
    ldrb w1, [x22, x0]
    ldrb w2, [x23, x0]
    cmp w1, w2
    b.ne fail
    add x0, x0, #1
    cmp x0, #512
    b.ne verify
    mov w0, w19
    svc #0x16
    cbnz w0, fail
    mov w0, #1
    str w0, [x21, #8]
    svc #7
fail:
    mov w0, #99
    str w0, [x21, #8]
    svc #7
.section .rodata
port:
.asciz "emu:demo"
.data
.space 0x700
