# Original libnx initialization to main

On October 25, 2026, a minimal NRO reached main through original __libnx_init. Zig 0.13.0/Clang 18.1.6 compiles official newlib sources plus the devkitA64 patch, avoiding unavailable binary packages.

## Actual startup

Pinned libnx b156b4774fb2df7b93402169509266da7547fc4c sources remain unchanged: crt0, dynamic, init, env, newlibSetup, virtmemSetup, heap/thread initialization, argvSetup and exit, with original svc.s wrappers.

The path performs relocations/RELRO; reads ConfigEntry through envSetup; initializes ThreadVars/copies TLS; reads memory regions with virtmemSetup; uses the real override heap; initializes main thread/stack with an uncontended libnx mutex; creates empty argv; runs a minimal app hook; executes original newlib __libc_init_array and a constructor; calls project main/cleanup; returns through original libnx exit.

**This target requests no default services.** Weak __appInit/__appExit hooks choose an application without console/applet/controllers/files; they do not replace library initialization. Project main is not external hello-world.

The exit bridge records main=77 then calls __libnx_exit. This revision returns loader result=0, checked separately. Full libc shutdown/stdio flushing are not exercised. Out-of-profile smAddOverrideHandle/nxlinkSetup are explicit traps, never fake successes, and are not reached.

## Emulator corrections

Homebrew TPIDRRO_EL0 previously used TLR+0x200 from older benches. libnx expects the TLR page base. Homebrew main/worker now use that base; older profiles retain their conventions. ThreadVars fields are TLR+0x1E0 magic, +0x1E4 handle, +0x1E8 thread pointer, +0x1F0 reent and +0x1F8 TLS pointer. Homebrew IPC uses that same canonical TLR base. Tests inspect actual guest writes.

### Bounded GetInfo, SVC 0x29

Only virtmemSetup information with current-process pseudo-handle and sub-ID zero is supported:

| IDs | Local information |
| --- | --- |
| 2,3 | Alias reservation base 0xA00000, size 0x200000 |
| 4,5 | Heap base 0x200000, capacity 0x800000 |
| 12,13 | Address interval from 0x10000 to end of 16 MiB RAM |
| 14,15 | Main stack base 0xFF0000, size 0x10000 |
| 28 | Additional alias size zero |

Loader conflicts with the advertised alias reservation are rejected. This free virtual interval does not implement alias-creation SVCs. Unknown IDs stay unsupported; wrong handles/sub-IDs return errors without invented values. These values do not describe real console memory.

### Encountered MOVI mask form

The compiler emitted byte-mask immediate MOVI for D/V.2D. This subset is implemented, not all MOVI/MVNI. Unicorn 2.1.4 covers 256 masks × two widths × destinations 0/31 = **1,024 independent cases**, checking target bits, D upper-half clearing and preservation of other registers/flags.

## Results

The **81,920-byte target** executes **1,660 instructions**. Report at **0x23000** records appInit/constructor/main/appExit flags=15, ThreadVars, heap bounds, initialized TLS and main=77. No dependency trap executes. Seven-instruction slices match.

build/libnx-init-trace.csv records **13 calls**: SVC 2/3, nine GetInfo IDs 2,3,28,4,5,12,13,14,15, stack QueryMemory, and local F001. Tests check order, GetInfo errors and alias-reservation load conflicts. **55 Debug/Release suites passed**; refreshed-source reconstruction reproduced the recorded NRO hash.

```powershell
.\tools\build_libnx_init.ps1 -Python <python_path>
ctest --test-dir build -C Debug -R "libnx_original_init_to_main|cpu_movi_mask_independent_reference" --output-on-failure
```

## Provenance and limits

Preparation verifies newlib 4.6.0.20260123 and official newlib-4.6.0.20260123-4.patch from devkitA64_r29.2, applies it inside the project cache and compiles original sources. Refreshing from the verified archive rejects a modified cache that no longer matches the patch. Manifest URLs/hashes/options/expected results and licenses/original constructor-init source accompany the bench.

This is not installed devkitA64 GCC or fully configured libc. Minimal configuration uses __SWITCH__, _HAVE_INITFINI_ARRAY=1, default reent and Clang GNU compatibility. Linked newlib provides reent/stdio data, constructor iteration, ctype and elementary string/memory functions. printf, malloc, files and pthread behavior are not claimed by this milestone. The official patch emits a warning for an unused inline condition-variable function without a return; sources were not changed to hide it.

[Original SM initialization/cloning/closure](LIBNX_SM.md) followed. Full toolchain/default services and hello-world remain distinct targets. Minimal historical Switch startup is not native Switch 2 compatibility.

## Reference links

- [libnx init.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/init.c)
- [Official newlib sources](https://sourceware.org/newlib/)
- [Official devkitA64 newlib patch](https://github.com/devkitPro/buildscripts/blob/devkitA64_r29.2/patches/newlib-4.6.0.20260123-4.patch)
