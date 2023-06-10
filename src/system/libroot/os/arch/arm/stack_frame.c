/*
 * Copyright 2012, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		François Revol <revol@free.fr>
 */

#include <SupportDefs.h>

#include <libroot_private.h>


void*
get_stack_frame(void)
{
	uint32 res;
	asm volatile ("mov %0, fp": "=r" (res));
	return (void*)res;
}

