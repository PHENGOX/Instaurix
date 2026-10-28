# Original libnx channel reference

See [the lifecycle policy](../../../docs/GPU_CHANNEL_LIFECYCLE.md).
The project-owned harness runs unchanged original NV, Nvmap and channel SDK
functions twice. It uses the original default application startup and cleanup,
with a 64 KiB NV transfer-memory override. Priority is intentionally unsupported.

Rebuild with `tools/build_libnx_gpu_channel.ps1 -Python <python executable>`.
The original `nvchannel.c` translation unit additionally needs the pinned
Arm GNU 14.3.1 compiler prepared by `tools/prepare_arm_gnu_reference.ps1`.
The SDK license texts are included beside this fixture. No commands are submitted.
