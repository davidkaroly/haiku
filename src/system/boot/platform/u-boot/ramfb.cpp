/*
 * Copyright 2021-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */

#include "ramfb.h"

#include <stdlib.h>


#include "debug.h"
#include "mmu.h"


static RasBuf32 sFramebuf = {NULL, 0, 0, 0};


void
ramfb_clear(RasBuf32 vb, uint32_t c)
{
	vb.stride -= vb.width;
	for (; vb.height > 0; vb.height--) {
		for (int x = 0; x < vb.width; x++) {
			*vb.colors = c;
			vb.colors++;
		}
		vb.colors += vb.stride;
	}
}


void
ramfb_init(uint32 width, uint32 height)
{
	sFramebuf.colors = (uint32_t*)malloc(4*width*height);
	sFramebuf.stride = width;
	sFramebuf.width = width;
	sFramebuf.height = height;

	ramfb_clear(sFramebuf, 0xff000000);
}


RasBuf32
ramfb_get(void)
{
	return sFramebuf;
}


addr_t
ramfb_get_framebuffer(void)
{
	return (addr_t)sFramebuf.colors;
}


size_t
ramfb_get_size(void)
{
	return sFramebuf.width * sFramebuf.height * 4;
}
