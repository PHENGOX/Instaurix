# Default application initialization and first APM session

Historical milestone dated October 25, 2026. A new target executes **original libnx __appInit**, linking unchanged setsys/applet/APM clients from b156b4774fb2df7b93402169509266da7547fc4c instead of no-service app hooks.

## Observed blocker and progress

Missing set:sys is not fatal by itself: original code ignores failure to obtain a system version and continues to appletInitialize, which first requires apmInitialize for ordinary applications.

Before APM, execution stopped after 2,790 instructions with libnx InitFail_AM=16,217. Bounded APM opening lets **the same NRO** reach appletOE after **3,785 instructions**, with the same high-level diagnostic. Trace context, not the diagnostic alone, distinguishes the causes.

| Native step | Result at this milestone |
| --- | --- |
| Original smInitialize | Success |
| set:sys lookup | Local PortNotFound, no handle; execution continues |
| apm lookup | Real session Move handle |
| APM 0 OpenSession | Distinct child Move handle |
| appletOE lookup | Local PortNotFound; actual applet initialization failure |
| Failure cleanup | Original code closes APM child then parent |

Original weak AppletType_Default becomes Application after APM. System version stays zero; no firmware, performance mode or clock is invented. **Neither constructors nor main are reached.** Tests require this exact stop.

## Exact APM scope

Only --hle-reference --homebrew-entry recognizes the native eight-byte apm name; the older HLE profile rejects it. Root command **0 OpenSession**, without parameters/descriptors, creates a distinct typed child after a complete SFCO reply write. Generic clone/pointer-size/close controls apply. Children cannot recursively open sessions.

Performance/power commands return **UnknownCommand**. Session state is not hardware power management. Closing parent leaves child valid; closed IDs stay invalid. The shared 128-handle quota bounds creation. Unwritable replies, malformed requests and exhausted quota consume no IDs and publish no handles.

## Validation

build/libnx-app-ipc.csv records **10 SendSyncRequest exchanges**, including set:sys/apm/appletOE, child creation and APM cleanup. Original applet cleanup also calls an uninitialized controller, receiving InvalidHandle; libnx continues. This is not converted to fake success.

Tests verify original diagnostic, absent main/constructors, applet type, zero system version, close ordering and seven-instruction determinism. Replayed SDK packets test malformed formats, extra arguments, unknown commands, protected replies, parent/child lifetime, quotas and legacy-profile rejection.

**57 Debug/Release suites passed**. Rebuilds preserve earlier SM/no-SM hashes and reproduce this target.

```powershell
.\tools\build_libnx_app.ps1 -Python <python_path>
ctest --test-dir build -C Debug -R libnx_default_app_and_native_apm --output-on-failure
```

At this stage the NRO was **212,992 bytes**, BSS **4,096 bytes**, report **0x43000**. Its manifest records APM/applet/set/NRO hashes and preceding newlib provenance. A larger retained applet function graph required more text space; older targets stayed unchanged.

Beyond-stop HID/time/FS helpers were explicit missing-symbol traps, not claimed functional. This historical test expected actual libnx diagnostics, rejecting dependency markers. Diagnostic/exit bridges remain project-owned; full libc shutdown and GCC devkitA64 installation were outside the milestone.

[Applet domains](LIBNX_APPLET_DOMAINS.md) followed, reaching GetAppletResourceUserId. Later progress is in README. Historical Switch infrastructure is not game or native Switch 2 compatibility.

## Reference links

- [libnx init.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/init.c)
- [libnx applet.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/applet.c)
- [libnx apm.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/apm.c)
