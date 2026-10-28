# Original libnx SM client: verified IPC

Milestone dated October 25, 2026. The NRO links original sm.c from libnx b156b4774fb2df7b93402169509266da7547fc4c above [original minimal initialization](LIBNX_INIT.md), requesting no emu:* service. Existing transport accepted SDK packets **without CPU or IPC-dispatch changes**. This adds a reproducible external client, not all services of a complete application.

## Observed path

The app hook calls smInitialize twice; libnx's real reference count avoids another connection/registration. The program requests set:sys, clones SM, retries on the clone, then closes it. First smExit retains original connection; final cleanup smExit closes it.

| Exchange | Session/request | Local result |
| --- | --- | --- |
| 1 | Manager control 3 QueryPointerBufferSize | SFCO u16 zero |
| 2 | Manager RegisterClient 0 with SendPid | Success |
| 3 | Manager GetService 1, set:sys | PortNotFound, no handle |
| 4 | Manager control 2 CloneCurrentObject | Distinct Move handle |
| 5 | Clone GetService set:sys | PortNotFound, confirming shared registration |
| 6 | Clone HIPC Close and CloseHandle | Independent closure |
| 7 | Manager lookup after first smExit | Still active, PortNotFound |
| 8 | Final manager HIPC Close/CloseHandle | Connection closed, libnx object reset |

One sm: connection, eight SendSyncRequest and two CloseHandle calls; **3,117 instructions**, preserved constructor/main/cleanup flags, main=77 and loader result=0. No dependency trap executes. PortNotFound=0x9FF, NotInitialized and AlreadyInitialized remain **local prototype codes**. set:sys is genuinely absent; no firmware identity/session is fabricated. App hooks still avoid default services.

## SDK byte validation

Single-stepping captures requests before SVC 0x21 and replies afterward. Checks include u16 pointer-buffer result/RawCount; RegisterClient Header1=0x8000000A, SendPid and SFCI+32; one distinct clone Move handle; set:sys and handle-free errors; transport/SFCO separation; reply-free HIPC Close and ordered handle release; ThreadVars preservation and seven-instruction determinism.

build/libnx-sm-ipc.csv stores complete request/reply hex from this synthetic target. Actual SDK packets are replayed on a clean process for early lookup, malformed envelopes, unwritable registration reply without publication, duplicates, independent connections and closed handles. Guest PID bytes are not identity. Multiprocess authentication/SAC are absent.

**56 Debug/Release suites passed**. Rebuilds reproduced both SM and no-SM references.

```powershell
.\tools\build_libnx_sm.ps1 -Python <python_path>
ctest --test-dir build -C Debug -R libnx_original_sm_ipc --output-on-failure
```

The shared builder compiles unchanged libnx/newlib, adding sm.c and app calls. Vendored SM is compared against the pinned archive. tests/programs/libnx_sm/reference.json and initialization provenance record NRO/licenses/dependencies/results. Original no-SM hashes remain unchanged.

[Default app initialization/APM](LIBNX_APP.md) followed. Original code ignores missing set:sys without inventing a system identity. External hello-world, native display and native Switch 2 compatibility remain unproven.

## Reference links

- [libnx sm.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/sm.c)
