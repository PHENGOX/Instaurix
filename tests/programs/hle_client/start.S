.text
.global _start
_start:
    bl work
    svc #7
.global client_connect
client_connect:
    mov x2, x1
    mov x1, x0
    svc #0x1F
    str w1, [x2]
    ret
.global client_send
client_send:
    svc #0x21
    ret
.global client_close_handle
client_close_handle:
    svc #0x16
    ret
.global client_tls
client_tls:
    mrs x0, tpidrro_el0
    ret
