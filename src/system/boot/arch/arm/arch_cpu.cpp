/*
 * Copyright 2012-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ithamar R. Adema <ithamar@upgrade-android.com>
 */

#include <boot/arch/arm/arch_cpu.h>
#include <kernel/arch/arm/arch_cpu.h>

#include <OS.h>
#include <boot/platform.h>
#include <boot/stdio.h>
#include <boot/kernel_args.h>
#include <boot/stage2.h>
#include <arch/cpu.h>
#include <arch_kernel.h>
#include <arch_system_info.h>
#include <string.h>


#define TRACE_CPU
#ifdef TRACE_CPU
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


uint32_t
get_cpu_features(void)
{
	uint32_t result;
	asm volatile("MRC p15, 0, %0, c0, c0, 0": "=r" (result));
	return result;
}


uint32_t
get_cpu_id_pfr0(void)
{
	uint32_t result;
	asm volatile("MRC p15, 0, %0, c0, c1, 0": "=r" (result));
	return result;
}


uint32_t
get_cpu_id_pfr1(void)
{
	uint32_t result;
	asm volatile("MRC p15, 0, %0, c0, c1, 1": "=r" (result));
	return result;
}


bool
is_counter_available(void)
{
	return (get_cpu_id_pfr1() & 0x000F0000) == 0x00010000;
}


uint32_t
get_counter_freq(void)
{
	uint32_t freq;
	asm volatile ("MRC p15, 0, %0, c14, c0, 0": "=r" (freq));
	return freq;
}


uint64_t
get_counter(void)
{
	uint32_t counter_low;
	uint32_t counter_high;

	asm volatile ("ISB\n"
		"MRRC p15, 0, %0, %1, c14"
		: "=r" (counter_low), "=r" (counter_high));
	return ((uint64_t)counter_high << 32) | counter_low;
}


/*! Detect ARM core version and features.
    Please note the fact that ARM7 and ARMv7 are two different things ;)
    ARMx is a specific ARM CPU core instance, while ARMvX refers to the
    ARM architecture specification version....
 
    Most of the architecture versions we're detecting here we will probably
    never run on, just included for completeness sake... ARMv5 and up are
    the likely ones for us to support (as they all have some kind of MMU).
*/
status_t
check_cpu_features()
{
	uint32 result = get_cpu_features();
	int arch = 0;
	int variant = 0;
	int part = 0;
	int revision = 0;
	int implementor = 0;

	implementor = (result >> 24) & 0xff;

	switch ((result >> 12) & 0xf) {
		case 0:	/* early ARMv3 or even older */
			arch = ARCH_ARM_PRE_ARM7;
			break;

		case 7:	/* ARM7 processor */
			arch = (result & (1 << 23)) ? ARCH_ARM_v4T : ARCH_ARM_v3;
			variant = (result >> 16) & 0x7f;
			part = (result >> 4) & 0xfff;
			revision = result & 0xf;
			break;

		default:
			revision = result & 0xf;
			part = (result >> 4) & 0xfff;
			switch((result >> 16) & 0xf) {
				case 1: arch = ARCH_ARM_v4; break;
				case 2: arch = ARCH_ARM_v4T; break;
				case 3: arch = ARCH_ARM_v5; break;
				case 4: arch = ARCH_ARM_v5T; break;
				case 5: arch = ARCH_ARM_v5TE; break;
				case 6: arch = ARCH_ARM_v5TEJ; break;
				case 7: arch = ARCH_ARM_v6; break;
				case 0xf:
					arch = ARCH_ARM_v7;
					// TODO ... or later. We apparently need to scan the
					// CPUID registers to decide.
					break;
			}
			variant = (result >> 20) & 0xf;
			break;
	}

	TRACE("%s: implementor=0x%x('%c'), arch=%d, variant=0x%x, part=0x%x, revision=0x%x\n",
		__func__, implementor, implementor, arch, variant, part, revision);

	return (arch < ARCH_ARM_v5) ? B_ERROR : B_OK;
}


status_t
boot_arch_cpu_init(void)
{
	status_t err = check_cpu_features();
	if (err != B_OK) {
		panic("Retire your old Acorn and get something modern to boot!\n");
		return err;
	}

	return B_OK;
}


void
arch_ucode_load(BootVolume& volume)
{
	// NOP on arm currently
}
