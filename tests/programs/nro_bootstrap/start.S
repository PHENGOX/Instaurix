.text
.global _start
_start:
    b entry
    .word 0
    .space 120
entry:
    mov x19,x0
    mov x20,x1
    mov x21,x30
    mov x22,sp
    adrp x8,saved_return
    str x30,[x8,:lo12:saved_return]
    adr x0,_start
    adrp x1,mod0
    add x1,x1,:lo12:mod0
    bl __nx_dynamic
    mov x0,x19
    mov x1,x20
    mov x2,x21
    mov x3,x22
    bl boot_main
    br x21
.global return_to_loader
return_to_loader:
    adrp x1,saved_return
    ldr x1,[x1,:lo12:saved_return]
    br x1
.global svcSetMemoryPermission
svcSetMemoryPermission:
    svc #2
    ret
.global svcSetMemoryAttribute
svcSetMemoryAttribute:
    svc #3
    ret
.global thread_id
thread_id:
    mov x1,x0
    svc #0x25
    cbnz x0,1f
    mov x0,x1
    ret
1:  mov x0,#0
    ret
.section .rodata.mod0,"a"
.global mod0
mod0:
    .word 0x30444f4d
    .word dynamic_table-mod0
    .word __bss_start-mod0
    .word __bss_end-mod0
    .word 0,0,0
    .space 36
dynamic_table:
    .quad 7,0x4080
    .quad 8,24
    .quad 9,24
    .quad 0,0
    .quad 0x5040,1027,0x5000
.section .data.slots,"aw"
.global relocated_slot
relocated_slot:
    .quad 0
.global saved_return
saved_return:
    .quad 0
