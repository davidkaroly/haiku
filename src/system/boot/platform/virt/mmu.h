/*
 * Copyright 2021-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */


#ifndef MMU_H
#define MMU_H


#include <SupportDefs.h>

#include <boot/platform.h>
#include <util/FixedWidthPointer.h>


extern phys_addr_t gMemBase;
extern size_t gTotalMem;


void mmu_init();
void mmu_init_for_kernel(phys_addr_t &pageDirectory);

phys_addr_t allocate_virtual_pages(size_t size);
phys_addr_t allocate_physical_page(void);


inline addr_t
fix_address(addr_t address)
{
	addr_t result;
	if (platform_bootloader_address_to_kernel_address((void *)address, &result)
		!= B_OK)
		return address;

	return result;
}


template<typename Type>
inline void
fix_address(FixedWidthPointer<Type>& p)
{
	if (p != NULL)
		p.SetTo(fix_address(p.Get()));
}


#endif	/* MMU_H */
