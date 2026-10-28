# Local two-thread scheduler

Milestone dated October 25, 2026. A project-owned ARM64 ELF can create/start a second thread, wait for its event and join its exit. This development profile does not validate native Switch 2 compatibility.

| SVC | Inputs | Result |
| --- | --- | --- |
| 0x08 CreateThread | X1=entry, X2=argument, X3=stack top, W4=priority, W5=core | W0=result, W1=handle on success |
| 0x09 StartThread | W0=thread handle | W0=result |
| 0x0A ExitThread | None | Exits current thread only |
| 0x0B SleepThread | X0=0 or positive ns | Yields or waits for virtual deadline |

Conventions were studied in public SwitchBrew/libnx sources on October 25; scheduling policy and addresses are local.

## Limits and initial state

Two threads total: main and one worker created once per process lifetime. The worker stays suspended until StartThread. Further creation returns OutOfResource even after worker exit. Closing a handle neither terminates a thread nor restores quota. Repeated start returns InvalidState; an event handle yields InvalidHandle.

Entry must be 4-byte-aligned executable RAM. Requested SP must be **0xC10000**, the top of the local 64 KiB stack **[0xC00000,0xC10000)**. Adjacent pages are forbidden; the stack is R/W non-executable. Worker TLS page is **0xC11000**, with local region **0xC11200** in this initial profile. HLE loading reserves these areas and rejects overlapping ELF segments. Threads share process memory.

Only priority **32**, core **0** or default core **-2** are supported; other choices explicitly stop as unsupported. Console priorities, affinity, capabilities and limits are not reproduced.

Worker context starts zero except PC, SP, X0=argument and TPIDRRO_EL0=local region. It must call ExitThread; no SDK return trampoline is supplied. Integer/FP/SIMD/FPCR/FPSR/TLS context is preserved. Context restoration invalidates CPU exclusive reservations.

## Scheduling

One interpreted CPU runs ready threads round-robin with a **64-executed-instruction quantum**, retained across host run calls so budget slicing does not change ordering. Wait, exit and yield schedule immediately. Successful SignalEvent also schedules immediately to resolve waits before a following ClearEvent.

Wait state belongs to each thread. Stop::Waiting means no started thread is runnable; repeated run consumes no instructions until wakeup. One signal can wake both threads; the event remains signaled until explicitly cleared.

Thread handles support WaitSynchronization and become permanently signaled on exit, but cannot SignalEvent/ClearEvent/ResetSignal. Waits retain object references after handle close.

ExitProcess terminates all threads. ExitThread affects only current; normal process completion waits for all created threads to exit. A created-but-never-started thread can leave the process waiting. CPU faults or unsupported calls stop the entire process. Successful loading resets contexts/waits/quantum; failed loading preserves them.

[Virtual time](HLE_TIME.md) supports finite waits. Cancellation, locks, thread-slot reuse and multicore execution remain outside this milestone. Host calls are sequential, without concurrent-access guarantees.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_threads\program.elf 2000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

The demo exits after **367 instructions** with shared-memory value **55**. Main waits first for the worker event, then its exit. Tests include a non-yielding main loop, both threads blocked and host-woken, one-instruction budget equivalence, context isolation and argument errors.

At this historical point the next task was bounded IPC; later progress is in README. CPU extensions follow measured blockers, and the guest GPU remains absent.

## Reference links

- [SwitchBrew system calls](https://switchbrew.org/wiki/SVC#CreateThread)
- [libnx svc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h)
