	.text
	.file	"work.c"
	.globl	work                            // -- Begin function work
	.p2align	2
	.type	work,@function
work:                                   // @work
.Lwork$local:
	.type	.Lwork$local,@function
// %bb.0:                               // %entry
	sub	sp, sp, #112
	stp	x29, x30, [sp, #16]             // 16-byte Folded Spill
	stp	x28, x27, [sp, #32]             // 16-byte Folded Spill
	stp	x26, x25, [sp, #48]             // 16-byte Folded Spill
	stp	x24, x23, [sp, #64]             // 16-byte Folded Spill
	stp	x22, x21, [sp, #80]             // 16-byte Folded Spill
	stp	x20, x19, [sp, #96]             // 16-byte Folded Spill
	str	wzr, [sp, #12]
	bl	client_tls
	adrp	x22, :got:report
	adrp	x23, m
	mov	w8, #2                          // =0x2
	ldr	x22, [x22, :got_lo12:report]
	str	x0, [x23, :lo12:m]
	adrp	x0, .L.str
	add	x0, x0, :lo12:.L.str
	add	x1, sp, #12
	str	x8, [x22]
	bl	client_connect
	cbz	w0, .LBB0_2
.LBB0_1:                                // %cleanup188
	ldp	x20, x19, [sp, #96]             // 16-byte Folded Reload
	ldp	x22, x21, [sp, #80]             // 16-byte Folded Reload
	ldp	x24, x23, [sp, #64]             // 16-byte Folded Reload
	ldp	x26, x25, [sp, #48]             // 16-byte Folded Reload
	ldp	x28, x27, [sp, #32]             // 16-byte Folded Reload
	ldp	x29, x30, [sp, #16]             // 16-byte Folded Reload
	add	sp, sp, #112
	ret
.LBB0_2:                                // %if.end
	ldr	x8, [x23, :lo12:m]
	mov	x9, xzr
.LBB0_3:                                // %for.body.i
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_3
// %bb.4:                               // %clear.exit
	mov	w9, #4                          // =0x4
	mov	w10, #10                        // =0xa
	mov	w24, #18003                     // =0x4653
	movk	w10, #32768, lsl #16
	str	w9, [x8]
	mov	w9, #1                          // =0x1
	movk	w24, #18755, lsl #16
	str	w10, [x8, #4]
	str	w9, [x8, #8]
	str	w24, [x8, #32]
	ldr	w0, [sp, #12]
	bl	client_send
	cbnz	w0, .LBB0_7
// %bb.5:                               // %land.lhs.true.i
	ldr	x8, [x23, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_7
// %bb.6:                               // %land.rhs.i
	ldr	w8, [x8, #24]
	cmp	w8, #0
	cset	w8, eq
	cbnz	w8, .LBB0_8
	b	.LBB0_1
.LBB0_7:
	mov	w8, wzr
	cbz	w8, .LBB0_1
.LBB0_8:                                // %if.end8
	ldr	w0, [sp, #12]
	ldr	x8, [x23, :lo12:m]
	mov	x9, xzr
.LBB0_9:                                // %for.body.i.i
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_9
// %bb.10:                              // %clear.exit.i
	mov	w10, #4                         // =0x4
	mov	w11, #10                        // =0xa
	mov	x9, xzr
	str	w10, [x8]
	mov	w12, #1                         // =0x1
	add	x10, x8, #32
	str	w11, [x8, #4]
	adrp	x11, .L.str.1
	add	x11, x11, :lo12:.L.str.1
	str	w24, [x8, #16]
	str	w12, [x8, #24]
.LBB0_11:                               // %for.body.i241
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w8, [x11, x9]
	strb	w8, [x10, x9]
	add	x9, x9, #1
	cmp	x9, #6
	b.ne	.LBB0_11
// %bb.12:                              // %for.cond.cleanup.i
	bl	client_send
	cbnz	w0, .LBB0_17
// %bb.13:                              // %land.lhs.true.i.i
	ldr	x8, [x23, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_17
// %bb.14:                              // %land.rhs.i.i
	ldr	w9, [x8, #24]
	cbnz	w9, .LBB0_17
// %bb.15:                              // %lor.lhs.false.i
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_17
// %bb.16:                              // %if.end.i
	ldr	w20, [x8, #12]
	b	.LBB0_18
.LBB0_17:
	mov	w20, wzr
.LBB0_18:                               // %get_service.exit
	ldr	w0, [sp, #12]
	ldr	x8, [x23, :lo12:m]
	mov	x9, xzr
.LBB0_19:                               // %for.body.i.i244
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_19
// %bb.20:                              // %clear.exit.i249
	mov	w10, #4                         // =0x4
	mov	w11, #10                        // =0xa
	mov	x9, xzr
	str	w10, [x8]
	mov	w12, #1                         // =0x1
	add	x10, x8, #32
	str	w11, [x8, #4]
	adrp	x11, .L.str.2
	add	x11, x11, :lo12:.L.str.2
	str	w24, [x8, #16]
	str	w12, [x8, #24]
.LBB0_21:                               // %land.rhs.i253
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w8, [x11, x9]
	strb	w8, [x10, x9]
	add	x9, x9, #1
	cmp	x9, #8
	b.ne	.LBB0_21
// %bb.22:                              // %for.cond.cleanup.i261
	bl	client_send
	cbnz	w0, .LBB0_27
// %bb.23:                              // %land.lhs.true.i.i265
	ldr	x8, [x23, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_27
// %bb.24:                              // %land.rhs.i.i268
	ldr	w9, [x8, #24]
	cbnz	w9, .LBB0_27
// %bb.25:                              // %lor.lhs.false.i271
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_27
// %bb.26:                              // %if.end.i274
	ldr	w19, [x8, #12]
	b	.LBB0_28
.LBB0_27:
	mov	w19, wzr
.LBB0_28:                               // %get_service.exit276
	cbz	w20, .LBB0_1
// %bb.29:                              // %get_service.exit276
	cbz	w19, .LBB0_1
// %bb.30:                              // %lor.lhs.false13
	ldr	w0, [sp, #12]
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.31:                              // %if.end17
	mov	w8, #3                          // =0x3
	mov	x9, xzr
	str	x8, [x22]
	ldr	x8, [x23, :lo12:m]
.LBB0_32:                               // %for.body.i277
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_32
// %bb.33:                              // %clear.exit283
	mov	w25, #4                         // =0x4
	adrp	x10, .L.str.3
	add	x10, x10, :lo12:.L.str.3
	movk	w25, #16, lsl #16
	mov	w9, #8                          // =0x8
	lsr	x11, x10, #34
	lsr	x12, x10, #4
	str	w25, [x8]
	mov	w0, w20
	str	w9, [x8, #4]
	mov	w9, #11                         // =0xb
	str	w9, [x8, #8]
	and	w9, w11, #0x3ffffffc
	and	w11, w12, #0xf0000000
	orr	w9, w9, w11
	str	w10, [x8, #12]
	str	w9, [x8, #16]
	str	w24, [x8, #32]
	bl	client_send
	cbnz	w0, .LBB0_1
// %bb.34:                              // %land.lhs.true.i289
	ldr	x8, [x23, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_1
// %bb.35:                              // %land.rhs.i292
	ldr	w8, [x8, #24]
	cbnz	w8, .LBB0_1
// %bb.36:                              // %lor.lhs.false23
	ldr	x8, [x23, :lo12:m]
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_1
// %bb.37:                              // %if.end26
	ldr	w21, [x8, #12]
	mov	w0, w20
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.38:                              // %if.end31
	ldr	x8, [x23, :lo12:m]
	mov	x9, xzr
.LBB0_39:                               // %for.body.i296
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_39
// %bb.40:                              // %clear.exit302
	add	w9, w25, #3840, lsl #12         // =15728640
	adrp	x12, sprite
	add	x12, x12, :lo12:sprite
	lsr	x11, x12, #34
	str	w9, [x8]
	lsr	x9, x12, #4
	mov	w10, #12                        // =0xc
	mov	w0, w21
	and	w11, w11, #0x3ffffffc
	and	w9, w9, #0xf0000000
	str	w10, [x8, #4]
	mov	w10, #272                       // =0x110
	orr	w9, w11, w9
	str	w10, [x8, #8]
	str	w12, [x8, #12]
	str	w9, [x8, #16]
	str	w24, [x8, #32]
	str	wzr, [x8, #48]
	str	wzr, [x8, #52]
	str	w10, [x8, #56]
	str	wzr, [x8, #60]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.41:                              // %lor.lhs.false37
	ldr	x8, [x23, :lo12:m]
	ldr	w9, [x8, #32]
	ldr	w8, [x8, #36]
	orr	x8, x9, x8, lsl #32
	cmp	x8, #272
	b.ne	.LBB0_1
// %bb.42:                              // %lor.lhs.false40
	mov	w0, w21
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.43:                              // %if.end44
	mov	w10, #272                       // =0x110
	mov	x9, xzr
	mov	w8, wzr
	str	x10, [x22, #8]
	adrp	x10, sprite
	add	x10, x10, :lo12:sprite
.LBB0_44:                               // %for.cond46.preheader
                                        // =>This Loop Header: Depth=1
                                        //     Child Loop BB0_46 Depth 2
	mov	x11, xzr
	b	.LBB0_46
.LBB0_45:                               // %for.inc
                                        //   in Loop: Header=BB0_46 Depth=2
	add	x11, x11, #1
	cmp	x11, #16
	b.eq	.LBB0_48
.LBB0_46:                               // %for.body49
                                        //   Parent Loop BB0_44 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldrb	w12, [x10, x11]
	cmp	w12, #46
	b.eq	.LBB0_45
// %bb.47:                              // %land.lhs.true
                                        //   in Loop: Header=BB0_46 Depth=2
	ldrb	w12, [x10, x11]
	cmp	w12, #35
	b.eq	.LBB0_45
	b	.LBB0_50
.LBB0_48:                               // %for.end
                                        //   in Loop: Header=BB0_44 Depth=1
	add	x11, x9, x9, lsl #4
	adrp	x12, sprite
	add	x12, x12, :lo12:sprite
	add	x11, x11, x12
	ldrb	w11, [x11, #16]
	cmp	w11, #10
	b.ne	.LBB0_50
// %bb.49:                              // %for.cond
                                        //   in Loop: Header=BB0_44 Depth=1
	cmp	x9, #14
	add	x9, x9, #1
	add	x10, x10, #17
	cset	w8, hi
	cmp	x9, #16
	b.ne	.LBB0_44
	b	.LBB0_51
.LBB0_50:                               // %cleanup73
	tbz	w8, #0, .LBB0_1
.LBB0_51:                               // %for.end75
	mov	w12, #4                         // =0x4
	mov	w11, #403                       // =0x193
	mov	w13, #40389                     // =0x9dc5
	mov	w14, #43691                     // =0xaaab
	mov	w15, #30739                     // =0x7813
	mov	w7, wzr
	mov	w9, wzr
	mov	x10, xzr
	movk	w11, #256, lsl #16
	str	x12, [x22]
	movk	w13, #33052, lsl #16
	adrp	x12, pixels+2
	add	x12, x12, :lo12:pixels+2
	movk	w14, #43690, lsl #16
	movk	w15, #8699, lsl #16
	mov	w16, #113                       // =0x71
	mov	w17, #-116                      // =0xffffff8c
	mov	w18, #-86                       // =0xffffffaa
	mov	w0, #-46                        // =0xffffffd2
	adrp	x8, sprite
	add	x8, x8, :lo12:sprite
	b	.LBB0_53
.LBB0_52:                               // %for.cond.cleanup86
                                        //   in Loop: Header=BB0_53 Depth=1
	ldr	w7, [sp, #8]                    // 4-byte Folded Reload
	add	x10, x10, #1
	add	x12, x12, #480
	cmp	x10, #96
	sub	w9, w9, #7
	add	w7, w7, #7
	b.eq	.LBB0_58
.LBB0_53:                               // %for.cond83.preheader
                                        // =>This Loop Header: Depth=1
                                        //     Child Loop BB0_55 Depth 2
	and	x1, x10, #0xffffffff
	mov	x3, #-48                        // =0xffffffffffffffd0
	mov	w21, w9
	umull	x2, w1, w14
	sub	x1, x10, #16
	mov	x26, x12
	ubfx	x4, x1, #2, #30
	lsl	w5, w1, #1
	str	w7, [sp, #8]                    // 4-byte Folded Spill
	lsr	x6, x2, #33
	add	w4, w4, w4, lsl #4
	neg	w2, w10
	add	w5, w5, #40
	add	w6, w6, #24
	b	.LBB0_55
.LBB0_54:                               // %if.end137
                                        //   in Loop: Header=BB0_55 Depth=2
	and	w20, w29, #0xff
	add	x3, x3, #1
	sub	w21, w21, #13
	eor	w13, w13, w20
	and	w20, w27, #0xff
	cmp	x3, #112
	mul	w13, w13, w11
	add	w7, w7, #13
	sturb	w29, [x26, #-2]
	sturb	w27, [x26, #-1]
	strb	w28, [x26], #3
	eor	w13, w13, w20
	and	w20, w28, #0xff
	mul	w13, w13, w11
	eor	w13, w13, w20
	mul	w13, w13, w11
	b.eq	.LBB0_52
.LBB0_55:                               // %for.body87
                                        //   Parent Loop BB0_53 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	umull	x27, w7, w15
	add	x30, x3, #48
	orr	w20, w3, w1
	lsr	x29, x30, #2
	lsr	x27, x27, #32
	add	w29, w29, #16
	sub	w28, w7, w27
	add	w27, w27, w28, lsr #1
	add	w28, w10, w3
	add	w28, w28, #48
	lsr	w27, w27, #6
	umull	x28, w28, w14
	madd	w27, w27, w16, w21
	lsr	x28, x28, #34
	cmp	w27, #0
	add	w28, w28, #48
	csel	w29, w17, w29, eq
	csel	w27, w18, w6, eq
	csel	w28, w0, w28, eq
	tst	x20, #0xffffffc0
	b.ne	.LBB0_54
// %bb.56:                              // %land.lhs.true115
                                        //   in Loop: Header=BB0_55 Depth=2
	add	w20, w4, w3, lsr #2
	ldrb	w20, [x8, w20, uxtw]
	cmp	w20, #35
	b.ne	.LBB0_54
// %bb.57:                              // %if.then126
                                        //   in Loop: Header=BB0_55 Depth=2
	add	w27, w30, #72
	mov	w29, w2
	mov	w28, w5
	b	.LBB0_54
.LBB0_58:                               // %for.cond.cleanup80
	str	x13, [x22, #24]
	mov	x9, xzr
	ldr	x8, [x23, :lo12:m]
.LBB0_59:                               // %for.body.i316
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_59
// %bb.60:                              // %clear.exit322
	adrp	x9, pixels
	add	x9, x9, :lo12:pixels
	mov	w10, #10                        // =0xa
	lsr	x11, x9, #34
	lsr	x12, x9, #4
	str	w25, [x8]
	str	w10, [x8, #4]
	mov	w10, #46080                     // =0xb400
	mov	w0, w19
	str	w10, [x8, #8]
	and	w10, w11, #0x3ffffffc
	and	w11, w12, #0xf0000000
	str	w9, [x8, #12]
	orr	w9, w10, w11
	str	w9, [x8, #16]
	mov	w9, #160                        // =0xa0
	str	w24, [x8, #32]
	str	w9, [x8, #48]
	mov	w9, #96                         // =0x60
	str	w9, [x8, #52]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.61:                              // %lor.lhs.false173
	ldr	x8, [x23, :lo12:m]
	ldr	w9, [x8, #32]
	ldr	w10, [x8, #36]
	orr	x9, x9, x10, lsl #32
	mov	w10, #46080                     // =0xb400
	cmp	x9, x10
	b.ne	.LBB0_1
// %bb.62:                              // %if.end178
	ldr	w9, [x8, #32]
	ldr	w8, [x8, #36]
	mov	w0, w19
	orr	x8, x9, x8, lsl #32
	str	x8, [x22, #16]
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.63:                              // %if.end183
	mov	w8, #1                          // =0x1
	str	x8, [x22]
	b	.LBB0_1
.Lfunc_end0:
	.size	work, .Lfunc_end0-work
	.size	.Lwork$local, .Lfunc_end0-work
                                        // -- End function
	.p2align	2                               // -- Begin function reply
	.type	reply,@function
reply:                                  // @reply
// %bb.0:                               // %entry
	str	x30, [sp, #-16]!                // 8-byte Folded Spill
	bl	client_send
	cbnz	w0, .LBB1_3
// %bb.1:                               // %land.lhs.true
	adrp	x8, m
	mov	w10, #18003                     // =0x4653
	ldr	x8, [x8, :lo12:m]
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB1_3
// %bb.2:                               // %land.rhs
	ldr	w8, [x8, #24]
	cmp	w8, #0
	cset	w0, eq
	ldr	x30, [sp], #16                  // 8-byte Folded Reload
	ret
.LBB1_3:
	mov	w0, wzr
	ldr	x30, [sp], #16                  // 8-byte Folded Reload
	ret
.Lfunc_end1:
	.size	reply, .Lfunc_end1-reply
                                        // -- End function
	.type	report,@object                  // @report
	.data
	.globl	report
	.p2align	3, 0x0
report:
	.xword	99                              // 0x63
	.xword	0                               // 0x0
	.xword	0                               // 0x0
	.xword	0                               // 0x0
	.size	report, 32

	.type	m,@object                       // @m
	.local	m
	.comm	m,8,8
	.type	.L.str,@object                  // @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"sm:"
	.size	.L.str, 4

	.type	.L.str.1,@object                // @.str.1
.L.str.1:
	.asciz	"emu:fs"
	.size	.L.str.1, 7

	.type	.L.str.2,@object                // @.str.2
.L.str.2:
	.asciz	"emu:disp"
	.size	.L.str.2, 9

	.type	.L.str.3,@object                // @.str.3
.L.str.3:
	.asciz	"sprite.txt"
	.size	.L.str.3, 11

	.type	sprite,@object                  // @sprite
	.local	sprite
	.comm	sprite,272,1
	.type	pixels,@object                  // @pixels
	.local	pixels
	.comm	pixels,46080,1
	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
