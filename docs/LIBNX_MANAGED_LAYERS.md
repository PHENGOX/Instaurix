# Process-owned managed display layers

The original pinned libnx applet client defines SelfController command 40 as a no-input request returning a u64. Its original viCreateLayer uses that ID in VI application command 2020 (OpenLayer), together with the display name, applet resource user ID, a PID marker and a 256-byte output map-alias buffer.

The explicit homebrew-reference profile now allocates logical layers through SelfController command 40. IDs start at 1 within each process and increase without reuse for that process lifetime. The process owns a bounded set of 64 resources, shared across all its applet domain objects and connections. Session closure does not retire these resources. Process exit/fault clears them; successful initialization resets the state. These are local reference rules, not measured Horizon or native Switch 2 semantics. There is no backing buffer, native window or GPU object yet, and no layer-destruction command is supported.

Requests require an active applet, the correct interface, no PID marker and no payload. Allocation commits only after the complete IPC response is written. Protected replies and malformed requests cannot consume IDs; quota exhaustion returns OutOfResource. Layer IDs are not kernel handles or domain object IDs.

The unchanged VI NRO now runs 23,623 instructions and returns UnsupportedFormat from its original VI OpenLayer request. The layer retains logical ID 1 while its initialized flag remains false. The current VI transport rejects the PID/output-buffer packet explicitly; no native-window parcel is fabricated. Original display close, VI exit and application cleanup still complete, and frame count remains zero. Its SHA-256 remains 810B9BBAE06A124792347A4B78733B8154424BEAC215450A8AC6D43B0490023B.

The existing VI integration suite compares continuous and seven-instruction runs, captures the actual next request and verifies its layer/resource IDs and PID/output descriptor, rejects the wrong interface and extra payload, protects reply memory, allocates every remaining layer, checks quota exhaustion and verifies retirement at exit.

Next: parse and validate VI OpenLayer's actual PID and receive-buffer request, connect it to process-owned resources and design the native-window/Binder lifetime. Do not return a successful parcel until its structure and downstream behavior are implemented and tested.
Validation: all 71 CTest suites pass on Windows in Debug and Release after rebuilding every executable. The unchanged guest hash and matching English READMEs were checked. Linux CI for this milestone has not been run locally.

Follow-up: [VI OpenLayer validation](LIBNX_OPEN_LAYER.md) now decodes and validates this request. Native-window creation remains unsupported at service level; the unchanged guest now executes 23,651 instructions.
