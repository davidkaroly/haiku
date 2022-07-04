/*
 * Copyright 2019-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef DTB_H
#define DTB_H

#include <SupportDefs.h>
#include <boot/addr_range.h>

void dtb_init(void);
void dtb_set_kernel_args(void);

bool dtb_get_reg(const void* fdt, int node, size_t idx, addr_range& range);
bool dtb_has_fdt_string(const char* prop, int size, const char* pattern);

extern void *gFDT;

#endif /* DTB_H */
