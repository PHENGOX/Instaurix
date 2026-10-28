# First execution of an external libnx component

On October 25, 2026, the emulator executed **unmodified** nx/source/runtime/dynamic.c from libnx revision `b156b4774fb2df7b93402169509266da7547fc4c`. Original source/license are in tests/programs/libnx_dynamic; reference.json records hashes/toolchain.

This validates a real startup component in a project ELF bench. Later stages add [NRO MOD0 entry](NRO_BOOTSTRAP.md), [original crt0/RELRO](LIBNX_CRT0.md) and [minimal original initialization to main](LIBNX_INIT.md). Full external hello-world/services remain separate. Historical Switch code does not establish native Switch 2 support.

| Case | Verified result | Instructions |
| --- | --- | --- |
| Valid MOD0, dirty BSS, RELA NONE/RELATIVE and RELR direct/bitmap | Cleared BSS, expected pointers, untouched non-targets | 2,353 |
| Invalid MOD0 signature | Early return, preserved memory | 2,142 |
| Unsupported relocation | Actual diagAbortWithResult, libnx BadReloc=857 | 2,253 |

Continuous and seven-instruction runs must match count/status/report; host checks resulting pointers directly. **51 Debug/Release suites passed** at this milestone; rebuilding reproduced the manifest hash.

The compiler exposed missing **LDPSW**. It now sign-extends both 32-bit words with 4-byte-scaled offset/pre/post forms. Tests cover signed limits, registers/flags, reserved encodings, equal destinations and out-of-range reads. Existing overlap/MMIO restrictions remain.

The bench supplies freestanding memset and a diagnostic exit trap, using explicit test SVC F000. Protection wrappers execute actual SVC 2/3, never fake success. LNY/RELRO extensions were not exercised here. Metadata is trusted and synthetic, not a validation of untrusted MOD0 files.

```powershell
.\tools\build_libnx_dynamic.ps1
cmake --build build --config Debug
ctest --test-dir build -C Debug -R "libnx_upstream_dynamic_runtime|cpu_pairs_and_compiled_program" --output-on-failure
```

Rebuilding needs Zig 0.13.0, verifies/downloads the pinned public archive, extracts headers and compares retained source against it. ELF/C headers come from Zig; no musl libc is linked. This is not a full devkitA64/newlib toolchain. Ordinary CTest uses supplied binaries without network access.

## NRO entry contract identified from pinned sources

- X0 points to 24-byte ConfigEntry records: u32 key/flags and two u64 values; key zero terminates.
- X1=UINT64_MAX; X30=loader return; SP is usable/aligned.
- Declared main handle, override heap, optional arguments and SVC masks must describe real resources.
- X0=0 selects envSetup's NSO path, so zeroed registers do not provide NRO startup.
- Guest __nx_dynamic runs before libnx initialization. The loader must validate tables/targets without applying relocations twice.

Subsequent NRO/bootstrap and minimal initialization implement bounded parts of this contract. This ELF bench does not simulate the full toolchain/default services.

## Reference links

- [libnx dynamic.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/dynamic.c)
- [libnx switch_crt0.s](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/switch_crt0.s)
- [libnx env.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/env.c)
- [Pinned external hello-world target](https://github.com/switchbrew/switch-examples/blob/669786898205b7beb25ff1731e72982e6d0397d3/graphics/printing/hello-world/source/main.c)
