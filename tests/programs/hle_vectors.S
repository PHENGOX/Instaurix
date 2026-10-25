.text
mrs x0, tpidrro_el0
mrs x1, tpidr_el0
msr tpidr_el0, x2
msr tpidrro_el0, x3
mrs x0, tpidr_el1
mrs xzr, tpidrro_el0
msr tpidr_el0, xzr
svc #1
svc #6
svc #7
svc #0x1234
movz w0, #0x1234, lsl #16
movk w0, #0x5678
movn w1, #0
movz wzr, #10
.inst 0x52c00000 // Reserved MOVZ W0 with hw=2
