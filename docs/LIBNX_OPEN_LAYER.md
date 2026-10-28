# VI OpenLayer request validation

This milestone handles the original pinned libnx VI application's command 2020 transport and validates its resources. It does not create a native window or render an image.

The audited original `_viOpenLayer` in services/vi.c sends an 80-byte input (64-byte display name, u64 layer ID, u64 applet resource user ID), a SendPid special header and one normal output map-alias buffer of 256 bytes. The scoped decoder accepts that format only for VI application sessions. Copied/moved handles, other special-header formats, non-normal buffers and other PID-bearing commands remain unsupported. The guest PID placeholder is never trusted as a process identity.

The decoder checks raw-data bounds, SFCI/version/token, descriptor size, writable RAM and non-overlap with the IPC request. Excess transfers, MMIO/unmapped/read-only buffers and malformed messages fail at transport level. Service validation requires exactly 80 input bytes, exactly 256 output bytes, the padded Default name, an open display reference on this service object, the current process resource ID and an allocated nonzero layer ID. Logical layers remain process owned; session state governs display references.

A valid call now returns an explicit service-level UnknownCommand for the missing native-window implementation, rather than failing to decode its transport. No parcel, output length, Binder handle or output-buffer bytes are published. Invalid calls also leave the output unchanged. The call neither allocates nor consumes layers or display references.

The unchanged NRO now executes **23,651 instructions**, retains layer ID 1 with initialized=false, closes its display references and VI sessions, and exits normally. Its SHA-256 remains `810B9BBAE06A124792347A4B78733B8154424BEAC215450A8AC6D43B0490023B`. Frame count remains zero. Continuous and seven-instruction execution produce identical reports and instruction counts.

The VI integration suite replays the original request with wrong process/layer/name, short payload/output size, modified PID placeholders, copied-handle headers, overlapping output, excessive transfer size and protected output memory. It closes all display references to check InvalidState, restores them, verifies unchanged output bytes and fills the logical-layer quota to detect unintended allocations. Prior layer lifetime/reload/fault checks remain active.

## Next boundary

Implement a documented native-window parcel and matching Binder object lifetime, then follow the original client's next actual request. This requires examining the original parcel reader, Binder relay client and buffer-queue protocol together. A successful OpenLayer response must not expose an unusable fabricated object. Guest GPU execution, presentation and native Switch 2 compatibility remain unvalidated.

## Validation

All 71 CTest suites pass on Windows in Debug and Release after rebuilding all executables. Guest SHA-256 and identical English READMEs were checked. Linux CI has not been executed for this milestone.

Follow-up: [native-window parcel and original Binder lifetime](LIBNX_BINDER_LIFETIME.md) now implements a bounded local parcel, producer reference counts and VI layer close. Historical unsupported-native-window results above describe the preceding milestone.
