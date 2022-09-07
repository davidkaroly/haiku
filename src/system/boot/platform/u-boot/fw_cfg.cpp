/*
 * Copyright 2021-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include "fw_cfg.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <boot/stage2.h>
#include <ByteOrder.h>
#include <KernelExport.h>

#include "mmu.h"
#include "ramfb.h"


//#define TRACE_FW_CFG
#ifdef TRACE_FW_CFG
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


static addr_t sFwCfgRegs = 0;

void
fw_cfg_select(uint16_t selector)
{
	*(volatile uint16 *)(sFwCfgRegs + 0x08) = B_BENDIAN_TO_HOST_INT16(selector);
}


void
fw_cfg_dma_op(uint8_t *bytes, size_t count, uint32_t op)
{
	__attribute__ ((aligned (8))) FwCfgDmaAccess volatile dma;
	dma.control = B_HOST_TO_BENDIAN_INT32(1 << op);
	dma.length = B_HOST_TO_BENDIAN_INT32(count);
	dma.address = B_HOST_TO_BENDIAN_INT64((addr_t)bytes);
	*(volatile uint64 *)(sFwCfgRegs + 0x10) = B_HOST_TO_BENDIAN_INT64((addr_t)&dma);
	while (uint32_t control = B_BENDIAN_TO_HOST_INT32(dma.control) != 0) {
		if (((1 << fwCfgDmaFlagsError) & control) != 0)
			abort();
	}
}


void
fw_cfg_read_bytes(uint8_t *bytes, size_t count)
{
	fw_cfg_dma_op(bytes, count, fwCfgDmaFlagsRead);
}


void
fw_cfg_write_bytes(uint8_t *bytes, size_t count)
{
	fw_cfg_dma_op(bytes, count, fwCfgDmaFlagsWrite);
}


uint8_t  fw_cfg_read8 () {uint8_t  val; fw_cfg_read_bytes(          &val, sizeof(val)); return val;}
uint16_t fw_cfg_read16() {uint16_t val; fw_cfg_read_bytes((uint8_t*)&val, sizeof(val)); return val;}
uint32_t fw_cfg_read32() {uint32_t val; fw_cfg_read_bytes((uint8_t*)&val, sizeof(val)); return val;}
uint64_t fw_cfg_read64() {uint64_t val; fw_cfg_read_bytes((uint8_t*)&val, sizeof(val)); return val;}


void
fw_cfg_list_dir()
{
	uint32_t count = B_BENDIAN_TO_HOST_INT32(fw_cfg_read32());
	TRACE("count: %" B_PRIu32 "\n", count);
	for (uint32_t i = 0; i < count; i++) {
		FwCfgFile file;
		fw_cfg_read_bytes((uint8_t*)&file, sizeof(file));
		file.size = B_BENDIAN_TO_HOST_INT32(file.size);
		file.select = B_BENDIAN_TO_HOST_INT16(file.select);
		file.reserved = B_BENDIAN_TO_HOST_INT16(file.reserved);
		TRACE("\n");
		TRACE("size: %" B_PRIu32 "\n", file.size);
		TRACE("select: %" B_PRIu32 "\n", file.select);
		TRACE("reserved: %" B_PRIu32 "\n", file.reserved);
		TRACE("name: %s\n", file.name);
	}
}


bool
fw_cfg_find_file(FwCfgFile& file, uint16_t dir, const char *name)
{
	fw_cfg_select(dir);
	uint32_t count = B_BENDIAN_TO_HOST_INT32(fw_cfg_read32());
	for (uint32_t i = 0; i < count; i++) {
		fw_cfg_read_bytes((uint8_t*)&file, sizeof(file));
		file.size = B_BENDIAN_TO_HOST_INT32(file.size);
		file.select = B_BENDIAN_TO_HOST_INT16(file.select);
		file.reserved = B_BENDIAN_TO_HOST_INT16(file.reserved);
		if (strcmp(file.name, name) == 0)
			return true;
	}
	return false;
}


extern "C" bool
mmu_get_virtual_mapping(addr_t virtualAddress, /*phys_*/addr_t *_physicalAddress);


void
fw_cfg_init_framebuffer()
{
	if (sFwCfgRegs == 0)
		return;

	TRACE("fw_cfg_init_framebuffer\n");

	FwCfgFile file;
	if (!fw_cfg_find_file(file, fwCfgSelectFileDir, "etc/ramfb")) {
		TRACE("[!] ramfb not found\n");
		return;
	}
	TRACE("file.select: %" B_PRIu16 "\n", file.select);

	RamFbCfg cfg;
	uint32_t width = 1024, height = 768;

	ramfb_init(width, height);

	addr_t ramfb_phys_addr;
	mmu_get_virtual_mapping(ramfb_get_framebuffer(), &ramfb_phys_addr);
	cfg.addr = B_HOST_TO_BENDIAN_INT64(ramfb_phys_addr);
	cfg.fourcc = B_HOST_TO_BENDIAN_INT32(ramFbFormatXrgb8888);
	cfg.flags = B_HOST_TO_BENDIAN_INT32(0);
	cfg.width = B_HOST_TO_BENDIAN_INT32(width);
	cfg.height = B_HOST_TO_BENDIAN_INT32(height);
	cfg.stride = B_HOST_TO_BENDIAN_INT32(4 * width);
	fw_cfg_select(file.select);
	fw_cfg_write_bytes((uint8_t*)&cfg, sizeof(cfg));

	gKernelArgs.frame_buffer.physical_buffer.start = ramfb_phys_addr;
	gKernelArgs.frame_buffer.physical_buffer.size = 4 * width * height;
	gKernelArgs.frame_buffer.width = width;
	gKernelArgs.frame_buffer.height = height;
	gKernelArgs.frame_buffer.depth = 32;
	gKernelArgs.frame_buffer.bytes_per_row = 4 * width;
	gKernelArgs.frame_buffer.enabled = width > 0 && height > 0;
}


void
fw_cfg_set_base(addr_t base)
{
	sFwCfgRegs = base;
	TRACE("sFwCfgRegs: 0x%08" B_PRIxADDR "\n", (addr_t)sFwCfgRegs);
}


void
fw_cfg_init()
{
	if (sFwCfgRegs == 0)
		return;
	sFwCfgRegs = mmu_map_physical_memory(sFwCfgRegs, B_PAGE_SIZE);

	fw_cfg_select(fwCfgSelectSignature);
	TRACE("fwCfgSelectSignature: 0x%08" B_PRIx32 "\n", fw_cfg_read32());
	fw_cfg_select(fwCfgSelectId);
	TRACE("fwCfgSelectId: : 0x%08" B_PRIx32 "\n", fw_cfg_read32());
	fw_cfg_select(fwCfgSelectFileDir);
	fw_cfg_list_dir();
	fw_cfg_init_framebuffer();
}


Framebuffer *fw_cfg_get_framebuffer()
{
	return new Framebuffer(ramfb_get_framebuffer());
}
