	.text
	.file	"work.c"
	.globl	work64                          // -- Begin function work64
	.p2align	2
	.type	work64,@function
work64:                                 // @work64
// %bb.0:
	mov	w8, #8                          // =0x8
	mov	x9, #17                         // =0x11
	mov	w10, #85                        // =0x55
.LBB0_1:                                // =>This Inner Loop Header: Depth=1
	eor	x11, x0, x0, lsr #11
	orr	x12, x1, #0x1
	madd	x11, x11, x1, x9
	ror	x11, x11, #55
	udiv	x13, x11, x12
	eor	x11, x13, x11
	udiv	x13, x11, x12
	msub	x12, x13, x12, x11
	add	x11, x11, x12
	eor	x12, x11, x10
	add	x13, x11, x1
	cmp	x11, x1
	csel	x0, x12, x13, lo
	subs	w8, w8, #1
	eor	x11, x0, x1, lsr #3
	orr	x1, x11, #0x1
	b.ne	.LBB0_1
// %bb.2:
	ret
.Lfunc_end0:
	.size	work64, .Lfunc_end0-work64
                                        // -- End function
	.globl	work32                          // -- Begin function work32
	.p2align	2
	.type	work32,@function
work32:                                 // @work32
// %bb.0:
	mov	w8, #8                          // =0x8
	mov	w9, #31                         // =0x1f
	mov	w10, #170                       // =0xaa
.LBB1_1:                                // =>This Inner Loop Header: Depth=1
	eor	w11, w0, w0, lsr #7
	orr	w12, w1, #0x1
	madd	w11, w11, w1, w9
	ror	w11, w11, #27
	udiv	w13, w11, w12
	eor	w11, w13, w11
	udiv	w13, w11, w12
	msub	w12, w13, w12, w11
	add	w11, w11, w12
	eor	w12, w11, w10
	add	w13, w11, w1
	cmp	w11, w1
	csel	w0, w12, w13, lo
	subs	w8, w8, #1
	eor	w11, w0, w1, lsr #3
	orr	w1, w11, #0x1
	b.ne	.LBB1_1
// %bb.2:
	ret
.Lfunc_end1:
	.size	work32, .Lfunc_end1-work32
                                        // -- End function
	.type	seed_a,@object                  // @seed_a
	.section	.rodata,"a",@progbits
	.globl	seed_a
	.p2align	3, 0x0
seed_a:
	.xword	-81985529216486896              // 0xfedcba9876543210
	.size	seed_a, 8

	.type	seed_b,@object                  // @seed_b
	.globl	seed_b
	.p2align	3, 0x0
seed_b:
	.xword	19088743                        // 0x1234567
	.size	seed_b, 8

	.type	seed_c,@object                  // @seed_c
	.globl	seed_c
	.p2align	2, 0x0
seed_c:
	.word	4045620583                      // 0xf1234567
	.size	seed_c, 4

	.type	seed_d,@object                  // @seed_d
	.globl	seed_d
	.p2align	2, 0x0
seed_d:
	.word	23                              // 0x17
	.size	seed_d, 4

	.type	observed64,@object              // @observed64
	.bss
	.globl	observed64
	.p2align	3, 0x0
observed64:
	.xword	0                               // 0x0
	.size	observed64, 8

	.type	observed32,@object              // @observed32
	.globl	observed32
	.p2align	2, 0x0
observed32:
	.word	0                               // 0x0
	.size	observed32, 4

	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
