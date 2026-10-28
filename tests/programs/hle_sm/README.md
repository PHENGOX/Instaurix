# Service-manager demonstration

Project-owned ARM64 ELF, Clang 18.1.6/Zig 0.13.0, without Nintendo files. Rebuild: `./tools/build_hle_sm.ps1`; ELF layout follows hle_memory.

Opens sm:, sends RegisterClient with SendPid, requests the eight-byte emu:demo name and obtains a Move handle. Closes SM, computes/checks 20+35=55 through the acquired session, closes and exits.

**85 instructions**. At 0x12000: acquired service handle (+0), old SM handle (+4), result 55 (+8), validation marker 1 (+16), or 99 on failure. See [manager/PID/handle limits](../../../docs/HLE_SM.md). This remains a local demo service.

## Recorded artifact SHA-256

`4BB27A6CD9AEF3F5EFD2E02AD05F6DBCF9E05975D4E5381F1FAEF651CE8DAB48`

