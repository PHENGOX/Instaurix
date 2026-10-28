# Reference SD object and libnx mount audit

Historical milestone followed by [validated filesystem TLS](LIBNX_TLS.md).

## Added functionality

fsp-srv domain command **18 OpenSdCardFileSystem**, after SetCurrentProcess, accepts no PID/arguments and creates an SD object only when an explicitly configured ReadOnlyFiles snapshot exists. Otherwise it returns local FilesDisabled. The guest cannot choose the host directory.

Each object retains a shared reference to the immutable snapshot. Removing configuration prevents future opens while existing objects retain it. Publication is transactional: unwritable replies or exhausted quota create no object and consume no ID. Domain quota is shared with other objects. Existing domain close rules apply.

No native file method is implemented: such methods return UnknownCommand. Opening the object is not a working fsdev mount. Native libnx file requests are not silently mapped to the project's custom file protocol.

## Verification

original_fsp_process_attachment_and_clones first executes real fsInitialize, then replays constructed command-18 packets for absent configuration, extra arguments/PID, failed reply publication, successful opening/closure, configuration removal, shared quota and no failed-operation ID gaps.

These extra requests were not yet emitted by normal startup. Its unchanged NRO still executes **15,244 instructions / 47 IPC exchanges**, stopping at **0xE017** before fsdevMountSdmc/main.

## Audit and next work

Exploratory original fs_dev.c compilation identified AddDevice, GetDeviceOpTab, setDefaultDevice, RemoveDevice, libnx allocation, string functions, decode_utf8, path buffer, chdir, bsearch and gmtime dependencies. The delivered build retains explicit stops instead of pretending these functions work.

Another prerequisite was __thread g_fsPriority: reference Clang uses TPIDR_EL0, whereas libnx accesses ThreadVars.tls_tp through __aarch64_read_tp. At this stage the linker only reserved a synthetic test TLS buffer. Real TLS sections/access and initialized/isolated values therefore needed validation; the [next milestone](LIBNX_TLS.md) addresses this.

Planned order: validate TLS, integrate original fsdev/newlib dependencies, observe mounting through the NRO, then implement required file operations. Historical libnx Switch validation is not native Switch 2 compatibility.

Full builds and **66 Debug/Release CTest suites passed**. The targeted filesystem test was also rebuilt/replayed after strengthening SendPid-without-arguments rejection.
