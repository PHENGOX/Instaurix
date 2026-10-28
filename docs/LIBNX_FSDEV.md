# Original fsdev mounting, main and cleanup

The separate `tests/programs/libnx_fsdev` reference links original libnx fs_dev.c, path_buf.c, alloc.c and decode_utf8.c, plus original patched newlib/libsysbase device-table, path, string, search and calendar dependencies. The existing libnx_app reference retains its earlier explicit stop and tests.

## Result

Original default __appInit now completes applet, HID, time, environment, filesystem initialization, optional SD mounting and initial-directory setup. The original newlib constructor runner invokes the project constructor; the reference reaches project main, which returns **77**. Original __appExit unmounts devices and closes services, and __libnx_exit returns **loader result 0**. These two return values are checked separately.

With an explicitly configured read-only snapshot, fsOpenSdCardFileSystem reaches fsp-srv command 18 and obtains a domain object. Original fsdevMountDevice registers **sdmc** through the real libsysbase AddDevice table, allocates its current-directory storage, and selects it as the default device. This registration happens through the original __appInit path, rather than an extra host-injected call.

Without a configured snapshot, the SD request returns local FilesDisabled. Original libnx ignores the optional mount failure and still reaches main, leaving no fabricated SD device.

Unmodified execution totals:

| Configuration | Instructions | Main result | Loader result |
| --- | --- | --- | --- |
| No SD snapshot | 19,775 | 77 | 0 |
| Explicit read-only snapshot | 28,008 | 77 | 0 |

The NRO is **360,448 bytes**, with **16,384 bytes BSS**. The report is at **0x66000**. Its constructor/main bits are **6**, and no diagnostic/dependency marker is reached. These counts describe the minimal reference, not game compatibility or performance.

## Validation

`libnx_original_fsdev_mount_and_main` runs both configurations and pauses at main to inspect the actual guest device table/filesystem object. It calls the original functions to check duplicate mount rejection, preservation of registration, unmount, missing-device errors and remount. It then resumes main and checks normal cleanup and retirement of applet state.

Separate clean runs execute the unchanged NRO continuously and in seven-instruction slices, comparing total instructions and all 32 report fields. The old libnx references retain their independent tests. No host stub pretends that AddDevice, allocation, directory management or cleanup succeeded.

The same pinned Clang/libnx TLS adaptation is applied to generated fs_dev.c assembly. Its original __thread path buffer is placed in real .tbss. Original library sources remain unchanged relative to the pinned libnx revision and official devkitA64 newlib patch. Manifest hashes record added sources and rebuilt artifacts; source notices are retained in [FSDEV_SOURCE_NOTICES.txt](licenses/FSDEV_SOURCE_NOTICES.txt).

## Rebuild and run

```powershell
.\tools\build_libnx_fsdev.ps1 -Python <python_path>
.\build\Debug\Instaurix.exe .\tests\programs\libnx_fsdev\program.nro 500000 --hle-reference --homebrew-entry --read-root .\tests\programs\hle_files\root
ctest --test-dir build -C Debug -R libnx_original_fsdev_mount_and_main --output-on-failure
```

Ordinary tests use included artifacts. Reconstruction uses the pinned source archives, official patch and Zig 0.13.0, with strict NRO hash verification.

## Limits and next step

This is **mount registration**, not native file I/O. SD file/directory methods remain explicitly unsupported. No file is opened/read through the new native filesystem object. Empty argv means the reference's initial-directory routine has no application path to process; arbitrary cwd handling is not validated. Several linked helper functions are retained through devoptab callbacks without being exercised by this minimal application.

Main and the constructor are project test code. The project exit bridge records main return and calls __libnx_exit; complete libc stdio flushing and C++ destructor shutdown are not validated. This does not execute the complete external switch-examples hello-world, initialize native graphics, render guest GPU commands, read controllers or run commercial games. Historical Switch/libnx behavior does not prove native Switch 2 compatibility.

Next: follow the original filesystem calls needed by a real client, beginning with opening and reading a known file through the native SD object. Keep directory/path and read-only errors explicit, then use a broader original application target to identify the next display/input requirements.

## Final validation

All **68 CTest suites pass in Debug and Release**. Strict reconstruction reproduces NRO SHA-256 `F83F17A94B71DE49E186A52745194847566E331B6DC60DE70D64EC9A068FB9DF`. The earlier libnx_app fixture still matches its own manifest.


Subsequent milestone: [native file opening and reading](LIBNX_NATIVE_FILES.md). The limitations above describe the mounting milestone.
