/*
 * Copyright 2004-2005, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include "arch_cpu.h"
#include "cpu.h"

#include <OS.h>
#include <boot/platform.h>
#include <boot/stdio.h>
#include <boot/kernel_args.h>
#include <boot/stage2.h>

extern "C" void
cpu_init()
{
	boot_arch_cpu_init();

	gKernelArgs.num_cpus = 1;
}


extern "C" void
platform_load_ucode(BootVolume& volume)
{
	// empty
}
