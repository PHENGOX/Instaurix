	.text
	.file	"work.c"
	.globl	work                            // -- Begin function work
	.p2align	2
	.type	work,@function
work:                                   // @work
.Lwork$local:
	.type	.Lwork$local,@function
// %bb.0:                               // %entry
	sub	sp, sp, #80
	stp	x30, x25, [sp, #16]             // 16-byte Folded Spill
	stp	x24, x23, [sp, #32]             // 16-byte Folded Spill
	stp	x22, x21, [sp, #48]             // 16-byte Folded Spill
	stp	x20, x19, [sp, #64]             // 16-byte Folded Spill
	str	wzr, [sp, #12]
	bl	client_tls
	adrp	x22, :got:report
	adrp	x23, message
	mov	w8, #2                          // =0x2
	ldr	x22, [x22, :got_lo12:report]
	str	x0, [x23, :lo12:message]
	adrp	x0, .L.str
	add	x0, x0, :lo12:.L.str
	add	x1, sp, #12
	str	x8, [x22]
	bl	client_connect
	cbz	w0, .LBB0_2
.LBB0_1:                                // %cleanup126
	ldp	x20, x19, [sp, #64]             // 16-byte Folded Reload
	ldp	x22, x21, [sp, #48]             // 16-byte Folded Reload
	ldp	x24, x23, [sp, #32]             // 16-byte Folded Reload
	ldp	x30, x25, [sp, #16]             // 16-byte Folded Reload
	add	sp, sp, #80
	ret
.LBB0_2:                                // %if.end
	ldr	w0, [sp, #12]
	ldr	x8, [x23, :lo12:message]
	mov	x9, xzr
.LBB0_3:                                // %for.body.i.i.i
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_3
// %bb.4:                               // %request.exit.i
	mov	w9, #5                          // =0x5
	mov	w24, #18003                     // =0x4653
	mov	w10, #8                         // =0x8
	str	w9, [x8]
	movk	w24, #18755, lsl #16
	mov	w9, #2                          // =0x2
	str	w10, [x8, #4]
	str	w24, [x8, #16]
	str	w9, [x8, #24]
	bl	client_send
	cbnz	w0, .LBB0_10
// %bb.5:                               // %land.lhs.true.i.i
	ldr	x8, [x23, :lo12:message]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_10
// %bb.6:                               // %land.rhs.i.i
	ldr	w9, [x8, #24]
	cbnz	w9, .LBB0_10
// %bb.7:                               // %lor.lhs.false.i
	ldr	w9, [x8, #4]
	mov	w10, #8                         // =0x8
	movk	w10, #32768, lsl #16
	cmp	w9, w10
	b.ne	.LBB0_10
// %bb.8:                               // %lor.lhs.false1.i
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_10
// %bb.9:                               // %if.end.i
	ldr	w19, [x8, #12]
	b	.LBB0_11
.LBB0_10:
	mov	w19, wzr
.LBB0_11:                               // %clone.exit
	cbz	w19, .LBB0_1
// %bb.12:                              // %if.end5
	ldr	x8, [x23, :lo12:message]
	mov	x9, xzr
.LBB0_13:                               // %for.body.i
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_13
// %bb.14:                              // %clear.exit
	mov	w25, #8                         // =0x8
	mov	w9, #4                          // =0x4
	mov	w10, #1                         // =0x1
	movk	w25, #32768, lsl #16
	str	w9, [x8]
	orr	w9, w25, #0x2
	str	w9, [x8, #4]
	str	w10, [x8, #8]
	str	w24, [x8, #32]
	ldr	w0, [sp, #12]
	bl	client_send
	cbnz	w0, .LBB0_17
// %bb.15:                              // %land.lhs.true.i
	ldr	x8, [x23, :lo12:message]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_17
// %bb.16:                              // %land.rhs.i
	ldr	w8, [x8, #24]
	cmp	w8, #0
	cset	w8, eq
	b	.LBB0_18
.LBB0_17:
	mov	w8, wzr
.LBB0_18:                               // %reply.exit
	cbz	w8, .LBB0_1
// %bb.19:                              // %if.end12
	ldr	w20, [sp, #12]
	ldr	x8, [x23, :lo12:message]
	mov	x9, xzr
.LBB0_20:                               // %for.body.i.i
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_20
// %bb.21:                              // %clear.exit.i
	mov	w9, #2                          // =0x2
	mov	w0, w20
	str	w9, [x8]
	bl	client_send
	cbnz	w0, .LBB0_24
// %bb.22:                              // %lor.lhs.false.i157
	mov	w0, w20
	bl	client_send
	mov	w8, #5631                       // =0x15ff
	cmp	w0, w8
	b.ne	.LBB0_24
// %bb.23:                              // %if.end.i158
	mov	w0, w20
	bl	client_close_handle
	cmp	w0, #0
	cset	w8, eq
	b	.LBB0_25
.LBB0_24:
	mov	w8, wzr
.LBB0_25:                               // %close_session.exit
	cbz	w8, .LBB0_1
// %bb.26:                              // %if.end16
	mov	w8, #1                          // =0x1
	mov	w9, #3                          // =0x3
	str	x8, [x22, #48]
	str	x9, [x22]
	mov	x9, xzr
	ldr	x8, [x23, :lo12:message]
.LBB0_27:                               // %for.body.i.i159
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_27
// %bb.28:                              // %request.exit
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
.LBB0_29:                               // %for.body
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w8, [x11, x9]
	strb	w8, [x10, x9]
	add	x9, x9, #1
	cmp	x9, #8
	b.ne	.LBB0_29
// %bb.30:                              // %for.cond.cleanup
	mov	w0, w19
	bl	client_send
	cbnz	w0, .LBB0_33
// %bb.31:                              // %land.lhs.true.i169
	ldr	x8, [x23, :lo12:message]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_33
// %bb.32:                              // %land.rhs.i172
	ldr	w8, [x8, #24]
	cmp	w8, #0
	cset	w8, eq
	b	.LBB0_34
.LBB0_33:
	mov	w8, wzr
.LBB0_34:                               // %reply.exit175
	cbz	w8, .LBB0_1
// %bb.35:                              // %lor.lhs.false
	ldr	x8, [x23, :lo12:message]
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_1
// %bb.36:                              // %if.end25
	ldr	w21, [x8, #12]
	mov	x9, xzr
.LBB0_37:                               // %for.body.i.i.i176
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_37
// %bb.38:                              // %request.exit.i181
	mov	w9, #5                          // =0x5
	mov	w10, #8                         // =0x8
	mov	w0, w21
	str	w9, [x8]
	mov	w9, #2                          // =0x2
	str	w10, [x8, #4]
	str	w24, [x8, #16]
	str	w9, [x8, #24]
	bl	client_send
	cbnz	w0, .LBB0_44
// %bb.39:                              // %land.lhs.true.i.i188
	ldr	x8, [x23, :lo12:message]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_44
// %bb.40:                              // %land.rhs.i.i191
	ldr	w9, [x8, #24]
	cbnz	w9, .LBB0_44
// %bb.41:                              // %lor.lhs.false.i194
	ldr	w9, [x8, #4]
	cmp	w9, w25
	b.ne	.LBB0_44
// %bb.42:                              // %lor.lhs.false1.i197
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_44
// %bb.43:                              // %if.end.i200
	ldr	w20, [x8, #12]
	b	.LBB0_45
.LBB0_44:
	mov	w20, wzr
.LBB0_45:                               // %clone.exit202
	cbz	w20, .LBB0_1
// %bb.46:                              // %lor.lhs.false29
	mov	w0, w21
	bl	close_session
	cbz	w0, .LBB0_1
// %bb.47:                              // %if.end33
	ldr	x8, [x22, #48]
	mov	w9, #4                          // =0x4
	orr	x8, x8, #0x2
	str	x8, [x22, #48]
	str	x9, [x22]
	mov	x9, xzr
	ldr	x8, [x23, :lo12:message]
.LBB0_48:                               // %for.body.i.i203
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_48
// %bb.49:                              // %request.exit212
	mov	w9, #5                          // =0x5
	mov	w10, #8                         // =0x8
	mov	w0, w20
	str	w9, [x8]
	mov	w9, #3                          // =0x3
	str	w10, [x8, #4]
	str	w24, [x8, #16]
	str	w9, [x8, #24]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.50:                              // %lor.lhs.false36
	ldr	x8, [x23, :lo12:message]
	ldr	w9, [x8, #4]
	cmp	w9, #9
	b.ne	.LBB0_1
// %bb.51:                              // %lor.lhs.false39
	ldr	w9, [x8, #32]
	tst	w9, #0xffff
	b.ne	.LBB0_1
// %bb.52:                              // %if.end43
	ldr	w10, [x8, #32]
	mov	x9, xzr
	and	x10, x10, #0xffff
	str	x10, [x22, #32]
.LBB0_53:                               // %for.body.i.i213
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_53
// %bb.54:                              // %request.exit222
	mov	w9, #4                          // =0x4
	mov	w10, #12                        // =0xc
	mov	w0, w20
	str	w9, [x8]
	mov	w9, #20                         // =0x14
	str	w10, [x8, #4]
	str	w24, [x8, #16]
	str	wzr, [x8, #24]
	str	w9, [x8, #32]
	mov	w9, #35                         // =0x23
	str	wzr, [x8, #36]
	str	w9, [x8, #40]
	str	wzr, [x8, #44]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.55:                              // %lor.lhs.false48
	ldr	x8, [x23, :lo12:message]
	ldr	w9, [x8, #32]
	ldr	w10, [x8, #36]
	orr	x9, x9, x10, lsl #32
	cmp	x9, #55
	b.ne	.LBB0_1
// %bb.56:                              // %if.end53
	ldr	w9, [x8, #32]
	ldr	w11, [x8, #36]
	mov	x10, xzr
	orr	x9, x9, x11, lsl #32
	mov	w11, #5                         // =0x5
	str	x9, [x22, #8]
	adrp	x9, source
	add	x9, x9, :lo12:source
	str	x11, [x22]
.LBB0_57:                               // %for.body60
                                        // =>This Inner Loop Header: Depth=1
	add	w11, w10, w10, lsl #4
	add	w11, w11, #3
	strb	w11, [x9, x10]
	add	x10, x10, #1
	cmp	x10, #512
	b.ne	.LBB0_57
// %bb.58:                              // %for.body.i236.preheader
	mov	x10, xzr
.LBB0_59:                               // %for.body.i236
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x10]
	add	x10, x10, #4
	cmp	x10, #256
	b.ne	.LBB0_59
// %bb.60:                              // %clear.exit241
	mov	w10, #4                         // =0x4
	mov	w11, #8                         // =0x8
	lsr	x12, x9, #34
	movk	w10, #272, lsl #16
	adrp	x21, destination
	add	x21, x21, :lo12:destination
	str	w10, [x8]
	mov	w10, #512                       // =0x200
	mov	w0, w20
	str	w11, [x8, #4]
	lsr	x11, x9, #4
	str	w10, [x8, #8]
	str	w9, [x8, #12]
	and	w9, w12, #0x3ffffffc
	and	w11, w11, #0xf0000000
	orr	w9, w9, w11
	lsr	x11, x21, #34
	lsr	x12, x21, #4
	str	w9, [x8, #16]
	str	w10, [x8, #20]
	and	w9, w11, #0x3ffffffc
	and	w10, w12, #0xf0000000
	orr	w9, w9, w10
	str	w21, [x8, #24]
	str	w9, [x8, #28]
	mov	w9, #2                          // =0x2
	str	w24, [x8, #32]
	str	w9, [x8, #40]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.61:                              // %lor.lhs.false74
	ldr	x8, [x23, :lo12:message]
	ldr	w9, [x8, #32]
	ldr	w10, [x8, #36]
	orr	x9, x9, x10, lsl #32
	cmp	x9, #512
	b.ne	.LBB0_1
// %bb.62:                              // %if.end79
	ldr	w9, [x8, #32]
	ldr	w12, [x8, #36]
	mov	x10, xzr
	mov	w11, wzr
	orr	x9, x9, x12, lsl #32
	mov	w12, #403                       // =0x193
	movk	w12, #256, lsl #16
	str	x9, [x22, #16]
	mov	w9, #40389                      // =0x9dc5
	movk	w9, #33052, lsl #16
.LBB0_63:                               // %for.body86
                                        // =>This Inner Loop Header: Depth=1
	add	w14, w10, w10, lsl #4
	ldrb	w13, [x21, x10]
	add	w14, w14, #3
	cmp	w13, w14, uxtb
	b.ne	.LBB0_65
// %bb.64:                              // %if.end97
                                        //   in Loop: Header=BB0_63 Depth=1
	ldrb	w15, [x21, x10]
	eor	w9, w9, w15
	mul	w9, w9, w12
.LBB0_65:                               // %cleanup
                                        //   in Loop: Header=BB0_63 Depth=1
	cmp	w13, w14, uxtb
	b.ne	.LBB0_67
// %bb.66:                              // %for.cond82
                                        //   in Loop: Header=BB0_63 Depth=1
	cmp	x10, #510
	add	x10, x10, #1
	cset	w11, hi
	cmp	x10, #512
	b.ne	.LBB0_63
.LBB0_67:                               // %cleanup104
	tbz	w11, #0, .LBB0_1
// %bb.68:                              // %for.end106
	mov	x10, xzr
	mov	w9, w9
	mov	w11, #6                         // =0x6
	str	x9, [x22, #24]
	str	x11, [x22]
.LBB0_69:                               // %for.body.i.i262
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x10]
	add	x10, x10, #4
	cmp	x10, #256
	b.ne	.LBB0_69
// %bb.70:                              // %request.exit271
	mov	w9, #4                          // =0x4
	mov	w10, #8                         // =0x8
	mov	w0, w20
	str	w9, [x8]
	mov	w9, #1                          // =0x1
	str	w10, [x8, #4]
	str	w24, [x8, #16]
	str	w9, [x8, #24]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.71:                              // %if.end111
	ldr	x8, [x23, :lo12:message]
	mov	w0, w19
	ldr	w9, [x8, #32]
	ldr	w8, [x8, #36]
	orr	x8, x9, x8, lsl #32
	str	x8, [x22, #40]
	bl	close_session
	cbz	w0, .LBB0_1
// %bb.72:                              // %lor.lhs.false115
	mov	w0, w20
	bl	close_session
	cbz	w0, .LBB0_1
// %bb.73:                              // %if.end119
	ldr	x8, [x22, #48]
	mov	w9, #1                          // =0x1
	orr	x8, x8, #0x4
	str	x8, [x22, #48]
	str	x9, [x22]
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
	adrp	x8, message
	mov	w10, #18003                     // =0x4653
	ldr	x8, [x8, :lo12:message]
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
	.p2align	2                               // -- Begin function close_session
	.type	close_session,@function
close_session:                          // @close_session
// %bb.0:                               // %entry
	stp	x30, x19, [sp, #-16]!           // 16-byte Folded Spill
	adrp	x8, message
	mov	w19, w0
	mov	x9, xzr
	ldr	x8, [x8, :lo12:message]
.LBB2_1:                                // %for.body.i
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB2_1
// %bb.2:                               // %clear.exit
	mov	w9, #2                          // =0x2
	mov	w0, w19
	str	w9, [x8]
	bl	client_send
	cbnz	w0, .LBB2_5
// %bb.3:                               // %lor.lhs.false
	mov	w0, w19
	bl	client_send
	mov	w8, #5631                       // =0x15ff
	cmp	w0, w8
	b.ne	.LBB2_5
// %bb.4:                               // %if.end
	mov	w0, w19
	bl	client_close_handle
	cmp	w0, #0
	cset	w0, eq
	ldp	x30, x19, [sp], #16             // 16-byte Folded Reload
	ret
.LBB2_5:
	mov	w0, wzr
	ldp	x30, x19, [sp], #16             // 16-byte Folded Reload
	ret
.Lfunc_end2:
	.size	close_session, .Lfunc_end2-close_session
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
	.xword	0                               // 0x0
	.xword	0                               // 0x0
	.xword	0                               // 0x0
	.size	report, 56

	.type	message,@object                 // @message
	.local	message
	.comm	message,8,8
	.type	.L.str,@object                  // @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"sm:"
	.size	.L.str, 4

	.type	.L.str.1,@object                // @.str.1
.L.str.1:
	.asciz	"emu:demo"
	.size	.L.str.1, 9

	.type	source,@object                  // @source
	.local	source
	.comm	source,512,1
	.type	destination,@object             // @destination
	.local	destination
	.comm	destination,512,1
	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
