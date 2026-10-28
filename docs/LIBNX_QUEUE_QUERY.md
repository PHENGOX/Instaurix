# Original buffer-queue Query client

Binder transaction 9 now supports the three query IDs declared by the pinned libnx display/types.h: WIDTH (0), HEIGHT (1) and FORMAT (2). The original bqQuery implementation sends one s32 attribute after the interface token and reads one s32 value followed by Binder status from the reply.

The explicit headless reference profile returns width 1280, height 720 and PIXEL_FORMAT_RGBA_8888 (1). The dimensions share their constants with the Connect reply. Format is a declared future buffer format, not evidence that graphic-buffer allocation or pixel submission works. These values are local profile choices and are not measured console behavior.

A valid reply is a 24-byte parcel: the standard 16-byte header, a four-byte value and zero Binder status. All common Binder transport, memory-range and UTF-16 token checks remain active. The Query payload must contain exactly one s32 attribute. Input flags must be zero; output buffer size remains the original client's 1024 bytes. Only produced reply bytes are written.

Queries require an opened layer with a live strong reference and an established CPU queue connection. Disconnected queues return NotInitialized under this local policy. Unknown attributes return explicit UnknownCommand without writing output. The undeclared/unsupported attributes 3, 6, 7 and -1 are covered by tests; no minimum-slot count or capacity is invented. Successful and unsuccessful queries leave connection state unchanged. Native-handle events and actual buffer slots remain unsupported.

The previous queue NRO is unchanged (SHA-256 B18233DEBF520C329C8F1EEEB26E71059B45E42365960A7EEDD46DD8FFE32B13). Its existing original bqQuery WIDTH call now succeeds and records 1280, then the original disconnect/reconnect/cleanup sequence completes. Execution is **38,984 instructions**, compared with 38,806 before Query support. Height and format use packet replay of the same original client protocol, including a synthetic zero-pointer-capacity AutoSelect variant. They are not additional original guest source calls.

The existing queue integration suite compares continuous and seven-instruction execution, checks the value/status parcel, validates malformed/unknown queries, protects the IPC reply and verifies querying while disconnected. Existing resource, fault/reload, memory, duplicate connection and cleanup tests remain active. No guest source or binary changes are required by this milestone.

## Next step

Move to the original native-window/framebuffer path and identify its first required buffer-slot, memory and fence operations. Query support alone provides neither allocated buffers nor valid submission. Any added slot capacity or synchronization semantics must have an explicit local contract and tests before successful responses are exposed. No frame, full external application or native Switch 2 graphics compatibility is demonstrated.

## Final validation

All 73 CTest suites pass on Windows in Debug and Release after rebuilding every executable. The original queue NRO still matches its recorded SHA-256, and both English README variants remain identical. Linux CI has not been executed for this milestone.

Follow-up: [original native-window object](LIBNX_NATIVE_WINDOW.md) now supports the producer's read-only availability event and runs original window create/dimensions/close. No slots, NVIDIA memory or frames are provided yet.
