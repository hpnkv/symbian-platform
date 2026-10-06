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
| Create a window and receive input | `w32std.h`, `gdi.h` | [Build a real GUI app](../guides/gui-build.md) |
| Open a file or query a volume | `f32file.h` | [Storage API](../capabilities/apis/storage.md) |
| Read HAL attributes or camera contracts | `hal.h`, `ecam.h` | [Device API map](../capabilities/device-apis.md) |
| Request secure random bytes | `e32math.h` | [TLS guide](../guides/tls.md) |
| Connect a TCP socket | `es_sock.h`, `in_sock.h` | [Native TCP client](../capabilities/apis/connectivity.md) |

Open the Doxygen file list from the [original header reference](../cpp/platform/index.html)
and search for the named class or function. For example, Window Server is the
OS service that owns application windows and routes drawing and input events;
`RWsSession` in `w32std.h` is a client connection to it. EUSER is the core
user library and provides process, thread, time and handle APIs. An `L` suffix
marks a Symbian call that can leave; a `TRAP` boundary converts that failure
into an error code. Descriptors carry length and capacity with their character
data, so callers must use the correct 8-bit or 16-bit variant.

The installed SDK retains the original public header spellings and frozen
ordinal imports. Link the owning `Symbian::` target; it supplies public
include directories, target ABI flags and required libraries. The
machine-readable `share/symbian/native/inventory.json` maps each reviewed
header and DLL to a target, classification, source revision and current block
reason. `share/symbian/qt/modules.json` does the same for Qt 4.8.1 public
modules, and `share/symbian/qtmobility/inventory.json` covers Qt Mobility 1.0.3.
The inventory still records unresolved exports and Belle FP2 version
equivalence as open work; do not infer a runtime guarantee from a header.

| Include | CMake target | Notes |
| --- | --- | --- |
| `mdaaudiosampleplayer.h` | `Symbian::Audio` | MDA playback and recording clients. |
| `mdaaudiooutputstream.h` | `Symbian::AudioStream` | Streaming audio. |
| `fbs.h` | `Symbian::Bitmap` | Font and bitmap server. |
| `imageconversion.h` | `Symbian::ImageConversion` | Image decoders and encoders. |
| `hwrmvibra.h` | `Symbian::Vibra` | Hardware Resource Manager vibration. |
| `QtNetwork/QHostAddress` | `Symbian::QtNetwork` | Original Qt 4.8.1 guest module. |
| `QtSql/QSqlDatabase` | `Symbian::QtSql` | Original Qt SQL; drivers are separate runtime plugins. |
| `QtXml/QDomDocument` | `Symbian::QtXml` | Original Qt XML. |
| `QtWebKit/QWebSettings` | `Symbian::QtWebKit` | Original Qt WebKit 4.9.0; brings QtOpenGL, Network, XmlPatterns and Script. |
| `QtOpenGL/QGLFormat` | `Symbian::QtOpenGL` | GLES 2 and EGL public imports. |
| `QtContacts/QContact`, `QtContacts/qcontact.h` | `Symbian::QtMobilityContacts` | Original Contacts class alias and flat header. |
| `QtLocation/QGeoCoordinate` | `Symbian::QtMobilityLocation` | Position and mapping declarations. |
| `QtMultimediaKit/QMediaPlayer` | `Symbian::QtMobilityMultimediaKit` | Mobility media API; distinct from QtMultimedia. |
| `QtVersit/QVersitContactExporter` | `Symbian::QtMobilityVersit` | Includes Mobility Contacts transitively. |

Qt targets also include `Symbian::QtCore`, `QtGui`, `QtSvg`, `QtScript`,
`QtXmlPatterns`, `QtDeclarative`, `QtMultimedia`, `QtOpenVG` and `QtTest`.
The original Qt release manifest makes several modules and image/SQL plugins
conditional; this SDK supplies their public import interfaces, not plugin
implementations. The bundled licenses are LGPL 2.1, its Qt exception and the
FDL notice. Qt Mobility adds `Bearer`, `Contacts`, `Location`, `Messaging`,
`MultimediaKit`, `PublishSubscribe`, `Sensors`, `ServiceFramework`,
`SystemInfo` and `Versit` under `Symbian::QtMobility*`. It stages 182 original
public headers, their class aliases and ten frozen EABI import interfaces.
`QtContacts/qcontactringtone.h` needs `<QUrl>` included first, and
`QtMessaging/qmessagedatacomparator.h` needs `<qmobilityglobal.h>` first; the
source-backed inventory records these original-header exceptions. Historical
source inclusion does not establish the version on every Belle firmware or
plugin/service availability. See [the capability map](../capabilities/index.md) for modern wrappers
that remain independent of these original APIs.
