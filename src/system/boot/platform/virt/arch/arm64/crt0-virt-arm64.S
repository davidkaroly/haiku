/*
 * Copyright 2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */


	.section	.text.head

_start:
	bl		_arm_start

_arm_start:
	// find out actual entry point address
	sub		x7, lr, #4

	// look for device tree at 0x40000000
	// if we are loaded as a firmware at zero start address
	// otherwise the device tree address should be preloaded in x0
	cmp		x7, #0
	bne		.l1
	mov		x0, #0x40000000
.l1:

	// Enable FP/SIMD
	mov		x10, #3 << 20
	msr		cpacr_el1, x10

	// set destination address: 0x40100000
	mov		x10, #0x40000000
	add		x10, x10, #0x00100000

	// set source address
	mov		x11, x7

	// set limit
	ldr		x12, =_end

.copy_loop:
	ldr		x9, [x11]
	str		x9, [x10]
	add		x10, x10, #8
	add		x11, x11, #8

	cmp		x10, x12
	bne		.copy_loop

	ldr		x12, =__stack_top
	mov		sp, x12

	ldr		x12, =start
	blr		x12

