/*
 * Copyright 2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */


#include <asm_defs.h>

#undef __x86_64__
#include <arch/x86/descriptors.h>
#define __x86_64__

.text
.code64
/*
extern "C" void arch_enter_kernel_short(uint32_t pageDirectory, addr_t kernelArgs,
	addr_t kernelEntry, addr_t kernelStackTop,
	struct gdt_idt_descr *gdtDescriptor);
*/
FUNCTION(arch_enter_kernel_short):
	cli
	cld

	// Load 32-bit enabled GDT
	movq	%r8, %rax
	lgdt	(%rax)

	// Jump into the 32-bit code segment to transition to compatibility mode
	pushw	$KERNEL_CODE_SELECTOR
	pushq	$(.Lcompat_trampoline - arch_enter_kernel_short + 0xa000)
	lretq

.align 8
.code32
.Lcompat_trampoline:
	// initialize stack
	movl	%ecx, %esp

	// Set data segments
	mov		$KERNEL_DATA_SELECTOR, %ax
	mov		%ax, %ss
	mov		%ax, %ds
	mov		%ax, %es
	mov		%ax, %fs
	mov		%ax, %gs

	// disable Paging and Protected Mode
	movl	%cr0, %eax
	andl	$0x7ffffffe, %eax
	movl	%eax, %cr0

	// disable PAE and PGE
	movl	%cr4, %eax
	andl	$0xffffff5f, %eax
	movl	%eax, %cr4

	// save EDX as it will be clobbered by RDMSR
	movl	%edx, %ebp

	// Disable long mode by clearing EFER.LME.
	movl	$0xc0000080, %ecx
	rdmsr
	andl	$0xfffffeff, %eax
	wrmsr

	// restore EDX
	movl	%ebp, %edx

	// set page directory
	movl	%edi, %eax
	movl	%eax, %cr3

	// initialize CR0
	// - bit #31: Enable Paging
	// - bit #16: Write Protect
	// - bit  #5: Numeric Error Handling
	// - bit  #0: Protected Mode
	movl	$0x80010021, %eax
	movl	%eax, %cr0

	// initialize floating point unit
	fninit

	// push kernel args on stack
	pushl	$0			// currentCpu
	pushl	%esi		// kernelArgs
	pushl	$0			// fake return address
	pushl	%edx		// kernelEntry
	ret

	//return
	movl	$-1, %eax
	ret
FUNCTION_END(arch_enter_kernel_short)
