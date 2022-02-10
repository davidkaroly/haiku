/*
 * Copyright 2012-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2004-2008, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Alexander von Gluck IV, kallisti5@unixzen.com
 */


#include "serial.h"

#include <boot/platform.h>
#include <arch/cpu.h>
#include <boot/stage2.h>
#include <new>
#include <string.h>

DebugUART *gUART;

static int32 sEarlySerialEnabled = 0;
static int32 sSerialEnabled = 0;
static char sBuffer[16384];
static uint32 sBufferPosition;


static void
serial_putc(char c)
{
	if (sEarlySerialEnabled) {
#if defined(__arm__) || defined(__aarch64__)
		*(volatile uint32_t *)0x09000000 = c;
#elif defined(__riscv)
		*(volatile uint32_t *)0x10000000 = c;
#endif
		return;
	}

	if (gUART == NULL || sSerialEnabled <= 0)
		return;

	gUART->PutChar(c);
}


extern "C" int
serial_getc(bool wait)
{
	if (gUART == NULL || sSerialEnabled <= 0)
		return 0;

	return gUART->GetChar(wait);
}


extern "C" void
serial_puts(const char* string, size_t size)
{
	if (sEarlySerialEnabled) {
		for (size_t i = 0; i < size; i++) {
			serial_putc(string[i]);
		}
		return;
	}

	if (sSerialEnabled <= 0)
		return;

	if (sBufferPosition + size < sizeof(sBuffer)) {
		memcpy(sBuffer + sBufferPosition, string, size);
		sBufferPosition += size;
	}

	while (size-- != 0) {
		char c = string[0];

		if (c == '\n') {
			serial_putc('\r');
			serial_putc('\n');
		} else if (c != '\r')
			serial_putc(c);

		string++;
	}
}


extern "C" void
serial_disable(void)
{
	sSerialEnabled = 0;
	sEarlySerialEnabled = 0;
}


extern "C" void
serial_enable(void)
{
	if (gUART != NULL) {
		gUART->InitEarly();
		gUART->InitPort(115200);
	}

	sSerialEnabled++;
	sEarlySerialEnabled = 0;
}


extern "C" void
serial_cleanup(void)
{
	if (sSerialEnabled <= 0)
		return;

	gKernelArgs.debug_output = kernel_args_malloc(sBufferPosition);
	if (gKernelArgs.debug_output != NULL) {
		memcpy(gKernelArgs.debug_output, sBuffer, sBufferPosition);
		gKernelArgs.debug_size = sBufferPosition;
	}
}


void
serial_init_early(void)
{
	sEarlySerialEnabled = 1;
}


extern "C" void
serial_init(void)
{
	serial_enable();
}
