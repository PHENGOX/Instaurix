# Original VI client and the first display-layer blocker

The separate `tests/programs/libnx_vi` reference links original services/vi.c from pinned libnx b156b4774fb2df7b93402169509266da7547fc4c. A project guest main exercises the original VI client and records its results. This isolates a graphics prerequisite; it does not compile/run the complete external hello-world or render pixels.

## Audited graphics path

The cached pinned sources show consoleInit selecting its software renderer, which uses the default native window/framebuffer path. Linking that window module makes its original __nx_win_init hook available to __appInit. That hook calls viInitialize(Default), viOpenDefaultDisplay, viCreateLayer, scaling and native-window creation. In the application profile, viCreateLayer first calls **appletCreateManagedDisplayLayer**, SelfController command **40**.

The inspected sources are runtime/devices/console.c, console_sw.c, display/default_window.c and services/vi.c/applet.c. Their hashes are recorded in the manifest. console/default_window/framebuffer code is **inspected, not executed by this target**. The target deliberately calls the VI portion from its own main to isolate it. A complete original window-startup hook would require layer creation before main and would abort when that prerequisite fails.

The external pinned hello-world source could not be fetched in this environment during this milestone. It remains the previously selected historical Switch target, without a new compilation or execution claim.

## Added bounded HLE behavior

Only homebrew-reference SM lookup of **vi:u** is enabled. vi:m and vi:s remain absent, so original viInitialize(Default) exercises its real manager/system/application fallback.

| Object/command | Behavior |
| --- | --- |
| vi:u root / 0 | Zero u32 input creates a typed application-display service session |
| Application display / 100 | No input creates a typed Binder relay session |
| Application display / 1010 | Exactly 64 bytes naming Default opens local logical display ID 1 |
| Application display / 1020 | ID 1 closes one open reference |
| Application display / 1102 | Explicit UnknownCommand; no invented resolution |
| Binder relay methods | Explicitly unsupported; no transaction success stub |

The logical Default identifier is a reference-profile value, not measured console hardware. A service object has at most **64 open references**. Clones use the existing shared service-state machinery; independent service objects start with their own counts. Duplicate opens refer to the same local ID, and closes require an existing reference. Unknown names/IDs, malformed inputs and exhausted quota fail explicitly. General IPC domains, buffers and PID-bearing calls are not added to this VI subset.

Typed sessions use the existing shared kernel-handle quota and transactional publication. Display reference changes also commit only after a complete reply write. Failed publication creates no hidden handle/reference. Closing a root endpoint does not destroy its child sessions; original viInitialize itself closes the root before acquiring its Binder relay.

## Original client results

The guest successfully initializes VI and opens Default twice. It then records unsupported resolution (leaving width/height sentinels unchanged) and calls original viCreateLayer. That function reaches the actual **SelfController command 40**, receives UnknownCommand, and returns without a layer ID or initialized layer. This is the next functional blocker on the audited window path.

The project guest closes both display references and runs original viExit, which clears application/Binder client sessions. Standard app cleanup returns normally. No frame is published: frameCount remains zero. The resolution call is an additional diagnostic probe, not a demonstrated prerequisite of default-window startup.

The reference executes **23,402 instructions**, file size **421,888 bytes**, BSS **16,384 bytes**. Base report: **0x75000**; VI report: **0x761D8**. Project main returns **77**, original libnx loader result is **0**. SD is not configured for this reference.

## Validation

`libnx_original_vi_sessions_and_display` verifies original service fallback, real session handles, duplicate display opens, explicit unsupported outputs, original close/exit behavior, constructor/main checks and zero rendered frames. Continuous and seven-instruction execution must match all 32 report fields and instruction counts.

A second run captures the actual SDK display packet, pauses with two references open, and replays malformed names/sizes/IDs and a protected reply. It fills the 64-reference quota, checks failure without extra references, closes them all, tests unsupported Binder requests, restores the expected two references and resumes the original layer call. The test confirms the SelfController command 40 request.

```powershell
.\tools\build_libnx_vi.ps1 -Python <python_path>
ctest --test-dir build -C Debug -R libnx_original_vi_sessions_and_display --output-on-failure
```

Original library licenses accompany the fixture; the VI source notice is in [LIBNX_SOURCE_NOTICES.txt](licenses/LIBNX_SOURCE_NOTICES.txt). Build inputs, generated reference symbols and NRO hashes are recorded in reference.json. Earlier NROs remain separate and unchanged.

## Next step

Define a coherent process-bound managed-layer state for SelfController command 40, then follow the original client's next VI/window/Binder request. Do not fabricate a native-window parcel or a successful buffer queue to skip that work. Presentation, guest GPU commands/shaders, Vulkan rendering, controller samples and native Switch 2/game compatibility remain unvalidated.

## Final validation

All **71 CTest suites pass in Debug and Release**. Strict reconstruction reproduces NRO SHA-256 `810B9BBAE06A124792347A4B78733B8154424BEAC215450A8AC6D43B0490023B`. Earlier application, filesystem and POSIX NROs still match their manifests.

## Follow-up milestone

The managed-layer blocker described above is historical. See [process-owned managed layers](LIBNX_MANAGED_LAYERS.md): the unchanged NRO now executes 23,623 instructions and reaches VI OpenLayer's unsupported PID/output-buffer transport. No frame is rendered.
