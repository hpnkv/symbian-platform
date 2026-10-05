# Original Symbian platform headers

This is a **curated snapshot of original public Symbian declarations** used to
understand the OS side of an application. It is separate from the
[SDK-owned native reference](../index.html). Use the file list and search box
to explore classes and functions; use the table below to choose a starting
header.

| Need | Header | Typical declarations |
| --- | --- | --- |
| Process, time, handles and errors | `e32std.h` | `User`, `RProcess`, `RThread`, `TTime` |
| Cleanup and active objects | `e32base.h` | `CBase`, `CActive`, `CActiveScheduler` |
| Descriptors, leaves and common types | `e32cmn.h` | `TDes8`, `TDesC16`, `TRAP` |
| Cryptographic random request and math | `e32math.h` | `Math::RandomL`, numeric helpers |
| Windows and input events | `w32std.h` | `RWsSession`, `RWindow`, `TWsEvent` |
| Drawing | `gdi.h` | graphics contexts, colors and geometry |
| Files and volumes | `f32file.h` | `RFs`, `RFile`, volume information |
| Hardware attributes | `hal.h` | `HAL::Get`, attribute identifiers |
| Camera framework | `ecam.h` | `CCamera` and observer contracts |
| Socket Server | `es_sock.h` | `RSocketServ`, `RSocket`, native request cancellation |
| Internet sockets | `in_sock.h` | `TInetAddr`, IPv4 and IPv6 protocol constants |

These declarations describe services supplied by the target firmware. Check the
SDK export index and your firmware's imports before using an original API. For
an application example, see the
[GUI application guide](../../guides/gui-build.html), the
[C++ application guide](../../capabilities/cpp.html), and the
[device API capability map](../../capabilities/device-apis.html).

These files preserve the upstream copyright and Eclipse Public License 1.0
notices in each header. `e32*`, `f32file.h` and `hal.h` come from the
[SymbianSource kernel repository](https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv)
at `0c3208650587ac0230aed8a74e9bddb5288023eb`; `w32std.h` and `gdi.h`
come from the
[graphics repository](https://github.com/SymbianSource/oss.FCL.sf.os.graphics)
at `ff133bc50e6158bfb08cc093b0f0055321dcde99`; `ecam.h` comes from the
[camera framework repository](https://github.com/SymbianSource/oss.FCL.sf.mw.camerasrv)
at `ebaa78373866f90dbf706e8d4eeb59ff65f1e107`. Header filenames were
lowercased for the documentation snapshot; contents were not edited.
`es_sock.h` comes from the
[commsfw repository](https://github.com/SymbianSource/oss.FCL.sf.os.commsfw)
at `bc8ac1a6d5273cbfa7852bbb8ce27d6ddc076984`; `in_sock.h` comes from
the [networkingsrv repository](https://github.com/SymbianSource/oss.FCL.sf.os.networkingsrv)
at `b283ce17f27f4a95f37cdb38c6ce79d38ae6ebf9`.
