/*
 * Copyright 2022 Haiku, Inc.  All rights reserved.
 * Copyright 2005-2008, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Copyright 2016, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 * Distributed under the terms of the MIT License.
 */


#include "accelerant_protos.h"
#include "accelerant.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#include <AutoDeleterOS.h>
#include <DriverInterface.h>


#define TRACE_ACCELERANT
#ifdef TRACE_ACCELERANT
extern "C" void _sPrintf(const char *format, ...);
#	define TRACE(x...) _sPrintf(x)
#else
#	define TRACE(x...) ;
#endif


struct accelerant_info *gInfo;


//	#pragma mark -


/*!	This is the common accelerant_info initializer. It is called by
	both, the first accelerant and all clones.
*/
static status_t
init_common(int device, bool isClone)
{
	TRACE("ramfb: init_common\n");

	// initialize global accelerant info structure

	gInfo = (accelerant_info *)malloc(sizeof(accelerant_info));
	MemoryDeleter infoDeleter(gInfo);
	if (gInfo == NULL)
		return B_NO_MEMORY;

	memset(gInfo, 0, sizeof(accelerant_info));

	gInfo->is_clone = isClone;
	gInfo->device = device;

	// get basic info from driver

	infoDeleter.Detach();
	return B_OK;
}


/*!	Cleans up everything done by a successful init_common(). */
static void
uninit_common(void)
{
	TRACE("ramfb: uninit_common\n");

	// close the file handle ONLY if we're the clone
	// (this is what Be tells us ;)
	if (gInfo->is_clone)
		close(gInfo->device);

	free(gInfo);
}


//	#pragma mark - public accelerant functions


/*!	Init primary accelerant */
status_t
ramfb_init_accelerant(int device)
{
	TRACE("ramfb_init_accelerant()\n");

	status_t status = init_common(device, false);
	if (status != B_OK)
		return status;

	return B_OK;
}


ssize_t
ramfb_accelerant_clone_info_size(void)
{
	// clone info is device name, so return its maximum size
	return B_PATH_NAME_LENGTH;
}


void
ramfb_get_accelerant_clone_info(void *info)
{
	ioctl(gInfo->device, RAMFB_GET_DEVICE_NAME, info, B_PATH_NAME_LENGTH);
}


status_t
ramfb_clone_accelerant(void *info)
{
	TRACE("ramfb_clone_accelerant()\n");

	// create full device name
	char path[MAXPATHLEN];
	strcpy(path, "/dev/");
	strcat(path, (const char *)info);

	int fd = open(path, B_READ_WRITE);
	if (fd < 0)
		return errno;

	status_t status = init_common(fd, true);
	if (status != B_OK)
		goto err1;

	return B_OK;

	uninit_common();
err1:
	close(fd);
	return status;
}


/*!	This function is called for both, the primary accelerant and all of
	its clones.
*/
void
ramfb_uninit_accelerant(void)
{
	TRACE("ramfb_uninit_accelerant()\n");

	uninit_common();
}


status_t
ramfb_get_accelerant_device_info(accelerant_device_info *info)
{
	TRACE("ramfb_get_accelerant_device_info\n");

	info->version = B_ACCELERANT_VERSION;
	strcpy(info->name, "Qemu Framebuffer Driver");
	strcpy(info->chipset, "QEMU");
	strcpy(info->serial_no, "n/a");
	info->memory = 8*1024*1024;
	info->dac_speed = 10;

	return B_OK;
}


sem_id
ramfb_accelerant_retrace_semaphore()
{
	return -1;
}

