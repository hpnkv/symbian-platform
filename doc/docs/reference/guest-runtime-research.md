# Guest runtime

The installed SDK contains target headers, ARM runtime archives, original
Symbian import proxies and CMake targets. Applications use the
[capability guides](../capabilities/index.md) to choose a supported feature;
this page points to the maintained probes behind those statements.

| Area | Probe or example | What to inspect |
| --- | --- | --- |
| C++ strings and containers | `examples/runtime_probe` | Target headers, archive linkage and guest exit |
| Status and hash containers | `examples/abseil_status_probe` | Installed `Symbian::AbseilStatusOr` target |
| Timers and task completion | `examples/runtime_probe` | Owner thread, cancellation and result controls |
| Dynamic libraries | `examples/mbedtls_dll_probe` | DLL load, ordinals and process lifetime |
| Window Server GUI | `examples/gui_app` | Startup, imports, redraw and input |

The runtime normally disables exceptions. Native libraries return explicit
status values; a boundary that needs exceptions must enable them only for its
selected translation units. `Symbian::Stackless` supplies bounded completion
and timer facilities, while the broader fiber and service contracts remain
separate development gates. The [concurrency guide](../capabilities/concurrency.md)
explains the application-facing subset.

The active SDK now vendors Mbed TLS adaptation source and headers. Its
[application TLS guide](../guides/tls.md) describes linkage and project-local
CA configuration. The [development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
separates archive and DLL probes from transport and authenticated handshake
results. ARM generation and emulator probes do not prove physical-device
compatibility.
