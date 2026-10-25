.text
/* This is a boundary trap, NOT a successful implementation of libnx init. */
.global __libnx_init
__libnx_init:
    adrp x8,report
    add x8,x8,:lo12:report
    mov x9,#1
    str x9,[x8]
    stp x0,x1,[x8,#8]
    str x2,[x8,#24]
    mov x9,sp
    str x9,[x8,#32]
    mov x1,x2
    mov x0,#0xc001
    b __nx_exit
.global __libnx_exception_entry
__libnx_exception_entry:
    brk #0x401
.global main
main:
    brk #0x402
.global exit
exit:
    brk #0x403
.global diagAbortWithResult
diagAbortWithResult:
    brk #0x404
.global svcSetMemoryPermission
svcSetMemoryPermission:
    svc #2
    ret
.global svcSetMemoryAttribute
svcSetMemoryAttribute:
    svc #3
    ret
.global memset
memset:
    mov x8,x0
    cbz x2,2f
1:  strb w1,[x8],#1
    subs x2,x2,#1
    b.ne 1b
2:  ret
.section .data.report,"aw"
.global report
report:
    .space 64
.global __system_argc
__system_argc:
    .word 0
    .balign 8
.global __system_argv
__system_argv:
    .quad 0
