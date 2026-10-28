# Original native-window object and availability event

The separate libnx_window reference now runs original `nwindowCreateFromLayer`, `nwindowGetDimensions` and `nwindowClose` from the pinned display/native_window.c. VI layer creation, original Binder reference acquisition, native-handle acquisition and queue connection all succeed. This initializes an SDK NWindow object; it does not create a visible host window, allocate a graphic buffer or render a frame.

## Native handle and lifetime

Binder relay command 2 accepts exactly eight input bytes: a producer ID and selector 0x0f, as used by the original client. It requires an opened process-owned layer and a live strong Binder reference. Unknown producer/selector/payload combinations fail explicitly. A successful reply publishes one copied readable event handle using the existing kernel handle quota and monotonic handle IDs.

Each layer owns one shared availability event; repeated acquisitions return distinct handles to that event. The endpoint is read-only and service managed. SignalEvent on the readable handle returns InvalidHandle; guest ClearEvent/ResetSignal cannot reset its state. Availability is unsignaled because no buffer slots are configured in this milestone. In particular, no permanent signal is used to skip the native client's wait logic. Future slot availability must drive this event from actual slot state.

The event and its handle are published only after the complete IPC response is written. Protected replies and invalid requests consume no hidden handle slot. Closing a copied handle does not destroy the producer-owned event. Original nwindowClose disconnects the queue, closes its remote event, releases Binder references and clears its local structure; the project harness then closes the VI layer. Process reload/exit/fault use the existing process resource lifetime rules.

## Reference behavior

The project guest first runs the existing startup/constructor checks, then directly calls the original window APIs from main. It records a valid connected NWindow, dimensions 1280 x 720, zero configured slots and current slot -1. Its original eventWait calls with timeout zero return TimedOut. Guest attempts to clear/signal the event fail. Repeated original binderGetNativeHandle calls obtain 107 additional distinct handles before OutOfHandles in this specific startup state. Original eventClose releases those handles, acquisition succeeds again, and native-window/layer/display/application cleanup completes normally.

The reference executes **97,352 instructions**, including the deliberate handle-quota and uniqueness probes; this is not a graphics-performance metric. File size is **446,464 bytes**, BSS **16,384 bytes**. Reports: base 0x7B000, window 0x7C1D8, VI 0x7C2D8. NRO SHA-256: `097B66B3691D1843DBB8D52DC976EA6F3F5CED03C4E355EB76A1A96C20928B44`.

The previous VI, Binder and queue guest binaries are unchanged. Binder and queue references now execute 25,317 and 39,046 instructions respectively because their existing native-handle request succeeds and original eventLoadRemote runs. Those historical small harnesses retain that optional returned handle until the end of their process; the new native-window reference explicitly exercises closing it.

`libnx_original_native_window_event` checks continuous/seven-instruction equivalence, original event waits and permissions, quota/reacquisition and original cleanup. It captures the actual native-handle packet and replays invalid producer/selector/payload and protected-reply cases, then checks that the original quota probe still obtains the same number of handles. Other queue/VI tests remain active.

## Build provenance and boundary

Original native_window.c is compiled unchanged. The fixture retains the preceding reference's unchanged buffer_producer.c prefix, excluding its final unused GCC-only preallocation function. Only reachable window functions survive section collection. Complete default-window initialization, framebuffer creation, buffer configuration/dequeue, NVIDIA fence functions and external hello-world are not executed here. Original source hashes and library licenses accompany the fixture.

Inspection of original framebuffer.c shows that framebufferCreate first sets dimensions, initializes NVIDIA services, nvmap and fences, allocates memory, creates an nvmap object and only then registers preallocated buffers with the window. Its source hash is recorded as inspected, not executed. Therefore the next step is to exercise that original memory-service path and define bounded memory/map ownership before making buffer-slot registration succeed. A local slot number alone would not give the guest usable buffer memory or synchronization.

## Final validation

All 74 CTest suites pass on Windows in Debug and Release after rebuilding every executable. Strict second reconstruction reproduces the new native-window NRO SHA-256. Seven guest binaries match their recorded hashes; previous guest binaries remain unchanged. The two English README variants remain identical. Linux CI has not been executed for this milestone.

Follow-up: [original NVIDIA transfer-memory client](LIBNX_NV_TRANSFER_MEMORY.md) adds scoped kernel transfer memory and validates the original Initialize packet. NVIDIA initialization still fails explicitly until service-retained memory ownership is implemented.
