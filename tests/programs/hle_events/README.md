# HLE event: wait and resume

Project-owned ARM64 assembled by Clang 18.1.6/Zig 0.13.0, without Nintendo files/code. Rebuild: `./tools/build_hle_events.ps1` from project root, reusing hle_memory ELF placement.

Creates an event, stores handles at 0x12000/0x12004 and waits. Host tests signal the write handle and resume. Guest checks result/index, resets the event, closes both handles, writes **55 at 0x12008** and exits; detected failure writes 99.

--hle-reference CLI reports waiting and cannot itself inject the required signal; hle_events_suspend_resume drives the complete path. Public Horizon register conventions do not establish native Switch 2 compatibility.

## Recorded artifact SHA-256

`E3922FA1CC07DEA10A33CFEAB5D6D359581461FF9E1A6F3B74BAB44E16EBADE1`

