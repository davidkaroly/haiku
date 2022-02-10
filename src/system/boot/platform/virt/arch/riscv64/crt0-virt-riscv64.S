/*
 * Copyright 2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */


	.section	.text.head
	.global		_start

_start:
	//show some debug output on the serial port
	//li a2, 0x10000000
	//li x9, 65
	//sw x9, 0(a2)

	// Enable FPU
	csrr x9, mstatus
	li x8, 0x2000
	or x9, x9, x8
	csrw mstatus, x9

	// Initialize stack
	la sp, __stack_top
	mv fp, sp

	// TODO: save hartId

	// Load FDT pointer
	mv a0, a1
	call start
	j .
