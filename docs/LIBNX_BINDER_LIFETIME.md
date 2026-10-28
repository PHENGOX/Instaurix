# Local native-window parcel and original Binder lifetime

Original libnx `viCreateLayer` now accepts the OpenLayer response and obtains an initialized layer. A separate `libnx_binder` reference additionally executes original `display/binder.c`: `binderCreate`, `binderInitSession`, `binderGetNativeHandle`, `binderClose` and original `viCloseLayer`. These are bounded reference-profile resources, not a GPU buffer queue or a validated native Switch 2 window.

## Parcel contract

The pinned `services/vi.c` checks the ParcelHeader bounds, requires at least three payload words and takes payload word 2 as the producer ID. The local response follows that reader contract:

| Offset | u32 value | Local meaning |
| --- | --- | --- |
| 0 | 12 | Payload length |
| 4 | 16 | Payload offset |
| 8 | 0 | Object table length |
| 12 | 28 | Empty object table offset |
| 16, 20 | 0 | Reserved local payload words |
| 24 | 0x1000 + layer index | Producer ID, separate from kernel handles |

OpenLayer reports length 28 and writes only those 28 bytes of its validated 256-byte output buffer. The IPC response, parcel bytes and resource publication form a transaction. Failed publication opens no layer and creates no producer. Remaining buffer bytes are preserved. This minimal local layout is supported by the inspected client reader; its two reserved words and producer IDs are not claimed to reproduce a hardware parcel format.

## Resource lifetime

Each process has at most 64 logical layers. A layer may be opened once at a time; duplicate OpenLayer returns AlreadyInitialized. The application service object owns the open reference. Its clones share ownership; another service object cannot close it. Closing a display reference does not implicitly close layers or release Binder references. Resources remain bounded by the process lifetime even if a client abandons them.

VI application command 2021 accepts a u64 layer ID and closes that service object's open layer. The logical layer ID is retained until process exit; it may be reopened with the same producer identity. Original viCloseLayer clears its client structure only after success. Duplicate close and wrong ownership fail explicitly.

Binder relay command 1 accepts exactly three s32 words: producer ID, delta and reference type. The local subset supports deltas +1/-1, type 0 (weak) and 1 (strong). Each counter is bounded at 64. Underflow, overflow, unknown objects and unsupported deltas/types fail. An open layer or existing references keep a producer alive. After layer close, references may be released; a completely retired producer cannot be resurrected through AdjustRefcount. Reopening its logical layer makes it available again. All updates publish only after a successful IPC reply. Exit, CPU fault and successful reload retire/reset the process resources.

Native-handle command 2 remains UnknownCommand. Binder transactions, events, fences, buffer allocation, queue connection and presentation are not implemented by this milestone. No frame is produced.

## Original client references

The previous VI NRO remains byte-for-byte unchanged and now runs **23,711 instructions**. The new separate Binder NRO runs **25,255 instructions**, with **434,176 file bytes** and **16,384 BSS bytes**. Its base report is at 0x78000 and VI report at 0x791D8. SHA-256: `E2A5CB2DA3B5710A8AB6BA86AC7450923A4FBAA8A646D3A262FEA9BA7AEC8342`.

The new project harness links unchanged pinned VI and Binder source, records the original Binder initialization and cleanup, observes UnknownCommand for GetNativeHandle, then closes the layer, display and VI sessions normally. Constructor/main checks and loader cleanup complete. This target does not link/run complete nwindowCreate, default-window initialization, console output or an external hello-world.

Tests compare continuous and seven-instruction execution, validate the actual parcel, replay malformed OpenLayer calls, protect reply/output memory, exhaust layer and weak-reference quotas, reject underflow/resurrection, preserve output-buffer tails and exercise cleanup after close/reload/fault. A strict second reference build reproduced the recorded NRO hash. Source hashes and original library licenses accompany the fixture.

## Next step

Exercise the original native-window/buffer-producer connection path and capture its Binder transaction packet. The inspected native_window.c queries native handle 0x0f after acquiring Binder references, then calls bqConnect; its GetNativeHandle return is not itself assigned to the outer result. Implementing a handle alone will not establish a functioning queue. Events, connection replies and producer lifetime need a coherent contract before buffer submission can succeed. GPU rendering and native Switch 2 compatibility remain unvalidated.

## Final validation

All 72 CTest suites pass on Windows in Debug and Release after rebuilding every executable. Strict reconstruction reproduces the new Binder NRO SHA-256. Five application/filesystem/POSIX/VI/Binder binary hashes match their manifests, and 115 source-file hashes in the VI/Binder manifests match the cached pinned inputs. A stale mallocr.c hash in those two manifests was corrected from the verified source without changing any library code. Linux CI has not been executed for this milestone.

Follow-up: [original queue connection](LIBNX_QUEUE_CONNECTION.md) adds bounded Connect/Disconnect parcel transactions. Native handles, Query and buffer submission remain unsupported.

Follow-up: [original native-window object](LIBNX_NATIVE_WINDOW.md) now supports the producer's read-only availability event and runs original window create/dimensions/close. No slots, NVIDIA memory or frames are provided yet.
