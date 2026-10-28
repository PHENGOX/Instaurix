# Bounded Maxwell linear DMA copy

Follow-up: [pitched 2D copying](GPU_MAXWELL_PITCHED_COPY.md) now adds explicit
multiline launch and row geometry. Single-line behavior is retained; earlier
multiline refusals and validation totals below describe the original milestone.

The Maxwell reference profile now has an executable B0B5 copy-class handler. A synthetic command stream can bind the copy engine, configure its registers and copy bytes between owned GPU mappings through the existing original libnx ioctl2 transport. This remains a CPU-interpreted historical GPU reference, not a native Switch 2 compatibility or rendering claim.

## Primary evidence

The pinned [NVIDIA B0B5 header](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/dma-copy/clb0b5.h) names MAXWELL_DMA_COPY_A and defines LAUNCH_DMA, source/destination addresses and length. The selected fields use 40-bit data addresses. The public [deko3d buffer-copy producer](https://raw.githubusercontent.com/devkitPro/deko3d/master/source/maxwell/gpu_transfer.cpp) programs addresses and byte length, then launches a non-pipelined, flushed, pitch-to-pitch transfer. Its copy subchannel is four, defined in [helpers.h](https://raw.githubusercontent.com/devkitPro/deko3d/master/source/maxwell/helpers.h).

These are public header definitions and an original Switch producer example, not a Switch 2 trace. The deko3d sources are reference evidence only; no library source is copied into the implementation.

## Admitted local profile

| Byte method | Local effect |
| --- | --- |
| 0x400 / 0x404 | Source upper / lower address |
| 0x408 / 0x40C | Destination upper / lower address |
| 0x418 | Transfer length in bytes |
| 0x300 | Launch with exactly 0x186 |
| 0x100 | Zero-payload copy-engine NOP |

Launch value 0x186 selects non-pipelined transfer, flush, source pitch and destination pitch. Multiline, remap, physical-address, alternate flush, interrupt and semaphore-launch variants remain unsupported. Accepting the flush bit models completion in the synchronous coherent-RAM interpreter; no hardware cache implementation is claimed.

All five address/length registers must be initialized before launch. Register banks are owned by each channel/subchannel and persist across successful submissions. SET_OBJECT rebinding clears that slot's copy registers as an explicit local policy. A launch captures its addresses and length immediately, so later register updates do not alter the prepared transfer.

The local transfer limit is 64 KiB, independent of the 64-byte golden test. Byte addresses and odd lengths are admitted. Zero-length launches are refused; the original producer avoids emitting work for zero size. One copy is allowed per submission. Two launches, or a mixture of DMA copying and semaphore stores, are refused in full until ordered staged data dependencies are implemented. Reference-register changes and engine bindings can accompany a copy.

## Memory and publication

The handler emits a CopyRequest without reading or changing RAM. The driver validates complete source and destination spans against current mappings, object boundaries, the channel's Nvmap owner, RAM permissions and bus ownership. Source needs read permission; destination needs write permission. CPU pointers are not GPU addresses.

Overlapping GPU ranges are refused, and resolved CPU ranges are checked again so aliases cannot hide overlap. Both source and destination are forbidden from overlapping the IPC or ioctl reply ranges. The complete source is copied into owned temporary storage only after validation.

Destination bytes, IPC reply and ioctl output are committed through one Memory::writeRanges transaction. Copy registers, bindings, reference state and submission count are published only after that transaction succeeds. Refused copies preserve destination data/versions, source data and prior channel state; an ordinary error can still publish an error reply. If allocation fails during the joint memory publication, both reply ranges and destination bytes remain unchanged. Earlier allocation failures may return a transport error or an error reply, with no DMA effect. Temporary allocation may still occur before failure, as in the existing sparse-memory policy.

No copy data, mapping reference or asynchronous job survives the request. Future launches resolve their addresses again. No fence increment or event signal is invented. Close, live reload, exit and fault retire copy registers; a failed reload preserves them.

## Validation boundary

The pure plan suite checks exact fields, persistent registers, post-launch register changes, unknown classes/methods, incomplete setup, overlap, byte addresses, limits, and full output-plan rollback.

The mapped integration suite injects field-derived copy words at the unchanged original SDK submission boundary. It checks every byte of a 64-byte pattern, source and guard preservation, destination reservation invalidation, cloned sessions, odd/unaligned spans, bad and crossing mappings, CPU-pointer rejection, read-only/inaccessible memory, reply collisions, late errors, multiple-launch refusal, physical aliases, unmapped aliases and foreign Nvmap ownership on both source and destination. Fault injection includes source capture, reply construction, cold destination backing/tracking and exact-once commit. Register lifecycle is checked across failed/successful reload and exit/fault. The existing MSVC Debug iterator-proxy-size exemption remains; Release has no such exemption.

This milestone used host-injected test data. A later [guest-generated copy reference](LIBNX_GUEST_GPU_COPY.md) now constructs and verifies copies inside a new project-owned ARM64 NRO through unchanged libnx wrappers, without normal-run host injection. The 37 pre-existing NROs and SDK sources remain unchanged. Larger workloads still need staged copy dependencies, additional launch modes and eventually image-layout support. Shaders, drawing and native Ampere execution remain separate work.

Windows validation: **119/119 CTest suites passed in Debug (60.57 seconds) and Release (13.62 seconds)**. All 37 recorded NRO SHA-256 values match. The original uninstrumented packet fixture remains at 77,756 guest instructions. Linux was not executed for this milestone.
