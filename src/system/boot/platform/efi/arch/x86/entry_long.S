/*
 * Copyright 2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */


#include <asm_defs.h>

#define __x86_64__
#include <arch/x86/descriptors.h>
#undef __x86_64__

.text
.code32

/*
extern "C" void arch_enter_kernel_long(uint32_t pageDirectory, addr_t kernelArgs,
	addr_t kernelEntry, addr_t kernelStackTop,
	struct gdt_idt_descr *gdtDescriptor);
*/
FUNCTION(arch_enter_kernel_long):
cli
cld

	movl	4(%esp), %edx	// pageDirectory
	movl	8(%esp), %edi	// kernelArgs
	movl	12(%esp), %ebx	// kernelEntry
	movl	16(%esp), %eax	// kernelStackTop
	movl	20(%esp), %esi	// gdtDescriptor

	// initialize stack
	movl	%eax, %esp

	// Currently running with 32-bit paging tables at an identity mapped
	// address. To switch to 64-bit paging we must first disable 32-bit paging,
	// otherwise loading the new CR3 will fault.

	// ensure that Paging is disabled
	movl	%cr0, %eax
	andl	$0x7fffffff, %eax
	movl	%eax, %cr0

	// ensure that PAE and PGE are enabled
	movl	%cr4, %eax
	orl		$0x000000a0, %eax
	movl	%eax, %cr4

	// Point CR3 to the kernel's PMLTop.
	movl	%edx, %eax
	movl	%eax, %cr3

	// Enable long mode by setting EFER.LME.
	movl	$0xc0000080, %ecx
	rdmsr
	orl		$0x00000100, %eax
	wrmsr

	// Re-enable paging, which will put us in compatibility mode as we are
	// currently in a 32-bit code segment.
	movl	%cr0, %eax
	orl		$0x80000001, %eax
	movl	%eax, %cr0

	// Load 64-bit enabled GDT
	movl	%esi, %eax
	lgdt	(%eax)

	// Set data segments
	mov		$KERNEL_DATA_SELECTOR, %ax
	mov		%ax, %ss
	xor		%ax, %ax
	mov		%ax, %ds
	mov		%ax, %es
	mov		%ax, %fs
	mov		%ax, %gs

	// Jump into the 64-bit code segment
	ljmp	$KERNEL_CODE_SELECTOR, $(.Llong_trampoline - arch_enter_kernel_long + 0xa000)

.align 8
.code64
.Llong_trampoline:
	// Clear the stack frame/RFLAGS.
	xorq	%rbp, %rbp
	push	$0
	popf

	// Get arguments and call the kernel entry point.
	// kernelArgs already loaded in %rdi
	xorl	%esi, %esi // current cpu
	movq	%rbx, %rax

	// apply fixup for addresses
	// this is a sad thing. this is a bad thing.
	movq	$0xffffffff00000000, %rdx
	orq		%rdx, %rax
	orq		%rdx, %rdi
	orq		%rdx, %rsp

	call	*%rax

	//return
	movl	$-1, %eax
	ret
FUNCTION_END(arch_enter_kernel_long)
