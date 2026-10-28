# Original libnx graphics-context reference

See [the policy and measurements](../../../docs/GPU_GRAPHICS_CONTEXT.md).
The project-owned harness runs two original NV/AS/Nvmap/channel lifetimes,
allocates GPFIFO and a historical graphics context, then queries GPU error
event 3. The event query is deliberately unsupported and creates no handle.
No commands are submitted or executed.

Rebuild with `tools/build_libnx_gpu_context.ps1 -Python <python executable>`.
Original nvchannel.c needs the pinned Arm GNU 14.3.1 compiler; remaining units
use the established Zig/Clang builder. SDK bodies are unchanged. Included
licenses and reference.json record provenance, source and output hashes.

gpu_as_report retains two 32-word blocks; words 0 through 18 follow the GPFIFO
reference, 19 records QueryEvent, 20 event-active state and 21 its read handle.
