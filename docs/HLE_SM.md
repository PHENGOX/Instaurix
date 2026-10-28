# Reference service manager

Milestone dated October 25, 2026. An ELF opens sm:, initializes its connection, requests a service by name and receives a session through HIPC Move. The obtained session survives manager closure. This local profile does not claim native Switch 2 compatibility.

## Interface

ConnectToNamedPort accepts **sm:** as well as the direct demo port. SendSyncRequest uses the current thread IPC buffer.

| CMIF command | Input | Reply |
| --- | --- | --- |
| 0 RegisterClient | SendPid special header and 8 inline bytes | SFCO result; initializes this connection |
| 1 GetServiceHandle | 8-byte service name, no descriptors | SFCO and one Move handle on success |

Local names: **emu:demo**, **emu:fs**, **emu:disp**, **emu:pad**. Files require an explicitly configured root. The homebrew profile adds [apm/OpenSession](LIBNX_APP.md), [appletOE domain interfaces](LIBNX_APPLET_DOMAINS.md), and further original-client services documented in later milestones. emu:demo retains [Add/time](HLE_IPC.md) and [Copy](HLE_IPC_BUFFERS.md). Repeated lookups create independent sessions. [Original libnx SM](LIBNX_SM.md) exercises registration, cloning and closure using actual SDK packets.

Registration is required before lookup. Separate direct manager connections have independent state; clones share initialization. Duplicate registration returns AlreadyInitialized=0x13FF; early lookup returns NotInitialized=0x11FF. These are local module-511 errors. Missing services return PortNotFound, never dummy sessions.

The allowed input special header contains only SendPid. Guest PID bytes and its placeholder are not guest-declared identity: the service binds to the simulated process. Multiprocess authentication, SAC and service filtering are not modeled; this is not a native security boundary.

TIPC, dynamic registration/unregistration and other SM commands remain unsupported. Unknown commands produce service errors. SM domains and general input-handle transfer are absent. [CloneCurrentObject, QueryPointerBufferSize and Close](HLE_CLIENT.md) are separate controls. The direct emu:demo port remains for older tests.

## Reply and lifetime

GetServiceHandle replies with Header1 **0x80000008**, special header **0x20**, Move handle **+12**, SFCO **+16**, result **+24**. Errors carry no handle.

New service/handle publication occurs only after the entire reply reaches guest RAM. Unwritable replies/allocation failures leave no hidden handle or committed registration. Full tables return SFCO OutOfHandles without Move metadata. The **128 handles** are shared with events, threads and direct sessions. Closing SM does not close obtained services; closing one service does not affect neighbors. Freed slots may be reused, old IDs may not. Successful reload removes all connections/state.

Move here delivers a new service handle; it is not a general transfer of existing handles between guest processes.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_sm\program.elf 1000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

The ELF exits after **85 instructions**: open/register SM, acquire emu:demo, close SM, compute 20+35=55, close service; final marker=1. Tests cover Move formatting, lifetime, initialization, unknown names, malformed messages, reply protection, quota and no consumed IDs on failure, including one-instruction budgets.

Session controls/C client and read-only files followed this stage. Native graphics/game interfaces remain separate. Public sources were consulted October 25; manager implementation/policy are project-owned.

## Reference links

- [libnx sm.c](https://github.com/switchbrew/libnx/blob/master/nx/source/services/sm.c)
- [libnx hipc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/hipc.h)
