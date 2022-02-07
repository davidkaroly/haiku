/*
 * Copyright 2020-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */

#include <asm_defs.h>


	.text

/*	status_t arch_enter_kernel(addr_t ttbr, addr_t kernelArgs,
		addr_t kernelEntry, addr_t kernelStackTop);

	x0	- ttbr
	x1	- kernelArgs
	x2	- kernelEntry
	x3	- kernelStackTop
*/
FUNCTION(arch_enter_kernel):

	// set the kernel stack
	mov		sp, x3

	// set up kernel _start args
	mov		x0, x1	// kernelArgs
	mov		x4, x2
	mov		x1, #0	// currentCPU=0

	// call the kernel
	br		x4

	// return
	mov		x0,#-1	// B_ERROR
	ret

FUNCTION_END(arch_enter_kernel)
