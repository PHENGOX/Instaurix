# Native libnx read-only file opening and reading

The fsp-srv domain now serves a bounded read-only file subset from the explicitly configured immutable snapshot. Tests call the **original libnx fsFsOpenFile, fsFileGetSize, fsFileRead and fsFileClose functions** after original fsdev startup reaches main. They do not replace these functions with host stubs or translate their calls to emu:fs.

## Implemented contract

| Object | Command | Supported behavior |
| --- | --- | --- |
| SD filesystem | 8 OpenFile | Root-absolute flat filename, mode Read=1, new domain file object |
| File | 4 GetSize | u64 snapshot byte count |
| File | 0 Read | Positional nonnegative offset, option=0, u64 returned byte count |
| Domain object | Close | Existing domain close protocol |

File objects retain shared immutable byte storage. They use the existing 64-object domain quota and transactional object publication, separately from kernel handles. Failed publication consumes no ID. Reads have no mutable cursor. Closing the file invalidates further operations.

Paths are sent by original libnx through one **SendStatic/X descriptor**, with its fixed **0x301-byte** path buffer and index zero. The complete buffer must be readable RAM. A terminating NUL and an absolute `/name` path are required. The existing snapshot name policy rejects traversal, nested directories, colons, empty or oversized names and unsupported characters. The guest never opens a PC path; it only searches the preloaded snapshot.

Read requests use one **Receive/B descriptor, NonSecure mode 1**, matching the original client. Other descriptor combinations, input handles, contexts and mappings remain unsupported. Full upper address/size bits are decoded before checking RAM, so truncation cannot bring invalid addresses into range. Buffer-enabled domain parsing is limited to filesystem sessions; applet domain restrictions remain unchanged.

Reads accept at most **64 KiB output capacity** and a requested count that fits it. Negative offsets and unsupported options fail. The complete nonempty output range must be writable RAM, outside MMIO and the IPC reply buffer. Zero-sized output is not dereferenced. EOF returns zero; crossing EOF copies remaining bytes only, leaving unused capacity intact. File writes/resizing and directory operations remain explicitly unsupported.

The IPC response and file bytes use the same transactional Memory::writeRanges operation. Invalid descriptors, inaccessible output or an unwritable reply produce no partial output. Service errors use existing local reference result codes, not an established match for every native Horizon filesystem error.

## Original-client validation

The new `libnx_native_file_open_and_read` suite boots the unchanged fsdev NRO with the supplied snapshot, obtains its real filesystem object, and invokes original ARM64 client entry points recorded from ELF symbols.

It opens `/sample.txt`, verifies **size 30**, reads exactly `Switch2Emu read-only fixture.` plus LF, checks partial reads and EOF, preserves unused bytes, and closes the file. Further checks cover negative offsets, unsupported options, zero-sized output, unsafe/missing paths, write-mode rejection, high descriptor address bits, transfer limits, reserved mapping mode, protected destination/reply pages, unchanged output/request on failure, no failed-open ID consumption, and stale file closure. Original main/cleanup still succeeds afterward.

SDK packets are captured at their actual SVC instruction and replayed for malformed-message checks. This is a controlled client-call integration test after startup, not a claim that the reference main itself performs POSIX file I/O or that a complete external application has executed.

## Provenance and reproduction

No guest source or NRO change is needed: original filesystem functions were already retained by fsdev callbacks. The symbol list adds their four public entry points, and its manifest hash is updated. The NRO remains SHA-256 `F83F17A94B71DE49E186A52745194847566E331B6DC60DE70D64EC9A068FB9DF`, with unchanged clean startup totals: **28,008 instructions with SD**, **19,775 without SD**.

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug -R libnx_native_file_open_and_read --output-on-failure
```

## Next step and limitations

Exercise standard libc/open/read through the original fsdev callbacks, including whatever path/metadata operations that client actually needs. Directory enumeration, arbitrary cwd behavior, stdio completeness, file writes, audio, controller samples and guest GPU rendering remain outside this milestone. Historical Switch/libnx validation does not establish native Switch 2 compatibility or playable games.

## Final validation

All **69 CTest suites pass in Debug and Release**. The unchanged fsdev NRO and updated symbol file both match their recorded manifest hashes.


Subsequent milestone: [guest libc open/read/close](LIBNX_POSIX_FILES.md).
