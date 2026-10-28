# IPC buffer transfers

Milestone dated October 25, 2026. emu:demo accepts HIPC A/B descriptors to transfer more than the 256-byte command's inline data. This remains a local development profile without native Switch 2 validation.

## Accepted subset

At most one **Send/A** and one **Receive/B**, Normal mode 0, without statics, exchange descriptors, receive list or special header. NonSecure/NonDevice modes are rejected. Initial CMIF/domain/handle limits follow [IPC](HLE_IPC.md).

Each descriptor is 12 bytes after HIPC. Full address/size fields, including upper bits, are decoded: truncation cannot turn an out-of-RAM address into a valid one. CMIF starts at the next 16-byte boundary. RawCount includes padding and must fit the 256-byte buffer.

Maximum is **64 KiB per descriptor**, including output capacity. Zero size does not dereference its address. Nonempty input must be entirely readable RAM; output entirely writable RAM. MMIO, forbidden-page crossings and RAM overflow fail. Output cannot overlap current IPC memory. Other process memory remains shared across threads.

This is a checked copy to a host service, not interprocess kernel mapping. Names, commands and service errors are project-specific.

## Command 2 Copy

emu:demo type-4 HIPC has one Send and one Receive; SFCI version 0, command 2, token 0, no inline arguments. It copies Send bytes into Receive, whose capacity must suffice. SFCO returns service result and u64 copied count. Unused output capacity is unchanged. Add/GetVirtualTime reject descriptors rather than ignore them.

Insufficient capacity or bad command arguments produce SFCO InvalidArguments without data copy. Bad descriptors/permissions are transport errors preserving both request/output. Additional module-511 errors: InvalidBuffer=0xDFF and TransferLimit=0xFFF.

## Transactional writes and overlap

Input bytes are captured before writing, so overlapping source/destination use the original input. Memory::writeRanges validates every range and allocates necessary pages before copying anything. IPC reply and output commit together; validation/allocation failure preserves both. Successful writes update exclusive-reservation versions.

General writeRanges applies overlapping ranges in order, but IPC rejects overlap between reply and output to avoid ambiguity. Host execution remains sequential without concurrent-access guarantees.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_ipc_buffers\program.elf 10000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

The demo fills/copies/checks **512 bytes**, exits after **5,677 instructions**, returns count 512 and final marker 1. Tests cover 64 KiB across pages, upper address/size bits, overlap, zero sizes, insufficient capacity, page/RAM boundaries, permissions, unsupported modes, CMIF alignment and atomic grouped output. Earlier commands remain tested.

[SM](HLE_SM.md), then [session controls/C client](HLE_CLIENT.md), followed. Native game services, domains and more buffer forms were outside this milestone. The parser/Copy command are project-owned; hipc.h was consulted October 25, 2026.

## Reference links

- [libnx hipc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/hipc.h)
