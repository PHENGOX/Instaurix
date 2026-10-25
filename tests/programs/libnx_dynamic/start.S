.text
.global _start
_start:
    bl probe_main
.global stop_probe
stop_probe:
    svc #0xf000
    b stop_probe

/* Real SVC wrappers, never success stubs. LNY protection is outside this probe. */
.global svcSetMemoryPermission
svcSetMemoryPermission:
    svc #2
    ret
.global svcSetMemoryAttribute
svcSetMemoryAttribute:
    svc #3
    ret
