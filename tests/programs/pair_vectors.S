.text
// Order is consumed by cpu_pair_tests.cpp. Assembler-verified encodings.
stp x0, x1, [x2, #16]
ldp x3, x4, [x2, #16]
stp x0, x1, [x2, #-16]!
ldp x3, x4, [x2], #16
stp w0, w1, [x2, #8]
ldp w3, w4, [x2, #8]
stp w0, w1, [x2, #-8]!
ldp w3, w4, [x2], #8
stp x29, x30, [sp, #-16]!
ldp x29, x30, [sp], #16
stp xzr, x1, [x2]
ldp xzr, x4, [x2]
stp x0, x1, [x2, #504]
ldp x3, x4, [x2, #-512]
stp w0, w1, [x2, #252]
ldp w3, w4, [x2, #-256]
stp x0, x1, [x2], #16
ldp x3, x4, [x2, #-16]!
stp w0, w1, [x2], #8
ldp w3, w4, [x2, #-8]!
// Load-only without writeback permits the base to be a destination.
ldp x2, x4, [x2]
// Non-temporal, signed-word and vector variants need distinct decoding.
stnp x0, x1, [x2]
ldpsw x3, x4, [x2]
stp q0, q1, [x2]
svc #0xf000
svc #0xf001
brk #0xf000
