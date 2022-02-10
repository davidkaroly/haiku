/*
 * Copyright 2020-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */

#include <asm_defs.h>

	.text

/*  status_t arch_enter_kernel(uint64 satp, addr_t kernelArgs,
        addr_t kernelEntry, addr_t kernelStackTop);

    a0  - SATP register value
    a1  - kernelArgs
    a2  - kernelEntry
    a3  - kernelStackTop
*/
FUNCTION(arch_enter_kernel):
	// initialize page directory
	csrw 	satp, a0
	sfence.vma

	// set the kernel stack
	mv		sp, a3
	li		fp, 0
	li		ra, 0

	// Setup kernel args
	mv		a0, a1	// kernelArgs
	mv		t0, a2	// kernelEntry
	li		a1, 0	// currentCPU=0

	// Initialize physical memory protection
	// enable access to all memory from S-mode
	li		a3, 0x0f
	csrw	pmpcfg0, a3
	li		a3, -1
	csrw	pmpaddr0, a3

	// Switch to S-mode
	csrr	a3, mstatus
	li		a4, 0x800
	or		a3, a3, a4
	csrw	mstatus, a3
	la		a3, .smode_trampoline
	csrw 	mepc, a3

	mret

.smode_trampoline:
	// call the kernel
	jr	t0

	// return
	li	a0,-1  // B_ERROR
	ret
FUNCTION_END(arch_enter_kernel)
