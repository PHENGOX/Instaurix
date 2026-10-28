# Original time client, virtual clocks and UTC calendar

Historical milestone after [HID](LIBNX_HID_SHARED_MEMORY.md), followed by [newlib environment](LIBNX_NEWLIB_ENVIRONMENT.md). The reference links unchanged services/time.c at libnx b156b4774fb2df7b93402169509266da7547fc4c, removing timeInitialize/timeExit/timeGetCurrentTime/timeToCalendarTimeWithMyRule traps.

## Clock profile

Homebrew-only time:u opens five ordinary kernel-session interfaces: user, network, steady, timezone and local, without IPC domains.

The three read-only system clocks share:

`Unix timestamp = 946684800 + floor(virtual nanoseconds / 1,000,000,000)`

Epoch **2000-01-01 00:00:00 UTC** is deterministic, not PC/physical-console/network time. Only host-driven virtual time advances it. Steady time returns elapsed virtual seconds plus a stable opaque 16-byte source ID, without Unix offset. Reads do not advance time or mutate calendar state.

| Interface | Command | Output |
| --- | --- | --- |
| time:u root | 0..4 | New user/network/steady/timezone/local session, Move handle |
| System clocks | 0 | u64 timestamp |
| Steady clock | 0 | Signed 64-bit time and 16-byte source ID |
| Timezone | 101 | UTC calendar/additional info from a u64 timestamp |

Conversion uses Gregorian/POSIX seconds without leap seconds. It returns year/month/day/hour/minute/second, weekday/year-day, UTC name, DST=0 and offset=0. Range: **1970-01-01 through 9999-12-31 23:59:59**. Larger timestamps fail without partial output.

Requests require exact sizes, no SendPid/descriptors or unsupported CMIF versions. Clock writes, other zones/DST rules, network accuracy and modern shared time memory are absent. No newer firmware is advertised to bypass client paths.

Sessions follow transactional publication/shared 128-handle quota; failed replies consume no ID. Clones preserve roles; children outlive root closure. Inactive processes reject time calls. Reload resets virtual time.

## Original startup progress

The original client initializes five interfaces, reads user time and converts calendar data. Original newlib __libnx_init_time constructs **<UTC>-00:00:00** for TZ. Tests observe actual setenv arguments: name TZ, constructed value and overwrite=0.

Next blocker is unlinked setenv, local marker **0xE01C / 57372**, a libc build dependency rather than time service failure. No fake setenv success is added. Main/constructors and filesystem startup remain pending.

Focused: **13,572 instructions / 41 IPC exchanges**. Initially unfocused: wait after **7,178**, then **7,697 more**. HID retains its 256 KiB mapping at 0xD93000.

## Section placement and provenance

Extra code exceeded earlier fixed constant addresses. The linker now page-aligns rodata/RELRO after preceding sections; RELA references real symbols, not fixed numeric offsets. Tests derive the report from NRO data plus RELRO size; replay buffers use reserved heap.

The first RELRO relocation must point to the actual report and stay read-only. Only libnx_app's layout changes. Original time.c is compiled with stdint.h preincluded for Clang/newlib compatibility. Rebuilt NRO SHA-256: `55575102A89357FA4AF00780AF868A3D109FDE89BCCAF26A9C8670966F6BB83B`, reproduced strictly.

libnx_time_clocks_and_utc_calendar checks original SDK/newlib flow and TZ arguments. Replays cover roles, fractional seconds, both clock types, stable source, independent Python datetime calendar vectors (including leap year 2000 and non-leap 2100), range/overflow, arguments/versions, protected replies, clones/closure/quota/termination.

**64 Debug/Release suites passed**, with reproducible reference. Next: real newlib environment storage, then filesystem startup. Modern shared time, HID samples and GPU remain separate. Sources inspected: pinned time.c, time.h and runtime/newlib.c. These historical Switch references do not establish native Switch 2 compatibility.
