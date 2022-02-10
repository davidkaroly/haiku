OUTPUT_FORMAT("elf64-littleriscv", "elf64-littleriscv", "elf64-littleriscv")
OUTPUT_ARCH(riscv)

STACK_SIZE = 16384;

ENTRY(_start)
SECTIONS
{
	. = 0x80000000;

  .text : {
    *(.text.head)
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)
  }

  . = ALIGN(0x8);
  __ctor_list = .;
  .init_array : { *(.init_array) }
  __ctor_end = .;
  __dtor_list = .;
  .fini_array : { *(.fini_array) }
  __dtor_end = .;

  .rodata : { *(.rodata .rodata.*) }

  /* writable data  */
  . = ALIGN(0x1000);
  __data_start = .;
  .data : {
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
  }
  .got : { *(.got.plt) *(.got) }
  .dynamic : { gDynamic = .; *(.dynamic) }

  /* uninitialized data (in same segment as writable data) */
  __bss_start = .;
  .bss : { *(.bss) }
  __bss_end = .;

  .stack : {
    __stack_start = .;
    /* *(.bss.stack) */
    . += STACK_SIZE;
    . = ALIGN(0x8);
    __stack_top = .;
  }

  . = ALIGN(0x1000);
  _end = . ;

  /* Strip unnecessary stuff */
  /DISCARD/ : { *(.comment .note .eh_frame .dtors .stab .stabstr .debug*) }
}
