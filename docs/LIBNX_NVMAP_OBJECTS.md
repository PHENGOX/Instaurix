# Unbound Nvmap objects

Historical milestone: [CPU backing](LIBNX_NVMAP_CPU_BACKING.md) now implements
the limited Alloc path that was unsupported when this milestone was recorded.

The explicit HLE reference profile now accepts the original libnx AutoSelect
read/write ioctl layout, including exactly aliased input/output buffers. Partial
overlap, inaccessible RAM, malformed descriptors and IPC-buffer overlap fail
before publication. The legacy A/B layout is also decoded but is not exercised
by the original SDK fixture.

`Create` (`0xC0080101`) records an unbound object with a process-monotonic ID,
owning device descriptor and page-aligned size. The local policy permits 64
objects and 8 MiB of logical sizes per service. These are reference limits,
not measured console limits. No physical or GPU memory is allocated.

`Free` (`0xC0180105`) removes an object belonging to the supplied device and
returns zero backing address, recorded size and zero flags. Stale IDs fail.
Closing a device drops its objects; cloned sessions share service state.
Reply and output publication precede object-table mutation.

The new `libnx_nvmap_cycle` fixture uses unchanged pinned libnx Create, Alloc
and Free client functions. It completes two Create/Free cycles. Alloc
(`0xC0200104`) remains an explicit unsupported command and leaves the supplied
CPU buffer unchanged. The fixture is a project harness, not a native Switch 2
application. Its pinned sources and licenses follow `reference.json` and the
adjacent license files.

Focused host checks cover malformed sizes, invalid descriptors, partial aliasing,
protected replies, object quota, successful reclamation and duplicate Free.
The next milestone is bounded CPU-memory binding for Alloc, followed by buffer
registration and an independently validated GPU path. No frame rendering or
commercial-game compatibility is demonstrated.

## Validation

All 79 CTest suites pass on Windows in Debug (50.48 s) and Release (11.18 s).
The final added logical-budget checks also pass in both configurations.
Strict reconstruction reproduces the new cycle NRO SHA-256
CD2EF23E9AEB4348142A81ECB56A3A1EEEB90AD864E9C72AE5D4D5227CFD3056.
All 15 NRO binaries with recorded hashes match their manifests.
The English README variants are identical. Linux execution was not performed
for this milestone.
