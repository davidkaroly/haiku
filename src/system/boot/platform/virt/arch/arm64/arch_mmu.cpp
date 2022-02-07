/*
 * Copyright 2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */

#include "arch_mmu.h"
#include "mmu.h"


//#define TRACE_MMU
#ifdef TRACE_MMU
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


void
arch_map_region(addr_t virtAddr, phys_addr_t physAddr, size_t size)
{
	// stub
}


static void
arch_mmu_allocate_page_tables(void)
{
	// stub
}


void
arch_mmu_init(void)
{
	arch_mmu_allocate_page_tables();
}


void
arch_mmu_init_for_kernel(phys_addr_t &pageDirectory)
{
	// stub
	pageDirectory = 0xdeadbeef;
}
