# Handheld/docked profile and applet initialization

Historical milestone after [NotifyRunning](LIBNX_NOTIFY_RUNNING.md), followed by [original HID/shared memory](LIBNX_HID_SHARED_MEMORY.md). Adds the mode reads and notification subscriptions actually requested by original libnx startup.

| Profile | GetOperationMode u8 | GetPerformanceMode u32 |
| --- | --- | --- |
| Handheld, default | 0 | Normal=0 |
| Docked | 1 | Boost=1 |

Values follow pinned libnx enums; pairing them is local policy. Boost is a logical label, not a hardware clock/power/CPU-speed/resolution/GPU simulation. It is independent of PC window focus and does not prove native Switch 2 configuration.

CommonStateGetter commands 5/6 require no arguments/SendPid; errors publish no value. Mode is shared across process sessions/domains/clones. Machine::setAppletOperationMode selects Handheld/Docked; invalid modes/inactive processes reject changes. Reload defaults to Handheld; the host can explicitly select Docked afterward.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\libnx_app\program.nro 100000 --hle-reference --homebrew-entry --operation-mode docked
```

The option accepts handheld/docked once, requires a homebrew process and applies before execution.

## Notifications

SelfController 11/12 independently enable operation/performance notifications with one u8 boolean. No object/handle is created; subscriptions commit after reply writing. Nonboolean values, incorrect sizes or SendPid fail.

Unsubscribed changes are silent. Enabled transitions enqueue OperationModeChanged=30 then PerformanceModeChanged=31 as subscribed, signal the common event, and can wake a real kernel wait. Reads return current values.

Insufficient space for both messages rejects the entire transition without mode/queue changes. Identical profiles generate no duplicates. Disabling subscriptions preserves queued messages. Consuming the final message clears the shared event. Performance changes currently follow handheld/docked transitions, without APM frequency control, power management or independent performance transitions.

## Original NRO result

**appletInitialize completes** identity/interfaces, focus/foreground, suspension, notification, both mode reads and subscriptions. Both profiles use the same NRO. Next is unlinked hidInitialize in original __appInit: explicit local missing_dependency **0xE010 / 57360**, not native HID error or an implemented HID service.

Initially focused: **8,302 instructions / 28 IPC exchanges**, with no applet service failures. The direct marker bypasses previous GetOperationMode cleanup, so counts are not performance comparisons. Initially OutOfFocus: wait after **7,178**, then **2,427 more** after focus. Main/constructors remain unreached.

NRO SHA-256 stays `10F4CF1CB2CD69FCB75C8935CB39A9F84210114236688BB50F3495AF2409553F`. Separate replays test host transitions/errors.

applet_operation_profile_and_notifications checks SDK profiles/output sizes, independent subscriptions, protected replies, invalid values, wakeup, clone consumption, message order, unsubscribe, transactional queue capacity, independent sessions and reload. Explicit domain-close replays remain even when native startup no longer emits them.

**62 Debug/Release suites passed**. Release succeeded after retrying a temporary MSBuild tracking-directory access error. Docked CLI and invalid/out-of-profile rejection were checked.

The next HID audit identified lookup hid → CreateAppletResource → copied shared-memory handle → **0x40000 read-only mapping** through original shmem. This was the next scope, not yet implemented here. Original __appInit still needs time/files before main. emu:pad is not this native HID path.

References: pinned libnx b156b4774fb2df7b93402169509266da7547fc4c, services/applet.c, runtime/init.c, services/applet.h and apm.h. Historical Switch references require separate native Switch 2 validation.
