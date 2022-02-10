/*
 * Copyright 2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */

#include <arch_cpu_defs.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <kernel.h>

#include "arch_mmu.h"
#include "mmu.h"


//#define TRACE_MMU
#ifdef TRACE_MMU
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


phys_addr_t sPageDirectory = 0;


static Pte*
lookup_pte(addr_t virtAddr, bool alloc)
{
	Pte *pte = (Pte*)sPageDirectory;
	for (int level = 2; level > 0; level--) {
		pte += VirtAdrPte(virtAddr, level);
		if (!((1 << pteValid) & pte->flags)) {
			if (!alloc)
				return NULL;
			phys_addr_t entry = allocate_physical_page();
			if (entry == 0)
				return NULL;
			memset((Pte*)entry, 0, B_PAGE_SIZE);
			pte->ppn = entry / B_PAGE_SIZE;
			pte->flags |= (1 << pteValid);
		}
		pte = (Pte *)(pte->ppn * B_PAGE_SIZE);
	}
	pte += VirtAdrPte(virtAddr, 0);
	return pte;
}


static void
map_page(addr_t virtAddr, phys_addr_t physAddr, uint64 flags)
{
	//TRACE("map_page(0x%" B_PRIxADDR ", 0x%" B_PRIxPHYSADDR ")\n", virtAddr, physAddr);
	Pte* pte = lookup_pte(virtAddr, true);
	if (pte == NULL)
		panic("can't allocate page table");

	pte->ppn = physAddr / B_PAGE_SIZE;
	pte->flags = (1 << pteValid) | (1 << pteAccessed) | (1 << pteDirty) | flags;
}


static void
map_range(addr_t virtAddr, phys_addr_t physAddr, size_t size, uint64 flags)
{
	//TRACE("map_range(0x%" B_PRIxADDR ", 0x%" B_PRIxPHYSADDR ", 0x%"
	//	B_PRIxADDR ", 0x%" B_PRIx64 ")\n", virtAddr, physAddr, size, flags);
	for (size_t i = 0; i < size; i += B_PAGE_SIZE)
		map_page(virtAddr + i, physAddr + i, flags);

	ASSERT_ALWAYS(insert_virtual_allocated_range(virtAddr, size) >= B_OK);
}


static void
insert_virtual_range_to_keep(uint64 start, uint64 size)
{
	status_t status = insert_address_range(
		gKernelArgs.arch_args.virtual_ranges_to_keep,
		&gKernelArgs.arch_args.num_virtual_ranges_to_keep,
		MAX_VIRTUAL_RANGES_TO_KEEP, start, size);

	if (status == B_ENTRY_NOT_FOUND)
		panic("too many virtual ranges to keep");
	else if (status != B_OK)
		panic("failed to add virtual range to keep");
}


static void
map_range_to_new_area(addr_range& range, uint32_t flags)
{
	if (range.size == 0) {
		range.start = 0;
		return;
	}

	phys_addr_t physAddr = range.start;
	addr_t virtAddr = allocate_virtual_pages(range.size);

	map_range(virtAddr, physAddr, range.size, flags);

	range.start = virtAddr;

	insert_virtual_range_to_keep(range.start, range.size);
}


void
arch_map_region(addr_t virtAddr, phys_addr_t physAddr, size_t size)
{
	map_range(virtAddr, physAddr, size,
		(1 << pteRead) | (1<< pteWrite) | (1 << pteExec));
}


static void
arch_mmu_allocate_page_tables(void)
{
	sPageDirectory = allocate_physical_page();
	memset((void *)sPageDirectory, 0, B_PAGE_SIZE);
	TRACE("sPageDirectory = 0x%08" B_PRIxPHYSADDR "\n", sPageDirectory);

	Pte *root = (Pte *)sPageDirectory;
	for (uint64 i = VirtAdrPte(KERNEL_BASE, 2); i <= VirtAdrPte(KERNEL_TOP, 2); i++) {
		Pte* pte = &root[i];
		pte->ppn = allocate_physical_page() / B_PAGE_SIZE;
		if (pte->ppn == 0)
			panic("can't alloc early physical page");
		memset((void *)(B_PAGE_SIZE * pte->ppn), 0, B_PAGE_SIZE);
		pte->flags |= (1 << pteValid);
	}
}


void
arch_mmu_init(void)
{
	arch_mmu_allocate_page_tables();
}


void
arch_mmu_init_for_kernel(phys_addr_t &pageDirectory)
{
	gKernelArgs.arch_args.num_virtual_ranges_to_keep = 0;

	// Physical memory mapping
	gKernelArgs.arch_args.physMap.size
		= gKernelArgs.physical_memory_range[0].size;
	gKernelArgs.arch_args.physMap.start = KERNEL_TOP + 1
		- gKernelArgs.arch_args.physMap.size;
	map_range(gKernelArgs.arch_args.physMap.start,
		gKernelArgs.physical_memory_range[0].start,
		gKernelArgs.arch_args.physMap.size,
		(1 << pteRead) | (1 << pteWrite));

	// mark the frame buffer as reserved area
	addr_t virtFrameFuffer;
	platform_bootloader_address_to_kernel_address(
		(void*)gKernelArgs.frame_buffer.physical_buffer.start,
		&virtFrameFuffer);
	insert_virtual_range_to_keep(virtFrameFuffer,
		gKernelArgs.frame_buffer.physical_buffer.size);

	// remap MMIO areas to high virtual address
	map_range_to_new_area(gKernelArgs.arch_args.clint,
		(1 << pteRead) | (1 << pteWrite));
	map_range_to_new_area(gKernelArgs.arch_args.htif,
		(1 << pteRead) | (1 << pteWrite));
	map_range_to_new_area(gKernelArgs.arch_args.plic,
		(1 << pteRead) | (1 << pteWrite));
	map_range_to_new_area(gKernelArgs.arch_args.uart.regs,
		(1 << pteRead) | (1 << pteWrite));

	sort_address_ranges(gKernelArgs.arch_args.virtual_ranges_to_keep,
		gKernelArgs.arch_args.num_virtual_ranges_to_keep);

	pageDirectory = sPageDirectory;
}
