# Windows host test diagnostics

Windows CTest commands now run through `host_test_runner`. Normal `ctest` usage automatically records each invocation under `build/diagnostics/<configuration>/<test>-<runner-pid>/`:

- `report.json`: test name, command, UTC start time, child PID, latest lifecycle stage, duration, native exit code in decimal and hexadecimal, process memory and system available RAM/commit.
- `output.log`: combined child stdout/stderr, also replayed to CTest when the child finishes.

The report is replaced atomically at launch, while running approximately once per second, and at completion. Memory is sampled approximately every 50 milliseconds. Peak working set also uses the Windows process peak counter; private memory and system minima are sampled and can miss brief spikes. Available commit is the system commit allowance, not free physical RAM. Command arguments and program output are stored locally, so inspect reports before sharing them.

Stages distinguish launching, running, completed, nonzero exit, native failure, timeout, launch failure and monitor setup/failure. A native-failure classification identifies an exceptional Windows status code, not its root cause. A program can also explicitly return such a code. An exit code alone cannot establish RAM exhaustion.

The wrapper uses a Windows job object with kill-on-close, assigns the suspended child before resuming it, and allows at most 240 seconds per invocation. If CTest terminates the wrapper earlier, the last running report and captured output remain and the child job is terminated. This reports the last host lifecycle stage; it does not record a guest PC, stack trace or crash dump. An interactive CRT dialog may appear as a timeout rather than a completed native failure.

Negative tests explicitly declare `EXPECT_FAILURE` through the CMake helper. Their ordinary nonzero exit is expected, but an exceptional status or monitor failure still fails the test. This avoids hiding a crash through CTest's `WILL_FAIL` inversion. Non-Windows builds retain direct CTest commands and their existing negative-test behavior.

`windows_host_diagnostic_capture` verifies argument quoting, normal output, ordinary nonzero exit, an actual deliberate native exception (`0xE0424242`), missing executable handling and negative-test classification. The probe has no large allocation and does not alter emulator code. Reports from the deliberate exception are expected self-test artifacts.

## Manual emulator diagnosis

CTest integration covers the Windows test suite. To monitor a separate CLI invocation, use the wrapper explicitly:

```powershell
.\build\Debug\host_test_runner.exe .\build\diagnostics\manual manual --expect-success .\build\Debug\Instaurix.exe .\program.bin 4
```

The test label must contain only letters, digits, underscore, dash or dot. Build with `BUILD_TESTING` enabled to obtain the runner. No network upload, registry-wide crash policy or large memory dump is configured.

The existing `legacy` test remains `RUN_SERIAL`. Its original 12 GiB virtual-memory case is unchanged; [sparse tracking](SPARSE_RESERVATION_TRACKING.md) reduced measured peak process RAM from approximately 6.05 GiB to 104 MiB. The earlier Debug CRT failure remains unattributed; a future report supplies evidence rather than assuming the same cause.

Validation on Windows: **110/110 CTest suites passed in Debug (52.95 seconds) and Release (12.25 seconds)** with the wrapper enabled. The captured Debug legacy report records a 108,568,576-byte peak working set (approximately 103.54 MiB), exit code zero, and completed stage. Linux was not run for this milestone.
