/*
 * Copyright 2022, Haiku, Inc.  All rights reserved.
 * Distributed under the terms of the MIT license.
 */

#ifndef DRIVERINTERFACE_H
#define DRIVERINTERFACE_H


#include <Accelerant.h>
#include <GraphicsDefs.h>
#include <Drivers.h>


enum {
	RAMFB_GET_DEVICE_NAME = B_DEVICE_OP_CODES_END + 1,
	RAMFB_SET_DISPLAY_MODE,
};


struct RamfbSetDisplayMode {
	uint64_t addrVirt;
	uint64_t addrPhys;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
};


#endif /* DRIVERINTERFACE_H */
