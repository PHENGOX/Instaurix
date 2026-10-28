# Foreground acquisition

Historical milestone following [identity/messages](LIBNX_APPLET_STATE.md); see [focus policy](LIBNX_FOCUS_POLICY.md) for the next stage. Applies to --hle-reference --homebrew-entry.

## Local behavior

WindowController **10 AcquireForegroundRights** accepts no arguments/SendPid on a valid window object. The process must be active and host-focused. Unfocused/retired calls return InvalidState; extra arguments return InvalidArguments. These are local policy choices, not exhaustive Horizon error semantics.

The idempotent process-wide grant is distinct from focus and shared across sessions/clones. It changes no resource identity, focus, queue, object or handle. It commits only after the entire reply reaches guest RAM.

Losing focus retains the grant but disables effective foreground ownership; restoring focus re-enables it. appletForegroundRights() and appletOwnsForeground() distinguish these states. Session closure retains the grant; exit/fault removes it, and reload starts without it.

This models one virtual application, without multi-application arbitration, HOME, sleep or automatic suspension. It never forces PC window focus.

## Original program

Unchanged NRO hash: `10F4CF1CB2CD69FCB75C8935CB39A9F84210114236688BB50F3495AF2409553F`.

Focused startup progresses from 7,428 to **7,732 instructions**, 35 to **36 IPC exchanges**. Next missing method is SelfController **13 SetFocusHandlingMode**, original three-bool payload `00 00 01`; cleanup requests `01 01 00`, also then absent. Diagnostic remains AM=16217; constructors/main are unreached.

Initially unfocused execution still waits after **7,178 instructions**; host wake/foreground acquisition then execute **1,857 more** to the new blocker.

Tests replay SDK requests for correct object types, malformed arguments, unwritable replies, unfocused calls, idempotence, clones, focus transitions, session closure, process fault and reload. Integration traces verify command 10 success before explicit command 13 failure, including sliced execution.

**60 Debug/Release suites passed**, with unchanged verified NRO hash. No CPU extension was needed. Next was defining suspension/resumption before accepting SetFocusHandlingMode. Native display and Switch 2 compatibility remain unproven. Reference: pinned libnx applet.c startup/focus/foreground sequence.
