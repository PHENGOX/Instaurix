# GPU profiles, class registry and subchannel dispatch

Follow-up: [pitched 2D copying](GPU_MAXWELL_PITCHED_COPY.md) extends the existing handler with separate row strides and atomic row publication. The single-line guest producer is unchanged.

Follow-up: [bounded B0B5 DMA copying](GPU_MAXWELL_LINEAR_COPY.md) now implements the first copy-class handler. Earlier no-copy statements and validation counts below record the initial modular-dispatch milestone.

The GPU command path now separates hardware-profile selection, exact class identity, per-channel object bindings and class-specific handlers. Existing reference-register and four-byte semaphore behavior is preserved behind the shared channel-control interpreter. This milestone adds architecture and a minimal engine-dispatch proof, not DMA copying, 3D drawing or native Switch 2 compatibility.

## Registry and capabilities

`gpu_classes.cpp` contains an allocation-free compiled registry. Profile membership and implementation status are separate properties. Each executable engine class has an explicit handler pointer; declaring another class does not give it a generic fallback implementation.

| Class | Role | Status |
| --- | --- | --- |
| B06F / MAXWELL_CHANNEL_GPFIFO_A | Channel control | Existing bounded reference/semaphore commands |
| B197 / MAXWELL_B | Maxwell 3D | SET_OBJECT binding and zero-payload NO_OPERATION only |
| B0B5 / MAXWELL_DMA_COPY_A | Maxwell copy | Bounded linear, pitched 2D and restricted block-source conversion; see follow-up |
| C6B5 / AMPERE_DMA_COPY_A | Ampere copy A | Declared, no handler |
| C7B5 / AMPERE_DMA_COPY_B | Ampere copy B | Declared, no handler |
| C797 / AMPERE_B | Ampere 3D | Declared, no handler |

The Maxwell reference profile allows the three Maxwell class identities. The Ampere research profile lists the three Ampere candidates, but has no validated FIFO packet ABI or executable classes. It cannot borrow Maxwell execution by changing an ID. Compute, inline and 2D roles are represented in the type model; no unresearched class IDs or working implementations are invented for them.

Identities and field provenance come from the pinned NVIDIA headers linked in [the decoder manifest](GPU_PACKET_DECODER_SOURCES.json), including [B0B5](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/dma-copy/clb0b5.h), [C6B5](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/dma-copy/clc6b5.h), [C7B5](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/dma-copy/clc7b5.h) and [C797](https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clc797.h). These declarations do not identify the complete native Switch 2 interface.

## Execution and transaction boundaries

`gpu_channel.cpp` validates the profile, decodes the owned command stream once, and stages a complete `ChannelExecution`. Each FIFO owns a `ChannelState`: profile, shared channel-control reference register, and eight engine-class bindings. This is fixed-size state; registering classes adds no emulated RAM backing or external plugin dependency.

SET_OBJECT accepts the plain 16-bit class-ID form only. Unknown, disallowed, declared-only and channel-control classes cannot be bound as executable engine objects. Alternate ENGINE encodings are unsupported. A rejected binding leaves the previous association intact.

Host methods below byte 0x100 remain separate from engine methods. The existing subchannel-zero host-control profile is preserved, with subchannel six also admitted for the historical FIFO convention. Engine methods use the exact class selected for their subchannel. B197 delegates to `gpu_maxwell_3d.cpp`, which supports only byte method 0x100 with payload zero. The zero payload and admitted binding positions are local reference restrictions, not a complete hardware-validation model. No other 3D method is silently treated as a no-op.

Bindings are staged in the same transaction as reference changes, semaphore stores, both replies and the accepted-submission count. The driver publishes the new FIFO only after `Memory::writeRanges` succeeds. Late unsupported commands, malformed packets and allocation/reply failures preserve previous bindings and memory effects. NV clones share a channel's state; independent channels and fresh FIFO allocations do not. Close, successful reload, exit and fault retire it; failed reload preserves the live state.

## Selecting a profile

The default remains `maxwell-reference`. For an explicit selection:

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\libnx_gpu_packets\program.nro 500000 --hle-reference --homebrew-entry --gpu-profile maxwell-reference
```

`--gpu-profile ampere-research` selects the candidate catalog and prints that execution is unavailable. Historical NV FIFO initialization returns UnknownCommand before allocating a FIFO or fence; the original client's failure cleanup still runs. This option does not select a native Switch 2 kernel/service ABI. Unknown names, duplicate selections and use without the reference homebrew profile are rejected.

The host API also accepts a `Gpu::ProfileId` as the fourth Machine constructor argument. Selection is immutable for that machine and persists through staged reloads; switching profiles means constructing another machine. No live profile hot-swap is exposed.

## Verification and next step

Tests cover registry identities and capabilities, profile isolation, staged SET_OBJECT, persistent and independent subchannels, exact handler dispatch, unknown classes, invalid existing state, host/engine separation, combined semaphore effects, full rollback, original NV clone transport, allocation failure, protected replies and reset/reload/fault behavior. CLI tests exercise both valid profiles and malformed selections. The original 37 NRO fixtures are unchanged and no SDK sources are modified.

The new binding test injects field-derived commands at the original libnx submission boundary; it is not a claim that a complete unmodified deko3d application now runs. Only a minimal B197 no-op subset executes.

The next bounded implementation can add a separate B0B5 copy handler with class-owned transfer state and validated source reads. It must address staged data dependencies and overlap policy while retaining the existing reply/memory transaction. Ampere handlers and a validated packet ABI remain separate work, rather than aliases to the Maxwell handler.

Windows validation: **117/117 CTest suites passed in Debug (62.40 seconds) and Release (13.19 seconds)**, including class binding, allocation/reply rollback, profile lifecycle and CLI contracts. All 37 recorded NRO SHA-256 values match. The unmodified original packet reference remains at 77,756 guest instructions. Linux was not run for this milestone.
