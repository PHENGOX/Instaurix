# Original libnx native-window reference

Separate project harness executing original VI layer setup, nwindowCreateFromLayer, nwindowGetDimensions, native event waits/permissions, event-handle quota/close/reacquire probes and nwindowClose. It uses the local logical queue profile and creates no graphic buffers or visible frame.

Rebuild with `tools/build_libnx_window.ps1 -Python <python_path>`. Original native_window.c compiles unchanged; the shared builder retains the documented original buffer_producer.c prefix and the preceding libnx/newlib compatibility setup. The recorded NRO hash is required by the builder. Sources, reports and source hashes are recorded in reference.json; original library licenses accompany the fixture.

Run CTest `libnx_original_native_window_event`. See [native-window behavior and limits](../../../docs/LIBNX_NATIVE_WINDOW.md).
