# Guest POSIX file application

Project-owned guest main using standard open/read/close through the original pinned newlib/libsysbase and libnx fsdev implementations. It reuses the existing initialization harness and checks independent file positions, EOF, descriptor closure/reuse and selected errors. All checks run inside the ARM64 guest; the host inspects the resulting report.

Rebuild: `tools/build_libnx_posix.ps1 -Python <python_path>` from the repository root. Original licenses are retained beside this file. reference.json records source/artifact hashes and expected results; no firmware or games are required.

See [the milestone report](../../../docs/LIBNX_POSIX_FILES.md) and [source notices](../../../docs/licenses/POSIX_SOURCE_NOTICES.txt). This is not complete POSIX/stdio support or native Switch 2 compatibility.
