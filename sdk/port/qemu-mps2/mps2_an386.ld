/* QEMU mps2-an386 memory map. Budget mirrors a small watch MCU: the SDK
 * reports usage against 512 KB code / 128 KB RAM even though QEMU has more. */
ENTRY(Reset_Handler)

MEMORY
{
  CODE (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
  RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > CODE
  .text : {
    *(.text*)
    KEEP(*(.init)) KEEP(*(.fini))
    *(.rodata*)
    . = ALIGN(4);
  } > CODE
  .ARM.exidx : { *(.ARM.exidx*) } > CODE
  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)); PROVIDE_HIDDEN(__preinit_array_end = .); } > CODE
  .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)); PROVIDE_HIDDEN(__init_array_end = .); } > CODE
  .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)); PROVIDE_HIDDEN(__fini_array_end = .); } > CODE

  .data : { . = ALIGN(4); *(.data*) . = ALIGN(4); } > RAM
  .bss (NOLOAD) : {
    . = ALIGN(4);
    __bss_start__ = .;
    *(.bss*) *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } > RAM
  . = ALIGN(8);
  end = .;
  __end__ = .;
  _end = .;
  /* fail the link if static RAM plus a 16 KB stack don't fit */
  ASSERT(. + 16K <= ORIGIN(RAM) + LENGTH(RAM), "vwatch: RAM budget exceeded")
}
