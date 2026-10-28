# Sparse reservation tracking

RAM backing was already allocated in 4 KiB pages on demand. Exclusive-access tracking previously allocated one 64-bit version for every 16 bytes of virtual RAM, including untouched memory. A 12 GiB address space therefore required 6 GiB for those counters alone.

Each virtual page now has a baseline version and an optional table of 256 granule versions. The 2 KiB table is allocated only when a write needs it. A newly allocated table inherits its page baseline, so writing one granule does not invalidate reservations on untouched granules. Version values are opaque invalidation tokens, not write counts.

Permission changes, discard and reset invalidate reservations through page baselines and release existing granule tables without allocating replacements. Reset also releases RAM backing. Tables and backing remain owned by the Memory instance and move with it. This does not introduce concurrent host CPU execution.

Multi-range writes validate all ranges and prepare backing and version tables before changing bytes or publishing versions. Allocation failure may leave zero-backed pages or baseline-filled tables allocated, but leaves guest data and reservation tokens unchanged. Scalar writes use the same path, including writes crossing a page boundary.

## Validation

The dedicated tracking suite covers cold pages, baseline inheritance, independent granules, same-value writes, cross-page writes, permission changes, reset, discard, partial final pages, moves, and injected allocation failures. CPU exclusive-access tests also cover two CPUs reserving separate granules before the first tracking allocation.

On the same Windows development machine, the unchanged legacy test containing a 12 GiB virtual-memory case passed with approximately 104 MiB peak working set, compared with approximately 6.05 GiB before this change. These are whole-process measurements, not a guarantee for game workloads. The earlier Debug CRT crash was not conclusively attributed to memory exhaustion.

Page tables still scale with virtual address-space size. Written pages additionally consume real backing and granule tables; this change does not remove the memory cost of populated RAM. The legacy suite remains isolated from other CTest processes.

No GPU execution, rendering or native Switch 2 compatibility is established by this change.

Full Windows validation: **108/108 CTest suites passed in Debug and Release**, including 8,817 CPU atomic checks and the unchanged legacy case. Debug completed in 59.64 seconds and Release in 11.96 seconds in this run.
