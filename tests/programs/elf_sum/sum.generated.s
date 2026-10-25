	.text
	.file	"sum.c"
	.globl	compute_sum                     // -- Begin function compute_sum
	.p2align	2
	.type	compute_sum,@function
compute_sum:                            // @compute_sum
// %bb.0:
	sub	sp, sp, #16
	adrp	x8, initial
	mov	x0, xzr
	ldr	x8, [x8, :lo12:initial]
	str	x8, [sp, #8]
.LBB0_1:                                // =>This Inner Loop Header: Depth=1
	ldr	x8, [sp, #8]
	ldr	x9, [sp, #8]
	sub	x9, x9, #1
	add	x0, x8, x0
	str	x9, [sp, #8]
	ldr	x9, [sp, #8]
	cbnz	x9, .LBB0_1
// %bb.2:
	adrp	x8, output
	str	x0, [x8, :lo12:output]
	add	sp, sp, #16
	ret
.Lfunc_end0:
	.size	compute_sum, .Lfunc_end0-compute_sum
                                        // -- End function
	.type	initial,@object                 // @initial
	.data
	.globl	initial
	.p2align	3, 0x0
initial:
	.xword	10                              // 0xa
	.size	initial, 8

	.type	expected,@object                // @expected
	.section	.rodata,"a",@progbits
	.globl	expected
	.p2align	3, 0x0
expected:
	.xword	55                              // 0x37
	.size	expected, 8

	.type	output,@object                  // @output
	.bss
	.globl	output
	.p2align	3, 0x0
output:
	.xword	0                               // 0x0
	.size	output, 8

	.type	zero_probe,@object              // @zero_probe
	.globl	zero_probe
	.p2align	3, 0x0
zero_probe:
	.xword	0                               // 0x0
	.size	zero_probe, 8

	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym initial
	.addrsig_sym output
