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
| `stdio.h`, `stdlib.h`, `string.h` | `Symbian::OpenC` | Original Open C `libc.dll` imports; C and pthread prerequisites follow transitively. |
| `apgcli.h`, `apgtask.h` | `Symbian::AppArc` | Application list and task services. |
| `eikapp.h`, `eikappui.h` | `Symbian::Eikon` | Eikon application framework. |
| `coecntrl.h`, `coemain.h` | `Symbian::Cone` | CONE controls and environment. |

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

The original Open C header export uses the familiar `<stdio.h>` and
`<sys/...>` spellings. Its `Symbian::OpenC` target uses the guest software
floating-point ABI and frozen `libc.dll` ordinals. The
`share/symbian/native/openc-header-usage.json` map records headers that need
historical prerequisite includes or C++ mode. `netinet6/in6.h` explicitly
requires inclusion through `<netinet/in.h>`. The exported `sys/event.h`
cannot be compiled from this source release because it contains an undefined
`struct klist`; the frozen libc interface also lacks `kqueue` and `kevent`.
It remains recorded as blocked rather than presented as a working event API.
The `examples/openc_app_classic` project uses the original libc functions
directly.

AppArc, Eikon and CONE retain separate targets, original frozen imports and
their own public headers. The nine representative framework headers compile
independently with their owner targets. `examples/apparc_app_classic` opens an
`RApaLsSession` through the original AppArc API. Avkon's complete frozen
`avkon.dll` interface is retained, but selecting `Symbian::Avkon` currently
fails during CMake configuration: its public `AknUtils.h` needs generated
`avkon.rsg`, and the original resource manifest requires generated icon and
Eikon resource headers that are not yet available from a reviewed SDK export.
This prevents an Avkon example from being presented as functional.

`Symbian::CentralRepository` supplies `<centralrepository.h>` and the frozen
`centralrepository.dll` imports. `Symbian::CenRepNotification` supplies
`<cenrepnotifyhandler.h>` and depends on the Central Repository target.
`examples/central_repository_app_classic` is a read-only original-API consumer;
its SIP repository and timer-key IDs come from the original public
`<sipsdkcrkeys.h>`, now delivered with the Central Repository target.
The read returned a positive timer value and exited normally in the preserved
RM-807/Dynarmic emulator. Other firmware and physical-device behavior remain
unknown.

`Symbian::Bafl` supplies the original resource-reader and file-utility
headers and transitively selects `Symbian::FileServer` and
`Symbian::StreamsNative`. `examples/bafl_app_classic` checks a public file
through `BaflUtils::FileExists` and reads an in-memory stream while linking
only BAFL. Both operations exited normally in the preserved RM-807/Dynarmic
emulator. The example's file path is fixture-specific; other firmware remains
unverified.

`Symbian::Calendar` provides the original 23 public Interim API headers and
the complete frozen `calinterimapi.dll` interface from the default MMP branch.
`examples/calendar_app_classic` installs an active scheduler, opens a
`CCalSession` and lists calendar filenames without changing them. That path
exited normally in the preserved RM-807/Dynarmic emulator. The original MMP
selects a different DEF under `SYMBIAN_CALENDAR_ENHANCEDSEARCHANDSORT`;
complete Belle FP2 ABI equivalence and other firmware availability remain
unknown.

`Symbian::Messaging` supplies the original `<msvapi.h>`, `<msvstd.h>` and
`<mtclbase.h>` headers with frozen `msgs.dll` imports. The direct
`examples/messaging_app_classic` consumer opens and closes a `CMsvSession`
under an active scheduler. It exited normally on the named RM-807/Dynarmic
fixture. SMS, MMS, email protocols, message contents and other firmware
remain separate unverified scope.
