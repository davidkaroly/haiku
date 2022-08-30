/*
 * Copyright 2022 Haiku, Inc.  All rights reserved.
 * Copyright 2005-2008, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Copyright 2016, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 * Distributed under the terms of the MIT License.
 */
#ifndef RAMFB_ACCELERANT_H
#define RAMFB_ACCELERANT_H


typedef struct accelerant_info {
	int					device;
	bool				is_clone;

	area_id				frameBufferArea;
	void*				frameBufferVirt;
	phys_addr_t			frameBufferPhys;

	display_mode		currentMode;
} accelerant_info;

extern accelerant_info *gInfo;

#endif	/* RAMFB_ACCELERANT_H */
