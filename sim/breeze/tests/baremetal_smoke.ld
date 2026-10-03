OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY {
  RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 16M
}

PHDRS {
  text PT_LOAD FLAGS(5);  /* read + execute */
  data PT_LOAD FLAGS(6);  /* read + write */
}

SECTIONS {
  .text : { *(.text.init) *(.text .text.*) } > RAM :text
  .rodata : { *(.rodata .rodata.*) } > RAM :text
  . = ALIGN(0x1000);
  .data : { *(.data .data.*) } > RAM :data
  .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } > RAM :data
  .tohost : { KEEP(*(.tohost)) } > RAM :data
}
