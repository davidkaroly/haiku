/*
 * Copyright 2022 Haiku, Inc.  All rights reserved.
 * Copyright 2005-2012, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2016, Jessica Hamilton, jessica.l.hamilton@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "accelerant_protos.h"
//#include "accelerant.h"

#include <new>


extern "C" void*
get_accelerant_hook(uint32 feature, void* data)
{
	switch (feature) {
		/* general */
		case B_INIT_ACCELERANT:
			return (void*)ramfb_init_accelerant;
		case B_UNINIT_ACCELERANT:
			return (void*)ramfb_uninit_accelerant;
		case B_CLONE_ACCELERANT:
			return (void*)ramfb_clone_accelerant;
		case B_ACCELERANT_CLONE_INFO_SIZE:
			return (void*)ramfb_accelerant_clone_info_size;
		case B_GET_ACCELERANT_CLONE_INFO:
			return (void*)ramfb_get_accelerant_clone_info;
		case B_GET_ACCELERANT_DEVICE_INFO:
			return (void*)ramfb_get_accelerant_device_info;
		case B_ACCELERANT_RETRACE_SEMAPHORE:
			return (void*)ramfb_accelerant_retrace_semaphore;

		/* mode configuration */
		case B_ACCELERANT_MODE_COUNT:
			return (void*)ramfb_accelerant_mode_count;
		case B_GET_MODE_LIST:
			return (void*)ramfb_get_mode_list;
		case B_SET_DISPLAY_MODE:
			return (void*)ramfb_set_display_mode;
		case B_GET_DISPLAY_MODE:
			return (void*)ramfb_get_display_mode;
		case B_GET_FRAME_BUFFER_CONFIG:
			return (void*)ramfb_get_frame_buffer_config;
		case B_GET_PIXEL_CLOCK_LIMITS:
			return (void*)ramfb_get_pixel_clock_limits;
	}

	return NULL;
}

