/*
 * Copyright 2021-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */


	.section	.text.head
	.global		_start
_start:
_arm_vectors:
	bl		_arm_start
	ldr		pc, _arm_undefined
	ldr		pc, _arm_syscall
	ldr		pc, _arm_prefetch_abort
	ldr		pc, _arm_data_abort
	ldr		pc, _arm_reserved
	ldr		pc, _arm_irq
	ldr		pc, _arm_fiq

_arm_reset:
	.word (_arm_start - _arm_vectors)

_arm_undefined:
	.word .stop

_arm_syscall:
	.word .stop

_arm_prefetch_abort:
	.word .stop

_arm_data_abort:
	.word .stop

_arm_reserved:
	.word .stop

_arm_irq:
	.word .stop

_arm_fiq:
	.word .stop

_arm_start:
	// find out actual entry point address
	sub		r0, lr, #4

	// look for device tree at 0x40000000
	// if we are loaded as a firmware at zero start address
	// otherwise the device tree address should be preloaded in r2
	cmp		r0, #0
	moveq	r2, #0x40000000

	// generate some output on the debug uart
	//mov		r9, #0x09000000
	//mov		r8, #65
	//str		r8, [r9]

	/*
	 * Turn off interrupts and make sure we are in SVC mode
	 */
	mrs		r4, cpsr
	bic		r4, r4, #0x1f
	orr		r4, r4, #0xd3
	msr		cpsr, r4

	/*
	 * Turn off MMU and data cache if necessary.
	 */
	mrc		p15, 0, r4, c1, c0, 0
	bic		r4, r4, #0x0000000f		     // WCAM bits
	bic		r4, r4, #0x00000300		     // RS   bits
	mcr		p15, 0, r4, c1, c0, 0

	// set destination address: 0x40100000
	mov		r10, #0x40000000
	add		r10, r10, #0x00100000

	// set source address
	mov		r11, r0

	// set limit
	ldr		r12, =_end

.copy_loop:
	ldr		r9, [r11]
	str		r9, [r10]
	add		r10, r10, #4
	add		r11, r11, #4

	cmp		r10, r12
	bne		.copy_loop

	// Initialize stack pointer
	ldr		sp, =__stack_top

	// Load FDT pointer
	mov		r0, r2

	ldr		r12, =start
	blx		r12

.stop:
	b		.
