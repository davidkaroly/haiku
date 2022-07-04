/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include "cpu.h"

#include <SupportDefs.h>
#include <boot/arch/arm/arch_cpu.h>


bigtime_t
system_time()
{
	#warning Implement system_time in ARM bootloader!
	static bigtime_t sSystemTimeCounter = 0;
	return sSystemTimeCounter++;
}


void
spin(bigtime_t microseconds)
{
	bigtime_t time = system_time();
	while ((system_time() - time) < microseconds)
		asm volatile ("nop;");
}
