# Guest runtime examples

The installed SDK contains target headers, ARM runtime archives, original
Symbian import proxies and CMake targets. Choose a
[runtime profile](../capabilities/runtime.md) before adding a library facility.

| Application task | Example | What to inspect |
| --- | --- | --- |
| Strings and containers | `probes/runtime_probe` | Target headers and archive linkage |
| Status and hash containers | `probes/abseil_status_probe` | `Symbian::AbseilStatusOr` target |
| Dynamic libraries | `probes/mbedtls_dll_probe` | Exports, import proxies and lifetime |
| Window Server GUI and timer Tasks | `examples/gui_app` | Startup, redraw, input and cancellation |

Guest code normally disables exceptions and returns explicit status values.
Use [concurrency APIs](../capabilities/concurrency.md) for asynchronous
completion and workers. The [TLS guide](../guides/tls.md) describes verified
peer connections, explicit CA configuration and the entropy requirement.
