# Original libnx filesystem client initialization

Historical milestone linking unchanged services/fs.c, sf/sessionmgr.c and kernel/condvar.c from libnx b156b4774fb2df7b93402169509266da7547fc4c. See [SD object](LIBNX_SD_ROOT.md) and [TLS](LIBNX_TLS.md) for subsequent work.

## Observed result

The original client opens fsp-srv, converts to a domain, executes SetCurrentProcess (command 1, SendPid without copied handle), then clones its session twice. Guest SessionMgr contains **three sessions** and free mask **7**. fsInitialize succeeds. After **15,244 instructions / 47 IPC exchanges**, local marker **0xE017** identifies unlinked fsdevMountSdmc. Main/constructors remain unreached.

## HLE model and checks

Each filesystem connection has current-process attachment state. Clones share it and the domain; independent connections do not. PID bytes are placeholders; simulated process context supplies identity. Parsing accepts SendPid alone while retaining appletOE's distinct copied-process-handle validation.

State commits only after complete reply writes. Tests cover unwritable replies, malformed arguments/formats, quotas, no failed-publication ID consumption, clones, closure and retired processes. Errors are local reference policy, not established identical across Horizon versions. Unsupported filesystem methods fail explicitly.

original_fsp_process_attachment_and_clones also reads the SessionMgr actually created by guest libnx. Applet captures distinguish their domain requests from filesystem requests.

## Limits and next work

At this milestone no host files/SD card are opened or mounted, and no file I/O or GPU rendering is demonstrated. Next was original fsdev and its dependencies, explicit SD opening policy, and auditing filesystem-priority TLS before its use. Sources/symbols/NRO hashes/metrics are in tests/programs/libnx_app/reference.json. Historical Switch references do not prove native Switch 2 compatibility.

**66 Debug/Release suites passed**. Strict rebuilding reproduced NRO SHA-256 `CC38D1F402FAEA4413E706DC01C2931AE0C5C5C5D515751998BDCCC2CE864BBA`.
