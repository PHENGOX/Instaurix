# C client packaged as static NRO0

Project-owned sample without Nintendo binaries. work.c comes from hle_client; start.S reserves a 0x80-byte header and extra BSS. Clang 18.1.6/Zig 0.13.0 builds the ELF, then tools/package_nro_reference.py packages it.

Rebuild: `./tools/build_nro_client.ps1 -Python <python_path>`. A matching ELF remains for execution comparison.

NRO: **12,288 file bytes / 12,288 BSS bytes**, **14,343 instructions**. Seven u64 report values from 0x12000 match ELF: **1,55,512,3505779141,0,virtual time,7**.

This MOD0-free sample uses the local HLE profile and validates neither libnx entry ABI nor native Switch 2 mode. See [loader](../../../docs/NRO_LOADER.md).

## Recorded artifact SHA-256

`A34BF47CD5EE27E274AB897609613CAF945568ADA0441C321A15FD41BC7E2092`

