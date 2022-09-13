/*
 * Copyright 2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */


#include <kernel.h>
#include <arch_kernel.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>

#include "efi_platform.h"
#include "generic_mmu.h"
#include "mmu.h"
#include "serial.h"


//#define TRACE_LONG_START
#ifdef TRACE_LONG_START
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


struct gdt_idt_descr {
	uint16_t	limit;
	uint32_t	base;
} _PACKED;


extern "C" typedef void (*enter_kernel_t)(uint32_t, addr_t, addr_t, addr_t,
	struct gdt_idt_descr *);


// From entry_long.S
extern "C" void arch_enter_kernel_long(uint32_t pageDirectory, addr_t kernelArgs,
	addr_t kernelEntry, addr_t kernelStackTop, struct gdt_idt_descr *gdtDescriptor);

// From arch_long_mmu.cpp
extern void arch_mmu_init_long_gdt(gdt_idt_descr &longBootGDTDescriptor);

extern void arch_long_mmu_post_efi_setup(size_t memoryMapSize,
	efi_memory_descriptor *memoryMap, size_t descriptorSize,
	uint32_t descriptorVersion);

extern uint32_t arch_long_mmu_generate_post_efi_page_tables(size_t memoryMapSize,
	efi_memory_descriptor *memoryMap, size_t descriptorSize,
	uint32_t descriptorVersion);


void
arch_long_start_kernel(addr_t kernelEntry)
{
	gdt_idt_descr longBootGDTDescriptor;
	arch_mmu_init_long_gdt(longBootGDTDescriptor);

	// Copy entry.S trampoline to lower 1M
	enter_kernel_t enter_kernel = (enter_kernel_t)0xa000;
	memcpy((void *)enter_kernel, (void *)arch_enter_kernel_long, B_PAGE_SIZE);

	// Allocate virtual memory for kernel args
	struct kernel_args *kernelArgs = NULL;
	if (platform_allocate_region((void **)&kernelArgs,
			sizeof(struct kernel_args), 0, false) != B_OK)
		panic("Failed to allocate kernel args.");

	addr_t virtKernelArgs;
	platform_bootloader_address_to_kernel_address((void*)kernelArgs,
		&virtKernelArgs);

	// Prepare to exit EFI boot services.
	// Read the memory map.
	// First call is to determine the buffer size.
	size_t memoryMapSize = 0;
	efi_memory_descriptor dummy;
	size_t mapKey;
	size_t descriptorSize;
	uint32_t descriptorVersion;
	if (kBootServices->GetMemoryMap(&memoryMapSize, &dummy, &mapKey,
			&descriptorSize, &descriptorVersion) != EFI_BUFFER_TOO_SMALL) {
		panic("Unable to determine size of system memory map");
	}

	// Allocate a buffer twice as large as needed just in case it gets bigger
	// between calls to ExitBootServices.
	size_t actualMemoryMapSize = memoryMapSize * 2;
	efi_memory_descriptor *memoryMap
		= (efi_memory_descriptor *)kernel_args_malloc(actualMemoryMapSize);

	if (memoryMap == NULL)
		panic("Unable to allocate memory map.");

	// Read (and print) the memory map.
	memoryMapSize = actualMemoryMapSize;
	if (kBootServices->GetMemoryMap(&memoryMapSize, memoryMap, &mapKey,
			&descriptorSize, &descriptorVersion) != EFI_SUCCESS) {
		panic("Unable to fetch system memory map.");
	}

	addr_t addr = (addr_t)memoryMap;
	dprintf("System provided memory map:\n");
	for (size_t i = 0; i < memoryMapSize / descriptorSize; i++) {
		efi_memory_descriptor *entry
			= (efi_memory_descriptor *)(addr + i * descriptorSize);
		dprintf("  phys: 0x%08" PRIx64 "-0x%08" PRIx64
			", virt: 0x%08" PRIx64 "-0x%08" PRIx64
			", type: %s (%#x), attr: %#" PRIx64 "\n",
			entry->PhysicalStart,
			entry->PhysicalStart + entry->NumberOfPages * B_PAGE_SIZE,
			entry->VirtualStart,
			entry->VirtualStart + entry->NumberOfPages * B_PAGE_SIZE,
			memory_region_type_str(entry->Type), entry->Type,
			entry->Attribute);
	}

	// Generate page tables for use after ExitBootServices.
	uint32_t pageDirectory = arch_long_mmu_generate_post_efi_page_tables(
		memoryMapSize, memoryMap, descriptorSize, descriptorVersion);

	// Attempt to fetch the memory map and exit boot services.
	// This needs to be done in a loop, as ExitBootServices can change the
	// memory map.
	// Even better: Only GetMemoryMap and ExitBootServices can be called after
	// the first call to ExitBootServices, as the firmware is permitted to
	// partially exit. This is why twice as much space was allocated for the
	// memory map, as it's impossible to allocate more now.
	// A changing memory map shouldn't affect the generated page tables, as
	// they only needed to know about the maximum address, not any specific
	// entry.
	dprintf("Calling ExitBootServices. So long, EFI!\n");
	while (true) {
		if (kBootServices->ExitBootServices(kImage, mapKey) == EFI_SUCCESS) {
			// The console was provided by boot services, disable it.
			stdout = NULL;
			stderr = NULL;
			// Also switch to legacy serial output
			// (may not work on all systems)
			serial_switch_to_legacy();
			dprintf("Switched to legacy serial output\n");
			break;
		}

		memoryMapSize = actualMemoryMapSize;
		if (kBootServices->GetMemoryMap(&memoryMapSize, memoryMap, &mapKey,
				&descriptorSize, &descriptorVersion) != EFI_SUCCESS) {
			panic("Unable to fetch system memory map.");
		}
	}

	// Update EFI, generate final kernel physical memory map, etc.
	arch_long_mmu_post_efi_setup(memoryMapSize, memoryMap,
		descriptorSize, descriptorVersion);

	// Copy final kernel args
	// This should be the last step before jumping to the kernel
	// as there are some fixups happening to kernel_args even in the last minute
	memcpy(kernelArgs, &gKernelArgs, sizeof(struct kernel_args));

	//smp_boot_other_cpus(pageDirectory, kernelEntry, virtKernelArgs);

	// Enter the kernel!
	dprintf("long_enter_kernel(pageDirectory: 0x%08" PRIx32 ", kernelArgs: 0x%08" B_PRIxADDR ", "
		"kernelEntry: 0x%08" B_PRIxADDR ", sp: 0x%08" B_PRIx64 ", longBootGDTDescriptor: %p)\n",
		pageDirectory, virtKernelArgs, kernelEntry,
		gKernelArgs.cpu_kstack[0].start + gKernelArgs.cpu_kstack[0].size,
		&longBootGDTDescriptor);

	enter_kernel(pageDirectory, virtKernelArgs, kernelEntry,
		gKernelArgs.cpu_kstack[0].start + gKernelArgs.cpu_kstack[0].size,
		&longBootGDTDescriptor);
}
