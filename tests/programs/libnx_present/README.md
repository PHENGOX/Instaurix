# Original libnx CPU presentation

The project harness uses original nwindowDequeueBuffer/nwindowQueueBuffer to
present a gradient and its complement from the bounded 16x16 RGBA CPU buffer.
It overwrites the backing after both queues, then performs original buffer,
map, service and window cleanup. This checks immutable frame snapshots.

The original full GCC producer and original SDK bodies are unchanged.
Normal execution is 143,267 instructions with exactly two RGB frames.
present_report records each dequeue result, slot, queue result, current slot,
availability wait, default dimensions, requested mask and overwritten backing.
Full field names, source hashes and transport sizes are in reference.json.

Rebuild with tools/build_libnx_present.ps1 after the pinned Arm GNU preparation.
Normal host tests use the included NRO. The host test's optional fifth argument
is a PNG export path; no PNG is written during the usual CTest run.
No GPU commands, hardware fences, vsync or native Switch 2 compatibility are
demonstrated. This is not execution of framebufferCreate.