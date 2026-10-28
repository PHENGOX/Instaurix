# ARM64 ELF loader and memory permissions

Verified October 24, 2026. This loader serves standalone ARM64 tests, not Switch 2 games or a Linux environment.

```powershell
cmake --build build --config Debug
.\build\Debug\Instaurix.exe .\tests\programs\elf_sum\program.elf 500 --test-protocol
ctest --test-dir build -C Debug --output-on-failure
```

Reference: **94 instructions, X2=55, normal exit code 0**. The program reads initialized data/constants, checks zero BSS, calls C, uses the stack and writes a memory result. SVC #0xF000 is a local test protocol requiring --test-protocol. ELF alone supplies no system services.

## Accepted profile

ELF64 little-endian version 1, System V ABI without extensions, AArch64 ET_EXEC and zero architecture flags. Files are limited to 64 MiB with 1–128 program headers of 56 bytes; headers/tables must be complete.

PT_LOAD virtual address, file/memory size, alignment and permissions are checked. Segments load at file-specified addresses without relocation; remaining memory/BSS is zeroed. Section/symbol tables are unnecessary for execution. Entry is 4-byte aligned with its complete first instruction inside loaded executable code.

PT_NULL is ignored. PT_NOTE/PT_PHDR are metadata after file-range checks. PT_GNU_STACK is accepted only without execution permission or a requested stack larger than 64 KiB.

## Restrictions

PT_LOAD segments must be address-sorted, fit RAM and never share a page, even with equal permissions. Writable/executable segments are rejected. Unsupported forms include ET_DYN/PIE, dynamic interpreters, PT_DYNAMIC, TLS and extended header numbering. Note platform properties, dynamic linking, Linux arguments and thread-local memory are not implemented by this ELF loader. Arbitrary AArch64 ELF files can therefore be rejected or need missing instructions/services.

## Protected memory

R/W/X permissions apply to **4 KiB pages**. ELF flags are translated to internal permissions, not reused as the same mask. Unoccupied pages have no rights.

The final **64 KiB** of RAM form a non-executable R/W stack; the preceding page is inaccessible. Initial SP equals RAM end. Segments cannot overlap either reservation.

Instruction fetch checks X separately from data R: execute-only pages need not permit data reads. 1/2/4/8-byte reads/writes validate complete ranges across pages; LDP/STP validate both elements together. Memory→Bus→CPU errors preserve the faulting instruction's PC/registers, without architectural exception entry. Errors remain until clearError(); CPU execution clears them before each instruction.

Permissions cover page padding as well. Addresses are storage offsets, without MMU translation or process isolation. Existing development MMIO routing is not a Switch 2 hardware map.

## Transactional loading

The loader stages a temporary memory image. Rejected files preserve previous data, permissions and image metadata. Machine::load resets CPU/timer/devices only after success; successful loads discard old data, zero BSS and select ELF entry. Raw binaries still begin at zero.

ELF magic is content-detected. A case-insensitive .elf extension also requires valid ELF and never falls back silently. Other non-ELF files retain the historical raw mode: size multiple of four, bounded by RAM, with **R/W/X across all RAM** for legacy tests. Segment protection applies to ELF mode.

## Validation

At this milestone 11 CTest suites passed. Coverage includes compiler-produced ELF, independent synthetic segments, constants/data, cross-page BSS, stack/guard, const reads, execute-only pages, cross-page transfers, pairs, failed-load preservation and clean reload.

**102 invalid ELF variants** cover truncation, out-of-range tables/segments, unsupported architecture/version, overflow, alignment, invalid entry and page collisions. Legacy raw/C tests still pass. See [test ELF reproduction](../tests/programs/elf_sum/README.md).

The optional [reference HLE profile](HLE_RUNTIME.md) reserves heap and thread-local space, rejecting conflicts before replacing a session. Its initial three reference ELF services do not establish native Switch 2 compatibility.

## Reference links

- [System V ELF header](https://gabi.xinuos.com/elf/02-eheader.html)
- [System V ELF program loading](https://gabi.xinuos.com/elf/07-pheader.html)
- [Arm AArch64 ELF ABI](https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst)
