# Original libnx GPU error-notifier reference

See [the notifier policy and measurements](../../../docs/GPU_ERROR_NOTIFIER.md).
The project-owned harness runs two original GPU channel lifetimes, enables
the notifier, attempts medium priority, disables the notifier, polls and closes
the error event, then closes its channel. Priority is deliberately unsupported.
No GPU error or command completion is generated.

Rebuild with `tools/build_libnx_gpu_notifier.ps1 -Python <python executable>`.
Original nvchannel.c requires the pinned Arm GNU 14.3.1 compiler; other units
use the established Zig/Clang builder. SDK bodies remain unchanged. Included
licenses and reference.json record provenance and source/binary hashes.

gpu_as_report retains two 32-word blocks. Words 0 through 26 follow the event
reference; word 27 adds the SetPriority result.
