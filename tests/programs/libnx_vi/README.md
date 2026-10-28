# Original libnx VI client reference

Project guest main calls unchanged pinned libnx VI functions after default application initialization. It records service fallback, application/Binder session creation, two logical Default-display opens, the unsupported resolution probe and the actual managed-layer blocker. Display close/VI exit and standard cleanup still run. No rendered frame is claimed.

Rebuild from the repository root with `tools/build_libnx_vi.ps1 -Python <python_path>`. Original library licenses are retained. reference.json records input/artifact hashes and expected results. See [the milestone report](../../../docs/LIBNX_VI.md) for scope and limitations.

Current follow-up: logical layer allocation succeeds; the next boundary is VI OpenLayer's PID/output-buffer transport. See [managed layers](../../../docs/LIBNX_MANAGED_LAYERS.md).

Latest milestone: [VI OpenLayer validation](../../../docs/LIBNX_OPEN_LAYER.md). The request transport and resources are validated; native-window creation still returns an explicit service error.
