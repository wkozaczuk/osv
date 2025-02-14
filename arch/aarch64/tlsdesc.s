// size_t __tlsdesc_static(size_t *a)
// {
// 	return a[1];
// }
.global __tlsdesc_static
.hidden __tlsdesc_static
.type __tlsdesc_static,@function
__tlsdesc_static:
	ldr x0,[x0,#8]
	ret

.global __tlsdesc_dynamic
.type __tlsdesc_dynamic,@function
__tlsdesc_dynamic:
//FAST PATH
        stp x1,x2,[sp, #-16]!
        stp x3,x4,[sp, #-16]!
        ldr x0,[x0,#8]        // p
        ldr x2,[x0]           // p->module
        mrs x1,tpidr_el0
        ldr x3,[x1,#8]        // point to the dtv info address
        ldr x4,[x3]           // load max_index
        cmp x4,x2
        b.lt 1f
        ldr x4,[x3,#8]        // load _tls address
        lsl x2,x2,#3
        ldr x4,[x4,x2]        // load tls address of this module
        cmp x4,#0
        b.eq 1f
2:      ldr x2,[x0,#8]        // load p->offset
        add x0,x4,x2
        sub x0,x0,x1
        ldp x3,x4,[sp],#16
        ldp x1,x2,[sp],#16
        ret
//SLOW PATH
1:      stp	x29, x30, [sp, #-48]!
        mov     x29, sp

        stp x0, xzr, [sp, #-16]!

        stp x1, x2, [sp, #-16]!
        stp x3, x4, [sp, #-16]!
        stp x5, x6, [sp, #-16]!
        stp x7, x8, [sp, #-16]!
        stp x9, x10, [sp, #-16]!
        stp x11, x12, [sp, #-16]!
        stp x13, x14, [sp, #-16]!
        stp x15, x16, [sp, #-16]!
        stp x17, x18, [sp, #-16]!

/*
        stp x19, x20, [sp, #-16]!
        stp x21, x22, [sp, #-16]!
        stp x23, x24, [sp, #-16]!
        stp x25, x26, [sp, #-16]!
        stp x27, x28, [sp, #-16]!*/

        bl __tls_setup
/* MAYBE later -> also FPU lock in __tls_setup
        ldp x27, x28, [sp], #16
        ldp x25, x26, [sp], #16
        ldp x23, x24, [sp], #16
        ldp x21, x22, [sp], #16
        ldp x19, x20, [sp], #16
*/
        ldp x17, x18, [sp], #16
        ldp x15, x16, [sp], #16
        ldp x13, x14, [sp], #16
        ldp x11, x12, [sp], #16
        ldp x9, x10, [sp], #16
        ldp x7, x8, [sp], #16
        ldp x5, x6, [sp], #16
        ldp x3, x4, [sp], #16
        ldp x1, x2, [sp], #16

        mov x4, x0
        ldp x0, xzr, [sp], #16

        ldp x29, x30, [sp], #48
        
        b 2b

/* ------- MUSL based version
	// save all registers __tls_get_addr may clobber
	// ugly because addr offset must be in [-512,509]
	stp x1,x2,[sp,#-32]!
	stp x3,x4,[sp,#16]
1:	stp x29,x30,[sp,#-160]!
	stp x5,x6,[sp,#16]
	stp x7,x8,[sp,#32]
	stp x9,x10,[sp,#48]
	stp x11,x12,[sp,#64]
	stp x13,x14,[sp,#80]
	stp x15,x16,[sp,#96]
	stp x17,x18,[sp,#112]
	stp q0,q1,[sp,#128]
	stp q2,q3,[sp,#-480]!
	stp q4,q5,[sp,#32]
	stp q6,q7,[sp,#64]
	stp q8,q9,[sp,#96]
	stp q10,q11,[sp,#128]
	stp q12,q13,[sp,#160]
	stp q14,q15,[sp,#192]
	stp q16,q17,[sp,#224]
	stp q18,q19,[sp,#256]
	stp q20,q21,[sp,#288]
	stp q22,q23,[sp,#320]
	stp q24,q25,[sp,#352]
	stp q26,q27,[sp,#384]
	stp q28,q29,[sp,#416]
	stp q30,q31,[sp,#448]

	ldr x0,[x0,#8]        // p
	bl __tls_get_addr
	//bl abort

	ldp q4,q5,[sp,#32]
	ldp q6,q7,[sp,#64]
	ldp q8,q9,[sp,#96]
	ldp q10,q11,[sp,#128]
	ldp q12,q13,[sp,#160]
	ldp q14,q15,[sp,#192]
	ldp q16,q17,[sp,#224]
	ldp q18,q19,[sp,#256]
	ldp q20,q21,[sp,#288]
	ldp q22,q23,[sp,#320]
	ldp q24,q25,[sp,#352]
	ldp q26,q27,[sp,#384]
	ldp q28,q29,[sp,#416]
	ldp q30,q31,[sp,#448]
	ldp q2,q3,[sp],#480
	ldp x5,x6,[sp,#16]
	ldp x7,x8,[sp,#32]
	ldp x9,x10,[sp,#48]
	ldp x11,x12,[sp,#64]
	ldp x13,x14,[sp,#80]
	ldp x15,x16,[sp,#96]
	ldp x17,x18,[sp,#112]
	ldp q0,q1,[sp,#128]
	ldp x29,x30,[sp],#160
	ldp x3,x4,[sp,#16]
	ldp x1,x2,[sp],#32
	ret*/
