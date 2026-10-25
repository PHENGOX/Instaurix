	.text
	.file	"work.c"
	.globl	work                            // -- Begin function work
	.p2align	2
	.type	work,@function
work:                                   // @work
.Lwork$local:
	.type	.Lwork$local,@function
// %bb.0:                               // %entry
	sub	sp, sp, #64
	stp	x30, x23, [sp, #16]             // 16-byte Folded Spill
	stp	x22, x21, [sp, #32]             // 16-byte Folded Spill
	stp	x20, x19, [sp, #48]             // 16-byte Folded Spill
	str	wzr, [sp, #12]
	bl	client_tls
	adrp	x21, :got:report
	adrp	x22, m
	mov	w8, #2                          // =0x2
	ldr	x21, [x21, :got_lo12:report]
	str	x0, [x22, :lo12:m]
	adrp	x0, .L.str
	add	x0, x0, :lo12:.L.str
	add	x1, sp, #12
	str	x8, [x21]
	bl	client_connect
	cbz	w0, .LBB0_2
.LBB0_1:                                // %cleanup176
	ldp	x20, x19, [sp, #48]             // 16-byte Folded Reload
	ldp	x22, x21, [sp, #32]             // 16-byte Folded Reload
	ldp	x30, x23, [sp, #16]             // 16-byte Folded Reload
	add	sp, sp, #64
	ret
.LBB0_2:                                // %if.end
	ldr	x8, [x22, :lo12:m]
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
	mov	w23, #18003                     // =0x4653
	movk	w10, #32768, lsl #16
	str	w9, [x8]
	mov	w9, #1                          // =0x1
	movk	w23, #18755, lsl #16
	str	w10, [x8, #4]
	str	w9, [x8, #8]
	str	w23, [x8, #32]
	ldr	w0, [sp, #12]
	bl	client_send
	cbnz	w0, .LBB0_7
// %bb.5:                               // %land.lhs.true.i
	ldr	x8, [x22, :lo12:m]
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
	ldr	x8, [x22, :lo12:m]
	mov	x9, xzr
.LBB0_9:                                // %for.body.i.i
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_9
// %bb.10:                              // %request.exit
	mov	w10, #4                         // =0x4
	mov	w11, #8                         // =0x8
	mov	x9, xzr
	str	w10, [x8]
	mov	w10, #1                         // =0x1
	mov	w12, #10                        // =0xa
	str	w11, [x8, #4]
	adrp	x11, .L.str.1
	add	x11, x11, :lo12:.L.str.1
	str	w23, [x8, #16]
	str	w10, [x8, #24]
	add	x10, x8, #32
	str	w12, [x8, #4]
.LBB0_11:                               // %for.body
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w8, [x11, x9]
	strb	w8, [x10, x9]
	add	x9, x9, #1
	cmp	x9, #6
	b.ne	.LBB0_11
// %bb.12:                              // %for.cond.cleanup
	ldr	w0, [sp, #12]
	bl	client_send
	cbnz	w0, .LBB0_15
// %bb.13:                              // %land.lhs.true.i209
	ldr	x8, [x22, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_15
// %bb.14:                              // %land.rhs.i212
	ldr	w8, [x8, #24]
	cmp	w8, #0
	cset	w8, eq
	b	.LBB0_16
.LBB0_15:
	mov	w8, wzr
.LBB0_16:                               // %reply.exit215
	cbz	w8, .LBB0_1
// %bb.17:                              // %lor.lhs.false
	ldr	x8, [x22, :lo12:m]
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_1
// %bb.18:                              // %if.end18
	ldr	w20, [x8, #12]
	ldr	w0, [sp, #12]
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.19:                              // %if.end23
	mov	w8, #3                          // =0x3
	mov	x9, xzr
	str	x8, [x21]
	ldr	x8, [x22, :lo12:m]
.LBB0_20:                               // %for.body.i216
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_20
// %bb.21:                              // %clear.exit221
	mov	w9, #4                          // =0x4
	adrp	x11, .L.str.2
	add	x11, x11, :lo12:.L.str.2
	movk	w9, #16, lsl #16
	mov	w10, #8                         // =0x8
	lsr	x12, x11, #34
	str	w9, [x8]
	lsr	x9, x11, #4
	mov	w0, w20
	str	w10, [x8, #4]
	mov	w10, #11                        // =0xb
	str	w10, [x8, #8]
	and	w10, w12, #0x3ffffffc
	and	w9, w9, #0xf0000000
	orr	w9, w10, w9
	str	w11, [x8, #12]
	str	w9, [x8, #16]
	str	w23, [x8, #32]
	bl	client_send
	cbnz	w0, .LBB0_1
// %bb.22:                              // %land.lhs.true.i227
	ldr	x8, [x22, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_1
// %bb.23:                              // %land.rhs.i230
	ldr	w8, [x8, #24]
	cbnz	w8, .LBB0_1
// %bb.24:                              // %lor.lhs.false29
	ldr	x8, [x22, :lo12:m]
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_1
// %bb.25:                              // %if.end33
	ldr	w19, [x8, #12]
	mov	w0, w20
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.26:                              // %if.end38
	ldr	x8, [x22, :lo12:m]
	mov	x9, xzr
.LBB0_27:                               // %for.body.i.i234
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_27
// %bb.28:                              // %request.exit242
	mov	w9, #4                          // =0x4
	mov	w10, #8                         // =0x8
	mov	w0, w19
	str	w9, [x8]
	mov	w9, #1                          // =0x1
	str	w10, [x8, #4]
	str	w23, [x8, #16]
	str	w9, [x8, #24]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.29:                              // %lor.lhs.false41
	ldr	x9, [x22, :lo12:m]
	ldr	w8, [x9, #32]
	ldr	w10, [x9, #36]
	orr	x8, x8, x10, lsl #32
	cmp	x8, #30
	b.ne	.LBB0_1
// %bb.30:                              // %if.end45
	ldr	w10, [x9, #32]
	ldr	w9, [x9, #36]
	mov	x8, xzr
	adrp	x20, bytes
	add	x20, x20, :lo12:bytes
	orr	x9, x10, x9, lsl #32
	mov	w10, #4                         // =0x4
	str	x9, [x21, #8]
	mov	w9, #165                        // =0xa5
	str	x10, [x21]
.LBB0_31:                               // %for.body51
                                        // =>This Inner Loop Header: Depth=1
	strb	w9, [x20, x8]
	add	x8, x8, #1
	cmp	x8, #128
	b.ne	.LBB0_31
// %bb.32:                              // %for.cond.cleanup50
	mov	w0, w19
	mov	x1, xzr
	mov	w2, #128                        // =0x80
	bl	read_file
	cbz	w0, .LBB0_1
// %bb.33:                              // %lor.lhs.false59
	ldr	x8, [x22, :lo12:m]
	ldr	w10, [x8, #32]
	ldr	w11, [x8, #36]
	orr	x10, x10, x11, lsl #32
	cmp	x10, #30
	b.ne	.LBB0_1
// %bb.34:                              // %if.end63
	ldr	w10, [x8, #32]
	ldr	w11, [x8, #36]
	mov	w13, #403                       // =0x193
	mov	x9, x20
	mov	x12, xzr
	movk	w13, #256, lsl #16
	orr	x11, x10, x11, lsl #32
	mov	w10, #40389                     // =0x9dc5
	movk	w10, #33052, lsl #16
	str	x11, [x21, #16]
	adrp	x11, .L__const.work.expected
	add	x11, x11, :lo12:.L__const.work.expected
.LBB0_35:                               // %for.body70
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w14, [x9, x12]
	ldrb	w15, [x11, x12]
	cmp	w14, w15
	b.ne	.LBB0_1
// %bb.36:                              // %if.end80
                                        //   in Loop: Header=BB0_35 Depth=1
	ldrb	w14, [x9, x12]
	add	x12, x12, #1
	cmp	x12, #30
	eor	w10, w10, w14
	mul	w10, w10, w13
	b.ne	.LBB0_35
// %bb.37:                              // %for.body92.preheader
	mov	w11, #30                        // =0x1e
.LBB0_38:                               // %for.body92
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w12, [x9, x11]
	cmp	w12, #165
	b.ne	.LBB0_1
// %bb.39:                              // %for.cond88
                                        //   in Loop: Header=BB0_38 Depth=1
	add	x11, x11, #1
	cmp	x11, #128
	b.ne	.LBB0_38
// %bb.40:                              // %for.end104
	mov	x9, xzr
	str	x10, [x21, #24]
	mov	w10, #5                         // =0x5
	str	x10, [x21]
.LBB0_41:                               // %for.body.i264
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_41
// %bb.42:                              // %clear.exit269
	mov	w9, #5                          // =0x5
	mov	w10, #8                         // =0x8
	mov	w0, w19
	str	w9, [x8]
	mov	w9, #2                          // =0x2
	str	w10, [x8, #4]
	str	w23, [x8, #16]
	str	w9, [x8, #24]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.43:                              // %lor.lhs.false112
	ldr	x8, [x22, :lo12:m]
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_1
// %bb.44:                              // %if.end117
	ldr	w23, [x8, #12]
	mov	w0, w19
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.45:                              // %if.end122
	mov	w0, w23
	mov	w1, #7                          // =0x7
	mov	w2, #9                          // =0x9
	bl	read_file
	cbz	w0, .LBB0_1
// %bb.46:                              // %lor.lhs.false125
	ldr	x8, [x22, :lo12:m]
	ldr	w9, [x8, #32]
	ldr	w10, [x8, #36]
	orr	x9, x9, x10, lsl #32
	cmp	x9, #9
	b.ne	.LBB0_1
// %bb.47:                              // %for.body136.preheader
	mov	x9, xzr
.LBB0_48:                               // %for.body136
                                        // =>This Inner Loop Header: Depth=1
	adrp	x10, .L__const.work.expected
	add	x10, x10, :lo12:.L__const.work.expected
	mov	x11, x20
	add	x10, x10, x9
	ldrb	w11, [x20, x9]
	ldrb	w10, [x10, #7]
	cmp	w11, w10
	b.ne	.LBB0_1
// %bb.49:                              // %for.cond132
                                        //   in Loop: Header=BB0_48 Depth=1
	add	x9, x9, #1
	cmp	x9, #9
	b.ne	.LBB0_48
// %bb.50:                              // %for.end152
	ldr	w9, [x8, #32]
	ldr	w8, [x8, #36]
	adrp	x19, bytes
	mov	w0, w23
	mov	w2, #128                        // =0x80
	orr	x8, x9, x8, lsl #32
	mov	w9, #127                        // =0x7f
	str	x8, [x21, #32]
	strb	w9, [x19, :lo12:bytes]
	ldr	x1, [x21, #8]
	bl	read_file
	cbz	w0, .LBB0_1
// %bb.51:                              // %lor.lhs.false156
	ldr	x8, [x22, :lo12:m]
	ldr	w9, [x8, #32]
	ldr	w10, [x8, #36]
	orr	w9, w9, w10
	cbnz	w9, .LBB0_1
// %bb.52:                              // %lor.lhs.false160
	ldrb	w9, [x19, :lo12:bytes]
	cmp	w9, #127
	b.ne	.LBB0_1
// %bb.53:                              // %if.end165
	ldr	w9, [x8, #32]
	ldr	w8, [x8, #36]
	mov	w0, w23
	orr	x8, x9, x8, lsl #32
	str	x8, [x21, #40]
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.54:                              // %if.end170
	mov	w8, #1                          // =0x1
	str	x8, [x21]
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
	.p2align	2                               // -- Begin function read_file
	.type	read_file,@function
read_file:                              // @read_file
// %bb.0:                               // %entry
	stp	x30, x19, [sp, #-16]!           // 16-byte Folded Spill
	adrp	x19, m
	mov	x9, xzr
	ldr	x8, [x19, :lo12:m]
.LBB2_1:                                // %for.body.i
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB2_1
// %bb.2:                               // %clear.exit
	mov	w9, #4                          // =0x4
	adrp	x11, bytes
	add	x11, x11, :lo12:bytes
	movk	w9, #256, lsl #16
	mov	w10, #12                        // =0xc
	lsr	x12, x11, #34
	str	w9, [x8]
	lsr	x9, x11, #4
	str	w10, [x8, #4]
	mov	w10, #128                       // =0x80
	str	w10, [x8, #8]
	and	w10, w12, #0x3ffffffc
	and	w9, w9, #0xf0000000
	orr	w9, w10, w9
	str	w11, [x8, #12]
	str	w9, [x8, #16]
	mov	w9, #18003                      // =0x4653
	movk	w9, #18755, lsl #16
	str	w9, [x8, #32]
	lsr	x9, x1, #32
	str	w1, [x8, #48]
	str	w9, [x8, #52]
	lsr	x9, x2, #32
	str	w2, [x8, #56]
	str	w9, [x8, #60]
	bl	client_send
	cbnz	w0, .LBB2_5
// %bb.3:                               // %land.lhs.true.i
	ldr	x8, [x19, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB2_5
// %bb.4:                               // %land.rhs.i
	ldr	w8, [x8, #24]
	cmp	w8, #0
	cset	w0, eq
	ldp	x30, x19, [sp], #16             // 16-byte Folded Reload
	ret
.LBB2_5:
	mov	w0, wzr
	ldp	x30, x19, [sp], #16             // 16-byte Folded Reload
	ret
.Lfunc_end2:
	.size	read_file, .Lfunc_end2-read_file
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
	.xword	99                              // 0x63
	.size	report, 48

	.type	.L__const.work.expected,@object // @__const.work.expected
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__const.work.expected:
	.asciz	"Switch2Emu read-only fixture.\n"
	.size	.L__const.work.expected, 31

	.type	m,@object                       // @m
	.local	m
	.comm	m,8,8
	.type	.L.str,@object                  // @.str
.L.str:
	.asciz	"sm:"
	.size	.L.str, 4

	.type	.L.str.1,@object                // @.str.1
.L.str.1:
	.asciz	"emu:fs"
	.size	.L.str.1, 7

	.type	.L.str.2,@object                // @.str.2
.L.str.2:
	.asciz	"sample.txt"
	.size	.L.str.2, 11

	.type	bytes,@object                   // @bytes
	.local	bytes
	.comm	bytes,128,4
	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
