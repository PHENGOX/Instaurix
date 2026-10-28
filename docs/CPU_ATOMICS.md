# Atomic and exclusive operations

Milestone dated October 24, 2026. The CPU remains a partial AArch64 interpreter.

## Coverage

- Byte, halfword, W/X LDXR/LDAXR and STXR/STLXR.
- W/X pair LDXP/LDAXP and STXP/STLXP.
- Byte, halfword, W/X LDAR/STLR.
- CAS, SWP, LDADD, LDCLR, LDEOR, LDSET, LDSMAX, LDSMIN, LDUMAX and LDUMIN, in four widths and acquire/release variants. No-result aliases use ZR destinations.
- Immediate CLREX, common DMB/DSB domain/type options, and ISB SY.

Accesses are RAM-only, naturally aligned (to total width for pairs), with permissions checked before effects. W results are zero-extended; flags are preserved. Constrained-unpredictable exclusive overlaps, reserved combinations and CASP are rejected.

## Reservation model

Each CPU holds a local reservation outside the saved architectural context: address, size and the version of a 16-byte RAM granule. Versions belong to Memory and observe direct host writes, buffer copies and other CPUs sharing RAM. Any write, including the same value, invalidates that granule's reservation. Permission changes and reset invalidate it as well.

An exclusive store consumes the reservation, returning Wstatus=0 on success or 1 on failure. Address and size must match the load. CLREX, CPU reset, context restoration, SVC and execution/instruction errors clear reservations. captureContext alone does not. Context switching therefore cannot transfer a reservation between threads; a resumed guest loop may need to retry.

Execution is sequential without reordered/deferred accesses, so acquire/release and barriers need no additional memory queue. Atomic transfers are indivisible in this model. **Parallel host-thread execution is not guaranteed**: a future scheduler must serialize accesses or synchronize them properly. Two-CPU tests interleave steps on one host thread. This is not a Switch 2 multicore memory-model validation.

## Validation and reproduction

Clang 18.1.6 from Zig 0.13.0 assembles 206 instructions in tests/programs/atomic_vectors.S. tools/build_atomic_samples.ps1 invokes the assembler directly with LSE because this version's cc driver loses that assembly option.

cpu_atomic_tests checks mathematical expectations for widths, ordering variants, LSE and boundary CAS. It covers single/pair exclusives, simulated contention, same-value/ABA writes, host copies, adjacent granules, two CPUs, permissions, MMIO, ZR/SP, invalid encodings and a retry loop after forced contention. Encodings are independent; expected results here are not generated from a console or external execution engine.

```powershell
./tools/build_atomic_samples.ps1
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

[Initial FP/Advanced SIMD](CPU_FP_SIMD.md) and its context are also available, with partial coverage. CCMP/CCMN are supported; other integer families remain absent.

## Reference links

- [Arm A64 synchronization primitives](https://documentation-service.arm.com/static/68c223238a337a2bc6645c0a)
- [Arm A64 instruction reference](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab)
- [Arm LSE introduction](https://learn.arm.com/learning-paths/servers-and-cloud-computing/lse/intro/)
