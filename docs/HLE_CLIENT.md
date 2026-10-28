# CMIF sessions and a compiled C client

Milestone dated October 25, 2026. A compiled ARM64 C program validates local session controls, not a complete SDK or native Switch 2 application.

| HIPC message | Function | Reference reply |
| --- | --- | --- |
| Type 5, command 2 | CloneCurrentObject | Move handle for a new endpoint of the same service object |
| Type 5, command 3 | QueryPointerBufferSize | SFCO u16 zero, RawCount=9 |
| Type 2, zero RawCount | Close | Closes endpoint without SFCO reply |

Controls use CMIF version/token 0 without arguments, domains or descriptors. Unknown controls return service errors; malformed envelopes return transport errors and remain intact. Pointer-buffer size zero reflects absent static descriptors, not the separate **64 KiB A/B limit**.

Clones share service state, including SM initialization, but retain independent endpoint closure. Closing original leaves clones valid. New handles publish only after complete reply writes, as in [SM](HLE_SM.md).

Valid Close returns zero locally, preserves request bytes and closes the endpoint. Subsequent requests return **local SessionClosed=0x15FF**. CloseHandle is still required to release the slot; afterward the old ID yields InvalidHandle. These transitions are not exhaustive native kernel behavior. IPC memory must be entirely R/W even for Close without a reply.

## Compiled program

The project-owned tests/programs/hle_client/work.c uses Clang 18.1.6/Zig 0.13.0, O1, freestanding without libc. Short assembly supplies entry/SVC wrappers. It links neither libnx nor the Nintendo SDK.

It clones SM before registration, registers/closes original, acquires emu:demo through the clone, clones the service and closes its first endpoint. It queries pointer-buffer size, computes 55, copies/checks 512 bytes and 32-bit FNV-1a, queries virtual time, and closes remaining endpoints/handles. No CPU instruction additions were needed.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_client\program.elf 50000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

Result: **14,342 instructions**. At 0x12000 the seven u64 report fields are status=1, sum=55, copied=512, hash=3505779141, pointer_size=0, injected virtual time, flags=7. Tests inspect this report rather than trust normal exit alone.

Further checks cover seven-instruction slices, stack/FP/SIMD/TLS preservation, malformed controls, failed replies, full-table cloning, no ID gaps on failure and CloseHandle slot reclamation.

The profile remains bounded to two threads/partial SM. [Read-only emu:fs](HLE_FILES.md) adds another C client. Domains were absent at this historical stage; later libnx domain support is separate. TIPC, static buffers and native game services remain outside the demo profile. The next historical task was using file resources in a graphics demo.

Public cmif.h was consulted October 25; client/profile code is project-owned.

## Reference links

- [libnx cmif.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/cmif.h)
