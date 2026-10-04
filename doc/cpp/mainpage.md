# SDK native C++ reference

This reference documents the native code maintained in this repository. Use it
alongside the [developer guides](../index.html): declarations tell you what a
function accepts, while the guides explain how to build, run and verify an
application. The [original Symbian header reference](platform/index.html) is a
separate, curated index of historical platform declarations.

The [SDK native API guide](../reference/native-sdk.html) maps public headers to
CMake targets, result types and object lifetime rules before you jump into
the generated symbol list.

## Choose the right surface

| Task | Start with | Where it runs |
| --- | --- | --- |
| Inspect or publish an E32 executable | `symbian::e32` in `e32.h` | Host tooling |
| Build or inspect a SIS package | `symbian::sis` in `sis.h` | Host tooling |
| Query supported device services | `symbian::api` display, power, storage, camera and system headers | Guest application |
| Schedule guest work | `symbian::concurrency` Future, Task, event executor and timer headers | Guest application |
| Configure optional application TLS | `symbian_mbedtls` platform and socket BIO headers | Guest application |
| Bound a development-agent wire frame | `symbian::agent::FrameDecoder` in `frame.h` | Host library; guest export pending |
| Work directly with EUSER or Window Server | [Original header reference](platform/index.html) | Guest application |

The host format libraries return `absl::Status` or `absl::StatusOr`. A failed
inspection of an unsupported E32 or SIS profile is not a general verdict on
the file. For guest code, read the [C++ application guide](../capabilities/cpp.html)
and [runtime limits](../capabilities/runtime.html) before using a class just
because its declaration appears here. The [device API map](../capabilities/device-apis.html)
shows which components are implemented and which remain planned.

## Common paths through this reference

1. **Build an application:** start with the [project guide](../guides/projects.html).
   The SDK's CMake targets and public headers belong in the application target;
   E32 conversion and SIS assembly are owned by the build and package tools.
2. **Use a device service:** choose a small `symbian::api` header, read its
   result and lifetime rules, then compare the corresponding capability guide
   with the firmware and emulator evidence.
3. **Use asynchronous work:** begin with `symbian::concurrency::Future` and
   its event executor. An asynchronous native request must keep its buffers
   alive until completion or cancellation has drained.
4. **Investigate an OS API:** open the separate original header reference and
   inspect its declarations. Check the SDK's import proxy and capability guide
   before assuming that an ordinal or service works in a selected ROM.

The [file list](files.html), [namespace list](namespaces.html) and search box
lead to the generated symbol detail. The index includes native implementation
files so maintainers can trace behavior; application code should start with
public headers. ARM compilation and emulator behavior do not prove Nokia 808
compatibility.
