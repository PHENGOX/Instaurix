# Initial HLE IPC transport

Milestone dated October 25, 2026. An ARM64 ELF opens a local service, writes a thread-region request and reads its response. Transport uses a publicly studied HIPC/CMIF subset; service names/commands are project-owned. No native Switch 2 compatibility is claimed. Later [Send/Receive buffers](HLE_IPC_BUFFERS.md) extend this descriptor-free format.

| SVC | Input | Output |
| --- | --- | --- |
| 0x1F ConnectToNamedPort | X1=NUL-terminated readable RAM name | W0=result, W1=session handle on success |
| 0x21 SendSyncRequest | W0=session handle | W0=transport result; reply in current thread IPC buffer |
| 0x16 CloseHandle | W0=handle | W0=result |

Initially the only port is **emu:demo**. Names are bounded to 12 bytes including termination. Unknown ports fail rather than implicitly opening console services. [sm:](HLE_SM.md) was added later.

Sessions share the 128-slot handle table with events/threads. Close frees slots, never old IDs. Session handles are not events or threads and reject SignalEvent/ClearEvent/ResetSignal/StartThread. Waiting on these local synchronous sessions is explicitly unsupported: no guest server manages their wait state. A stateless host service session can be used sequentially by both guest threads; requests reply immediately without a server process.

## Buffer and format

Requests use the current thread's canonical first **256 local-region bytes**, initially main 0xFEE200 and worker 0xC11200. The dispatcher does not accept an arbitrary guest-provided IPC pointer. Each thread has its own buffer.

The whole buffer must be readable/writable RAM. Requests are copied into a bounded host array, validated, and answered through a separately built zero-padded reply and transactional memory copy. Validation errors preserve requests; bytes after the buffer are untouched.

Initial format: HIPC **type 4**, no descriptors/PID/transferred handles/receive list; Header1 contains only RawCount, **8–62 32-bit words**. **SFCI** CMIF at offset 16 has version 0, command ID and zero token. Inline parameters start at 32; RawCount includes 16 total padding bytes.

Replies have HIPC type 0, **SFCO** at +16, service result +24 and data +32. RawCount is 10 for a u64 result, 8 for a service error without data. Later [A/B](HLE_IPC_BUFFERS.md), [PID/MoveHandle](HLE_SM.md), and [session controls](HLE_CLIENT.md) have separately bounded formats. Domains were absent at this initial milestone; later libnx domain support is documented separately. TIPC, static descriptors and general handle transfers remain outside it.

## Demo commands and errors

| Command | Input | Result |
| --- | --- | --- |
| 0 Add | Exactly two unsigned u64 values / 16 bytes | Sum modulo 2^64 |
| 1 GetVirtualTime | No parameters | Local virtual nanoseconds |

Unknown commands or wrong sizes produce SFCO service errors with W0=0, since transport succeeded. Envelope/handle/memory errors return transport errors in W0 without a fake success response.

Local module 511 results: InvalidMessage=0x3FF, UnsupportedFormat=0x5FF, UnknownCommand=0x7FF, PortNotFound=0x9FF, InvalidArguments=0xBFF. These are not claimed as native console errors. Existing HLE categories cover memory/handles. Transport errors preserve the session for correction; native automatic closure behavior is not modeled. Successful reload resets sessions; failed loading preserves them.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_ipc\program.elf 1000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

The ELF exits after **116 instructions**. Main requests 20+35=55; worker uses the same session in its own buffer for 44+55=99. Main joins, checks its unchanged reply, queries time and closes the session. Tests cover names/envelopes, service-versus-transport errors, permissions, typed/invalid handles, shared quota and one-instruction slicing.

The historical next tasks were buffer descriptors and SM. Demo services do not replace game services; guest GPU/native application formats require separate implementation. Transport/demo code is project-owned; public sources were consulted October 25, 2026.

## Reference links

- [SwitchBrew system calls](https://switchbrew.org/wiki/SVC)
- [SwitchBrew IPC marshalling](https://switchbrew.org/wiki/IPC_Marshalling)
- [libnx cmif.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/cmif.h)
