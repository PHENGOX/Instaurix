# Application identity, messages and focus

Historical milestone extending homebrew appletOE domains; see [foreground rights](LIBNX_FOREGROUND.md) and README for subsequent work. This is a historical Switch libnx reference, not native Switch 2 validation.

## Process identity

GetAppletResourceUserId returns a u64 allocated on successful process initialization, stable across sessions/domains/clones/threads. IDs increase monotonically during host execution without recycling or dependence on PC PID/time. Successful reload gets a new ID; failed loading preserves the earlier session.

Exit/fault retires active applet state, clears messages and disables its event. Historical identity remains available for diagnostics, while applet methods reject inactive state.

## Messages and focus

CommonStateGetter exposes a copied readable event, ReceiveMessage and GetCurrentFocusState. Multiple handles observe one event. The queue holds at most **32 messages**. Focus transitions enqueue **FocusStateChanged=15** and signal; consuming the last message clears the signal. Empty queues return AM NoMessages=0x680.

Unchanged focus generates no message; a full queue rejects transitions without changing focus. Unwritable replies publish no handle/consume no message. ClearEvent/ResetSignal cannot clear this service-managed event, and its read handle cannot SignalEvent.

A headless virtual application begins focused. Machine::setAppletFocus changes host state; the interactive window forwards real focus and keeps processing events during waits without deadlines. This local policy is not full Horizon applet management.

## Original wait and CPU counter

Original kernel/wait.c replaces the waitObjects trap. CNTPCT_EL0/CNTFRQ_EL0 share GetSystemTick's host-driven virtual **19.2 MHz** reference clock; instructions alone never advance it. Without a bound counter, CPU reads fail explicitly.

Starting unfocused lets the original client drain its notification and wait. Host focus wakes it: **7,178 instructions before waiting**, then **1,553** to the next blocker. Repeated unsatisfied waits execute no guest instructions.

## Result and validation

Initially focused startup executes **7,428 instructions / 35 IPC exchanges**, ending with libnx AM=16217. First missing method: WindowController **10 AcquireForegroundRights**; cleanup also requests an unsupported SelfController method. AudioController, DisplayController and DebugFunctions exist without functional methods. Main, native display/audio/GPU and games are unvalidated.

Adding original wait code changes the NRO hash to `10F4CF1CB2CD69FCB75C8935CB39A9F84210114236688BB50F3495AF2409553F`. libnx remains b156b4774fb2df7b93402169509266da7547fc4c; SDK sources remain original. Zig/Clang and documented dependencies are a partial reference build, not a complete SDK installation.

Dedicated tests: applet_identity_messages_and_native_wait, cpu_counter_and_virtual_clock, native_applet_domains_and_lifetimes, libnx_default_app_and_native_apm. **60 Debug/Release suites passed**; rebuilding reproduced this hash. Next was coherent foreground acquisition and subsequent original calls; graphics work follows observed interfaces.
