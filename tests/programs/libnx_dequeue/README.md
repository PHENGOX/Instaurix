# Original libnx CPU-only slot cycle

The project harness registers the existing bounded 16x16 linear RGBA buffer,
then calls original nwindowDequeueBuffer/nwindowCancelBuffer twice. The SDK
requests metadata once, caches the requested-slot bit and returns empty fences.
Availability is signaled while free and cleared while acquired.

The original GCC buffer producer and original libnx function bodies are
unchanged. Linked fence sources satisfy the wrapper's optional wait branch;
the tested path provides an output fence and submits no GPU work.

Rebuild with tools/build_libnx_dequeue.ps1 after preparing the pinned Arm GNU
compiler. Normal host tests use the included NRO and symbols. reference.json
documents all dequeue_report fields and source hashes.
Normal execution is 134,129 instructions; no frame is produced.