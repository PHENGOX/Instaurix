# ARM64 ELF C demonstration

Checks segment loading/memory permissions, not a Switch 2 ABI.

| Range | Contents | Rights |
| --- | --- | --- |
| Page 0x10000 | Assembly entry and C function | R/X |
| Page 0x11000 | Expected constant 55 | R |
| Page 0x12000 | initial=10, BSS output/zero_probe | R/W |
| Final 64 KiB RAM | Stack | R/W |
| Page before stack | Guard | None |

start.S checks zero_probe, calls compute_sum and compares its result with the constant. **55** is written at **0x12008** and retained in **X2**, with restored SP. Opt-in local SVC F000 exits 0 on success, 1 on mismatch; it is not a console service.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\elf_sum\program.elf 500 --test-protocol
.\tools\build_arm64_samples.ps1
```

Verified: **94 instructions, X2=55, normal exit 0**. Zig 0.13.0 compiles sum.c/start.S and links separate PT_LOAD segments using link.ld. Exact options are in the script; normal emulator building downloads nothing. elf_loader_tests inspects code/data/BSS, forbidden accesses and invalid files. See [loader limits](../../../docs/ELF_LOADER.md) and [portable toolchain](../stack_sum/README.md).

## Recorded artifact SHA-256

`ff469000d206386e234079ddd10d73e34696df6075766098e33266a2dc2e7e2c`

