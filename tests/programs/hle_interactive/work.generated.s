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
	adrp	x23, :got:report
	adrp	x24, m
	mov	w8, #2                          // =0x2
	ldr	x23, [x23, :got_lo12:report]
	str	x0, [x24, :lo12:m]
	adrp	x0, .L.str
	add	x0, x0, :lo12:.L.str
	add	x1, sp, #60
	str	x8, [x23]
	bl	client_connect
	cbz	w0, .LBB0_2
.LBB0_1:                                // %cleanup277
	ldp	x20, x19, [sp, #144]            // 16-byte Folded Reload
	ldp	x22, x21, [sp, #128]            // 16-byte Folded Reload
	ldp	x24, x23, [sp, #112]            // 16-byte Folded Reload
	ldp	x26, x25, [sp, #96]             // 16-byte Folded Reload
	ldp	x28, x27, [sp, #80]             // 16-byte Folded Reload
	ldp	x29, x30, [sp, #64]             // 16-byte Folded Reload
	add	sp, sp, #160
	ret
.LBB0_2:                                // %if.end
	ldr	x8, [x24, :lo12:m]
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
	mov	w25, #18003                     // =0x4653
	movk	w10, #32768, lsl #16
	str	w9, [x8]
	mov	w9, #1                          // =0x1
	movk	w25, #18755, lsl #16
	str	w10, [x8, #4]
	mov	w22, #18003                     // =0x4653
	str	w9, [x8, #8]
	movk	w22, #20291, lsl #16
	str	w25, [x8, #32]
	ldr	w0, [sp, #60]
	bl	client_send
	cbnz	w0, .LBB0_7
// %bb.5:                               // %land.lhs.true.i
	ldr	x8, [x24, :lo12:m]
	ldr	w9, [x8, #16]
	cmp	w9, w22
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
	ldr	x8, [x24, :lo12:m]
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
	str	w25, [x8, #16]
	str	w12, [x8, #24]
.LBB0_11:                               // %for.body.i367
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
	ldr	x8, [x24, :lo12:m]
	ldr	w9, [x8, #16]
	cmp	w9, w22
	b.ne	.LBB0_17
// %bb.14:                              // %land.rhs.i.i
	ldr	w9, [x8, #24]
	cbnz	w9, .LBB0_17
// %bb.15:                              // %lor.lhs.false.i
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_17
// %bb.16:                              // %if.end.i
	ldr	w21, [x8, #12]
	b	.LBB0_18
.LBB0_17:
	mov	w21, wzr
.LBB0_18:                               // %get_service.exit
	ldr	w0, [sp, #60]
	ldr	x8, [x24, :lo12:m]
	mov	x9, xzr
.LBB0_19:                               // %for.body.i.i370
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_19
// %bb.20:                              // %clear.exit.i375
	mov	w10, #4                         // =0x4
	mov	w11, #10                        // =0xa
	mov	x9, xzr
	str	w10, [x8]
	mov	w12, #1                         // =0x1
	add	x10, x8, #32
	str	w11, [x8, #4]
	adrp	x11, .L.str.2
	add	x11, x11, :lo12:.L.str.2
	str	w25, [x8, #16]
	str	w12, [x8, #24]
.LBB0_21:                               // %land.rhs.i379
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w8, [x11, x9]
	strb	w8, [x10, x9]
	add	x9, x9, #1
	cmp	x9, #8
	b.ne	.LBB0_21
// %bb.22:                              // %for.cond.cleanup.i387
	bl	client_send
	cbnz	w0, .LBB0_27
// %bb.23:                              // %land.lhs.true.i.i391
	ldr	x8, [x24, :lo12:m]
	ldr	w9, [x8, #16]
	cmp	w9, w22
	b.ne	.LBB0_27
// %bb.24:                              // %land.rhs.i.i394
	ldr	w9, [x8, #24]
	cbnz	w9, .LBB0_27
// %bb.25:                              // %lor.lhs.false.i397
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_27
// %bb.26:                              // %if.end.i400
	ldr	w19, [x8, #12]
	b	.LBB0_28
.LBB0_27:
	mov	w19, wzr
.LBB0_28:                               // %get_service.exit402
	ldr	w0, [sp, #60]
	ldr	x8, [x24, :lo12:m]
	mov	x9, xzr
.LBB0_29:                               // %for.body.i.i403
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_29
// %bb.30:                              // %clear.exit.i408
	mov	w10, #4                         // =0x4
	mov	w11, #10                        // =0xa
	mov	x9, xzr
	str	w10, [x8]
	mov	w12, #1                         // =0x1
	add	x10, x8, #32
	str	w11, [x8, #4]
	adrp	x11, .L.str.3
	add	x11, x11, :lo12:.L.str.3
	str	w25, [x8, #16]
	str	w12, [x8, #24]
.LBB0_31:                               // %for.body.i416
                                        // =>This Inner Loop Header: Depth=1
	ldrb	w8, [x11, x9]
	strb	w8, [x10, x9]
	add	x9, x9, #1
	cmp	x9, #7
	b.ne	.LBB0_31
// %bb.32:                              // %for.cond.cleanup.i420
	bl	client_send
	cbnz	w0, .LBB0_37
// %bb.33:                              // %land.lhs.true.i.i424
	ldr	x8, [x24, :lo12:m]
	ldr	w9, [x8, #16]
	cmp	w9, w22
	b.ne	.LBB0_37
// %bb.34:                              // %land.rhs.i.i427
	ldr	w9, [x8, #24]
	cbnz	w9, .LBB0_37
// %bb.35:                              // %lor.lhs.false.i430
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_37
// %bb.36:                              // %if.end.i433
	ldr	w20, [x8, #12]
	b	.LBB0_38
.LBB0_37:
	mov	w20, wzr
.LBB0_38:                               // %get_service.exit435
	cbz	w21, .LBB0_1
// %bb.39:                              // %get_service.exit435
	cbz	w19, .LBB0_1
// %bb.40:                              // %get_service.exit435
	cbz	w20, .LBB0_1
// %bb.41:                              // %lor.lhs.false16
	ldr	w0, [sp, #60]
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.42:                              // %if.end20
	mov	w8, #3                          // =0x3
	mov	x9, xzr
	str	x8, [x23]
	ldr	x8, [x24, :lo12:m]
.LBB0_43:                               // %for.body.i436
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_43
// %bb.44:                              // %clear.exit442
	mov	w26, #4                         // =0x4
	adrp	x10, .L.str.4
	add	x10, x10, :lo12:.L.str.4
	movk	w26, #16, lsl #16
	mov	w9, #8                          // =0x8
	lsr	x11, x10, #34
	lsr	x12, x10, #4
	str	w26, [x8]
	mov	w0, w21
	str	w9, [x8, #4]
	mov	w9, #11                         // =0xb
	str	w9, [x8, #8]
	and	w9, w11, #0x3ffffffc
	and	w11, w12, #0xf0000000
	orr	w9, w9, w11
	str	w10, [x8, #12]
	str	w9, [x8, #16]
	str	w25, [x8, #32]
	bl	client_send
	cbnz	w0, .LBB0_1
// %bb.45:                              // %land.lhs.true.i448
	ldr	x8, [x24, :lo12:m]
	ldr	w9, [x8, #16]
	cmp	w9, w22
	b.ne	.LBB0_1
// %bb.46:                              // %land.rhs.i451
	ldr	w8, [x8, #24]
	cbnz	w8, .LBB0_1
// %bb.47:                              // %lor.lhs.false26
	ldr	x8, [x24, :lo12:m]
	ldr	w9, [x8, #8]
	cmp	w9, #32
	b.ne	.LBB0_1
// %bb.48:                              // %if.end29
	ldr	w22, [x8, #12]
	mov	w0, w21
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.49:                              // %if.end34
	ldr	x8, [x24, :lo12:m]
	mov	x9, xzr
.LBB0_50:                               // %for.body.i455
                                        // =>This Inner Loop Header: Depth=1
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_50
// %bb.51:                              // %clear.exit461
	add	w9, w26, #3840, lsl #12         // =15728640
	adrp	x12, sprite
	add	x12, x12, :lo12:sprite
	lsr	x11, x12, #34
	str	w9, [x8]
	lsr	x9, x12, #4
	mov	w10, #12                        // =0xc
	mov	w0, w22
	and	w11, w11, #0x3ffffffc
	and	w9, w9, #0xf0000000
	str	w10, [x8, #4]
	mov	w10, #272                       // =0x110
	orr	w9, w11, w9
	str	w10, [x8, #8]
	str	w12, [x8, #12]
	str	w9, [x8, #16]
	str	w25, [x8, #32]
	str	wzr, [x8, #48]
	str	wzr, [x8, #52]
	str	w10, [x8, #56]
	str	wzr, [x8, #60]
	bl	reply
	cbz	w0, .LBB0_1
// %bb.52:                              // %lor.lhs.false40
	ldr	x8, [x24, :lo12:m]
	ldr	w9, [x8, #32]
	ldr	w8, [x8, #36]
	orr	x8, x9, x8, lsl #32
	cmp	x8, #272
	b.ne	.LBB0_1
// %bb.53:                              // %lor.lhs.false43
	mov	w0, w22
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.54:                              // %if.end47
	mov	w10, #272                       // =0x110
	mov	x9, xzr
	mov	w8, wzr
	str	x10, [x23, #8]
	adrp	x10, sprite
	add	x10, x10, :lo12:sprite
.LBB0_55:                               // %for.cond49.preheader
                                        // =>This Loop Header: Depth=1
                                        //     Child Loop BB0_57 Depth 2
	mov	x11, xzr
	b	.LBB0_57
.LBB0_56:                               // %for.inc
                                        //   in Loop: Header=BB0_57 Depth=2
	add	x11, x11, #1
	cmp	x11, #16
	b.eq	.LBB0_59
.LBB0_57:                               // %for.body52
                                        //   Parent Loop BB0_55 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	ldrb	w12, [x10, x11]
	cmp	w12, #46
	b.eq	.LBB0_56
// %bb.58:                              // %land.lhs.true
                                        //   in Loop: Header=BB0_57 Depth=2
	ldrb	w12, [x10, x11]
	cmp	w12, #35
	b.eq	.LBB0_56
	b	.LBB0_61
.LBB0_59:                               // %for.end
                                        //   in Loop: Header=BB0_55 Depth=1
	add	x11, x9, x9, lsl #4
	adrp	x12, sprite
	add	x12, x12, :lo12:sprite
	add	x11, x11, x12
	ldrb	w11, [x11, #16]
	cmp	w11, #10
	b.ne	.LBB0_61
// %bb.60:                              // %for.cond
                                        //   in Loop: Header=BB0_55 Depth=1
	cmp	x9, #14
	add	x9, x9, #1
	add	x10, x10, #17
	cset	w8, hi
	cmp	x9, #16
	b.ne	.LBB0_55
	b	.LBB0_62
.LBB0_61:                               // %cleanup76
	tbz	w8, #0, .LBB0_1
.LBB0_62:                               // %for.end78
	mov	w8, #4                          // =0x4
	str	x8, [x23]
	ldr	x8, [x23, #48]
	cbz	x8, .LBB0_1
// %bb.63:                              // %lor.lhs.false80
	ldr	x8, [x23, #48]
	cmp	x8, #600
	b.hi	.LBB0_1
// %bb.64:                              // %for.cond85.preheader
	ldr	x8, [x23, #48]
	cbz	x8, .LBB0_94
// %bb.65:                              // %for.body90.lr.ph
	adrp	x9, pixels
	add	x9, x9, :lo12:pixels
	mov	w29, #403                       // =0x193
	lsr	x8, x9, #34
	lsr	x9, x9, #4
	mov	w27, #43691                     // =0xaaab
	movk	w29, #256, lsl #16
	mov	w22, #48                        // =0x30
	mov	w21, #16                        // =0x10
	and	w8, w8, #0x3ffffffc
	and	w9, w9, #0xf0000000
	movk	w27, #43690, lsl #16
	orr	w8, w8, w9
	mov	w28, #-46                       // =0xffffffd2
	stp	wzr, wzr, [sp, #36]             // 8-byte Folded Spill
	str	xzr, [sp, #24]                  // 8-byte Folded Spill
	str	w8, [sp, #12]                   // 4-byte Folded Spill
.LBB0_66:                               // %for.body90
                                        // =>This Loop Header: Depth=1
                                        //     Child Loop BB0_67 Depth 2
                                        //     Child Loop BB0_72 Depth 2
                                        //       Child Loop BB0_74 Depth 3
                                        //     Child Loop BB0_82 Depth 2
	ldr	x8, [x24, :lo12:m]
	mov	x9, xzr
.LBB0_67:                               // %for.body.i475
                                        //   Parent Loop BB0_66 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	str	wzr, [x8, x9]
	add	x9, x9, #4
	cmp	x9, #256
	b.ne	.LBB0_67
// %bb.68:                              // %clear.exit481
                                        //   in Loop: Header=BB0_66 Depth=1
	mov	w9, #4                          // =0x4
	mov	w0, w20
	str	w9, [x8]
	mov	w9, #8                          // =0x8
	str	w9, [x8, #4]
	str	w25, [x8, #16]
	bl	reply
	cbz	w0, .LBB0_92
// %bb.69:                              // %if.end97
                                        //   in Loop: Header=BB0_66 Depth=1
	ldr	x9, [x24, :lo12:m]
	ldr	w8, [x9, #32]
	str	x9, [sp, #16]                   // 8-byte Folded Spill
	ldr	wzr, [x9, #36]
	tbnz	w8, #4, .LBB0_80
// %bb.70:                              // %if.end102
                                        //   in Loop: Header=BB0_66 Depth=1
	and	w10, w8, #0x3
	cmp	w22, #3
	sub	w11, w22, #4
	ccmp	w10, #1, #0, hi
	and	w8, w8, #0xc
	mov	x9, xzr
	csel	w11, w11, w22, eq
	adrp	x4, pixels+2
	add	x4, x4, :lo12:pixels+2
	cmp	w11, #93
	add	w12, w11, #4
	ccmp	w10, #2, #0, lo
	sub	w10, w21, #4
	csel	w22, w12, w11, eq
	cmp	w21, #3
	mov	w11, #40389                     // =0x9dc5
	ccmp	w8, #4, #0, hi
	str	x22, [x23, #32]
	movk	w11, #33052, lsl #16
	csel	w13, w10, w21, eq
	add	w10, w22, #64
	neg	x12, x22
	cmp	w13, #29
	add	w14, w13, #4
	ccmp	w8, #8, #0, lo
	csel	w21, w14, w13, eq
	neg	w13, w22
	str	x21, [x23, #40]
	add	w14, w21, #64
	ldp	w17, w5, [sp, #36]              // 8-byte Folded Reload
	b	.LBB0_72
.LBB0_71:                               // %for.cond.cleanup150
                                        //   in Loop: Header=BB0_72 Depth=2
	ldr	w5, [sp, #44]                   // 4-byte Folded Reload
	ldr	x4, [sp, #48]                   // 8-byte Folded Reload
	add	x9, x9, #1
	cmp	x9, #96
	sub	w17, w17, #7
	add	w5, w5, #7
	add	x4, x4, #480
	b.eq	.LBB0_81
.LBB0_72:                               // %for.cond147.preheader
                                        //   Parent Loop BB0_66 Depth=1
                                        // =>  This Loop Header: Depth=2
                                        //       Child Loop BB0_74 Depth 3
	and	x8, x9, #0xffffffff
	sub	w0, w9, w21
	mov	x18, xzr
	umull	x8, w8, w27
	lsr	w1, w0, #2
	lsl	w2, w0, #1
	mov	w6, w17
	str	x4, [sp, #48]                   // 8-byte Folded Spill
	add	w1, w1, w1, lsl #4
	str	w5, [sp, #44]                   // 4-byte Folded Spill
	lsr	x3, x8, #33
	mov	w8, #-16                        // =0xfffffff0
	sub	w0, w8, w0
	add	w8, w2, #40
	add	w3, w3, #24
	b	.LBB0_74
.LBB0_73:                               // %if.end206
                                        //   in Loop: Header=BB0_74 Depth=3
	and	w15, w30, #0xff
	add	x18, x18, #1
	sub	w6, w6, #13
	eor	w11, w11, w15
	and	w15, w7, #0xff
	cmp	x18, #160
	mul	w11, w11, w29
	add	w5, w5, #13
	sturb	w30, [x4, #-2]
	sturb	w7, [x4, #-1]
	strb	w2, [x4], #3
	eor	w11, w11, w15
	and	w15, w2, #0xff
	mul	w11, w11, w29
	eor	w11, w11, w15
	mul	w11, w11, w29
	b.eq	.LBB0_71
.LBB0_74:                               // %for.body151
                                        //   Parent Loop BB0_66 Depth=1
                                        //     Parent Loop BB0_72 Depth=2
                                        // =>    This Inner Loop Header: Depth=3
	mov	w15, #30739                     // =0x7813
	lsr	x30, x18, #2
	movk	w15, #8699, lsl #16
	umull	x2, w5, w15
	mov	w15, #113                       // =0x71
	add	w30, w30, #16
	lsr	x2, x2, #32
	sub	w7, w5, w2
	add	w2, w2, w7, lsr #1
	add	w7, w9, w18
	umull	x7, w7, w27
	lsr	w2, w2, #6
	madd	w2, w2, w15, w6
	lsr	x7, x7, #34
	mov	w15, #-116                      // =0xffffff8c
	cmp	w2, #0
	add	w2, w7, #48
	csel	w30, w15, w30, eq
	mov	w15, #-86                       // =0xffffffaa
	csel	w2, w28, w2, eq
	csel	w7, w15, w3, eq
	cmp	x18, x22
	b.lo	.LBB0_73
// %bb.75:                              // %for.body151
                                        //   in Loop: Header=BB0_74 Depth=3
	cmp	x18, x10
	b.hs	.LBB0_73
// %bb.76:                              // %for.body151
                                        //   in Loop: Header=BB0_74 Depth=3
	cmp	x9, x21
	b.lo	.LBB0_73
// %bb.77:                              // %for.body151
                                        //   in Loop: Header=BB0_74 Depth=3
	cmp	x9, x14
	b.hs	.LBB0_73
// %bb.78:                              // %land.lhs.true183
                                        //   in Loop: Header=BB0_74 Depth=3
	add	w15, w13, w18
	adrp	x16, sprite
	add	x16, x16, :lo12:sprite
	add	w15, w1, w15, lsr #2
	ldrb	w15, [x16, w15, uxtw]
	cmp	w15, #35
	b.ne	.LBB0_73
// %bb.79:                              // %if.then195
                                        //   in Loop: Header=BB0_74 Depth=3
	add	x15, x12, x18
	mov	w30, w0
	mov	w2, w8
	add	w7, w15, #120
	b	.LBB0_73
.LBB0_80:                               //   in Loop: Header=BB0_66 Depth=1
	mov	w9, wzr
	mov	w8, #8                          // =0x8
	b	.LBB0_89
.LBB0_81:                               // %for.cond.cleanup144
                                        //   in Loop: Header=BB0_66 Depth=1
	str	x11, [x23, #24]
	mov	x8, xzr
	ldr	x9, [sp, #16]                   // 8-byte Folded Reload
.LBB0_82:                               // %for.body.i488
                                        //   Parent Loop BB0_66 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	str	wzr, [x9, x8]
	add	x8, x8, #4
	cmp	x8, #256
	b.ne	.LBB0_82
// %bb.83:                              // %clear.exit494
                                        //   in Loop: Header=BB0_66 Depth=1
	mov	w8, #10                         // =0xa
	str	w26, [x9]
	mov	w0, w19
	str	w8, [x9, #4]
	mov	w8, #46080                      // =0xb400
	str	w8, [x9, #8]
	adrp	x8, pixels
	add	x8, x8, :lo12:pixels
	str	w8, [x9, #12]
	ldr	w8, [sp, #12]                   // 4-byte Folded Reload
	str	w8, [x9, #16]
	mov	w8, #160                        // =0xa0
	str	w25, [x9, #32]
	str	w8, [x9, #48]
	mov	w8, #96                         // =0x60
	str	w8, [x9, #52]
	bl	reply
	cbz	w0, .LBB0_88
// %bb.84:                              // %lor.lhs.false242
                                        //   in Loop: Header=BB0_66 Depth=1
	ldr	x8, [x24, :lo12:m]
	ldr	w9, [x8, #32]
	ldr	w8, [x8, #36]
	orr	x8, x9, x8, lsl #32
	mov	w9, #46080                      // =0xb400
	cmp	x8, x9
	b.ne	.LBB0_88
// %bb.85:                              // %if.end247
                                        //   in Loop: Header=BB0_66 Depth=1
	ldr	x8, [sp, #24]                   // 8-byte Folded Reload
	add	w8, w8, #1
	str	x8, [x23, #16]
	ldr	x9, [x23, #48]
	cmp	x9, x8
	b.ls	.LBB0_87
// %bb.86:                              // %if.then254
                                        //   in Loop: Header=BB0_66 Depth=1
	mov	w0, #57600                      // =0xe100
	movk	w0, #1525, lsl #16
	bl	client_sleep
.LBB0_87:                               // %cleanup257
                                        //   in Loop: Header=BB0_66 Depth=1
	mov	w8, wzr
	mov	w9, #1                          // =0x1
	b	.LBB0_89
.LBB0_88:                               //   in Loop: Header=BB0_66 Depth=1
	mov	w9, wzr
	mov	w8, #1                          // =0x1
.LBB0_89:                               // %cleanup257
                                        //   in Loop: Header=BB0_66 Depth=1
	tbz	w9, #0, .LBB0_93
// %bb.90:                              // %for.cond85
                                        //   in Loop: Header=BB0_66 Depth=1
	ldr	x8, [x23, #48]
	ldr	x9, [sp, #24]                   // 8-byte Folded Reload
	add	w9, w9, #1
	cmp	x8, x9
	ldr	w8, [sp, #36]                   // 4-byte Folded Reload
	str	x9, [sp, #24]                   // 8-byte Folded Spill
	sub	w9, w8, #11
	ldr	w8, [sp, #40]                   // 4-byte Folded Reload
	add	w8, w8, #11
	stp	w9, w8, [sp, #36]               // 8-byte Folded Spill
	b.hi	.LBB0_66
// %bb.91:
	mov	w8, #8                          // =0x8
	b	.LBB0_93
.LBB0_92:
	mov	w8, #1                          // =0x1
.LBB0_93:                               // %cleanup261.loopexit
	cmp	w8, #8
	b.ne	.LBB0_1
.LBB0_94:                               // %for.end263
	mov	w0, w20
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.95:                              // %lor.lhs.false266
	mov	w0, w19
	bl	client_close_handle
	cbnz	w0, .LBB0_1
// %bb.96:                              // %if.end270
	mov	w8, #1                          // =0x1
	str	x8, [x23]
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
	.xword	48                              // 0x30
	.xword	16                              // 0x10
	.xword	60                              // 0x3c
	.size	report, 56

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
	.asciz	"emu:pad"
	.size	.L.str.3, 8

	.type	.L.str.4,@object                // @.str.4
.L.str.4:
	.asciz	"sprite.txt"
	.size	.L.str.4, 11

	.type	sprite,@object                  // @sprite
	.local	sprite
	.comm	sprite,272,1
	.type	pixels,@object                  // @pixels
	.local	pixels
	.comm	pixels,46080,1
	.ident	"clang version 18.1.6 (https://github.com/ziglang/zig-bootstrap 98bc6bf4fc4009888d33941daf6b600d20a42a56)"
	.section	".note.GNU-stack","",@progbits
