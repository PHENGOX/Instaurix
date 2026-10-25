.section .text.nxlinkSetup,"ax"
.global nxlinkSetup
nxlinkSetup:
    mov x0,#0xe002
    b missing_dependency
.section .text.__libnx_exception_entry,"ax"
.global __libnx_exception_entry
__libnx_exception_entry:
    brk #0xe003
