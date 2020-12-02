/*
 *	flush_icache_range(start,end)
 *
 *	Ensure that the I and D caches are coherent within specified region.
 *	This is typically used when code has been written to a memory region,
 *	and will be executed.
 *
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 */
.global flush_icache_range
.type flush_icache_range, @function
flush_icache_range:
/*        mrs	x3, ctr_el0		// read CTR
	ubfm	x3, x3, #16, #19	// cache line size encoding
	mov	x2, #4			// bytes per word
	lsl	x2, x2, x3		// actual cache line size*/
        mov     x2, #64
        
	sub	x3, x2, #1
	bic	x4, x0, x3
1:
        dc civac, x4
	add	x4, x4, x2
	cmp	x4, x1
	b.lo	1b
	dsb	ish

        /*mrs     x3, ctr_el0     // read CTR
	and	x3, x3, #0xf	// cache line size encoding
	mov	x2, #4		// bytes per word
	lsl	x2, x2, x3	// actual cache line size*/
        mov     x2, #64

	sub	x3, x2, #1
	bic	x3, x0, x3
2:
        ic ivau, x3 		// invalidate I line PoU
	add	x3, x3, x2 
	cmp	x3, x1
	b.lo	2b

	dsb	ish
	isb

	mov	x0, #0
	ret
