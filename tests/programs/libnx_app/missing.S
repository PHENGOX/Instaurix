.macro MISSING name, marker
.section .text.\name,"ax"
.global \name
\name:
    mov x0,#\marker
    b missing_dependency
.endm
MISSING nxlinkSetup,0xe002
MISSING hidInitialize,0xe010
MISSING hidExit,0xe011
MISSING timeInitialize,0xe012
MISSING timeExit,0xe013
MISSING fsInitialize,0xe015
MISSING fsExit,0xe016
MISSING fsdevMountSdmc,0xe017
MISSING fsdevUnmountAll,0xe018
MISSING __libnx_init_cwd,0xe019
MISSING timeGetCurrentTime,0xe01a
MISSING timeToCalendarTimeWithMyRule,0xe01b
MISSING setenv,0xe01c
MISSING __libnx_free,0xe01d
.section .text.__libnx_exception_entry,"ax"
.global __libnx_exception_entry
__libnx_exception_entry:
    brk #0xe003
