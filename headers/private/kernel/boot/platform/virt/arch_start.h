/*
 * Copyright 2019-2021, Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */
#ifndef __ARCH_START_H
#define __ARCH_START_H


void arch_convert_kernel_args(void);
void arch_start_kernel(void *trampoline, phys_addr_t pageDirectory,
	addr_t kernelEntry, addr_t kernelArgs);


#endif /* __ARCH_START_H */
