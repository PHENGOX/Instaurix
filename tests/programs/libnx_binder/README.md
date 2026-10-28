# Original libnx Binder lifetime reference

Separate project-owned guest harness with original default app initialization, VI layer creation and unmodified `display/binder.c` client calls. It records producer identity, Binder initialization, unsupported native-handle query, Binder close and VI layer close before normal display/application cleanup. This does not run a full native window or render pixels.

Build with `tools/build_libnx_binder.ps1 -Python <python_path>`. The shared builder verifies pinned sources, packages the NRO, extracts test symbols and requires the recorded NRO SHA-256. The newlib/libsysbase compatibility and TLS adaptation are the same documented reference build setup used by the preceding VI/FSdev fixtures. Original library licenses are retained here. Source hashes and report fields are in reference.json.

CTest `libnx_original_binder_lifetime` uses the same original-packet and resource-lifetime checks as the earlier VI reference and additionally checks the guest's actual Binder initialization/cleanup results. See [the milestone](../../../docs/LIBNX_BINDER_LIFETIME.md).
