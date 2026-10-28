# Local FIFO reference-method interpreter

The layer now also prepares [bounded semaphore memory releases](GPU_MEMORY_RELEASE.md). The reference-only helper continues to refuse memory-writing streams so callers cannot accidentally discard their effects.

The historical packet decoder now has a small host-side execution layer in `gpu_reference.cpp`. It accepts the same bounded owned command snapshots, decodes the whole stream, stages a local reference register, and commits only if every method is supported.

The pinned NVIDIA B06F header identifies `SET_REFERENCE` at byte offset `0x50` with a full 32-bit `COUNT` field. See the primary-source revision and hashes in [GPU_PACKET_DECODER_SOURCES.json](GPU_PACKET_DECODER_SOURCES.json). This provides the method identity and payload width. The local interpreter models assigning that payload to a reference register. It does not claim native hardware scheduling, synchronization or Switch 2 semantics.

The local profile permits only `SET_REFERENCE` on subchannel zero and exact DMA_NOP packets. Subchannel zero is a deliberate local restriction. All supported packet encodings can feed the interpreter; incremented successors must themselves be supported. Repeated reference writes execute in order across command ranges and the last value wins. Zero is a valid written value, distinguished from an untouched register by a separate boolean. DMA_NOP preserves both fields.

Unsupported methods or subchannels, malformed packets, unsupported control encodings, transfer limits and allocation failures leave the caller's previous register state unchanged. The complete decode precedes any commit. Independent caller-owned states remain independent.

The driver now invokes this layer through a [bounded synchronous submission policy](GPU_DRIVER_REFERENCE_SUBMISSION.md). The original supported Kickoff calls succeed; unsupported work remains refused. The register is not a completion fence or a native syncpoint. No GPU image, shader, Vulkan backend or commercial-game compatibility is demonstrated.

`gpu_local_reference_execution` checks the documented reference words, all supported packet modes, full-width and zero values, DMA_NOP, cross-range ordering, independent states, and rollback after late unknown methods, nonzero subchannels, truncation and unsupported controls.

## Driver integration

The driver stages per-channel state, preserves the no-increment fence, and publishes only after the IPC and ioctl replies are written together. Ownership, rollback and lifecycle tests are documented in the linked submission milestone. This remains an explicitly local policy.

Windows validation: **110/110 CTest suites passed in Debug and Release**, including the new local interpreter suite. Guest reference binaries were not rebuilt or modified for this milestone.
