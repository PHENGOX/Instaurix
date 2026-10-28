# Historical FIFO packet reference

See [the decoder policy](../../../docs/GPU_PACKET_DECODER.md) and its pinned
primary-source manifest. The project-owned harness constructs documented B06F
SET_REFERENCE and DMA_NOP packets in Nvmap-backed memory. Original libnx mapping,
channel and submission wrappers are unchanged. The driver now executes the
bounded stream through its [synchronous reference policy](../../../docs/GPU_DRIVER_REFERENCE_SUBMISSION.md).
Original Kickoff succeeds and clears the client queue. No fence advances or
frame is rendered. The harness's decode-only comment records its original
milestone and is retained to keep the NRO and its provenance reproducible.

Only around Kickoff, the saved client version is temporarily set to 4.0.0 to
select ioctl2, then restored. Rebuild with tools/build_libnx_gpu_packets.ps1
-Python <python executable>. Original nvchannel.c needs the existing pinned Arm
GNU compiler. Included licenses and reference.json record provenance.

The two 64-word report blocks follow the mapped-preflight reference. Words
39/40 now preserve the reference header and its argument; word 41 preserves the
DMA_NOP word after cleanup.
