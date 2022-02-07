/*
 * Copyright 2019-2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */


#include <kernel.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>

#include "mmu.h"


extern "C" typedef void (*arch_enter_kernel_t)(addr_t, addr_t, addr_t, addr_t);


void
arch_convert_kernel_args(void)
{
	fix_address(gKernelArgs.arch_args.fdt);
}

void
arch_start_kernel(void *trampoline, phys_addr_t pageDirectory,
	addr_t kernelEntry, addr_t kernelArgs)
{
	arch_enter_kernel_t enter_kernel = (arch_enter_kernel_t)trampoline;
	addr_t ttbr0 = pageDirectory;

	// Enter the kernel!
	dprintf("enter_kernel(ttbr0: 0x%08" B_PRIxADDR ", kernelArgs: 0x%08" B_PRIxADDR ", "
		"kernelEntry: 0x%08" B_PRIxADDR ", sp: 0x%08" B_PRIx64 ")\n",
		ttbr0, kernelArgs, kernelEntry,
		gKernelArgs.cpu_kstack[0].start + gKernelArgs.cpu_kstack[0].size);

	enter_kernel(ttbr0, kernelArgs, kernelEntry,
		gKernelArgs.cpu_kstack[0].start + gKernelArgs.cpu_kstack[0].size);
}
