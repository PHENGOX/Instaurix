# Events and process resumption

Integration milestone dated October 24, 2026. CPU, memory and system calls cooperate on a reproducible ARM64 ELF. The opt-in --hle-reference profile does not run Switch 2 games.

| SVC | Input | Success output |
| --- | --- | --- |
| 0x45 CreateEvent | None | W0=0, W1=write handle, W2=read handle |
| 0x11 SignalEvent | W0=write handle | W0=0 |
| 0x12 ClearEvent | W0=either endpoint | W0=0 |
| 0x16 CloseHandle | W0=handle | W0=0 |
| 0x17 ResetSignal | W0=signaled read handle | W0=0 |
| 0x18 WaitSynchronization | X1=handle array, W2=count, X3=timeout | W0=0, W1=index |

These public SwitchBrew conventions were consulted October 24; they do not prove native Switch 2 identity.

Signals persist until ClearEvent/ResetSignal; waiting does not consume them. Typed handles enforce write-only signaling, read-only waiting/ResetSignal, and either endpoint for ClearEvent. ResetSignal on an unsignaled event returns InvalidState=0xFA01.

The table has **128 handles**. Event pairs allocate atomically or return OutOfHandles=0xD201. Closed IDs are never reused within a process even if slots are reused. Endpoints and waits retain shared object references, so closing one endpoint leaves the other valid.

WaitSynchronization validates the entire array, limited to **64 read handles**, in readable RAM. MMIO, overflow and forbidden-page crossings fail. It returns the first signaled index; errors preserve X1. Results include InvalidHandle=0xE401, InvalidPointer=0xE601, TimedOut=0xEA01 and TooManyHandles=0xEE01.

## Suspend/resume

Timeout zero polls; -1 waits infinitely. An unsatisfied wait returns Stop::Waiting with post-SVC context. Repeated run consumes no instructions until satisfied. Machine::signalEvent signals a write handle; next run installs result/index before the next instruction. Host calls are sequential without concurrency guarantees.

Empty arrays are allowed: immediate timeout or an infinite wait without a wake source in this subset. Successful reload resets handles/waits; failed loading preserves the session.

[Virtual time](HLE_TIME.md) later added positive durations without inventing an instruction-to-nanosecond conversion. Waits are per-thread; Stop::Waiting means no started thread can run, as described by the [scheduler](HLE_THREADS.md). Cancellation is absent.

hle_events_suspend_resume checks full ELF execution, FP/SIMD/TLS/stack preservation, errors, permissions, limits and unsupported durations. hle_events_waiting_cli checks the English waiting message. [Sample sources and rebuilding](../tests/programs/hle_events/README.md) accompany the demo. At this stage bounded IPC was the next milestone; later work is tracked separately.

## Reference links

- [SwitchBrew system calls](https://switchbrew.org/wiki/SVC)
