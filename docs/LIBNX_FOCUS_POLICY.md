# Focus policy and process suspension

Historical milestone after [foreground acquisition](LIBNX_FOREGROUND.md), followed by [NotifyRunning](LIBNX_NOTIFY_RUNNING.md). Reference NRO/sources remain unchanged.

## Contract

SelfController **13 SetFocusHandlingMode** accepts exactly three booleans from libnx:

| Bytes | Local policy |
| --- | --- |
| 00 00 01 | SuspendHomeSleep: suspend in Background, continue OutOfFocus |
| 01 01 00 | NoSuspend: continue even in Background |

Wrong length, SendPid or nonboolean bytes return InvalidArguments; other combinations return UnknownCommand; retired process returns InvalidState. Policy commits only after complete reply writing. These are local results, not exhaustive Horizon behavior. Notification-specific modes, command 16 SetOutOfFocusSuspendingEnabled and AlwaysSuspend are absent. No newer firmware is invented to skip dependencies.

## Focus and scheduling

States: **InFocus=1**, **OutOfFocus=2** (another applet), **Background=3** (HOME/sleep). GetCurrentFocusState returns them. Machine::setAppletFocusState drives transitions into the existing FocusStateChanged queue. Invalid values/full queues preserve state.

The boolean API remains InFocus/OutOfFocus: Windows focus loss does not automatically mean console HOME/sleep. No HOME UI is added; the host API drives Background.

When policy requires suspension, every process thread stops. Machine::run returns Waiting; repeated calls execute no instructions. Enabling suspension by IPC while Background stops immediately after the completed SVC, retaining its reply/post-SVC PC without replay on resume.

Kernel waits remain distinct. Virtual time/deadlines can progress while suspended, but cannot run threads until suspension ends. nextWakeDelay reports no runnable deadline during suspension, avoiding a host loop on expired waits. Existing scheduling consumes resolved waits on resume.

Background→OutOfFocus resumes SuspendHomeSleep; NoSuspend continues in Background. Exit/fault/reload retires active state; new loads have no installed policy. Hardware sleep, specialized resume notifications, multi-app arbitration and every Horizon mode are not modeled.

## Original execution

NRO SHA-256 stays `10F4CF1CB2CD69FCB75C8935CB39A9F84210114236688BB50F3495AF2409553F`. libnx acquires foreground and installs 00 00 01, then hits absent ApplicationFunctions **40 NotifyRunning**. Its wrapper directly aborts with **12633 / LibnxError_BadAppletNotifyRunning**.

Focused path: **7,244 instructions / 24 IPC exchanges**. These are below the previous 7,732/36 because this diagnostic bypasses old failure cleanup; this is neither a performance result nor regression. APM/applet handles remain in the diagnostic report, while emulator applet state retires on exit.

Initially OutOfFocus: wait after **7,178**, then **1,369 additional instructions** after focus. Main/constructors remain unreached.

applet_focus_policy_and_suspension reuses real SDK packets and checks invalid arguments/modes, failed replies, transitions, immediate background suspension, PC preservation, expired deadlines, both threads and reload. Domain tests retain explicit-close checks because the new native failure no longer emits them. Scheduler scenarios use development-memory test code; measured startup retains the original NRO.

**61 Debug/Release suites passed**. Next was a defined NotifyRunning state/reply, then following actual program dependencies. Guest GPU and native Switch 2 compatibility remain unvalidated.

## Reference links

- [libnx applet.h](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/services/applet.h)
- [libnx applet.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/applet.c)
