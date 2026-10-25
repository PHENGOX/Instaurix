.text
.global _start
_start:
    movz x21, #1, lsl #16
    movk x21, #0x2000
    mov x19, #0x1111
    dup v31.2d, x19
    mrs x22, tpidrro_el0
    mov x23, #0x1234
    msr tpidr_el0, x23
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
    ldr w0, [x21, #96]
    cbz w0, wait_event
spin_main:
    ldr w0, [x21, #12]
    cmp w0, #55
    b.ne spin_main
    b record_main
wait_event:
    add x1, x21, #4
    mov w2, #1
    mov x3, #-1
    svc #0x18
    cbnz w0, fail
record_main:
    str x19, [x21, #16]
    str q31, [x21, #32]
    mrs x0, tpidr_el0
    str x0, [x21, #64]
    str x22, [x21, #80]
    add x1, x21, #8
    mov w2, #1
    mov x3, #-1
    svc #0x18
    cbnz w0, fail
    ldr w0, [x21, #12]
    cmp w0, #55
    b.ne fail
    svc #0xA
fail:
    mov w0, #99
    str w0, [x21, #12]
    svc #7
worker:
    mov x21, x0
    mov x19, #0x2222
    dup v31.2d, x19
    mov x23, #0x5678
    msr tpidr_el0, x23
    mrs x22, tpidrro_el0
    str x19, [sp, #-16]!
    str x19, [x22, #0x180]
    ldr w0, [x21, #100]
    cbz w0, produce
    add x1, x21, #4
    mov w2, #1
    mov x3, #-1
    svc #0x18
    cbnz w0, fail
produce:
    mov w0, #42
    str w0, [x21, #12]
    ldr w0, [x21]
    svc #0x11
    mov x24, #150
spin:
    subs x24, x24, #1
    b.ne spin
    ldr x19, [sp], #16
    str x19, [x21, #24]
    str q31, [x21, #48]
    mrs x0, tpidr_el0
    str x0, [x21, #72]
    str x22, [x21, #88]
    mov w0, #55
    str w0, [x21, #12]
    svc #0xA
.data
.space 112
