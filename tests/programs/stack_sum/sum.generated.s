	.text
	.file	"sum.c"
	.globl	sum_to                          // -- Begin function sum_to
	.p2align	2
	.type	sum_to,@function
sum_to:                                 // @sum_to
// %bb.0:
	sub	sp, sp, #16
	mov	x8, x0
	mov	x0, xzr
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
	add	sp, sp, #16
	ret
.Lfunc_end0:
	.size	sum_to, .Lfunc_end0-sum_to
                                        // -- End function
	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
