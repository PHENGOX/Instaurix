# Guest newlib environment and real allocation

Historical milestone after [original time/UTC](LIBNX_TIME_CLOCKS.md), followed by [filesystem initialization](LIBNX_FSP_INITIALIZATION.md). It removes the setenv trap and links original newlib 4.6.0.20260123 environment/allocator sources plus libsysbase from official devkitA64_r29.2.

## Executed code

Original __libnx_init_time calls real setenv. setenv/getenv/unsetenv and reentrant variants manage a NULL-terminated environ table with copied strings in guest heap. **TZ=<UTC>-00:00:00** is actually stored and read back through original getenv.

Original newlib dlmalloc (_LIBC=1), realloc and free supply storage. Original libsysbase _sbrk_r uses fake_heap_start/fake_heap_end established by libnx from the heap override, initially **2 MiB**. No host malloc/setenv substitutes return fake success.

Environment/allocator locks pass through original syscall_support, libnx callbacks and actual RMutex. Tests observe recursive depth **2** for each, main-thread ownership, and release after success/errors. Variables remain in guest RAM without changing PC environment. Complete POSIX/newlib edge-case or multithread conformance is not claimed.

## Construction and observation

Third-party sources remain unchanged. A minimal explicit libsysbase config.h supplements reference headers. This is selected-source Zig/Clang compilation, not full devkitA64 installation.

The linker retains test libc entry points. tools/extract_reference_symbols.py initially recorded fourteen function/data addresses from ELF into symbols.txt. This is test metadata, not an emulator symbol resolver; later milestones add symbols.

At this stage tests pause before the missing fsInitialize trap, then call original ARM64 functions in the initialized machine, checking return values, stack and callee-saved registers. Guest environment/heap persist between calls; no host C++ function replaces them. Diagnostic report slot 22 exposes environ. SDK __appInit/__appExit remain original. A replay buffer formerly at a fixed address moved into reserved heap for automatic section placement.

## Targeted validation

original_newlib_environment_and_allocator checks original TZ/environ heap storage; overwrite=0 without break growth; new variables and copied arguments; shorter/longer replacement; table growth over 24 variables; retained earlier entries; invalid '=' names with EINVAL; removal/missing lookup; malloc alignment; realloc data preservation; free/free(NULL); true heap-bound exhaustion with ENOMEM followed by recovery; recursive locks and ABI preservation.

Time tests now check stored TZ rather than arguments to an absent function. Full startup remains compared with small-budget execution. **65 Debug/Release suites passed**, with strict reproducible NRO rebuilding.

## Result and next stage

Original applet/HID/time/environment initialization completes, reaching unlinked **fsInitialize**, local marker **0xE015 / 57365**, before any native filesystem IPC. Main/constructors remain unreached.

Focused path: **14,386 instructions / 41 IPC exchanges**. Initially unfocused: wait after **7,178**, then **8,511 more**. Rebuilt NRO SHA-256: `2B7916E173C2450AF67659707F183D445EB755BA09D69ECC4BAD4693B56D8580`. reference.json records source/config/symbol hashes; newlib/libgloss notices and attribution terms accompany files.

Next was original fs/fsp-srv and a bounded native-file scope, distinct from emu:fs. Existing CPU instructions suffice for this libc milestone. This is not Switch 2 game compatibility.
