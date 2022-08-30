/*
 * Copyright 2022 Haiku, Inc.  All rights reserved.
 * Copyright 2005-2015, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2016, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 * Distributed under the terms of the MIT License.
 */


#include <stdlib.h>
#include <string.h>

#include <compute_display_timing.h>
#include <create_display_modes.h>

#include "accelerant_protos.h"
#include "accelerant.h"

#include <DriverInterface.h>

#define TRACE_MODE
#ifdef TRACE_MODE
extern "C" void _sPrintf(const char* format, ...);
#	define TRACE(x...) _sPrintf(x)
#else
#	define TRACE(x...) ;
#endif


/*****************************************************************************/
#define T_POSITIVE_SYNC (B_POSITIVE_HSYNC | B_POSITIVE_VSYNC)
#define MODE_FLAGS      (B_SCROLL | B_8_BIT_DAC | B_PARALLEL_ACCESS)
#define MODE_COUNT (sizeof (sModesList) / sizeof (display_mode))
/*****************************************************************************/
static const display_mode sModesList[] = {
{ { 78750, 1024, 1040, 1136, 1312, 768, 769, 772, 800, T_POSITIVE_SYNC}, B_RGB32, 1024, 768, 0, 0, MODE_FLAGS}, /* Vesa_Monitor_@75Hz_(1024X768X8.Z1) */
};


//	#pragma mark -


uint32
ramfb_accelerant_mode_count(void)
{
	TRACE("ramfb_accelerant_mode_count() = %d\n", MODE_COUNT);
	return MODE_COUNT;
}


status_t
ramfb_get_mode_list(display_mode* modeList)
{
	TRACE("ramfb_get_mode_info()\n");
	memcpy(modeList, sModesList, sizeof(sModesList));
	return B_OK;
}


status_t
ramfb_set_display_mode(display_mode* _mode)
{
	TRACE("ramfb_set_display_mode(%dx%d)\n",
		_mode->virtual_width, _mode->virtual_height);

	void* frameBufferVirt = NULL;
	size_t size = _mode->virtual_width * _mode->virtual_height * 4;
	area_id area = create_area("ramfb", &frameBufferVirt, B_ANY_ADDRESS,
		size, B_CONTIGUOUS, B_READ_AREA | B_WRITE_AREA);
	if (area < B_OK)
		return area;

	struct RamfbSetDisplayMode ramfbSetDisplayMode;
	ramfbSetDisplayMode.addrVirt = (uint64_t)frameBufferVirt;
	ramfbSetDisplayMode.addrPhys = 0;
	ramfbSetDisplayMode.width = _mode->virtual_width;
	ramfbSetDisplayMode.height = _mode->virtual_height;
	ramfbSetDisplayMode.stride = _mode->virtual_width * 4;
	ioctl(gInfo->device, RAMFB_SET_DISPLAY_MODE, &ramfbSetDisplayMode, sizeof(ramfbSetDisplayMode));

	uint32* p = (uint32*)frameBufferVirt;
	for (int i=0; i<_mode->virtual_width * _mode->virtual_height; i++)
		p[i] = 0xff000000;

	if (gInfo->frameBufferArea != 0) {
		delete_area(gInfo->frameBufferArea);
		gInfo->frameBufferArea = 0;
	}

	gInfo->frameBufferArea = area;
	gInfo->frameBufferVirt = frameBufferVirt;
	gInfo->frameBufferPhys = 0;

	gInfo->currentMode = *_mode;

	return B_OK;
}


status_t
ramfb_get_display_mode(display_mode* _currentMode)
{
	TRACE("ramfb_get_display_mode return %dx%d\n",
		gInfo->currentMode.virtual_width, gInfo->currentMode.virtual_height);
	*_currentMode = gInfo->currentMode;
	return B_OK;
}


status_t
ramfb_get_frame_buffer_config(frame_buffer_config* config)
{
	TRACE("ramfb_get_frame_buffer_config()\n");

	config->frame_buffer = (void*)gInfo->frameBufferVirt;
	config->frame_buffer_dma = (void*)gInfo->frameBufferPhys;
	config->bytes_per_row = 4*gInfo->currentMode.virtual_width;

	return B_OK;
}


status_t
ramfb_get_pixel_clock_limits(display_mode* mode, uint32* _low, uint32* _high)
{
	TRACE("ramfb_get_pixel_clock_limits()\n");

	// TODO: do some real stuff here (taken from radeon driver)
	uint32 totalPixel = (uint32)mode->timing.h_total
		* (uint32)mode->timing.v_total;
	uint32 clockLimit = 2000000;

	// lower limit of about 48Hz vertical refresh
	*_low = totalPixel * 48L / 1000L;
	if (*_low > clockLimit)
		return B_ERROR;

	*_high = clockLimit;
	return B_OK;
}

