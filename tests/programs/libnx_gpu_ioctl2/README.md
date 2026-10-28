# Original libnx nvIoctl2 transport reference

See [the transport policy and measurements](../../../docs/GPU_IOCTL2_TRANSPORT.md).
The project-owned harness calls original full GPU channel initialization,
AppendEntry and Kickoff. Its single descriptor contains a synthetic unmapped
GPU VA, not a real command stream. Submission remains an explicit error.

Only around Kickoff, the harness saves the unknown client HOS version, selects
4.0.0 via original hosversionSet to choose ioctl2, then restores the saved value.
This does not change the emulator's default profile or claim firmware support.

Rebuild with tools/build_libnx_gpu_ioctl2.ps1 -Python <python executable>.
Original nvchannel.c requires the pinned Arm GNU 14.3.1 compiler; remaining
units use the existing Zig/Clang builder. SDK bodies are unchanged. Included
licenses and reference.json record provenance and source/output hashes.

gpu_as_report retains two 32-word blocks. Words 0 through 25 follow the channel
initialization reference; 26 through 31 record AppendEntry result, descriptor,
entry count, Kickoff result, retained entry count and fence increment.
