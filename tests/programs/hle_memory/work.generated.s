	.text
	.file	"work.c"
	.globl	fill_heap                       // -- Begin function fill_heap
	.p2align	2
	.type	fill_heap,@function
fill_heap:                              // @fill_heap
// %bb.0:
	sub	sp, sp, #16
	mov	x8, xzr
	mov	w9, #10                         // =0xa
	str	x9, [sp, #8]
.LBB0_1:                                // =>This Inner Loop Header: Depth=1
	ldr	x9, [sp, #8]
	ldr	x10, [sp, #8]
	sub	x10, x10, #1
	add	x8, x9, x8
	str	x10, [sp, #8]
	ldr	x10, [sp, #8]
	cbnz	x10, .LBB0_1
// %bb.2:
	adrp	x9, result
	str	x8, [x0]
	str	x8, [x1, #384]
	str	x8, [x9, :lo12:result]
	adrp	x8, last_tls
	str	x1, [x8, :lo12:last_tls]
	add	sp, sp, #16
	ret
.Lfunc_end0:
	.size	fill_heap, .Lfunc_end0-fill_heap
                                        // -- End function
	.type	expected,@object                // @expected
	.section	.rodata,"a",@progbits
	.globl	expected
	.p2align	3, 0x0
expected:
	.xword	55                              // 0x37
	.size	expected, 8

	.type	result,@object                  // @result
	.bss
	.globl	result
	.p2align	3, 0x0
result:
	.xword	0                               // 0x0
	.size	result, 8

	.type	last_tls,@object                // @last_tls
	.globl	last_tls
	.p2align	3, 0x0
last_tls:
	.xword	0                               // 0x0
	.size	last_tls, 8

	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym result
	.addrsig_sym last_tls
