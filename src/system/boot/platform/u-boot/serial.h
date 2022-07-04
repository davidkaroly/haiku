/*
 * Copyright 2004-2007, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef SERIAL_H
#define SERIAL_H

#include <arch/generic/debug_uart.h>
#include <SupportDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_init_early(void);
void serial_init(void);
void serial_cleanup(void);

void serial_puts(const char *string, size_t size);
int serial_getc(bool wait);

void serial_disable(void);
void serial_enable(void);

extern DebugUART *gUART;

#ifdef __cplusplus
}
#endif

#endif	/* SERIAL_H */
