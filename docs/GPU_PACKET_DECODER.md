# Historical NVIDIA FIFO packet decoding

This milestone adds a bounded structural decoder after mapped-memory preflight.
It does not admit a job, execute a method or acknowledge GPU completion.

Current driver follow-up: [synchronous reference submissions](GPU_DRIVER_REFERENCE_SUBMISSION.md)
now accept the supported original stream. Submission refusals and measurements
below describe the historical decoder-only milestone.

Follow-up: a separate [local reference-method interpreter](GPU_REFERENCE_EXECUTION.md)
now executes SET_REFERENCE against caller-owned host state. The guest driver
still refuses submissions; the decoder itself remains structural.

## Pinned primary evidence

The exact revisions, URLs and SHA-256 values are in
[GPU_PACKET_DECODER_SOURCES.json](GPU_PACKET_DECODER_SOURCES.json).

- NVIDIA open-gpu-doc revision 9fdf5c4062007929d9f4e6cbad9c9771fe61b880:
  classes/host/clb06f.h defines historical B06F packet fields, DMA_NOP and
  SET_REFERENCE at byte offset 0x50. Its GPFIFO entry length is high-word bits
  30:10; high-word bit 31 is a separate synchronization bit.
- NVIDIA open-gpu-kernel-modules revision
  ca2d03e6fafd4d4b56b383497d931c25b283bfcf: uvm_push_macros.h and
  nvidia-push-methods.h demonstrate byte method addresses shifted right by two
  when constructing packet headers. Those producer examples use the later
  C46F class; they are supporting unit evidence, not a native Switch 2 trace.
- NVIDIA clb197.h identifies B197 as MAXWELL_B. The current libnx reference
  remains historical and does not establish native Switch 2 command semantics.

Source licenses are retained in the reference fixture and third-party notices.
The reference stream is constructed from documented fields, not captured from
a console, game, firmware or proprietary command dump.

## Supported grammar

The decoder reads little-endian words from owned preflight snapshots and emits
method tuples containing buffer index, header-word offset, subchannel, byte
method address and value. The bounded profile supports incrementing (opcode 1),
non-incrementing (3), immediate (4) and increment-once (5) packet forms. Immediate
data comes from the header; other forms consume their declared payload words.
Incrementing advances addresses by four bytes. Increment-once advances after
the first value and then holds the second address in this reference profile.

An exact zero word outside a payload is DMA_NOP and is counted separately.
Legacy/other opcode groups, including end-segment control, remain Unsupported.
Unknown method addresses may be structurally decoded; this is not evidence
that their semantics are supported.

Known packet forms reject header bit 12, zero payload count, truncated data and
method-address overflow. Each packet must fit one GPFIFO command range; crossing
between ranges is not supported by this initial profile. Total input is limited
to 64 KiB. Empty, non-word-aligned or oversized snapshots fail explicitly.
The output trace is staged and published only on complete success, so even a
late malformed or unsupported packet preserves the prior output trace.

Manufacturer evidence also refines preflight: descriptor length uses bits
42 through 62, and sync bit 63 is rejected explicitly rather than treated as
length. Privilege/level control bits 40/41 remain outside the local profile.

## First identifiable method

The reference words are:

| Word | Meaning |
| --- | --- |
| 0x20010014 | Incrementing packet, one payload, subchannel zero, word method index 0x14 |
| 0x12345678 | SET_REFERENCE argument; byte method offset is 0x14 times 4 = 0x50 |
| 0x00000000 | DMA_NOP outside the preceding payload |

The decoder emits one method tuple for SET_REFERENCE with value 0x12345678,
plus one DMA_NOP. It does **not** write a reference register. Driver handling
maps malformed known packets to InvalidArguments, unsupported formats to
UnknownCommand, and successfully decoded streams still to UnknownCommand.
There is no partial execution, persistent method state or job admission.

## Original transport reference and tests

Windows validation: **107/107 CTest suites pass in a serial Debug run
(108.05 seconds) and in Release (17.71 seconds)**. All 37 recorded NRO hashes
match; the preceding 36 binaries are unchanged. A strict second SDK build
reproduces the new NRO hash. Linux was not run for this milestone.

The initial concurrent Debug run had one diagnostic-free failure in the existing
legacy suite. Its unchanged executable passed alone, then the complete serial
run passed. The legacy 12 GiB virtual-memory test allocates substantial tracking
metadata; CTest now marks that suite RUN_SERIAL to reduce concurrent pressure.
No legacy assertion or CPU/memory implementation was weakened or changed.

Later work replaced that dense tracking allocation with
[sparse reservation metadata](SPARSE_RESERVATION_TRACKING.md) and added
[automatic Windows test diagnostics](HOST_DIAGNOSTICS.md). The paragraph above
records the original milestone's behavior, not the current memory cost.

libnx_gpu_packets constructs the documented words in mapped Nvmap memory and
uses unchanged original libnx channel initialization, mapping, AppendEntry and
Kickoff functions. The existing temporary client-version override selects ioctl2
around Kickoff only, then restores the prior version before cleanup. SDK bodies
remain unchanged. The two lifetimes finish in 77,630 instructions, with original
client entries and fences preserved and no rendered frame.

NRO SHA-256:
`69EF1BD8220547A996EBDDF497AB57060E87DE57BF69F3E13A0867F7A8B00D3B`.

The decoder suite independently checks golden packet words, all supported
packet forms, immediate zero values, subchannels, method-address units, last
address handling, truncation, unsupported groups, segment boundaries, aggregate
limits and complete trace rollback. Integration tests inspect the actual mapped
stream, verify changed payloads decode from fresh snapshots, and exercise driver
errors for truncation, late malformed packets, reserved header bits, unsupported
opcodes and GPFIFO synchronization. They also verify original cleanup, unchanged
data/fences and normal versus seven-instruction execution slices.

## Next bounded milestone

Specify a transactional execution model for a first FIFO method such as
SET_REFERENCE, together with honest submission/reply and fence semantics.
Rendering, 3D methods, shader execution and native Switch 2 compatibility remain
separate work. Recognizing a documented packet is not executing it.
