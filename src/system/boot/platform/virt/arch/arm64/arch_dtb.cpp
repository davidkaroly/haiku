/*
 * Copyright 2022 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <boot/stage2.h>

extern "C" {
#include <libfdt.h>
}

#include "dtb.h"


void
arch_handle_fdt(const void* fdt, int node)
{
	// empty
}


void
arch_dtb_set_kernel_args(void)
{
	void *kernel_args_fdt;
	dtb_copy_fdt(&kernel_args_fdt, gFdt);
	gKernelArgs.arch_args.fdt = kernel_args_fdt;
}
