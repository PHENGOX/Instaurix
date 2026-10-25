.text
.global _start
_start:
    movz x21, #1, lsl #16
    movk x21, #0x2000
    adr x1, port
    svc #0x1F
    cbnz w0, fail
    mov w19, w1
    str w19, [x21, #4]
    mrs x20, tpidrro_el0
    mov w0, #4
    str w0, [x20]
    movz w0, #10
    movk w0, #0x8000, lsl #16
    str w0, [x20, #4]
    mov w0, #1
    str w0, [x20, #8]
    str xzr, [x20, #12]
    movz w0, #0x4653
    movk w0, #0x4943, lsl #16
    str w0, [x20, #32]
    str wzr, [x20, #36]
    str xzr, [x20, #40]
    str xzr, [x20, #48]
    mov w0, w19
    svc #0x21
    cbnz w0, fail
    ldr w0, [x20, #24]
    cbnz w0, fail
    mov w0, #4
    str w0, [x20]
    mov w0, #10
    str w0, [x20, #4]
    str xzr, [x20, #8]
    movz w0, #0x4653
    movk w0, #0x4943, lsl #16
    str w0, [x20, #16]
    str wzr, [x20, #20]
    mov w0, #1
    str w0, [x20, #24]
    str wzr, [x20, #28]
    adr x0, service
    ldr x0, [x0]
    str x0, [x20, #32]
    mov w0, w19
    svc #0x21
    cbnz w0, fail
    ldr w0, [x20, #24]
    cbnz w0, fail
    ldr w0, [x20, #8]
    cmp w0, #32
    b.ne fail
    ldr w22, [x20, #12]
    cbz w22, fail
    str w22, [x21]
    mov w0, w19
    svc #0x16
    cbnz w0, fail
    mov w0, #4
    str w0, [x20]
    mov w0, #12
    str w0, [x20, #4]
    str xzr, [x20, #8]
    movz w0, #0x4653
    movk w0, #0x4943, lsl #16
    str w0, [x20, #16]
    str wzr, [x20, #20]
    str xzr, [x20, #24]
    mov x0, #20
    str x0, [x20, #32]
    mov x0, #35
    str x0, [x20, #40]
    mov w0, w22
    svc #0x21
    cbnz w0, fail
    ldr w0, [x20, #24]
    cbnz w0, fail
    ldr x0, [x20, #32]
    cmp x0, #55
    b.ne fail
    str x0, [x21, #8]
    mov w0, w22
    svc #0x16
    cbnz w0, fail
    mov w0, #1
    str w0, [x21, #16]
    svc #7
fail:
    mov w0, #99
    str w0, [x21, #16]
    svc #7
.section .rodata
port:
.asciz "sm:"
service:
.ascii "emu:demo"
.data
.space 32
