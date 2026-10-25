	.text
	.file	"work.c"
	.globl	fp_work                         // -- Begin function fp_work
	.p2align	2
	.type	fp_work,@function
fp_work:                                // @fp_work
.Lfp_work$local:
	.type	.Lfp_work$local,@function
// %bb.0:                               // %entry
	sub	sp, sp, #16
	fmov	s3, #1.50000000
	fmov	d0, #-2.00000000
	mov	x8, xzr
	fmov	s1, #1.25000000
	fmov	d2, #0.50000000
	adrp	x9, singles
	add	x9, x9, :lo12:singles
	adrp	x10, doubles
	add	x10, x10, :lo12:doubles
.LBB0_1:                                // %for.body
                                        // =>This Inner Loop Header: Depth=1
	fmul	s3, s3, s1
	fmul	d0, d0, d2
	str	s3, [sp, #12]
	str	d0, [sp]
	ldr	s0, [sp, #12]
	ldr	s3, [x9, x8, lsl #2]
	ldr	d4, [sp]
	ldr	d5, [x10, x8, lsl #3]
	add	x8, x8, #1
	fadd	s3, s0, s3
	cmp	x8, #8
	fadd	d0, d4, d5
	b.ne	.LBB0_1
// %bb.2:                               // %for.cond.cleanup
	fcmp	s3, #0.0
	str	s3, [x0]
	cset	w8, gt
	str	w8, [x0, #4]
	str	d0, [x0, #8]
	add	sp, sp, #16
	ret
.Lfunc_end0:
	.size	fp_work, .Lfunc_end0-fp_work
	.size	.Lfp_work$local, .Lfunc_end0-fp_work
                                        // -- End function
	.globl	conversion_work                 // -- Begin function conversion_work
	.p2align	2
	.type	conversion_work,@function
conversion_work:                        // @conversion_work
.Lconversion_work$local:
	.type	.Lconversion_work$local,@function
// %bb.0:                               // %entry
	scvtf	d0, x1
	fmov	d1, #-0.50000000
	ucvtf	s2, x2
	ucvtf	d3, x2
	fmul	d1, d0, d1
	fcvtzs	w8, s2
	str	d0, [x0]
	fcvt	d4, s2
	str	s2, [x0, #8]
	fcvt	s2, d0
	fcvtzu	x9, d1
	fmov	d1, #0.50000000
	str	w8, [x0, #12]
	fmadd	d0, d0, d1, d3
	str	x9, [x0, #16]
	str	d4, [x0, #24]
	str	s2, [x0, #32]
	str	wzr, [x0, #36]
	str	d0, [x0, #40]
	ret
.Lfunc_end1:
	.size	conversion_work, .Lfunc_end1-conversion_work
	.size	.Lconversion_work$local, .Lfunc_end1-conversion_work
                                        // -- End function
	.type	singles,@object                 // @singles
	.section	.rodata.cst32,"aM",@progbits,32
	.p2align	2, 0x0
singles:
	.word	0x3f800000                      // float 1
	.word	0xc0200000                      // float -2.5
	.word	0x3e000000                      // float 0.125
	.word	0x40800000                      // float 4
	.word	0xc1000000                      // float -8
	.word	0x40500000                      // float 3.25
	.word	0x40000000                      // float 2
	.word	0xbf000000                      // float -0.5
	.size	singles, 32

	.type	doubles,@object                 // @doubles
	.section	.rodata,"a",@progbits
	.p2align	3, 0x0
doubles:
	.xword	0xbff0000000000000              // double -1
	.xword	0x4000000000000000              // double 2
	.xword	0xc012000000000000              // double -4.5
	.xword	0x4020000000000000              // double 8
	.xword	0xbfc0000000000000              // double -0.125
	.xword	0x4030000000000000              // double 16
	.xword	0xc040000000000000              // double -32
	.xword	0x3fd0000000000000              // double 0.25
	.size	doubles, 64

	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
