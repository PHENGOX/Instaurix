ENTRY(_start)
PHDRS {
    code PT_LOAD FLAGS(5);
    constants PT_LOAD FLAGS(4);
    variables PT_LOAD FLAGS(6);
    stack PT_GNU_STACK FLAGS(6);
}
SECTIONS {
    . = 0x10000;
    .text : { *(.crt0) *(.text) *(.text.*) } :code
    . = 0x14000;
    .rodata : {
        _DYNAMIC = .;
        QUAD(7); QUAD(0x4080);
        QUAD(8); QUAD(24);
        QUAD(9); QUAD(24);
        QUAD(0); QUAD(0);
        . = _DYNAMIC + 0x80;
        QUAD(0x5000); QUAD(1027); QUAD(0x7000);
        *(.rodata*)
        __eh_frame_hdr_start = .;
        __eh_frame_hdr_end = .;
    } :constants
    . = 0x15000;
    .relro : {
        __relro_start = .;
        QUAD(0);
        __got_start__ = .;
        *(.got) *(.got.*)
        __got_end__ = .;
        . = __relro_start + 0x2000;
    } :variables
    __data_start = .;
    .data : { *(.data.report) *(.data*) } :variables
    . = 0x18000;
    __bss_start__ = .;
    .bss (NOLOAD) : { *(.bss*) *(COMMON) . = ALIGN(0x1000); } :variables
    __bss_end__ = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
