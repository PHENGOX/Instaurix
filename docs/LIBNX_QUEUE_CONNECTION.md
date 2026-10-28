# Original buffer-queue connection client

The separate `libnx_queue` guest reference runs original libnx bqConnect, bqDisconnect and bqQuery functions through original parcel.c and binder.c after original VI layer creation and Binder initialization. A local logical connection is now implemented. It allocates no graphic buffers, publishes no frame and does not validate native Switch 2 graphics.

## Source and build provenance

The pinned buffer_producer.c ends with bqSetPreallocatedBuffer, whose variable-length structure member is a GCC extension rejected by Clang even when that function is unused. The reference builder generates an exact unchanged prefix of the original file before that final function. The prefix retains original Connect, Disconnect and Query code, the original enum and interface token. Unused functions in that prefix are removed by normal link-time section collection. Original source and compiled-prefix hashes are recorded in reference.json. No upstream function body is rewritten. The source cache remains unchanged. This build does not include preallocated-buffer support.

Original parcel.c and binder.c compile unchanged. Existing library licenses accompany the fixture. The project harness deliberately calls queue methods; full native_window.c, default-window initialization and an external hello-world are not executed by this target.

## Scoped transport and parcel validation

The actual current guest selects legacy Binder command 0 with one normal A input and B output descriptor. The decoder also supports command 3's zero-pointer-capacity AutoSelect fallback: one null X descriptor, active normal A/B buffers, zero output-pointer size and one null C descriptor. The latter format is exercised by replay, not claimed as the actual default guest path. Non-null pointer fallback, PID/handle headers, other descriptor shapes and buffer modes remain unsupported.

All descriptor ranges require appropriate RAM permissions and bounded size. Input/output ranges cannot overlap each other or the IPC request. Service input is at most 1024 bytes; output must be the client's 1024-byte parcel buffer. Payload flags must be zero. ParcelHeader offsets, lengths, empty object table and the UTF-16 `android.gui.IGraphicBufferProducer` token are checked before operation arguments are read. Unallocated producers, closed layers and missing strong references are refused.

## Local connection policy

| Operation | Behavior |
| --- | --- |
| CONNECT (10) | Null listener, CPU API 2 and producerControlledByApp=false establish one connection |
| Duplicate CONNECT | AlreadyInitialized, no second connection |
| DISCONNECT (11) | CPU API 2 disconnects an existing connection |
| Duplicate DISCONNECT | NotInitialized |
| QUERY (9) and other transactions | Explicit UnknownCommand; no successful stub |

Connect returns a bounded ParcelHeader and BqBufferOutput followed by zero Binder status: width 1280, height 720, transform hint zero and pending buffers zero. These dimensions define an explicit fixed headless reference profile; they are not measured display hardware or docked/handheld behavior. Disconnect returns only zero Binder status in its parcel. Only the produced parcel bytes are written; output-buffer tails remain unchanged.

Connection state is process owned with its layer. A layer cannot be closed, nor its final strong reference released, while connected: the client must disconnect first. Fault/exit and successful process initialization clear all resources. IPC response, output parcel and connection-state changes are transactional. Invalid/protected replies cannot leave a hidden connection or consume a disconnect.

## Reference result and tests

The original client connects, rejects duplicate connect, observes unsupported Query without changing its sentinel, disconnects, rejects duplicate disconnect, reconnects and disconnects again. Original Binder close, VI layer/display close and application cleanup complete. The guest executes **38,806 instructions**, with **446,464 file bytes** and **16,384 BSS bytes**. Report addresses: base 0x7B000, queue 0x7C1D8, VI 0x7C258. NRO SHA-256: `B18233DEBF520C329C8F1EEEB26E71059B45E42365960A7EEDD46DD8FFE32B13`.

`libnx_original_queue_connection` compares continuous/seven-instruction runs, captures the real client packet, replays invalid producer/flags/token/parcel/API, oversized and overlapping buffers, protected memory, duplicate state changes and a synthetic AutoSelect fallback. It checks unchanged failed outputs, reply tails and refusal of connected-resource teardown. A separate connected-process run checks failed reload preservation, fault cleanup and fresh connection after reload. Earlier guest binaries remain unchanged.

## Next step

The fixture's next explicit unsupported queue operation is Query. Native-handle/event support also remains absent. Add the query/slot/allocation protocol only as required by an original native-window/framebuffer path, then implement buffer lifetime and submission. Connecting alone does not establish usable buffer slots, fences, CPU framebuffer mapping, guest GPU execution or presentation.

## Final validation

All 73 CTest suites pass on Windows in Debug and Release after rebuilding every executable. Strict second reconstruction reproduces the queue NRO SHA-256. Six guest binary hashes match their manifests; the generated unchanged-prefix hash matches its recorded value. English README variants remain identical. Linux CI has not been executed for this milestone.

Follow-up: [original queue Query](LIBNX_QUEUE_QUERY.md) now supports WIDTH/HEIGHT/FORMAT; historical unsupported Query results above refer to the preceding milestone. The same guest binary now executes 38,984 instructions.

Follow-up: [original native-window object](LIBNX_NATIVE_WINDOW.md) now supports the producer's read-only availability event and runs original window create/dimensions/close. No slots, NVIDIA memory or frames are provided yet.
