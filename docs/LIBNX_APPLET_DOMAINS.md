# appletOE domains and IPC objects

Historical report; see [application identity/messages/focus](LIBNX_APPLET_STATE.md) and README for later progress.

On October 25, 2026, the unchanged default-app NRO opens appletOE, converts to a domain, and receives an application proxy plus five interfaces. The next unsupported call is **IWindowController::GetAppletResourceUserId**.

| SDK operation | Domain object |
| --- | --- |
| appletOE conversion, CMIF control 0 | Root 1 |
| Root 0 OpenApplicationProxy | Proxy 2 |
| Proxy 20 | ApplicationFunctions 3 |
| Proxy 11 | LibraryAppletCreator 4 |
| Proxy 0 | CommonStateGetter 5 |
| Proxy 1 | SelfController 6 |
| Proxy 2 | WindowController 7 |
| WindowController 1 | Explicit UnknownCommand, no value/object |

These are real bounded object IDs, not functional implementations of every interface method. Resource identity, focus, events and screen state were not supplied by this milestone.

Original cleanup calls SelfController 13 with three parameter bytes. Transport accepts non-four-byte payload size and service returns UnknownCommand. Objects close in order 7,6,5,4,3,2, then appletOE/APM handles close.

The path executes **5,851 instructions and 26 SendSyncRequest exchanges**, ending with InitFail_AM=16,217. Constructors/main remain unreached. The unchanged NRO hash shows progress came from emulator changes.

## Domain model

Initially limited to homebrew appletOE. Root ID=1; duplicate conversion returns InvalidState without resetting objects. Generic clone/pointer-size controls remain.

- Maximum **64 objects including root**, separate from **128 kernel handles**.
- IDs increase and are never reused; slots can be reused with fresh IDs.
- Clones share a domain but retain independent handles/closure.
- Object close does not close the session. Closing an endpoint leaves objects accessible through a live clone; the last endpoint releases its domain reference.
- Independent appletOE connections have separate ID spaces, even when numbers coincide.
- Conversion/object publication occurs only after complete replies; failed writes consume no ID or table.

Object types determine supported creation methods; other methods return service errors. InvalidObject/OutOfObjects are local codes, not claims of Horizon equivalence.

## Protocol subset

HIPC type 4 contains domain header then SFCI. Validation checks RawCount, bounds, reserved fields, versions and tokens. CMIF sizes need not be multiples of four; HIPC word counts round correctly, including the original three-byte cleanup payload.

OpenApplicationProxy requires the actually observed SendPid plus one copied current-process pseudo-handle and a zero reserved u64. Guest PID bytes do not establish identity; no general cross-process handle/object transfer is added.

Domain replies contain output header, SFCO and a new object ID on successful creation, not a new kernel handle. Conversion/clone controls remain non-domain CMIF replies. Valid object close preserves request bytes without fabricated SFCO; repeated close yields transport InvalidObject.

Domain buffers/descriptors, input objects, contexts, conversion to independent handles and other unsupported controls remain rejected. This is not a general Horizon domain engine.

```powershell
ctest --test-dir build -C Debug -R "native_applet_domains_and_lifetimes|libnx_default_app_and_native_apm" --output-on-failure
```

Single-stepping checks one conversion, six creations, two functional errors and six closes. Seven-instruction slices match report/count. SDK replays cover malformed headers, wrong process-handle type, reserved arguments, unwritable conversion/creation replies, stale IDs, clone sharing, independent domains and quotas. Full tables publish nothing; freeing a slot permits the next ID without failure-induced gaps. Trace: build/libnx-app-ipc.csv.

**58 Debug/Release suites passed**. NRO SHA-256 stayed `64A983071DEC5EF405DF6B797807326684F074BA8182B9BB62B97BAA37774F6E`.

The next requirement was process-bound resource identity and actual controller/event methods. Focus, notifications, display and input must have coherent effects rather than fake success to reach main. This historical Switch reference does not prove native Switch 2 compatibility.

## Reference links

- [libnx cmif.h](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/sf/cmif.h)
- [libnx applet.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/applet.c)
