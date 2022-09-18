/* 
 * Copyright 2005, Ingo Weinhold <bonefish@cs.tu-berlin.de>. All rights
 * reserved. Distributed under the terms of the Haiku License.
 */

#include <asm_defs.h>

/* int __siglongjmp(jmp_buf buffer, int value) */
FUNCTION(siglongjmp):
FUNCTION(longjmp):
FUNCTION(_longjmp):
	// Load FPU status
	add		r2, r0, #44
	ldr		r3, [r2]
	vmsr	fpscr, r3

	// Load FPU registers d8-d15
	add		r2, r0, #48
	fldmiad	r2!, {d8-d15}

	ldmia	r0, {r4-r14}
	b		__longjmp_return
FUNCTION_END(siglongjmp)
FUNCTION_END(longjmp)
FUNCTION_END(_longjmp)

#pragma weak longjmp=siglongjmp
