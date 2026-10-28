# Application startup notification

Historical milestone following [focus/suspension](LIBNX_FOCUS_POLICY.md), succeeded by [operation mode](LIBNX_OPERATION_MODE.md). Implements ApplicationFunctions **40 NotifyRunning** in --hle-reference --homebrew-entry.

## Reference and uncertainty

Pinned libnx sends no arguments, expects a u8 boolean, and records guest-side notification state. Failure triggers LibnxError_BadAppletNotifyRunning. Default startup passes NULL for the unused output.

Public SwitchBrew documentation confirms the no-input/bool-output shape and describes official clients ignoring the value, without fully defining its meaning. The local profile therefore defines **true** as accepted notification, including repeats. This is not validation of exact Horizon/Switch 2 semantics.

## State and lifetime

Process runningNotified starts false and commits only after complete reply writes. appletRunningNotified() exposes it while active. It means notification was accepted, not main reached or a playable game. It changes no focus, foreground rights, suspension, messages or identity, and creates no object/handle.

State is shared across sessions/clones, survives focus changes/session closure, and retires on exit/fault. New loads begin unnotified; repeats are idempotent. Only ApplicationFunctions supports the method. Arguments/SendPid yield InvalidArguments; inactive process yields InvalidState. Failed reply writes preserve state. No unsupported extra foreground condition is imposed.

## Original program progress

NRO hash remains `10F4CF1CB2CD69FCB75C8935CB39A9F84210114236688BB50F3495AF2409553F`.

After notification, libnx requests absent CommonStateGetter **5 GetOperationMode**. Diagnostic returns to AM=16217 and follows app cleanup: NoSuspend, nine domain-object closes, message event, appletOE and APM sessions. Main/constructors remain unreached.

Focused path: **8,303 instructions / 38 IPC exchanges**. Initially unfocused: wait after **7,178**, then **2,428 more** after wake. Cleanup contributes to these totals; they do not measure game compatibility.

Tests verify the domain u8 reply/header, transactional state, wrong objects, arguments/PID, protected replies, clones, repeats, focus loss, session closure, fault/reload and full-versus-sliced SDK execution. Trace remains build/libnx-app-ipc.csv. **61 Debug/Release suites passed**, with unchanged hash.

Next was an explicit handheld/docked operation profile and related original requests, independent of focus and without invented console hardware characteristics. GPU/native Switch 2 validation remain future work.

## Reference links

- [switchbrew.org](https://switchbrew.org/wiki/Applet_Manager_services#NotifyRunning)
- [libnx applet.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/applet.c)
