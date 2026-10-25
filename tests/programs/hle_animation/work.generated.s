	.text
	.file	"work.c"
	.globl	work                            // -- Begin function work
	.p2align	2
	.type	work,@function
work:                                   // @work
.Lwork$local:
	.type	.Lwork$local,@function
// %bb.0:                               // %entry
	sub	sp, sp, #160
	stp	x29, x30, [sp, #64]             // 16-byte Folded Spill
	stp	x28, x27, [sp, #80]             // 16-byte Folded Spill
	stp	x26, x25, [sp, #96]             // 16-byte Folded Spill
	stp	x24, x23, [sp, #112]            // 16-byte Folded Spill
	stp	x22, x21, [sp, #128]            // 16-byte Folded Spill
	stp	x20, x19, [sp, #144]            // 16-byte Folded Spill
	str	wzr, [sp, #60]
	bl	client_tls
	adrp	x22, :got:report
	adrp	x23, m
	mov	w8, #2                          // =0x2
	ldr	x22, [x22, :got_lo12:report]
	str	x0, [x23, :lo12:m]
	adrp	x0, .L.str
	add	x0, x0, :lo12:.L.str
	add	x1, sp, #60
	str	x8, [x22]
	bl	client_connect
	cbz	w0, .LBB0_2
.LBB0_1:                                // %cleanup211
	ldp	x20, x19, [sp, #144]            // 16-byte Folded Reload
	ldp	x22, x21, [sp, #128]            // 16-byte Folded Reload
	ldp	x24, x23, [sp, #112]            // 16-byte Folded Reload
	ldp	x26, x25, [sp, #96]             // 16-byte Folded Reload
	ldp	x28, x27, [sp, #80]             // 16-byte Folded Reload
	ldp	x29, x30, [sp, #64]             // 16-byte Folded Reload
	add	sp, sp, #160
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
	ldr	w0, [sp, #60]
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
	ldr	w0, [sp, #60]
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
.LBB0_11:                               // %for.body.i276
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
	ldr	w0, [sp, #60]
	ldr	x8, [x23, :lo12:m]
	mov	x9, xzr
.LBB0_19:                               // %for.body.i.i279
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_19
// %bb.20:                              // %clear.exit.i284
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
.LBB0_21:                               // %land.rhs.i288
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w8, [x11, x9]
	strb	w8, [x10, x9]
	add	x9, x9, #1
	cmp	x9, #8
	b.ne	.LBB0_21
// %bb.22:                              // %for.cond.cleanup.i296
	bl	client_send
	cbnz	w0, .LBB0_27
// %bb.23:                              // %land.lhs.true.i.i300
	ldr	x8, [x23, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_27
// %bb.24:                              // %land.rhs.i.i303
	ldr	w9, [x8, #24]
	cbnz	w9, .LBB0_27
// %bb.25:                              // %lor.lhs.false.i306
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_27
// %bb.26:                              // %if.end.i309
	ldr	w19, [x8, #12]
	b	.LBB0_28
.LBB0_27:
	mov	w19, wzr
.LBB0_28:                               // %get_service.exit311
	cbz	w20, .LBB0_1
// %bb.29:                              // %get_service.exit311
	cbz	w19, .LBB0_1
// %bb.30:                              // %lor.lhs.false13
	ldr	w0, [sp, #60]
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.31:                              // %if.end17
	mov	w8, #3                          // =0x3
	mov	x9, xzr
	str	x8, [x22]
	ldr	x8, [x23, :lo12:m]
.LBB0_32:                               // %for.body.i312
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_32
// %bb.33:                              // %clear.exit318
	mov	w12, #4                         // =0x4
	adrp	x10, .L.str.3
	add	x10, x10, :lo12:.L.str.3
	movk	w12, #16, lsl #16
	mov	w9, #8                          // =0x8
	lsr	x11, x10, #34
	str	w12, [x8]
	lsr	x12, x10, #4
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
// %bb.34:                              // %land.lhs.true.i324
	ldr	x8, [x23, :lo12:m]
	mov	w10, #18003                     // =0x4653
	movk	w10, #20291, lsl #16
	ldr	w9, [x8, #16]
	cmp	w9, w10
	b.ne	.LBB0_1
// %bb.35:                              // %land.rhs.i327
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
.LBB0_39:                               // %for.body.i331
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_39
// %bb.40:                              // %clear.exit337
	mov	w9, #4                          // =0x4
	adrp	x12, sprite
	add	x12, x12, :lo12:sprite
	movk	w9, #16, lsl #16
	lsr	x11, x12, #34
	mov	w10, #12                        // =0xc
	add	w9, w9, #3840, lsl #12          // =15728640
	mov	w0, w21
	str	w9, [x8]
	lsr	x9, x12, #4
	and	w11, w11, #0x3ffffffc
	str	w10, [x8, #4]
	mov	w10, #272                       // =0x110
	and	w9, w9, #0xf0000000
	str	w10, [x8, #8]
	orr	w9, w11, w9
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
	adrp	x9, pixels
	add	x9, x9, :lo12:pixels
	mov	w29, #403                       // =0x193
	lsr	x8, x9, #34
	lsr	x9, x9, #4
	mov	w28, #43691                     // =0xaaab
	mov	w21, #30739                     // =0x7813
	mov	w12, wzr
	mov	w13, wzr
	and	w8, w8, #0x3ffffffc
	and	w9, w9, #0xf0000000
	mov	x11, xzr
	mov	w10, #4                         // =0x4
	movk	w29, #256, lsl #16
	orr	w8, w8, w9
	movk	w28, #43690, lsl #16
	movk	w21, #8699, lsl #16
	mov	w26, #113                       // =0x71
	mov	w25, #-116                      // =0xffffff8c
	mov	w20, #-86                       // =0xffffffaa
	mov	w27, #-46                       // =0xffffffd2
	str	xzr, [sp, #48]                  // 8-byte Folded Spill
	str	x10, [x22]
	str	w8, [sp, #12]                   // 4-byte Folded Spill
	b	.LBB0_53
.LBB0_52:                               // %for.inc199
                                        //   in Loop: Header=BB0_53 Depth=1
	ldp	w13, w12, [sp, #28]             // 8-byte Folded Reload
	ldr	x8, [sp, #48]                   // 8-byte Folded Reload
	cmp	x11, #8
	sub	x8, x8, #12
	sub	w13, w13, #11
	add	w12, w12, #11
	str	x8, [sp, #48]                   // 8-byte Folded Spill
	b.eq	.LBB0_68
.LBB0_53:                               // %for.body80
                                        // =>This Loop Header: Depth=1
                                        //     Child Loop BB0_55 Depth 2
                                        //       Child Loop BB0_57 Depth 3
                                        //     Child Loop BB0_63 Depth 2
	add	x9, x11, x11, lsl #1
	str	x11, [sp, #16]                  // 8-byte Folded Spill
	mov	w11, #40389                     // =0x9dc5
	mov	x8, xzr
	movk	w11, #33052, lsl #16
	mov	w2, w12
	lsl	x10, x9, #2
	mov	w3, w13
	adrp	x14, pixels+2
	add	x14, x14, :lo12:pixels+2
	stp	w13, w12, [sp, #28]             // 8-byte Folded Spill
	add	x9, x10, #8
	add	x10, x10, #72
	b	.LBB0_55
.LBB0_54:                               // %for.cond.cleanup93
                                        //   in Loop: Header=BB0_55 Depth=2
	ldp	w3, w2, [sp, #36]               // 8-byte Folded Reload
	add	x8, x8, #1
	add	x14, x14, #480
	cmp	x8, #96
	sub	w3, w3, #7
	add	w2, w2, #7
	b.eq	.LBB0_62
.LBB0_55:                               // %for.cond90.preheader
                                        //   Parent Loop BB0_53 Depth=1
                                        // =>  This Loop Header: Depth=2
                                        //       Child Loop BB0_57 Depth 3
	and	x16, x8, #0xffffffff
	sub	x17, x8, #16
	mov	x15, xzr
	umull	x16, w16, w28
	ubfx	x18, x17, #2, #30
	lsl	w0, w17, #1
	and	x17, x17, #0xffffffc0
	mov	x4, x14
	stp	w3, w2, [sp, #36]               // 8-byte Folded Spill
	add	w18, w18, w18, lsl #4
	add	w0, w0, #40
	lsr	x1, x16, #33
	neg	w16, w8
	add	w1, w1, #24
	b	.LBB0_57
.LBB0_56:                               // %if.end147
                                        //   in Loop: Header=BB0_57 Depth=3
	and	w12, w6, #0xff
	add	x15, x15, #1
	sub	w3, w3, #13
	eor	w11, w11, w12
	and	w12, w5, #0xff
	cmp	x15, #160
	mul	w11, w11, w29
	add	w2, w2, #13
	sturb	w6, [x4, #-2]
	sturb	w5, [x4, #-1]
	strb	w7, [x4], #3
	eor	w11, w11, w12
	and	w12, w7, #0xff
	mul	w11, w11, w29
	eor	w11, w11, w12
	mul	w11, w11, w29
	b.eq	.LBB0_54
.LBB0_57:                               // %for.body94
                                        //   Parent Loop BB0_53 Depth=1
                                        //     Parent Loop BB0_55 Depth=2
                                        // =>    This Inner Loop Header: Depth=3
	umull	x5, w2, w21
	lsr	x7, x15, #2
	add	w7, w7, #16
	lsr	x5, x5, #32
	sub	w6, w2, w5
	add	w5, w5, w6, lsr #1
	add	w6, w8, w15
	umull	x6, w6, w28
	lsr	w5, w5, #6
	madd	w5, w5, w26, w3
	lsr	x6, x6, #34
	add	w30, w6, #48
	cmp	w5, #0
	csel	w6, w25, w7, eq
	csel	w5, w20, w1, eq
	csel	w7, w27, w30, eq
	cmp	x15, x9
	b.lo	.LBB0_56
// %bb.58:                              // %for.body94
                                        //   in Loop: Header=BB0_57 Depth=3
	cbnz	x17, .LBB0_56
// %bb.59:                              // %for.body94
                                        //   in Loop: Header=BB0_57 Depth=3
	cmp	x15, x10
	b.hs	.LBB0_56
// %bb.60:                              // %land.lhs.true125
                                        //   in Loop: Header=BB0_57 Depth=3
	ldr	x12, [sp, #48]                  // 8-byte Folded Reload
	adrp	x13, sprite
	add	x13, x13, :lo12:sprite
	add	x30, x12, x15
	sub	x30, x30, #8
	add	w12, w18, w30, lsr #2
	ldrb	w12, [x13, w12, uxtw]
	cmp	w12, #35
	b.ne	.LBB0_56
// %bb.61:                              // %if.then136
                                        //   in Loop: Header=BB0_57 Depth=3
	add	w5, w30, #120
	mov	w6, w16
	mov	w7, w0
	b	.LBB0_56
.LBB0_62:                               // %for.cond.cleanup87
                                        //   in Loop: Header=BB0_53 Depth=1
	str	x11, [x22, #24]
	mov	x9, xzr
	ldr	x8, [x23, :lo12:m]
.LBB0_63:                               // %for.body.i351
                                        //   Parent Loop BB0_53 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_63
// %bb.64:                              // %clear.exit357
                                        //   in Loop: Header=BB0_53 Depth=1
	mov	w9, #4                          // =0x4
	mov	w0, w19
	movk	w9, #16, lsl #16
	str	w9, [x8]
	mov	w9, #10                         // =0xa
	str	w9, [x8, #4]
	mov	w9, #46080                      // =0xb400
	str	w9, [x8, #8]
	adrp	x9, pixels
	add	x9, x9, :lo12:pixels
	str	w9, [x8, #12]
	ldr	w9, [sp, #12]                   // 4-byte Folded Reload
	str	w9, [x8, #16]
	mov	w9, #160                        // =0xa0
	str	w24, [x8, #32]
	str	w9, [x8, #48]
	mov	w9, #96                         // =0x60
	str	w9, [x8, #52]
	bl	reply
	mov	w10, #46080                     // =0xb400
	cbz	w0, .LBB0_1
// %bb.65:                              // %lor.lhs.false183
                                        //   in Loop: Header=BB0_53 Depth=1
	ldr	x8, [x23, :lo12:m]
	ldr	w9, [x8, #32]
	ldr	w8, [x8, #36]
	orr	x8, x9, x8, lsl #32
	cmp	x8, x10
	b.ne	.LBB0_1
// %bb.66:                              // %if.end188
                                        //   in Loop: Header=BB0_53 Depth=1
	ldr	x8, [sp, #16]                   // 8-byte Folded Reload
	add	x11, x8, #1
	cmp	x8, #7
	str	x11, [x22, #16]
	b.eq	.LBB0_52
// %bb.67:                              // %if.then194
                                        //   in Loop: Header=BB0_53 Depth=1
	mov	w0, #57600                      // =0xe100
	str	x11, [sp, #40]                  // 8-byte Folded Spill
	movk	w0, #1525, lsl #16
	bl	client_sleep
	ldr	x11, [sp, #40]                  // 8-byte Folded Reload
	b	.LBB0_52
.LBB0_68:                               // %for.end203
	mov	w0, w19
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.69:                              // %if.end207
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
