/*
 * Copyright 2022 Haiku, Inc.  All rights reserved.
 * Copyright 2005-2008, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2016, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _ACCELERANT_PROTOS_H
#define _ACCELERANT_PROTOS_H


#include <Accelerant.h>
#include "video_overlay.h"


#ifdef __cplusplus
extern "C" {
#endif

// general
status_t ramfb_init_accelerant(int fd);
ssize_t ramfb_accelerant_clone_info_size(void);
void ramfb_get_accelerant_clone_info(void *data);
status_t ramfb_clone_accelerant(void *data);
void ramfb_uninit_accelerant(void);
status_t ramfb_get_accelerant_device_info(accelerant_device_info *adi);
sem_id ramfb_accelerant_retrace_semaphore(void);

// modes & constraints
uint32 ramfb_accelerant_mode_count(void);
status_t ramfb_get_mode_list(display_mode *dm);
status_t ramfb_set_display_mode(display_mode *modeToSet);
status_t ramfb_get_display_mode(display_mode *currentMode);
status_t ramfb_get_frame_buffer_config(frame_buffer_config *config);
status_t ramfb_get_pixel_clock_limits(display_mode *dm, uint32 *low,
	uint32 *high);

#ifdef __cplusplus
}
#endif

#endif	/* _ACCELERANT_PROTOS_H */
