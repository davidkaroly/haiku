/*
 * Copyright 2022 Haiku, Inc.  All rights reserved.
 * Distributed under the terms of the MIT license.
 */

#include <stdio.h>
#include <string.h>

#include <device_manager.h>
#include <debug.h>
#include <graphic_driver.h>

#include <AutoDeleterDrivers.h>
#include <drivers/ACPI.h>
#include <drivers/bus/FDT.h>
#include <vm/vm.h>

#include <acpi.h>
#include <DriverInterface.h>

//#define TRACE_QFW
#ifdef TRACE_QFW
#define TRACE(x...) dprintf(x)
#else
#define TRACE(x...) ;
#endif

#define QFW_DRIVER_MODULE_NAME "bus_managers/qfw/driver_v1"

#define RAMFB_DEVICE_MODULE_NAME "bus_managers/qfw/ramfb/device_v1"
#define RAMFB_DEVICE_NAME "graphics/ramfb"
#define RAMFB_ACCELERANT_NAME "ramfb.accelerant"

static device_manager_info* sDeviceManager;


static void qfw_select(uint16 selector);
static void qfw_dma_write(void* bytes, size_t count);


//#pragma mark Ramfb Device


uint16 ramfbSelector = 0xffff;


struct __attribute__((packed)) RamFbCfg {
	uint64_t addr;
	uint32_t fourcc;
	uint32_t flags;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
};


enum {
	ramFbFormatXrgb8888 = ((uint32_t)('X') | ((uint32_t)('R') << 8)
		| ((uint32_t)('2') << 16) | ((uint32_t)('4') << 24)),
};


static status_t
ramfb_init_device(void* _info, void** _cookie)
{
	TRACE("ramfb_init_device(%p)\n", _info);

	*_cookie = _info;

	return B_OK;
}


static void
ramfb_uninit_device(void* _cookie)
{
	TRACE("ramfb_uninit_device(%p)\n", _cookie);
}


static status_t
ramfb_open(void* _info, const char* path, int openMode, void** _cookie)
{
	TRACE("ramfb_open(%p)\n", _info);

	*_cookie = (void*)0xdeadbeef;

	return B_OK;
}


static status_t
ramfb_close(void* cookie)
{
	TRACE("ramfb_close(%p)\n", cookie);
	return B_OK;
}


static status_t
ramfb_free(void* cookie)
{
	TRACE("ramfb_free(%p)\n", cookie);
	return B_OK;
}


static status_t
ramfb_ioctl(void* cookie, uint32 op, void* buffer, size_t length)
{
	TRACE("ramfb_ioctl(%p,%" B_PRIu32 ",%p,%" B_PRIuSIZE ")\n",
		cookie, op, buffer, length);

	switch (op) {
		case B_GET_ACCELERANT_SIGNATURE: {
			TRACE("ramfb_ioctl: get_accelerant_signature\n");
			status_t status = user_strlcpy((char*)buffer, RAMFB_ACCELERANT_NAME, length);
			if (status < B_OK)
				return status;

			return B_OK;
			}

		case RAMFB_GET_DEVICE_NAME:
			TRACE("ramfb_ioctl: get_device_name\n");
			if (user_strlcpy((char*)buffer, RAMFB_DEVICE_NAME, B_PATH_NAME_LENGTH) < B_OK)
				return B_BAD_ADDRESS;

			return B_OK;

		case RAMFB_SET_DISPLAY_MODE: {
			TRACE("ramfb_ioctl: set_display_mode\n");
			if (length != sizeof(RamfbSetDisplayMode)) {
				TRACE("invalid request size\n");
				return B_BAD_DATA;
			}

			struct RamfbSetDisplayMode ramfbSetDisplayMode;
			status_t status = user_memcpy(&ramfbSetDisplayMode, buffer, sizeof(ramfbSetDisplayMode));
			if (status < B_OK)
				return status;

			TRACE("set mode %dx%d\n", ramfbSetDisplayMode.width, ramfbSetDisplayMode.height);

			addr_t frameBufferAddressVirt = ramfbSetDisplayMode.addrVirt;
			phys_addr_t frameBufferAddressPhys;

			vm_get_page_mapping(team_get_current_team_id(), frameBufferAddressVirt, &frameBufferAddressPhys);

			TRACE("framebuffer virt=0x%" B_PRIxADDR ", phys=0x%" B_PRIxPHYSADDR "\n",
				frameBufferAddressVirt, frameBufferAddressPhys);

			RamFbCfg ramfbCfg;
			ramfbCfg.addr = B_HOST_TO_BENDIAN_INT64(frameBufferAddressPhys);
			ramfbCfg.fourcc = B_HOST_TO_BENDIAN_INT32(ramFbFormatXrgb8888);
			ramfbCfg.flags = 0;
			ramfbCfg.width = B_HOST_TO_BENDIAN_INT32(ramfbSetDisplayMode.width);
			ramfbCfg.height = B_HOST_TO_BENDIAN_INT32(ramfbSetDisplayMode.height);
			ramfbCfg.stride = B_HOST_TO_BENDIAN_INT32(ramfbSetDisplayMode.stride);

			qfw_select(ramfbSelector);
			qfw_dma_write(&ramfbCfg, sizeof(ramfbCfg));

			return B_OK;
			}
	}

	return B_DEV_INVALID_IOCTL;
}


//#pragma mark QFW Device


addr_t sFwCfgBase = 0;
addr_t sFwCfgCommVirt = 0;
phys_addr_t sFwCfgCommPhys = 0;


enum {
	fwCfgSelectSignature = 0x0000,
	fwCfgSelectId        = 0x0001,
	fwCfgSelectFileDir   = 0x0019,
	fwCfgSelectFileFirst = 0x0020,
};


enum {
	fwCfgSignature = 0x554D4551,

	fwCfgIdTraditional   = 0,
	fwCfgIdDma           = 1,
};


struct __attribute__((packed)) FwCfgDmaAccess {
	uint32_t control;
	uint32_t length;
	uint64_t address;
};


enum {
	fwCfgDmaFlagsError  = 0,
	fwCfgDmaFlagsRead   = 1,
	fwCfgDmaFlagsSkip   = 2,
	fwCfgDmaFlagsSelect = 3,
	fwCfgDmaFlagsWrite  = 4,
};


struct __attribute__((packed)) FwCfgFile {
	uint32_t size;
	uint16_t select;
	uint16_t reserved;
	char name[56]; // '/0' terminated
};


static void
qfw_select(uint16 selector)
{
	*(volatile uint16 *)(sFwCfgBase + 0x08) = B_BENDIAN_TO_HOST_INT16(selector);
}


static uint32
qfw_read32(void)
{
	return *(volatile uint32 *)sFwCfgBase;
}


static void
qfw_dma(void* bytes, size_t count, uint32_t op)
{
	FwCfgDmaAccess* dma = (FwCfgDmaAccess*)sFwCfgCommVirt;

	addr_t dataVirt = sFwCfgCommVirt + sizeof(FwCfgDmaAccess);
	phys_addr_t dataPhys = sFwCfgCommPhys + sizeof(FwCfgDmaAccess);

	if (op == fwCfgDmaFlagsWrite)
		memcpy((void*)dataVirt, bytes, count);

	dma->control = B_HOST_TO_BENDIAN_INT32(1 << op);
	dma->length = B_HOST_TO_BENDIAN_INT32(count);
	dma->address = B_HOST_TO_BENDIAN_INT64(dataPhys);

	*(volatile uint64 *)(sFwCfgBase + 0x10) = B_HOST_TO_BENDIAN_INT64(sFwCfgCommPhys);

	while (uint32_t control = B_BENDIAN_TO_HOST_INT32(dma->control) != 0) {
		if (((1 << fwCfgDmaFlagsError) & control) != 0)
			panic("qfw DMA transfer failed\n");
	}

	if (op == fwCfgDmaFlagsRead)
		memcpy(bytes, (void*)dataVirt, count);
}


static uint32
qfw_read32_dma(void)
{
	uint32 val;
	qfw_dma(&val, sizeof(val), fwCfgDmaFlagsRead);
	return val;
}


static void
qfw_dma_write(void* bytes, size_t count)
{
	qfw_dma(bytes, count, fwCfgDmaFlagsWrite);
}


static uint16
qfw_find_file(const char* name)
{
	qfw_select(fwCfgSelectFileDir);
	uint32_t count = B_BENDIAN_TO_HOST_INT32(qfw_read32_dma());
	TRACE("file count = %" PRIu32 "\n", count);

	for (uint32_t i = 0; i < count; i++) {
		FwCfgFile file;
		qfw_dma(&file, sizeof(file), fwCfgDmaFlagsRead);
		if (strcmp(file.name, name) == 0)
			return B_BENDIAN_TO_HOST_INT16(file.select);
	}

	return 0xffff;
}


//#pragma mark Driver


static float
qfw_supports_device(device_node* parent)
{
	TRACE("qfw_supports_device(%p)\n", parent);

	const char* bus;

	status_t status;
	status = sDeviceManager->get_attr_string(parent, B_DEVICE_BUS, &bus, false);

	if (status < B_OK)
		return -1.0f;

	if (strcmp(bus, "fdt") == 0) {
		const char* compatible;
		status = sDeviceManager->get_attr_string(parent, "fdt/compatible",
			&compatible, false);

		if (status < B_OK)
			return -1.0f;

		if (strcmp(compatible, "qemu,fw-cfg-mmio") == 0) {
			TRACE("found fw-cfg device from FDT\n");
			return 1.0;
		}
	}

	if (strcmp(bus, "acpi") == 0) {
		 const char* hid;
		status = sDeviceManager->get_attr_string(parent, "acpi/hid",
			&hid, false);

		if (status < B_OK)
			return -1.0f;

		if (strcmp(hid, "QEMU0002") == 0) {
			TRACE("found fw-cfg device from ACPI\n");
			return 1.0f;
		}
	}

	return 0.0f;
}


static status_t
qfw_register_device(device_node* parent)
{
	TRACE("qfw_register_device(%p)\n", parent);

	device_attr attrs[] = {
		{ B_DEVICE_PRETTY_NAME, B_STRING_TYPE, { string: "QEMU Firmware Configuration Device" } },
		{ NULL }
	};

	return sDeviceManager->register_node(parent, QFW_DRIVER_MODULE_NAME,
		attrs, NULL, NULL);
}


struct qfw_memory_range {
	uint64 base;
	uint64 length;
};


static acpi_status
qfw_crs_find_address(acpi_resource *res, void *context)
{
	qfw_memory_range &range = *((qfw_memory_range *)context);

	if (res->type == ACPI_RESOURCE_TYPE_FIXED_MEMORY32) {
		range.base = res->data.fixed_memory32.address;
		range.length = res->data.fixed_memory32.address_length;
	}

	return B_OK;
}


static status_t
qfw_init_driver(device_node *node, void **_cookie)
{
	TRACE("qfw_init_driver(%p)\n", node);

	*_cookie = node;

	DeviceNodePutter<&sDeviceManager>
		parent(sDeviceManager->get_parent_node(node));

	if (parent.Get() == NULL)
		return B_ERROR;

	const char *bus;
	if (sDeviceManager->get_attr_string(parent.Get(), B_DEVICE_BUS, &bus, false) < B_OK)
		return B_ERROR;

	uint64 regs;
	uint64 regsLen;

	if (strcmp(bus, "fdt") == 0) {
		TRACE("initialize fw_cfg from FDT\n");

		status_t res;
		fdt_device_module_info* parentModule;
		fdt_device* parentDev;

		res = sDeviceManager->get_driver(parent.Get(),
			(driver_module_info**)&parentModule, (void**)&parentDev);
		if (res != B_OK) {
			TRACE("can't get parent node driver\n");
			return B_ERROR;
		}

		parentModule->get_reg(parentDev, 0, &regs, &regsLen);
	} else if (strcmp(bus, "acpi") == 0) {
		TRACE("initialize fw-cfg from ACPI\n");

		acpi_device_module_info *parentModule;
		acpi_device parentDev;
		if (sDeviceManager->get_driver(parent.Get(), (driver_module_info**)&parentModule,
				(void**)&parentDev)) {
			TRACE("can't get parent node driver");
			return B_ERROR;
		}

		qfw_memory_range range = { 0, 0 };
		parentModule->walk_resources(parentDev, (char *)"_CRS",
			qfw_crs_find_address, &range);
		regs = range.base;
		regsLen = range.length;
	} else {
		return B_ERROR;
	}

	TRACE("fw_cfg regs(0x%" B_PRIx64 ", 0x%" B_PRIx64 ")\n",
		regs, regsLen);

	area_id area = map_physical_memory("qemu fw_cfg regs",
		regs, regsLen, B_ANY_KERNEL_ADDRESS,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA, (void **)&sFwCfgBase);

	if (area < 0)
		return B_ERROR;

	virtual_address_restrictions virtualRestrictions = {};
	virtualRestrictions.address_specification = B_ANY_KERNEL_ADDRESS;
	physical_address_restrictions physicalRestrictions = {};

	area_id comm_area = vm_create_anonymous_area(B_SYSTEM_TEAM, "qemu fw_cfg comms",
		B_PAGE_SIZE, B_CONTIGUOUS, B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA, 0, 0,
		&virtualRestrictions, &physicalRestrictions, true, (void **)&sFwCfgCommVirt);

	if (comm_area < 0)
		return B_ERROR;

	vm_get_page_mapping(VMAddressSpace::KernelID(), sFwCfgCommVirt, &sFwCfgCommPhys);

	TRACE("fw_cfg comms virt=%" B_PRIxADDR ", phys=%" B_PRIxPHYSADDR "\n",
		sFwCfgCommVirt, sFwCfgCommPhys);

	qfw_select(fwCfgSelectSignature);
	uint32 signature = qfw_read32();
	TRACE("fw_cfg signature: 0x%08" B_PRIx32 "\n", signature);
	if (signature != fwCfgSignature) {
		TRACE("invalid signature!\n");
		return B_ERROR;
	}

	ramfbSelector = qfw_find_file("etc/ramfb");
	TRACE("ramfbSelector = 0x%04" B_PRIx16 "\n", ramfbSelector);

	return B_OK;
}


static status_t
qfw_register_child_devices(void *cookie)
{
	TRACE("qfw_register_child_devices(%p)\n", cookie);

	device_node *node = (device_node*)cookie;

	status_t status;

	if (ramfbSelector != 0xffff) {
		status = sDeviceManager->publish_device(node, RAMFB_DEVICE_NAME, RAMFB_DEVICE_MODULE_NAME);
		if (status < B_OK)
			return status;
	}

	return B_OK;
}


//#pragma mark -


module_dependency module_dependencies[] = {
	{ B_DEVICE_MANAGER_MODULE_NAME, (module_info**)&sDeviceManager },
	{ NULL }
};


static device_module_info sRamfbDevice = {
	{
		RAMFB_DEVICE_MODULE_NAME,
		0,		// flags
		NULL	// std_ops
	},
	ramfb_init_device,
	ramfb_uninit_device,
	NULL,	// remove
	ramfb_open,
	ramfb_close,
	ramfb_free,
	NULL,	// read
	NULL,	// write
	NULL,	// io
	ramfb_ioctl,
	NULL,	// select
	NULL,	// deselect
};


static driver_module_info sQfwDriver = {
	{
		QFW_DRIVER_MODULE_NAME,
		0,		// flags
		NULL	// std_ops
	},
	qfw_supports_device,
	qfw_register_device,
	qfw_init_driver,
	NULL,	// uninit_driver
	qfw_register_child_devices,
	NULL,	// rescan_child_devices
	NULL,	// device_removed
	NULL,	// suspend
	NULL,	// resume
};


module_info* modules[] = {
	(module_info* )&sQfwDriver,
	(module_info* )&sRamfbDevice,
	NULL
};
