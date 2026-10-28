ENTRY(_start)
EXTERN(FindDevice GetDeviceOpTab fsdevGetDeviceFileSystem fsdevUnmountDevice)
EXTERN(setenv getenv unsetenv malloc free realloc _sbrk_r referenceTlsReadSeed referenceTlsReadZero referenceTlsWrite fsOpenSdCardFileSystem)
PHDRS { code PT_LOAD FLAGS(5); constants PT_LOAD FLAGS(4); variables PT_LOAD FLAGS(6); tls PT_TLS FLAGS(4); stack PT_GNU_STACK FLAGS(6); }
SECTIONS {
    . = 0x10000;
    .text : { KEEP(*(.crt0)) *(.text) *(.text.*) } :code
    . = ALIGN(0x1000);
    .rodata : {
        _DYNAMIC = .;
        QUAD(7); QUAD(_DYNAMIC+0x80-0x10000); QUAD(8); QUAD(24); QUAD(9); QUAD(24); QUAD(0); QUAD(0);
        . = _DYNAMIC+0x80; QUAD(__relro_start-0x10000); QUAD(1027); QUAD(__data_start-0x10000);
        *(.rodata*)
        . = ALIGN(8); __tls_align = .; QUAD(16);
        __eh_frame_hdr_start = .; __eh_frame_hdr_end = .;
        
    } :constants
    . = ALIGN(0x1000);
    .relro : {
        __relro_start = .; QUAD(0);
        __got_start__ = .; *(.got) *(.got.*) __got_end__ = .;
        __preinit_array_start = .; KEEP(*(.preinit_array)) __preinit_array_end = .;
        __init_array_start = .; KEEP(*(.init_array)) KEEP(*(.init_array.*)) __init_array_end = .;
        . = __relro_start+0x2000;
    } :variables
    __data_start = .;
    .data : { *(.data.report) *(.data*) } :variables
    . = ALIGN(16);
    .tdata : { KEEP(*(.tdata.reference_seed)) *(.tdata*) . = ALIGN(16); } :variables :tls
    __tdata_lma = ADDR(.tdata); __tdata_lma_end = ADDR(.tdata)+SIZEOF(.tdata);
    .tbss (NOLOAD) : { *(.tbss*) *(.tcommon) . = ALIGN(16); } :variables :tls
    . = ALIGN(0x1000); __bss_start__ = .;
    .bss (NOLOAD) : {
        *(.bss*) *(COMMON)
        . = ALIGN(16); __tls_start = .; . += SIZEOF(.tdata)+SIZEOF(.tbss); __tls_end = .;
        . = ALIGN(0x1000);
    } :variables
    __bss_end__ = .; __argdata__ = .; PROVIDE(__end__ = .);
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
