/*
 * Copyright 2012-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2004-2005, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include "arch_cpu.h"
#include "cpu.h"

#include <OS.h>
#include <boot/platform.h>

//static uint32_t sCounterFreq = 1000000;

//#define TRACE_CPU
#ifdef TRACE_CPU
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif

bigtime_t
system_time()
{
	#warning Implement system_time in ARM bootloader!
	static bigtime_t sSystemTimeCounter = 0;
	return sSystemTimeCounter++;
	//return get_counter() * 1000000 / sCounterFreq;
}


#if 0
static void
spin_cycles(uint64_t cycles)
{
	uint64_t time = get_counter();
	while ((get_counter() - time) < cycles)
		asm volatile ("nop;");
}


void
spin(bigtime_t microseconds)
{
	uint64_t cycles = microseconds * sCounterFreq / 1000000;
	spin_cycles(cycles);
}
#endif


void
spin(bigtime_t microseconds)
{
	bigtime_t time = system_time();
	while ((system_time() - time) < microseconds)
		asm volatile ("nop;");
}


#if 0
status_t
boot_arch_cpu_init(void)
{
	status_t err = check_cpu_features();
	if (err != B_OK) {
		panic("Retire your old Acorn and get something modern to boot!\n");
		return err;
	}

	if (is_counter_available()) {
		sCounterFreq = get_counter_freq();
		TRACE("timer freq is %d\n", sCounterFreq);
	}

	return B_OK;
}
#endif
