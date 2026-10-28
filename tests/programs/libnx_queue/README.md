# Original libnx buffer-queue connection reference

This separate project harness calls original VI/Binder setup, original bqConnect/bqDisconnect/bqQuery and original cleanup. The queue profile only tracks logical connection and fixed headless geometry; no buffers or frames are allocated.

Rebuild with `tools/build_libnx_queue.ps1 -Python <python_path>`. Original buffer_producer.c is compiled as its unchanged prefix before the final unused bqSetPreallocatedBuffer function, which contains an unsupported GCC extension. Original source and compiled-prefix hashes, report fields and NRO hash are recorded in reference.json. Original library licenses are retained here; newlib compatibility and TLS lowering follow the preceding reference fixtures.

Run CTest `libnx_original_queue_connection`. See [the milestone and limits](../../../docs/LIBNX_QUEUE_CONNECTION.md).

Current follow-up: [Query support](../../../docs/LIBNX_QUEUE_QUERY.md) adds WIDTH/HEIGHT/FORMAT. The unchanged original guest now reads WIDTH successfully; height/format and unsupported attributes are exercised by packet replay.
