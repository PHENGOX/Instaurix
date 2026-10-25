ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); ro PT_LOAD FLAGS(4); data PT_LOAD FLAGS(6); }
SECTIONS {
    . = 0x10000;
    .text : { *(.text*) } :text
    . = ALIGN(0x1000);
    .rodata : { *(.rodata*) } :ro
    . = ALIGN(0x1000);
    .data : { *(.data*) *(.bss*) *(COMMON) } :data
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
