/*
 * Copyright 2003-2010, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2008, François Revol, revol@free.fr. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <KernelExport.h>
#include <boot/platform.h>
#include <boot/heap.h>
#include <boot/stage2.h>
#include <arch/cpu.h>
#include <kernel.h>

#include "arch_mmu.h"
#include "arch_start.h"
#include "cpu.h"
#include "dtb.h"
#include "fw_cfg.h"
#include "mmu.h"
#include "serial.h"
#include "virtio.h"

#define HEAP_SIZE (1024*1024)

// GCC defined globals
extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);
extern uint8 __bss_start;
extern uint8 __bss_end;

extern "C" int main(stage2_args *args);
extern "C" void arch_enter_kernel(void);

static uint32 sBootOptions;
static bool sBootOptionsValid = false;


static void
clear_bss(void)
{
	memset(&__bss_start, 0, &__bss_end - &__bss_start);
}


static void
call_ctors(void)
{
	void (**f)(void);

	for (f = &__ctor_list; f < &__ctor_end; f++)
		(**f)();
}


static uint32
check_for_boot_keys()
{
	uint32 options = 0;
	bigtime_t t0 = system_time();
	while (system_time() - t0 < 100000) {
		int key = virtio_input_get_key();
		switch(key) {
			case 57 /* space */:
				options |= BOOT_OPTION_MENU;
				break;
		}
	}
	return options;
}


extern "C" uint32
platform_boot_options(void)
{
	if (!sBootOptionsValid) {
		sBootOptions = check_for_boot_keys();
		sBootOptionsValid = true;
/*
		if (sBootOptions & BOOT_OPTION_DEBUG_OUTPUT)
			serial_enable();
*/
	}

	return sBootOptions;
}


template<class T> static void
convert_preloaded_image(preloaded_image* _image)
{
	T* image = static_cast<T*>(_image);
	fix_address(image->next);
	fix_address(image->name);
	fix_address(image->debug_string_table);
	fix_address(image->syms);
	fix_address(image->rel);
	fix_address(image->rela);
	fix_address(image->pltrel);
	fix_address(image->debug_symbols);
}


/*!	Convert all addresses in kernel_args to virtual addresses. */
static void
convert_kernel_args()
{
	fix_address(gKernelArgs.boot_volume);
	fix_address(gKernelArgs.vesa_modes);
	fix_address(gKernelArgs.edid_info);
	fix_address(gKernelArgs.debug_output);
	fix_address(gKernelArgs.boot_splash);

	arch_convert_kernel_args();

	if (gKernelArgs.kernel_image->elf_class == ELFCLASS64) {
		convert_preloaded_image<preloaded_elf64_image>(gKernelArgs.kernel_image);
	} else {
		convert_preloaded_image<preloaded_elf32_image>(gKernelArgs.kernel_image);
	}
	fix_address(gKernelArgs.kernel_image);

	// Iterate over the preloaded images. Must save the next address before
	// converting, as the next pointer will be converted.
	preloaded_image* image = gKernelArgs.preloaded_images;
	fix_address(gKernelArgs.preloaded_images);
	while (image != NULL) {
		preloaded_image* next = image->next;
		if (image->elf_class == ELFCLASS64) {
			convert_preloaded_image<preloaded_elf64_image>(image);
		} else {
			convert_preloaded_image<preloaded_elf32_image>(image);
		}
		image = next;
	}

	// Fix driver settings files.
	driver_settings_file* file = gKernelArgs.driver_settings;
	fix_address(gKernelArgs.driver_settings);
	while (file != NULL) {
		driver_settings_file* next = file->next;
		fix_address(file->next);
		fix_address(file->buffer);
		file = next;
	}
}


static addr_t
get_kernel_entry(void)
{
	if (gKernelArgs.kernel_image->elf_class == ELFCLASS64) {
		preloaded_elf64_image *image = static_cast<preloaded_elf64_image *>(
			gKernelArgs.kernel_image.Pointer());
		return image->elf_header.e_entry;
	} else if (gKernelArgs.kernel_image->elf_class == ELFCLASS32) {
		preloaded_elf32_image *image = static_cast<preloaded_elf32_image *>(
			gKernelArgs.kernel_image.Pointer());
		return image->elf_header.e_entry;
	}
	panic("Unknown kernel format! Not 32-bit or 64-bit!");
	return 0;
}


static void
get_kernel_regions(addr_range& text, addr_range& data)
{
	if (gKernelArgs.kernel_image->elf_class == ELFCLASS64) {
		preloaded_elf64_image *image = static_cast<preloaded_elf64_image *>(
			gKernelArgs.kernel_image.Pointer());
		text.start = image->text_region.start;
		text.size = image->text_region.size;
		data.start = image->data_region.start;
		data.size = image->data_region.size;
		return;
	} else if (gKernelArgs.kernel_image->elf_class == ELFCLASS32) {
		preloaded_elf32_image *image = static_cast<preloaded_elf32_image *>(
			gKernelArgs.kernel_image.Pointer());
		text.start = image->text_region.start;
		text.size = image->text_region.size;
		data.start = image->data_region.start;
		data.size = image->data_region.size;
		return;
	}
	panic("Unknown kernel format! Not 32-bit or 64-bit!");
}


static void *
allocate_trampoline_page(void)
{
	void *trampolinePage = NULL;
	if (platform_allocate_lomem(&trampolinePage, B_PAGE_SIZE) == B_OK)
		return trampolinePage;

	return NULL;
}


extern "C" void
platform_start_kernel(void)
{
	addr_t kernelEntry = get_kernel_entry();

	addr_range textRegion = {.start = 0, .size = 0}, dataRegion = {.start = 0, .size = 0};
	get_kernel_regions(textRegion, dataRegion);
	dprintf("kernel:\n");
	dprintf("  text: %#" B_PRIx64 ", %#" B_PRIx64 "\n", textRegion.start, textRegion.size);
	dprintf("  data: %#" B_PRIx64 ", %#" B_PRIx64 "\n", dataRegion.start, dataRegion.size);
	dprintf("  entry: %#lx\n", kernelEntry);

	// Allocate virtual memory for kernel args
	struct kernel_args *kernelArgs = NULL;
	if (platform_allocate_region((void **)&kernelArgs,
			sizeof(struct kernel_args), 0, false) != B_OK)
		panic("Failed to allocate kernel args.");

	addr_t virtKernelArgs;
	platform_bootloader_address_to_kernel_address((void*)kernelArgs,
		&virtKernelArgs);

	// Allocate identity mapped region for entry.S trampoline
	void *trampolinePage = allocate_trampoline_page();
	if (trampolinePage == NULL)
		panic("Failed to allocate trampoline page.");

	memcpy(trampolinePage, (void *)arch_enter_kernel, B_PAGE_SIZE);

	// map in a kernel stack
	void *stack_address = NULL;
	if (platform_allocate_region(&stack_address,
		KERNEL_STACK_SIZE + KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE, 0, false)
		!= B_OK) {
		panic("Unabled to allocate a stack");
	}
	gKernelArgs.cpu_kstack[0].start = fix_address((addr_t)stack_address);
	gKernelArgs.cpu_kstack[0].size = KERNEL_STACK_SIZE
		+ KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE;
	dprintf("Kernel stack at %#" B_PRIx64 "\n", gKernelArgs.cpu_kstack[0].start);

	// Avoid interrupts from virtio devices before kernel driver takes control.
	virtio_fini();

	dtb_set_kernel_args();
	convert_kernel_args();

	phys_addr_t pageDirectory;
	mmu_init_for_kernel(pageDirectory);

	// Copy final kernel args
	// This should be the last step before jumping to the kernel
	// as there are some fixups happening to kernel_args even in the last minute
	memcpy(kernelArgs, &gKernelArgs, sizeof(struct kernel_args));

	// Begin architecture-centric kernel entry.
	arch_start_kernel(trampolinePage, pageDirectory, kernelEntry, virtKernelArgs);

	panic("Shouldn't get here!");
}


extern "C" void
platform_exit(void)
{
}


extern "C" void
start(void *fdt)
{
	clear_bss();
	call_ctors();

	stage2_args args;
	args.heap_size = HEAP_SIZE;
	args.arguments = NULL;

	//serial_init_early();

	dtb_init(fdt);

	serial_init();
	cpu_init();
	mmu_init();

	main(&args);
}
