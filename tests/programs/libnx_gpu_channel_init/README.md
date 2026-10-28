# Original full GPU channel-initialization reference

See [the priority policy and helper validation](../../../docs/GPU_CHANNEL_INITIALIZATION.md).
The project-owned harness initializes NV/AS/Nvmap, invokes unchanged original
nvGpuChannelCreate with Medium and High priority, polls its error event, then
uses unchanged nvGpuChannelClose and repeated Close. No submission is attempted.

Rebuild with tools/build_libnx_gpu_channel_init.ps1 -Python <python executable>.
Original nvchannel.c requires the pinned Arm GNU 14.3.1 compiler; other units
use the existing Zig/Clang builder. GPU channel helper bodies are unchanged.
Included license texts and reference.json record source/output provenance.

gpu_as_report retains two 32-word blocks; the detailed layout is documented in
the helper validation above. This is historical libnx channel initialization,
not validated complete GPU startup or native Switch 2 compatibility.
