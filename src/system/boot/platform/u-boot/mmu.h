/*
 * Copyright 2004-2005, Axel Dörfler, axeld@pinc-software.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef MMU_H
#define MMU_H


#include <SupportDefs.h>


#ifdef __cplusplus
extern "C" {
#endif

extern void mmu_init(void* fdt);
extern void mmu_init_for_kernel(void);
extern addr_t mmu_map_physical_memory(addr_t physicalAddress,
	size_t size);
extern void *mmu_allocate(void *virtualAddress, size_t size);
extern void mmu_free(void *virtualAddress, size_t size);

extern status_t platform_bootloader_address_to_kernel_address(void *address,
	addr_t *_result);

extern status_t platform_kernel_address_to_bootloader_address(addr_t address,
	void **_result);


#ifdef __cplusplus
}
#endif


#endif	/* MMU_H */
