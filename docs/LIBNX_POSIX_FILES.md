# Guest libc open/read/close through original fsdev

A separate `tests/programs/libnx_posix` target now executes standard **open, read and close from its own guest main**. Calls pass through original newlib syscall connectors, original libsysbase descriptor management, original libnx fsdev callbacks and the existing native fsp-srv read-only implementation. No host stub replaces these library functions, and this milestone adds no CPU or HLE service behavior.

## Guest application

The application first runs the existing startup/main consistency checks, then opens `sdmc:/sample.txt` twice with O_RDONLY. It reads seven bytes and then the remaining 23 bytes through the first descriptor, checks EOF, and independently reads all 30 bytes through the second descriptor. Both buffers are checked against `Switch2Emu read-only fixture.` followed by LF.

It then performs a zero-length read, closes the first descriptor, checks EBADF on a subsequent read, reopens the same file into the freed descriptor slot and verifies that its new position starts at zero. Both live descriptors are closed. Further application checks exercise repeated close, a missing filename, write-only mode and invalid read-only append.

Descriptor allocation, file-structure allocation, locks, offsets, dispatch and release are the actual library implementations. With no configured SD device, the guest records the real failed open and returns normally without claiming that file I/O succeeded.

## Error behavior and limits

The pinned guest errno values observed are EBADF=9 for closed descriptors, EINVAL=22 for O_RDONLY|O_APPEND, and ENOSYS=88 when no device is registered. Missing files and rejected write modes currently become EIO=5 through the original fsdev fallback, because the native service still uses local reference result codes rather than a complete Horizon FS result mapping. The tests record this limitation rather than fabricate ENOENT or a success result.

This validates the selected guest libc path, not complete POSIX or stdio conformance. It does not validate fopen/fread, seeking, directory traversal/enumeration, arbitrary cwd, every invalid descriptor, writes, all errno translations, parallel threads or complete libc shutdown.

## Original sources and provenance

The target adds unchanged newlib syscalls/sysopen.c, sysread.c and sysclose.c, plus patched libsysbase open.c, read.c and handle_manager.c. It reuses the previously validated fsdev/libnx/newlib sources and the documented TLS compiler adaptation. The project-owned guest harness contains the application checks; SDK/library functions remain original.

Original licenses are retained in the fixture. [Additional source headers and hashes](licenses/POSIX_SOURCE_NOTICES.txt) accompany the new sources. reference.json records source, harness, symbol and NRO hashes, report fields and expected errno values.

## Results and validation

| Configuration | Instructions | Application status | Main result | Loader result |
| --- | --- | --- | --- | --- |
| No configured SD | 20,315 | 2: optional open failed | 77 | 0 |
| Read-only snapshot configured | 61,517 | 1: file checks completed | 77 | 0 |

The NRO is **372,736 bytes** with **16,384 bytes BSS**. Original startup/constructor/main report is at **0x67000**; the 32-word application I/O report is at **0x6A200**.

`libnx_guest_posix_open_read_close` executes both configurations without injected function calls or guest code patches. It verifies the application report, original constructor/main checks, absence of dependency diagnostics, and normal return through cleanup. Continuous execution and seven-instruction slices must match all 32 I/O fields and instruction totals. Earlier direct native-client and fsdev targets keep their own tests and binary hashes.

```powershell
.\tools\build_libnx_posix.ps1 -Python <python_path>
.\build\Debug\Instaurix.exe .\tests\programs\libnx_posix\program.nro 500000 --hle-reference --homebrew-entry --read-root .\tests\programs\hle_files\root
ctest --test-dir build -C Debug -R libnx_guest_posix_open_read_close --output-on-failure
```

## Next step

Use a broader libnx application to identify actual remaining library/service requirements, including its display and input path. The complete external switch-examples hello-world, guest GPU rendering, audio, controller samples and commercial games remain unvalidated. These historical Switch references do not prove native Switch 2 compatibility.

## Final validation

All **70 CTest suites pass in Debug and Release**. Strict reconstruction reproduces NRO SHA-256 `7900708F2340B2607830649B77F9E3E06E60E2D8D359AF39D40050E45BB9CAF9`. Earlier reference NRO hashes remain unchanged.


Subsequent milestone: [original VI client and the managed-layer blocker](LIBNX_VI.md).
