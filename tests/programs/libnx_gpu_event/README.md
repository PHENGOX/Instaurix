# Original libnx GPU error-event reference

See [the event policy and measurements](../../../docs/GPU_ERROR_EVENT.md).
The project-owned harness runs two original GPU channel/context lifetimes,
queries error event 3, attempts notifier enable/disable, performs a zero-time
eventWait and closes the event. Notifier configuration is still unsupported.
No GPU work executes and no error signal is generated.

Rebuild with `tools/build_libnx_gpu_event.ps1 -Python <python executable>`.
Original nvchannel.c requires the pinned Arm GNU 14.3.1 compiler; remaining
units use the existing Zig/Clang builder. SDK bodies are unchanged. License
texts and reference.json record provenance and source/binary hashes.

gpu_as_report retains two 32-word blocks. Words 0 through 21 match the context
reference; 22/23 record notifier enable/disable, 24 the zero-time wait, and
25/26 event-active state and read handle after Close.
