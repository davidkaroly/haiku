/*
 * Copyright 2021-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */


#ifndef DTB_H
#define DTB_H

#include <SupportDefs.h>
#include <boot/addr_range.h>

extern void* gFdt;


void dtb_init(void* fdt);
void dtb_set_kernel_args();

void dtb_copy_fdt(void **dest, void *src);

bool dtb_get_reg(const void* fdt, int node, size_t idx, addr_range& range);
bool dtb_has_fdt_string(const char* prop, int size, const char* pattern);


#endif /* DTB_H */
