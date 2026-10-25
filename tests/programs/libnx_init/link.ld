ENTRY(_start)
PHDRS { code PT_LOAD FLAGS(5); constants PT_LOAD FLAGS(4); variables PT_LOAD FLAGS(6); stack PT_GNU_STACK FLAGS(6); }
SECTIONS {
    . = 0x10000;
    .text : { KEEP(*(.crt0)) *(.text) *(.text.*) } :code
    . = 0x20000;
    .rodata : {
        _DYNAMIC = .;
        QUAD(7); QUAD(0x10080); QUAD(8); QUAD(24); QUAD(9); QUAD(24); QUAD(0); QUAD(0);
        . = _DYNAMIC+0x80; QUAD(0x11000); QUAD(1027); QUAD(0x13000);
        *(.rodata*)
        . = ALIGN(8); __tls_align = .; QUAD(16);
        __eh_frame_hdr_start = .; __eh_frame_hdr_end = .;
        __tdata_lma = .; QUAD(0x1122334455667788); QUAD(0x8877665544332211); __tdata_lma_end = .;
    } :constants
    . = 0x21000;
    .relro : {
        __relro_start = .; QUAD(0);
        __got_start__ = .; *(.got) *(.got.*) __got_end__ = .;
        __preinit_array_start = .; KEEP(*(.preinit_array)) __preinit_array_end = .;
        __init_array_start = .; KEEP(*(.init_array)) KEEP(*(.init_array.*)) __init_array_end = .;
        . = __relro_start+0x2000;
    } :variables
    __data_start = .;
    .data : { *(.data.report) *(.data*) } :variables
    . = ALIGN(0x1000); __bss_start__ = .;
    .bss (NOLOAD) : {
        *(.bss*) *(COMMON)
        . = ALIGN(16); __tls_start = .; . += 64; __tls_end = .;
        . = ALIGN(0x1000);
    } :variables
    __bss_end__ = .; __argdata__ = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
