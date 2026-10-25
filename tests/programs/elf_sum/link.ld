ENTRY(_start)
PHDRS
{
    code PT_LOAD FLAGS(5);
    constants PT_LOAD FLAGS(4);
    variables PT_LOAD FLAGS(6);
    stack PT_GNU_STACK FLAGS(6);
}
SECTIONS
{
    . = 0x10000;
    .text : { *(.text) *(.text.*) } :code
    . = 0x11000;
    .rodata : { *(.rodata*) } :constants
    . = 0x12000;
    .data : { *(.data*) } :variables
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } :variables
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
