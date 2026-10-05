# Device API source layout

`include/symbian/api/` is the public, modern C++ surface. The matching source
directory implements each component and calls only SDK-owned native bridges.
Legacy Symbian headers stay in the bridge translation units under
`cpp/symbian/runtime/` until a component needs a separate bridge. This avoids
mixing Symbian placement-new and descriptor declarations with modern libc++.

Every implemented capability is an independent static archive with its own
CMake target. `Symbian::System` provides typed native counter readings;
`Symbian::Power` and `Symbian::Display` expose HAL snapshots; and
`Symbian::Storage` owns File Server handles for streaming reads and explicit
writes. `Symbian::Camera` exposes a typed inventory snapshot.
`Symbian::Connectivity` offers a bounded worker-facing TCP client through the
original Socket Server. See the [device API guide](../device-apis.md) for the available components.

Each component article records its native starting point, the modern C++
ownership, threading, performance considerations and restrictions:

| Implemented archive | Component notes |
| --- | --- |
| `Symbian::System` | [system](system.md) |
| `Symbian::Power` | [power](power.md) |
| `Symbian::Display` | [display](display.md) |
| `Symbian::Storage` | [storage](storage.md) |
| `Symbian::Camera` | [camera](camera.md) |
| `Symbian::Connectivity` | [connectivity](connectivity.md) |

The [sensors](sensors.md) and [media](media.md) notes describe proposed models
only. No target or header is exported for those components yet.

## Link only the services you use

A note-taking application that stores files and checks power can select just
those components:

```cmake
target_link_libraries(my_app PRIVATE Symbian::Storage Symbian::Power)
```

Use [storage](storage.md#save-a-small-draft) for draft persistence and
[power](power.md#decide-whether-to-postpone-background-sync) for a background-sync
policy. Their errors and unknown values remain explicit in application code.

## Original API escape hatch

The installed SDK also retains the original EPL-licensed platform headers
under `include/platform` and frozen import proxies under `proxies/`. An
application that needs a File Server operation outside `Symbian::Storage` may
opt in to `<f32file.h>` and the `efsrv` proxy; one that needs `User::` may opt
in to `<e32std.h>` and the `euser` proxy. These are separate, explicit legacy
translation units. The application then owns every `RFs`, `RFile` and `RDir`
handle it opens, closes it on the same thread after pending requests drain,
and keeps descriptors valid for the whole native call. The modern component
owners do not lend out their private raw handles: borrowing one across a move,
close or worker hop would obscure its lifetime. Normal application code uses
the typed component API and does not need the legacy headers.
