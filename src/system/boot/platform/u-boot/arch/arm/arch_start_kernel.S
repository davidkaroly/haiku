/*
 * Copyright 2011, François Revol <revol@free.fr>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <arch/arm/arch_cpu.h>

#include <asm_defs.h>


	.text

/*	status_t arch_start_kernel(struct kernel_args *kernelArgs,
		addr_t kernelEntry, addr_t kernelStackTop);

	r0	- kernelArgs
	r1	- kernelEntry
	r2	- kernelStackTop
*/
FUNCTION(arch_start_kernel):

	// set the kernel stack
	mov		sp,r2

	// enable full access for coprocessors P10, P11
	// by setting the required flags in Access Control Register
	MRC p15, #0, r9, c1, c0, #2
	orr r9, r9, #0x00f00000
	MCR p15, #0, r9, c1, c0, #2

	// flush prefetch buffer
	mov r9, #0
	MCR p15, #0, r9, c7, c5, #4

	// enable FPU
	mov r9, #0x40000000
	FMXR FPEXC, r9

	// set up kernel _start args
	//mov	r0,r0	// kernelArgs
	mov		r4,r1
	mov		r1,#0	// currentCPU=0

	// call the kernel
	mov		pc,r4

	// return
	mov		r0,#-1	// B_ERROR
	mov		pc,lr

FUNCTION_END(arch_start_kernel)

