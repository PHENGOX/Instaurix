# Virtual time and finite waits

Milestone dated October 25, 2026. Positive-nanosecond SleepThread and WaitSynchronization durations use a deterministic virtual clock. This does not establish CPU timing or native Switch 2 behavior.

## Host-driven clock

Time starts at zero on load. Machine::advanceTime(ns) advances explicitly; process().virtualTime() reads it; process().nextWakeDelay() returns time until the next deadline, or no value if none exists. A resolved-but-not-resumed wait returns zero delay.

run(budget) never advances time and executes at most its instruction budget. Deadlines persist across calls. PC wall time, real sleeps and arbitrary instruction costs do not determine guest delays. The host may advance time while another thread is runnable.

The CLI advances idle periods to the next deadline and resumes with remaining instruction budget. Infinite-only waits are reported. A continuously runnable guest does not advance CLI virtual time; a future driver needs a policy for that case. This implements deadlines, not CPU calibration.

The clock is unsigned 64-bit ns. Overflowing host advances fail without effects; overflowing guest deadlines are explicitly unsupported. Exited/faulted machines reject advanceTime. Successful loads reset clock/deadlines; failed loads preserve them. The instruction-count MMIO timer remains separate and is not converted to nanoseconds.

## Guest conventions

| Call | Behavior |
| --- | --- |
| SleepThread 0x0B, X0>0 | Block until deadline; no result register written |
| SleepThread, X0=0 | Yield |
| WaitSynchronization 0x18, X3>0 | Wait for object or deadline; 0 on signal, TimedOut=0xEA01 on expiry |
| WaitSynchronization, X3=0 | Poll |
| WaitSynchronization, X3=-1 | Infinite wait |
| GetSystemTick 0x1E | X0=floored virtual 19.2 MHz counter |

Other negative durations remain unsupported. The [two-thread/one-core limits](HLE_THREADS.md) remain. Public conventions and Horizon reference frequency were consulted October 25; the frequency is not claimed as native Switch 2 validation. Direct ARM hardware-counter reads were not added by this milestone.

## Ordering

A resolved wait retains its result until resumption. A signal before expiration remains success after later clear/time advance. A recorded timeout remains TimedOut after later signal. At the exact deadline, sequential host-call order decides: advance first records timeout; signal first records success. Concurrent access is not guaranteed.

Timeout preserves X1; success writes the first signaled object's index. An empty handle array can wait for a finite timeout. Handle/pointer validation precedes registration. Thread-exit waits may have deadlines. Wakeup preserves post-SVC PC, stack, registers, FP/SIMD and TLS; fixed results release wait-object references and do not re-execute the SVC.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_time\program.elf 1000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

The demo exits after **43 instructions at 1,500 virtual ns**, with memory value **55**. Main waits 2,000 ns for an event; worker sleeps 1,000 ns, signals, sleeps another 500 ns and exits; main joins. A main timeout of 500 ns checks expiry followed by continued execution.

Tests cover before/at-deadline boundaries, signal/expiry ordering, signal-then-clear, runnable peers, empty/infinite waits, overflow, ticks and one-instruction slicing equivalence. [IPC](HLE_IPC.md) was subsequently added; buffer descriptors and SM were the next historical tasks. Cancellation, locks and time progression during computation remain separate work.

## Reference links

- [SwitchBrew system calls](https://switchbrew.org/wiki/SVC)
- [libnx svc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h)
