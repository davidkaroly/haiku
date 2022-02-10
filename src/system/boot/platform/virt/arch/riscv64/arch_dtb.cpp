/*
 * Copyright 2022 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <boot/stage2.h>

extern "C" {
#include <libfdt.h>
}

#include "dtb.h"


static addr_range sHtif = {0, 0};
static addr_range sPlic = {0, 0};
static addr_range sClint = {0, 0};

void
arch_handle_fdt(const void* fdt, int node)
{
	int compatibleLen;
	const char* compatible = (const char*)fdt_getprop(fdt, node,
		"compatible", &compatibleLen);

	if (compatible == NULL)
		return;

	//TODO: htif

	if (dtb_has_fdt_string(compatible, compatibleLen, "riscv,clint0")) {
		//uint64* reg = (uint64*)fdt_getprop(fdt, node, "reg", NULL);
		//sClint.start = fdt64_to_cpu(*(reg + 0));
		//sClint.size  = fdt64_to_cpu(*(reg + 1));
		dtb_get_reg(fdt, node, 0, sClint);
	}

	if (dtb_has_fdt_string(compatible, compatibleLen, "riscv,plic0")) {
		dtb_get_reg(fdt, node, 0, sPlic);
	}
}


void
arch_dtb_set_kernel_args(void)
{
	void *kernel_args_fdt;
	dtb_copy_fdt(&kernel_args_fdt, gFdt);
	gKernelArgs.arch_args.fdt = kernel_args_fdt;

	gKernelArgs.arch_args.htif = sHtif;
	gKernelArgs.arch_args.plic = sPlic;
	gKernelArgs.arch_args.clint = sClint;
}
