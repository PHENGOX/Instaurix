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
    mov x0, #20
    mov x1, #35
    bl add_request
    cmp x0, #55
    b.ne fail
    str w0, [x21]
    adr x1, worker
    mov x2, x19
    movz x3, #0xC1, lsl #16
    mov w4, #32
    mov w5, #0
    svc #8
    cbnz w0, fail
    str w1, [x21, #8]
    mov w0, w1
    svc #9
    cbnz w0, fail
    add x1, x21, #8
    mov w2, #1
    mov x3, #-1
    svc #0x18
    cbnz w0, fail
    ldr w0, [x21, #4]
    cmp w0, #99
    b.ne fail
    ldr x0, [x20, #32]
    cmp x0, #55
    b.ne fail
    mov w0, #4
    str w0, [x20]
    mov w0, #8
    str w0, [x20, #4]
    movz w0, #0x4653
    movk w0, #0x4943, lsl #16
    str w0, [x20, #16]
    str wzr, [x20, #20]
    mov w0, #1
    str w0, [x20, #24]
    str wzr, [x20, #28]
    mov w0, w19
    svc #0x21
    cbnz w0, fail
    ldr w0, [x20, #24]
    cbnz w0, fail
    ldr x0, [x20, #32]
    str x0, [x21, #16]
    mov w0, w19
    svc #0x16
    cbnz w0, fail
    mov w0, #1
    str w0, [x21, #24]
    svc #7
worker:
    mov w19, w0
    movz x21, #1, lsl #16
    movk x21, #0x2000
    mrs x20, tpidrro_el0
    mov x0, #44
    mov x1, #55
    bl add_request
    str w0, [x21, #4]
    svc #0xA
add_request:
    str x0, [x20, #32]
    str x1, [x20, #40]
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
    mov w0, w19
    svc #0x21
    cbnz w0, fail
    ldr w0, [x20, #16]
    movz w1, #0x4653
    movk w1, #0x4F43, lsl #16
    cmp w0, w1
    b.ne fail
    ldr w0, [x20, #24]
    cbnz w0, fail
    ldr x0, [x20, #32]
    ret
fail:
    mov w0, #99
    str w0, [x21, #24]
    svc #7
.section .rodata
port:
.asciz "emu:demo"
.data
.space 32
