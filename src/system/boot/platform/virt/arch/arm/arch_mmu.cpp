/*
 * Copyright 2019-2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */


#include <arm_mmu.h>
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


//#define TRACE_PAGE_DIRECTORY

#define ALIGN_PAGEDIR			(1024 * 16)
#define MAX_PAGE_TABLES			192
#define PAGE_TABLE_AREA_SIZE	(MAX_PAGE_TABLES * ARM_MMU_L2_COARSE_TABLE_SIZE)

static uint32_t *sPageDirectory = NULL;
static uint32_t *sNextPageTable = NULL;
static uint32_t *sLastPageTable = NULL;


#ifdef TRACE_PAGE_DIRECTORY
static void
dump_page_dir(void)
{
	dprintf("=== Page Directory ===\n");
	for (uint32_t i = 0; i < ARM_MMU_L1_TABLE_ENTRY_COUNT; i++) {
		uint32 directoryEntry = sPageDirectory[i];
		if (directoryEntry != 0) {
			dprintf("virt 0x%08x --> page table 0x%08x type 0x%08x\n",
				i << 20, directoryEntry & ARM_PDE_ADDRESS_MASK,
				directoryEntry & ARM_PDE_TYPE_MASK);
			uint32_t *pageTable = (uint32_t *)(directoryEntry & ARM_PDE_ADDRESS_MASK);
			for (uint32_t j = 0; j < ARM_MMU_L2_COARSE_ENTRY_COUNT; j++) {
				uint32 tableEntry = pageTable[j];
				if (tableEntry != 0) {
					dprintf("virt 0x%08x     --> page 0x%08x type+flags 0x%08x\n",
						(i << 20) | (j << 12),
						tableEntry & ARM_PTE_ADDRESS_MASK,
						tableEntry & (~ARM_PTE_ADDRESS_MASK));
				}
			}
		}
	}
}
#endif


static uint32 *
get_next_page_table(void)
{
	uint32 *pageTable = sNextPageTable;
	sNextPageTable += ARM_MMU_L2_COARSE_ENTRY_COUNT;
	if (sNextPageTable >= sLastPageTable)
		panic("ran out of page tables\n");
	return pageTable;
}


static void
map_page(addr_t virtAddr, phys_addr_t physAddr, uint32_t flags)
{
	physAddr &= ~(B_PAGE_SIZE - 1);

	uint32 *pageTable = NULL;
	uint32 pageDirectoryIndex = VADDR_TO_PDENT(virtAddr);
	uint32 pageDirectoryEntry = sPageDirectory[pageDirectoryIndex];

	if (pageDirectoryEntry == 0) {
		pageTable = get_next_page_table();
		sPageDirectory[pageDirectoryIndex] = (uint32_t)pageTable | ARM_MMU_L1_TYPE_COARSE;
	} else {
		pageTable = (uint32 *)(pageDirectoryEntry & ARM_PDE_ADDRESS_MASK);
	}

	uint32 pageTableIndex = VADDR_TO_PTENT(virtAddr);
	pageTable[pageTableIndex] = physAddr | flags | ARM_MMU_L2_TYPE_SMALLNEW;
}


static void
map_range(addr_t virtAddr, phys_addr_t physAddr, size_t size, uint32_t flags)
{
	//TRACE("map 0x%08" B_PRIxADDR " --> 0x%08" B_PRIxPHYSADDR
	//	", len=0x%08" B_PRIxSIZE ", flags=0x%08" PRIx32 "\n",
	//	virtAddr, physAddr, size, flags);

	for (addr_t offset = 0; offset < size; offset += B_PAGE_SIZE) {
		map_page(virtAddr + offset, physAddr + offset, flags);
	}
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
		ARM_MMU_L2_FLAG_B | ARM_MMU_L2_FLAG_C | ARM_MMU_L2_FLAG_AP_KRW);
}


static void
arch_mmu_allocate_page_tables(void)
{
	if (platform_allocate_region((void **)&sPageDirectory,
		ARM_MMU_L1_TABLE_SIZE + ALIGN_PAGEDIR + PAGE_TABLE_AREA_SIZE, 0, false) != B_OK)
		panic("Failed to allocate page directory.");
	sPageDirectory = (uint32 *)ROUNDUP((uint32)sPageDirectory, ALIGN_PAGEDIR);
	memset(sPageDirectory, 0, ARM_MMU_L1_TABLE_SIZE);

	sNextPageTable = (uint32*)((uint32)sPageDirectory + ARM_MMU_L1_TABLE_SIZE);
	sLastPageTable = (uint32*)((uint32)sNextPageTable + PAGE_TABLE_AREA_SIZE);

	memset(sNextPageTable, 0, PAGE_TABLE_AREA_SIZE);

	TRACE("sPageDirectory = 0x%08x\n", (uint32)sPageDirectory);
	TRACE("sNextPageTable = 0x%08x\n", (uint32)sNextPageTable);
	TRACE("sLastPageTable = 0x%08x\n", (uint32)sLastPageTable);
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

	// mark the frame buffer as reserved area
	addr_t virtFrameFuffer;
	platform_bootloader_address_to_kernel_address(
		(void*)gKernelArgs.frame_buffer.physical_buffer.start,
		&virtFrameFuffer);
	insert_virtual_range_to_keep(virtFrameFuffer,
		gKernelArgs.frame_buffer.physical_buffer.size);

	// remap UART registers to high virtual address
	map_range_to_new_area(gKernelArgs.arch_args.uart.regs,
		ARM_MMU_L2_FLAG_B | ARM_MMU_L2_FLAG_AP_KRW | ARM_MMU_L2_FLAG_XN);

	sort_address_ranges(gKernelArgs.arch_args.virtual_ranges_to_keep,
		gKernelArgs.arch_args.num_virtual_ranges_to_keep);

	addr_t virtPageDirectory;
	platform_bootloader_address_to_kernel_address((void*)sPageDirectory, &virtPageDirectory);

	gKernelArgs.arch_args.phys_pgdir = (uint32)sPageDirectory;
	gKernelArgs.arch_args.vir_pgdir = (uint32)virtPageDirectory;
	gKernelArgs.arch_args.next_pagetable = (uint32)(sNextPageTable) - (uint32)sPageDirectory;
	gKernelArgs.arch_args.last_pagetable = (uint32)(sLastPageTable) - (uint32)sPageDirectory;

	TRACE("gKernelArgs.arch_args.phys_pgdir     = 0x%08x\n",
		(uint32_t)gKernelArgs.arch_args.phys_pgdir);
	TRACE("gKernelArgs.arch_args.vir_pgdir      = 0x%08x\n",
		(uint32_t)gKernelArgs.arch_args.vir_pgdir);
	TRACE("gKernelArgs.arch_args.next_pagetable = 0x%08x\n",
		(uint32_t)gKernelArgs.arch_args.next_pagetable);
	TRACE("gKernelArgs.arch_args.last_pagetable = 0x%08x\n",
		(uint32_t)gKernelArgs.arch_args.last_pagetable);

#ifdef TRACE_PAGE_DIRECTORY
	dump_page_dir();
#endif

	pageDirectory = (phys_addr_t)(addr_t)sPageDirectory;
}
