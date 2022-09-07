/*
 * Copyright 2021-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */


#ifndef _RAMFB_H
#define _RAMFB_H

#include <SupportDefs.h>


template <typename Color>
struct RasBuf {
	Color* colors;
	int32 stride, width, height;

	RasBuf<Color> Clip(int x, int y, int w, int h) const;
};

typedef RasBuf<uint8>  RasBuf8;
typedef RasBuf<uint32> RasBuf32;


void ramfb_init(uint32 width, uint32 height);
RasBuf32 ramfb_get(void);
addr_t ramfb_get_framebuffer(void);
size_t ramfb_get_size(void);
void ramfb_clear(RasBuf32 vb, uint32_t c);
void ramfb_blit_mask_rgb(RasBuf32 dst, RasBuf8 src, int32 x, int32 y, uint32_t c);


#endif	// _RAMFB_H
