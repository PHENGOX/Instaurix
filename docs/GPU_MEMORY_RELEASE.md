# First bounded GPU memory-write operation

Follow-up: [modular channel dispatch](GPU_MODULAR_PROFILES.md) now hosts this interpreter and preserves its atomic memory/reply behavior. The historical reference-only helper remains available for its existing tests.

The historical synchronous reference channel now accepts a four-byte B06F semaphore release. A decoded command can therefore change guest RAM through GPU virtual-address translation. This is CPU interpretation of a GPU command in the explicit HLE reference profile; there is no host GPU rendering or native Switch 2 compatibility claim.

## Primary evidence and local scope

The already pinned [NVIDIA B06F header](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/host/clb06f.h) defines SEMAPHOREA/B/C/D at byte offsets `0x10/0x14/0x18/0x1C`, an eight-bit upper address, aligned lower address, full-width payload, RELEASE operation `2`, and the four-byte size bit at bit 24. Revision, hash and license provenance are retained in [the source manifest](GPU_PACKET_DECODER_SOURCES.json).

The local interpreter accepts exactly `0x01000002` for SEMAPHORED: four-byte release with WFI enabled. It models storing the payload little-endian at the assembled 40-bit GPU address. Sixteen-byte releases, waits/acquires, reductions, alternate control flags and nonzero subchannels remain unsupported. This establishes a bounded implementation of documented fields, not native hardware timing or cache coherence.

The constructed stream is synthetic and field-derived, not a console capture:

| Word | Purpose |
| --- | --- |
| `0x20040004` | Incrementing packet, four methods starting at byte `0x10` |
| `address >> 32` | Upper address |
| `address & 0xFFFFFFFF` | Lower aligned address |
| `payload` | Four-byte value |
| `0x01000002` | Release operation and size |

Address and payload registers must all be initialized within the current submission. Repeated releases may reuse those values within that submission. Semaphore registers do not persist across submissions; setup-only streams or unconsumed setup at the end are refused. SET_REFERENCE and DMA_NOP remain supported alongside releases. The plan admits at most 256 four-byte stores and retains the existing 64 KiB input cap.

## Atomic publication

`prepareReferenceExecution` stages reference state and an ordered list of memory effects without touching RAM. The driver validates every destination against the channel's bound address space, its Nvmap owner, the live object backing and current CPU write permissions. CPU pointers, unmapped targets and another Nvmap owner's objects are not interchangeable with owned GPU addresses. All four bytes must fit one mapping. Reserved address bits and unaligned lower addresses fail explicitly.

The driver resolves each valid destination to CPU RAM and stages its bytes. GPU stores cannot overlap the IPC reply or ioctl output. The complete IPC reply, ioctl output and GPU stores are passed to one `Memory::writeRanges` transaction, using at most 258 ranges. Backing and tracking storage are prepared before bytes change. A late bad destination, unsupported method, protected output or allocation failure leaves guest data, reservation tokens and committed channel state unchanged. Only after the memory transaction succeeds does the staged channel register/count become visible.

Stores retain stream order, including repeated writes to one address. Command snapshots are owned before execution, so writing into command backing cannot change the already prepared stream. Successful stores invalidate the existing CPU reservation granules through the normal Memory write path. No new event, syncpoint increment, completion fence, asynchronous job or guest query is invented.

## Validation and provenance

`gpu_four_byte_release_plan` checks field decoding, little-endian bytes, zero/full-width values, address limits, unsupported forms, incomplete setup, ordered effects, rollback and the exact 256-store bound.

`gpu_mapped_release_transaction` reaches the unchanged original libnx ioctl2 boundary, then injects synthetic release words into owned command backing and submits them through that transport. This is an instrumented host test, not a new unmodified guest application that generates semaphore commands itself. The original NRO, SDK sources and recorded hashes are unchanged.

Integration coverage includes exact bytes and neighboring guards, reservation invalidation, changed payload through a clone, late malformed/unsupported work, late unmapped destinations, read-only targets, reply overlap, self-modifying command backing, foreign Nvmap ownership, and immediate resource cleanup. Allocation injection checks both replies, cold GPU backing and destination tracking, preserving data and versions until the first complete attempt commits once. MSVC Debug injection exempts allocations matching its internal iterator-proxy size; Release covers the production path without that exemption. This does not promise recovery from every C++ runtime allocation failure.

The earlier [reference-only submission milestone](GPU_DRIVER_REFERENCE_SUBMISSION.md) remains a historical record. The first next rendering step still requires identifying an engine class, its data-transfer or clear methods, resources and expected image. This semaphore store alone does not implement copying images, shaders or drawing.

Windows validation: **113/113 CTest suites passed in Debug (58.88 seconds) and Release (12.47 seconds)**. The mapped-release fault test traversed 15 failed allocation boundaries per lifetime in Debug and the corresponding full production sequence in Release, including cold destination backing. All 37 recorded NRO SHA-256 values match. Linux was not run for this milestone.
