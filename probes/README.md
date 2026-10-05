# Runnable diagnostic probes

These small programs are runnable examples for checking a specific SDK or
Symbian contract. They are diagnostic probes, not user-oriented applications.
For an application with a window, input handling and menu resources, use
[gui_app](../examples/gui_app/) or follow
[Build a real GUI app](../doc/docs/guides/from-source.md). For a generated
starter, follow [Build an application](../doc/docs/guides/building.md).

| Probes | What they check |
| --- | --- |
| `e32_probe`, `import_probe`, `pointer_probe`, `abi_probe` | E32 publication, imports, pointer relocation and ARM ABI behavior |
| `runtime_probe` | C++ runtime, allocation, clocks and selected library contracts |
| `abseil_allocator_probe`, `abseil_status_probe` | Guest allocation and Abseil Status/StatusOr behavior |
| `cxx20_probe`, `cxx20_module_probe` | C++20 language features and named-module builds |
| `dll_probe`, `dll_data_probe`, `dll_lifecycle_probe` | DLL exports, writable data and load/close lifetime |
| `mbedtls_dll_probe` | App-linked TLS/cryptographic code and a DLL boundary |
| `connectivity_probe`, `http_probe` | Native sockets and HTTP exchanges |
| `eka1_probe`, `eka1_import_probe` | The initial legacy process and EUSER import profile |

After preparing the SDK and selecting its tools, build a minimal probe from the
repository root:

```sh
symbian build --project probes/e32_probe --output .symbian/e32-probe
symbian inspect --format e32 .symbian/e32-probe/e32_probe.exe
```

Each probe has its own CMake project and target requirements. An image passing
inspection has not executed; use the corresponding integration checks and a
compatible disposable emulator or device. Runtime and networking probes need
their selected OS imports. The EKA1 probes use a separate legacy image profile.
