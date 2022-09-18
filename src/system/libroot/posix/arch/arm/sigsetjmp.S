/* 
 * Copyright 2005, Ingo Weinhold <bonefish@cs.tu-berlin.de>. All rights
 * reserved. Distributed under the terms of the Haiku License.
 */

#include <asm_defs.h>

/* int sigsetjmp(jmp_buf buffer, int saveMask) */
FUNCTION(__sigsetjmp):
FUNCTION(sigsetjmp):
	stm		r0, {r4-r14}

	// Save FPU status
	add		r2, r0, #44
	vmrs	r3, fpscr
	str		r3, [r2]

	// Save FPU registers d8-d15
	add		r2, r0, #48
	fstmiad	r2!, {d8-d15}

	b		__setjmp_save_sigs
FUNCTION_END(sigsetjmp)
FUNCTION_END(__sigsetjmp)


/* int setjmp(jmp_buf buffer) */
FUNCTION(setjmp):
FUNCTION(_setjmp):
	mov		r1, #0
	b		sigsetjmp
FUNCTION_END(setjmp)

#pragma weak _setjmp=setjmp
