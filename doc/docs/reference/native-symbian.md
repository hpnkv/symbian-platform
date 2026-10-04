# Native Symbian interfaces

Symbian applications call OS services through public C++ headers and numbered
exports in system DLLs. The headers describe types and function signatures;
the selected firmware decides which exports and service behavior are present.
This project offers a [curated Doxygen reference](../cpp/platform/index.html)
to original declarations alongside its [SDK C++ reference](../cpp/index.html).

## Find the right header

| Goal | Original header | Read with |
| --- | --- | --- |
| Handle process, thread or time state | `e32std.h` | [Runtime support](../capabilities/runtime.md) |
| Use descriptors, leaves or cleanup | `e32cmn.h`, `e32base.h` | [C++ usage](../capabilities/cpp.md) |
| Create a window and receive input | `w32std.h`, `gdi.h` | [GUI tutorial](../tutorials/gui-app.md) |
| Open a file or query a volume | `f32file.h` | [Storage API](../capabilities/apis/storage.md) |
| Read HAL attributes or camera contracts | `hal.h`, `ecam.h` | [Device API map](../capabilities/device-apis.md) |
| Request secure random bytes | `e32math.h` | [TLS guide](../guides/tls.md) |

Open the Doxygen file list from the [original header reference](../cpp/platform/index.html)
and search for the named class or function. For example, Window Server is the
OS service that owns application windows and routes drawing and input events;
`RWsSession` in `w32std.h` is a client connection to it. EUSER is the core
user library and provides process, thread, time and handle APIs. An `L` suffix
marks a Symbian call that can leave; a `TRAP` boundary converts that failure
into an error code. Descriptors carry length and capacity with their character
data, so callers must use the correct 8-bit or 16-bit variant.

The snapshots preserve upstream notices and are scoped to nine useful headers.
They are not a complete platform SDK. Some declarations need further include
files or import proxies before they can compile in a new project. The
[capability map](../capabilities/index.md) and [status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
state which paths have execution evidence. ARM output or emulator execution is
not proof of Nokia 808 compatibility.
