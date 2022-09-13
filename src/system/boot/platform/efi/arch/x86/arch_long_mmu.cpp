/*
 * Copyright 2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */


#include <algorithm>

// Include the x86_64 version of descriptors.h
#define __x86_64__
#include <arch/x86/descriptors.h>
#undef __x86_64__

#include <kernel.h>
#include <boot/platform.h>
#include <boot/stage2.h>

#include "efi_platform.h"
#include "generic_mmu.h"
#include "mmu.h"

//#define TRACE_LONG_MMU
#ifdef TRACE_LONG_MMU
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


//#define TRACE_MEMORY_MAP


// Ignore memory above 512GB
#define PHYSICAL_MEMORY_LOW		0x00000000
#define PHYSICAL_MEMORY_HIGH	0x8000000000ull


struct gdt_idt_descr {
	uint16_t	limit;
	uint32_t	base;
} _PACKED;


void
arch_mmu_init_long_gdt(gdt_idt_descr &longBootGDTDescriptor)
{
	segment_descriptor *longBootGDT;

	if (platform_allocate_region((void **)&longBootGDT,
			BOOT_GDT_SEGMENT_COUNT * sizeof(segment_descriptor), 0, false) != B_OK) {
		panic("Failed to allocate GDT.\n");
	}

	STATIC_ASSERT(BOOT_GDT_SEGMENT_COUNT > KERNEL_CODE_SEGMENT
		&& BOOT_GDT_SEGMENT_COUNT > KERNEL_DATA_SEGMENT
		&& BOOT_GDT_SEGMENT_COUNT > USER_CODE_SEGMENT
		&& BOOT_GDT_SEGMENT_COUNT > USER_DATA_SEGMENT);

	clear_segment_descriptor(&longBootGDT[0]);

	// Set up code/data segments (TSS segments set up later in the kernel).
	set_segment_descriptor(&longBootGDT[KERNEL_CODE_SEGMENT], DT_CODE_EXECUTE_ONLY,
		DPL_KERNEL);
	set_segment_descriptor(&longBootGDT[KERNEL_DATA_SEGMENT], DT_DATA_WRITEABLE,
		DPL_KERNEL);
	set_segment_descriptor(&longBootGDT[USER_CODE_SEGMENT], DT_CODE_EXECUTE_ONLY,
		DPL_USER);
	set_segment_descriptor(&longBootGDT[USER_DATA_SEGMENT], DT_DATA_WRITEABLE,
		DPL_USER);

	longBootGDTDescriptor.limit = BOOT_GDT_SEGMENT_COUNT * sizeof(segment_descriptor);
	longBootGDTDescriptor.base = (uint32_t)longBootGDT;

	TRACE("gdt base = 0x%08" PRIx32 ", limit = %" PRIu16 "\n",
		longBootGDTDescriptor.base, longBootGDTDescriptor.limit);
}


void
arch_long_mmu_post_efi_setup(size_t memoryMapSize,
	efi_memory_descriptor *memoryMap, size_t descriptorSize,
	uint32_t descriptorVersion)
{
	build_physical_allocated_list(memoryMapSize, memoryMap,
		descriptorSize, descriptorVersion);

	// Switch EFI to virtual mode, using the kernel pmap.
	kRuntimeServices->SetVirtualAddressMap(memoryMapSize, descriptorSize,
		descriptorVersion, memoryMap);

#ifdef TRACE_MEMORY_MAP
	dprintf("phys memory ranges:\n");
	for (uint32_t i = 0; i < gKernelArgs.num_physical_memory_ranges; i++) {
		uint64 start = gKernelArgs.physical_memory_range[i].start;
		uint64 size = gKernelArgs.physical_memory_range[i].size;
		dprintf("    0x%08" B_PRIx64 "-0x%08" B_PRIx64 ", length 0x%08" B_PRIx64 "\n",
			start, start + size, size);
	}

	dprintf("allocated phys memory ranges:\n");
	for (uint32_t i = 0; i < gKernelArgs.num_physical_allocated_ranges; i++) {
		uint64 start = gKernelArgs.physical_allocated_range[i].start;
		uint64 size = gKernelArgs.physical_allocated_range[i].size;
		dprintf("    0x%08" B_PRIx64 "-0x%08" B_PRIx64 ", length 0x%08" B_PRIx64 "\n",
			start, start + size, size);
	}

	dprintf("allocated virt memory ranges:\n");
	for (uint32_t i = 0; i < gKernelArgs.num_virtual_allocated_ranges; i++) {
		uint64 start = gKernelArgs.virtual_allocated_range[i].start;
		uint64 size = gKernelArgs.virtual_allocated_range[i].size;
		dprintf("    0x%08" B_PRIx64 "-0x%08" B_PRIx64 ", length 0x%08" B_PRIx64 "\n",
			start, start + size, size);
	}
#endif
}


addr_t
arch_long_mmu_generate_post_efi_page_tables(size_t memoryMapSize,
	efi_memory_descriptor *memoryMap, size_t descriptorSize,
	uint32_t descriptorVersion)
{
	build_physical_memory_list(memoryMapSize, memoryMap,
		descriptorSize, descriptorVersion,
		PHYSICAL_MEMORY_LOW, PHYSICAL_MEMORY_HIGH);

	// Find the highest physical memory address. We map all physical memory
	// into the kernel address space, so we want to make sure we map everything
	// we have available.
	uint64 maxAddress = 0;
	for (size_t i = 0; i < memoryMapSize / descriptorSize; ++i) {
		efi_memory_descriptor *entry
			= (efi_memory_descriptor *)((addr_t)memoryMap + i * descriptorSize);
		maxAddress = std::max(maxAddress,
				      entry->PhysicalStart + entry->NumberOfPages * 4096);
	}

	// Want to map at least 4GB, there may be stuff other than usable RAM that
	// could be in the first 4GB of physical address space.
	maxAddress = std::max(maxAddress, (uint64)0x100000000ll);
	maxAddress = ROUNDUP(maxAddress, 0x40000000);

	// Currently only use 1 PDPT (512GB). This will need to change if someone
	// wants to use Haiku on a box with more than 512GB of RAM but that's
	// probably not going to happen any time soon.
	if (maxAddress / 0x40000000 > 512)
		panic("Can't currently support more than 512GB of RAM!");

	// Allocate the top level PML4.
	uint64_t *pml4 = NULL;
	if (platform_allocate_region((void**)&pml4, B_PAGE_SIZE, 0, false) != B_OK)
		panic("Failed to allocate PML4.");
	memset(pml4, 0, B_PAGE_SIZE);

	// Store PML4 physical and virtual address in kernel args
	addr_t virPgdir;
	gKernelArgs.arch_args.phys_pgdir = (uint32_t)(addr_t)pml4;
	platform_bootloader_address_to_kernel_address(pml4,
		&virPgdir);
	gKernelArgs.arch_args.vir_pgdir = virPgdir | 0xffffffff00000000;

	// Store the virtual memory usage information.
	gKernelArgs.virtual_allocated_range[0].start = KERNEL_LOAD_BASE_64_BIT;
	gKernelArgs.virtual_allocated_range[0].size
		= get_current_virtual_address() - KERNEL_LOAD_BASE;
	gKernelArgs.num_virtual_allocated_ranges = 1;
	gKernelArgs.arch_args.virtual_end = ROUNDUP(KERNEL_LOAD_BASE_64_BIT
		+ gKernelArgs.virtual_allocated_range[0].size, 0x200000);

	uint64_t *pdpt;
	uint64_t *pageDir;

	// Create page tables for the physical map area. Also map this PDPT
	// temporarily at the bottom of the address space so that we are identity
	// mapped.

	pdpt = (uint64*)mmu_allocate_page();
	memset(pdpt, 0, B_PAGE_SIZE);
	pml4[510] = (uint64_t)pdpt | kTableMappingFlags;
	pml4[0] = (uint64_t)pdpt | kTableMappingFlags;

	for (uint64 i = 0; i < maxAddress; i += 0x40000000) {
		pageDir = (uint64*)mmu_allocate_page();
		memset(pageDir, 0, B_PAGE_SIZE);
		pdpt[i / 0x40000000] = (uint64_t)pageDir | kTableMappingFlags;

		for (uint64 j = 0; j < 0x40000000; j += 0x200000) {
			pageDir[j / 0x200000] = (i + j) | kLargePageMappingFlags;
		}
	}

	// Allocate tables for the kernel mappings.

	pdpt = (uint64*)mmu_allocate_page();
	memset(pdpt, 0, B_PAGE_SIZE);
	pml4[511] = (uint64_t)pdpt | kTableMappingFlags;

	pageDir = (uint64*)mmu_allocate_page();
	memset(pageDir, 0, B_PAGE_SIZE);
	pdpt[510] = (uint64_t)pageDir | kTableMappingFlags;

	// We can now allocate page tables and duplicate the mappings across from
	// the 32-bit address space to them.
	uint64_t *pageTable = NULL; // shush, compiler.
	for (uint32 i = 0; i < gKernelArgs.virtual_allocated_range[0].size
			/ B_PAGE_SIZE; i++) {
		if ((i % 512) == 0) {
			pageTable = (uint64*)mmu_allocate_page();
			memset(pageTable, 0, B_PAGE_SIZE);
			pageDir[i / 512] = (uint64_t)pageTable | kTableMappingFlags;
		}

		// Get the physical address to map.
		void *phys;
		if (platform_kernel_address_to_bootloader_address(
			KERNEL_LOAD_BASE + (i * B_PAGE_SIZE), &phys) != B_OK) {
			continue;
		}

		pageTable[i % 512] = (addr_t)phys | kPageMappingFlags;
	}

	return (addr_t)pml4;
}
