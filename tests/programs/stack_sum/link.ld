ENTRY(_start)
SECTIONS
{
    . = 0;
    .text : { *(.text) *(.text.*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
