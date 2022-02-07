/*
 * Copyright 2011, François Revol <revol@free.fr>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <asm_defs.h>


	.text

/*	status_t arch_enter_kernel(addr_t ttbr, addr_t kernelArgs,
		addr_t kernelEntry, addr_t kernelStackTop);

	r0	- ttbr
	r1	- kernelArgs
	r2	- kernelEntry
	r3	- kernelStackTop
*/
FUNCTION(arch_enter_kernel):

	// save input parameters
	mov		r5, r0		// ttbr
	mov		r4, r2		// kernelEntry

	// set up kernel _start args
	mov		r0,r1		// kernelArgs
	mov		r1,#0		// currentCPU=0

	// enable full access for coprocessors P10, P11
	// by setting the required flags in Access Control Register
	mrc		p15, #0, r9, c1, c0, #2
	orr		r9, r9, #0x00f00000
	mcr		p15, #0, r9, c1, c0, #2

	// flush prefetch buffer
	mov		r9, #0
	mcr		p15, #0, r9, c7, c5, #4

	// enable FPU
	mov		r9, #0x40000000
	FMXR	FPEXC, r9

	// flush TLB
	mcr		p15, 0, r1, c8, c7, 0

	// set TTBR0
	// cacheability attributes for the page tables are:
	// Normal Memory, Inner/Outer Write-Back no Write-Allocate Cacheable

	// Note that this relies on ARMv7 multiprocessing extensions
	// on uniprocessors we need only the flag 0x01 i.e. Inner Cacheable

	orr		r5, r5, #0x59
	mcr		p15, 0, r5, c2, c0, 0

	// initialize TTBCR to zero (no LPAE, use only TTBR0)
	mcr		p15, 0, r1, c2, c0, 2

	// flush TLB (again)
	mcr		p15, 0, r1, c8, c7, 0

	// write DACR
	mov		r9, #0x00000001
	mcr		p15, 0, r9, c3, c0, 0

	// enable MMU and caches
	mrc		p15, 0, r9, c1, c0, 0
	bic		r9, r9, #0x20000000		// access flag disabled
	bic		r9, r9, #0x10000000		// TEX remap disabled
	orr		r9, r9, #0x00001000		// i-cache enabled
	orr		r9, r9, #0x00000004		// d-cache enabled
	orr		r9, r9, #0x00000001		// MMU enabled
	mcr		p15, 0, r9, c1, c0, 0

	// set the kernel stack
	mov		sp,r3

	// call the kernel
	mov		pc,r4

	// return
	mov		r0,#-1		// B_ERROR
	mov		pc,lr

FUNCTION_END(arch_enter_kernel)
