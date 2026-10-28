# Synchronous GPU virtual-memory access

Follow-up: [original GPU channel lifecycle](GPU_CHANNEL_LIFECYCLE.md) now supports Open, SetNvmapFd and Close. Priority, address-space binding and command submission remain unsupported; earlier next-step statements below describe the preceding milestone.

The reference runtime now exposes `Process::readGpuMemory` and
`Process::writeGpuMemory` for future synchronous GPU consumers. These are host
interfaces, not new guest services or graphics commands.

Each access requires a live NV session handle, its initialized address-space
descriptor, a typed GPU virtual address and a nonempty byte span. The runtime
resolves the published mapping to its owning service's Nvmap object and then to
the CPU backing plus the object-relative offset. The supplied bus must belong to
that process. GPU virtual addresses are never treated as CPU pointers.

The entire span must fit within one mapping, one live object and accessible RAM.
An access crossing even two adjacent mappings is rejected. Reads require current
CPU read permission; writes require current CPU write permission. Validation
precedes copying, so invalid reads preserve the entire destination and invalid
writes preserve backing bytes. Memory's existing write path also preallocates
pages before mutation and updates CPU exclusive-reservation versions.

This is an immediate, coherent local reference policy: CPU writes are visible to
GPU reads and GPU writes to every alias and CPU reader. There are no deferred
jobs, cache simulation, synchronization commands or hardware coherency claims.
Unmap and address-space Close revoke access immediately. A future asynchronous
consumer must add explicit job ownership and backing retention; these APIs do
not grant a lease on a mapping snapshot.

## Validation

Windows validation: **94/94 CTest suites pass in Debug (70.97 seconds) and
Release (18.28 seconds)**. All 27 recorded NRO SHA-256 hashes match their manifests.
Linux was not run for this milestone. No additional suite was added; the existing
mapping integration suite now covers the memory-access behavior.

The existing original `libnx_gpu_map` NRO is unchanged. Host probes in
`libnx_gpu_map_tests` verify its CPU markers, second-page subrange, last word,
unaligned writes, clone/alias visibility, wrong endpoints/descriptors, foreign
RAM, empty spans, overflowing and cross-mapping accesses, page protections,
atomic rejection, Unmap and Close revocation. Original guest execution and
cleanup remain covered by the same integration suite.

No channel, GPFIFO, command decoder, shader, host GPU backend or native Switch 2
compatibility is demonstrated. The next bounded milestone is an audited channel
endpoint and lifecycle; submission must remain unsupported until its transport,
ownership and synchronization requirements have been implemented and tested.
