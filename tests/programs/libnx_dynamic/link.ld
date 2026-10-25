ENTRY(_start)
PHDRS {
    code PT_LOAD FLAGS(5);
    constants PT_LOAD FLAGS(4);
    variables PT_LOAD FLAGS(6);
    stack PT_GNU_STACK FLAGS(6);
}
SECTIONS {
    . = 0x10000;
    .text : { *(.text) *(.text.*) } :code
    . = 0x14000;
    .rodata : { BYTE(0); *(.rodata*) } :constants
    . = 0x15000;
    .data : { *(.data.report) *(.data*) } :variables
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } :variables
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
