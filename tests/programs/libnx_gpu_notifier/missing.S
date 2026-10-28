.macro MISSING name, marker
.section .text.\name,"ax"
.global \name
\name:
    mov x0,#\marker
    b missing_dependency
.endm
MISSING nxlinkSetup,0xe002
.section .text.__libnx_exception_entry,"ax"
.global __libnx_exception_entry
__libnx_exception_entry:
    brk #0xe003
