/*
 * Copyright 2004-2007, Axel Dörfler, axeld@pinc-software.de.
 * Based on code written by Travis Geiselbrecht for NewOS.
 *
 * Distributed under the terms of the MIT License.
 */


#include "arch_mmu.h"
#include "mmu.h"

#include <boot/platform.h>
#include <boot/stdio.h>
#include <boot/kernel_args.h>
#include <boot/stage2.h>
#include <arch/cpu.h>
#include <arch_kernel.h>
#include <kernel.h>
#include <AutoDeleter.h>

//#define TRACE_MMU
#ifdef TRACE_MMU
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif

//#define TRACE_MEMORY_MAP
	// Define this to print the memory map to serial debug,


struct MemoryRegion
{
	MemoryRegion* next;
	addr_t virtAddr;
	phys_addr_t physAddr;
	size_t size;
	uint32 protection;
};


// 8 MB for the kernel, kernel args, modules, driver settings, ...
static const size_t kMaxKernelSize = 0x800000;

// Start and end of ourselfs (from ld script)
extern int _start, _end;

MemoryRegion* sRegions = NULL;

phys_addr_t gMemBase = 0x40000000;
size_t gTotalMem = 0x10000000;

static addr_t sNextPhysicalAddress = 0;
static addr_t sNextVirtualAddress = 0;

static phys_addr_t
allocate_physical_pages(size_t size)
{
	size = ROUNDUP(size, B_PAGE_SIZE);
	phys_addr_t addr = ROUNDUP(sNextPhysicalAddress, B_PAGE_SIZE);

	if (addr + size - gMemBase > gTotalMem)
		return 0;

	sNextPhysicalAddress = addr + size;

	return addr;
}


phys_addr_t
allocate_physical_page(void)
{
	return allocate_physical_pages(B_PAGE_SIZE);
}


static void
free_physical_pages(phys_addr_t physAddr, size_t size)
{
	if (physAddr + size == (phys_addr_t)sNextPhysicalAddress)
		sNextPhysicalAddress -= size;
}


phys_addr_t
allocate_virtual_pages(size_t size)
{
	size = ROUNDUP(size, B_PAGE_SIZE);
	phys_addr_t addr = ROUNDUP(sNextVirtualAddress, B_PAGE_SIZE);
	sNextVirtualAddress = addr + size;

	return addr;
}


static void
free_virtual_pages(addr_t virtAddr, size_t size)
{
	if (virtAddr + size == sNextVirtualAddress)
		sNextVirtualAddress -= size;
}



//	#pragma mark -

extern "C" status_t
platform_allocate_region(void** address, size_t size, uint8 protection,
	bool exactAddress)
{
	size = ROUNDUP(size, B_PAGE_SIZE);

	if (exactAddress)
		return B_ERROR;

	ObjectDeleter<MemoryRegion> region(new(std::nothrow) MemoryRegion());
	if (!region.IsSet())
		return B_NO_MEMORY;

	region->physAddr = allocate_physical_pages(size);
	if (region->physAddr == 0)
		return B_NO_MEMORY;

#ifdef __riscv
	if (*address == (void *)0x80000000)
		*address = (void *)KERNEL_LOAD_BASE;
#endif

	if (*address == NULL)
		region->virtAddr = allocate_virtual_pages(size);
	else
		region->virtAddr = (addr_t)*address;

	region->size = size;
	region->protection = protection;

	*address = (void*)region->physAddr;

	region->next = sRegions;
	sRegions = region.Detach();

	return B_OK;
}


extern "C" status_t
platform_free_region(void* address, size_t size)
{
	MemoryRegion* prev = NULL;
	MemoryRegion* region = sRegions;
	while (region != NULL && !(region->physAddr == (phys_addr_t)address)) {
		prev = region;
		region = region->next;
	}
	if (region == NULL) {
		panic("platform_free_region: address %p is not allocated\n", address);
		return B_ERROR;
	}
	free_physical_pages(region->physAddr, region->size);
	free_virtual_pages(region->virtAddr, region->size);
	if (prev == NULL)
		sRegions = region->next;
	else
		prev->next = region->next;

	delete region;

	return B_OK;
}

extern "C" status_t
platform_allocate_lomem(void **address, size_t size)
{
	size = ROUNDUP(size, B_PAGE_SIZE);

	if (sNextPhysicalAddress + size > KERNEL_LOAD_BASE - B_PAGE_SIZE)
		return B_NO_MEMORY;

	ObjectDeleter<MemoryRegion> region(new(std::nothrow) MemoryRegion());
	if (!region.IsSet())
		return B_NO_MEMORY;

	region->physAddr = allocate_physical_pages(size);
	if (region->physAddr == 0)
		return B_NO_MEMORY;

	region->virtAddr = region->physAddr;
	region->size = size;
	region->protection = 0;

	*address = (void*)region->physAddr;

	region->next = sRegions;
	sRegions = region.Detach();

	return B_OK;
}


bool
mmu_next_region(void** cookie, addr_t* vaddr, phys_addr_t* paddr, size_t* size)
{
	if (*cookie == NULL)
		*cookie = sRegions;
	else
		*cookie = ((MemoryRegion *)*cookie)->next;

	MemoryRegion * region = (MemoryRegion *)*cookie;
	if (region == NULL)
		return false;

	if (region->virtAddr == 0)
		region->virtAddr = allocate_virtual_pages(region->size);

	*vaddr = region->virtAddr;
	*paddr = region->physAddr;
	*size = region->size;
	return true;
}


void
platform_release_heap(struct stage2_args* args, void* base)
{
	// empty as it will be freed automatically
}


status_t
platform_init_heap(struct stage2_args* args, void** _base, void** _top)
{
	addr_t heap = allocate_physical_pages(args->heap_size);
	if (heap == 0)
		return B_NO_MEMORY;

	*_base = (void *)heap;
	*_top = (void *)(heap + args->heap_size);
	return B_OK;
}


status_t
platform_bootloader_address_to_kernel_address(void* address, addr_t* result)
{
	MemoryRegion* region = sRegions;
	while (region != NULL && !((phys_addr_t)address >= region->physAddr
		&& (phys_addr_t)address < region->physAddr + region->size))
		region = region->next;

	if (region == NULL)
		return B_ERROR;

	*result = (addr_t)address - region->physAddr + region->virtAddr;
	return B_OK;
}


status_t
platform_kernel_address_to_bootloader_address(addr_t address, void** result)
{
	MemoryRegion* region = sRegions;
	while (region != NULL && !((phys_addr_t)address >= region->virtAddr
		&& (phys_addr_t)address < region->virtAddr + region->size))
		region = region->next;

	if (region == NULL)
		return B_ERROR;

	*result = (void*)(address - region->virtAddr + region->physAddr);
	return B_OK;
}


//	#pragma mark -

void
mmu_init(void)
{
	sNextPhysicalAddress = ROUNDUP((addr_t)&_end, 0x100000);
	sNextVirtualAddress = KERNEL_LOAD_BASE + kMaxKernelSize;
}


static void
build_physical_memory_list()
{
	gKernelArgs.num_physical_memory_ranges = 0;

	insert_physical_memory_range(gMemBase, gTotalMem);

	sort_address_ranges(gKernelArgs.physical_memory_range,
		gKernelArgs.num_physical_memory_ranges);
}


static void
build_physical_allocated_list()
{
	gKernelArgs.num_physical_allocated_ranges = 0;

	uint64_t base = gMemBase;
	uint64_t size = (uint64_t)sNextPhysicalAddress - gMemBase;
	insert_physical_allocated_range(base, size);

	sort_address_ranges(gKernelArgs.physical_allocated_range,
		gKernelArgs.num_physical_allocated_ranges);
}


void
mmu_init_for_kernel(phys_addr_t &pageDirectory)
{
	arch_mmu_init();

	void* cookie = NULL;
	addr_t vaddr;
	phys_addr_t paddr;
	size_t size;
	while (mmu_next_region(&cookie, &vaddr, &paddr, &size)) {
		arch_map_region(vaddr, paddr, size);

		if (vaddr >= KERNEL_LOAD_BASE)
			ASSERT_ALWAYS(insert_virtual_allocated_range(vaddr, size) >= B_OK);
	}

	sort_address_ranges(gKernelArgs.virtual_allocated_range,
		gKernelArgs.num_virtual_allocated_ranges);

	build_physical_memory_list();

	arch_mmu_init_for_kernel(pageDirectory);

	build_physical_allocated_list();

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

	dprintf("virt memory ranges to keep:\n");
	for (uint32_t i = 0; i < gKernelArgs.arch_args.num_virtual_ranges_to_keep; i++) {
		uint64 start = gKernelArgs.arch_args.virtual_ranges_to_keep[i].start;
		uint64 size = gKernelArgs.arch_args.virtual_ranges_to_keep[i].size;
		dprintf("    0x%08" B_PRIx64 "-0x%08" B_PRIx64 ", length 0x%08" B_PRIx64 "\n",
			start, start + size, size);
	}
#endif
}

