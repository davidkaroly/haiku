/*
 * Copyright, 2019-2020 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef ARCH_MMU_H
#define ARCH_MMU_H

#include <SupportDefs.h>

void arch_mmu_init(void);
void arch_mmu_init_for_kernel(phys_addr_t &pageDirectory);

void arch_map_region(addr_t virtAddr, phys_addr_t physAddr, size_t size);

#endif /* ARCH_MMU_H */
