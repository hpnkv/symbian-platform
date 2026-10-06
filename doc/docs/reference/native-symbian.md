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
`share/symbian/portable/zlib.json` separately records source-built zlib; it is
an optional third-party dependency, not a firmware API.
`share/symbian/portable/png.json` likewise records source-built libpng and
its dependency on portable zlib.
The inventory still records unresolved exports and Belle FP2 version
equivalence as open work; do not infer a runtime guarantee from a header.

The `_classic` examples provide a native Window Server interface with Run
and Close buttons. They describe the original API call, show a specific result
or error after Run, and keep the window open until Close is pressed. The bitmap,
image, PNG, JPEG and FreeType examples also show pixels produced by their
respective features. The Qt GUI example uses Qt buttons for its QByteArray
check and Close action.

| Include | CMake target | Notes |
| --- | --- | --- |
| `mdaaudiosampleplayer.h` | `Symbian::Audio` | MDA playback and recording clients. |
| `mdaaudiooutputstream.h` | `Symbian::AudioStream` | Streaming audio. |
| `fbs.h` | `Symbian::Bitmap` | Font and bitmap server. |
| `imageconversion.h` | `Symbian::ImageConversion` | Image decoders and encoders. |
| `hwrmvibra.h` | `Symbian::Vibra` | Hardware Resource Manager vibration. |
| `hwrmvibrasdkcrkeys.h` | `Symbian::Vibra` | Public vibration Central Repository keys. |
| `sensrvaccelerometersensor.h`, `sensrvchannelconditionlistener.h` | `Symbian::SensorNative` | Public sensor data definitions and listener callbacks. |
| `mmf/server/mmfcodec.h`, `mmf/server/mmfdatapath.h` | `Symbian::MmfServerBase` | Original MMF server/codec base classes. |
| `mmf/server/mmfformat.h` | `Symbian::MmfFormatBase` | Original format plug-in base classes. |
| `speechrecognitionutility.h` | `Symbian::SpeechRecognition` | Original speech-recognition utility, with command/data imports transitively. |
| `msvrcpt.h`, `msventry.h` | `Symbian::Messaging` | Original message server/client data interfaces. |
| `msvoffpeaktime.h`, `msvschedulesend.h` | `Symbian::ScheduledMessaging` | Original scheduled-send interfaces. |
| `mtmuibas.h` | `Symbian::MessagingUiBase` | Message type module UI base classes. |
| `biouids.h`, `biocmtm.h` | `Symbian::BioTransport`, `Symbian::BioClient` | Original BIO/Smart Messaging interfaces. |
| `thttpfields.h` | `Symbian::WapPushUtils` | Original WAP Push HTTP field definitions. |
| `authority8.h`, `delimitedquery16.h`, `uriutils.h`, `wspdecoder.h` | `Symbian::Uri` | Original InetProtUtil public URI, parser, WSP and date headers. |
| `babackup.h`, `baclipb.h`, `basched.h`, `barsread2.h` | `Symbian::Bafl` | Original BAFL backup, clipboard, scheduler and resource utilities. |
| `ecom/implementationproxy.h`, `ecom/publicregistry.h`, `ecom/resolver.h` | `Symbian::ECom` | Original public plug-in registration and resolver headers. |
| `sdpdocument.h`, `sdpcodecstringconstants.h` | `Symbian::SdpCodec` | Original SDP codec and generated string table. |
| `sipaddress.h`, `sipstrconsts.h` | `Symbian::SipCodec` | Original SIP message codec and generated string table. |
| `sip.h`, `sipconnection.h` | `Symbian::SipClient` | Original SIP session and transaction client; pulls in the codec. |
| `sipprofile.h`, `sipprofileregistry.h` | `Symbian::SipProfiles` | Public profile client; links `Symbian::SipProfileCore` and SIP client imports. |
| `xml/parser.h`, `xml/matchdata.h` | `Symbian::Xml` | Original XML framework. |
| `stdapis/libxml2/libxml2_parser.h` | `Symbian::XmlEngine` | Original public libxml2 2.6.10 API, backed by `xmlengine.dll`. |
| `xml/utils/xmlengutils.h` | `Symbian::XmlEngineUtils` | XML engine string and memory utilities. |
| `xml/dom/xmlengdocument.h` | `Symbian::XmlDom` | Original DOM/XPath API. |
| `xml/dom/xmlengserializer.h` | `Symbian::XmlSerializer` | Original DOM serialization; links DOM and XML framework imports. |
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
| `zlib.h`, `zconf.h` | `Symbian::PortableZlib` | zlib 1.3.1 static archive and matching headers under `include/portable/zlib`. |
| `png.h`, `pngconf.h`, `pnglibconf.h` | `Symbian::PortablePng` | libpng 1.6.53 static archive and matching headers under `include/portable/png`; depends on portable zlib. |
| `jpeglib.h`, `jconfig.h`, `jmorecfg.h`, `jerror.h` | `Symbian::PortableJpeg` | IJG libjpeg 8c static archive and matching headers under `include/portable/jpeg`. |
| `ft2build.h`, `freetype/freetype.h` and applicable `freetype/*.h` | `Symbian::PortableFreeType` | FreeType 2.13.2 static archive and 54 public/configuration headers under `include/portable/freetype`; Mac-only `ftmac.h` is unavailable to Symbian guests. |

The SIP/SDP targets use the pinned public Symbian^3 headers and complete
frozen EABI definitions for `sdpcodec.dll`, `sipcodec.dll`, `sipclient.dll`,
`sipprofile.dll` and `sipprofilecli.dll`. A relocated installed SDK compiled
their public headers on ARMv5T and ARMv6 and linked an E32 consumer through
the profile and SDP targets. The named RM-807 Belle Z-drive contains those
DLL filenames, so selecting that complete drive passes configuration. The
current E32 inspector cannot parse those firmware DLL image profiles; ordinal
identity and execution of SIP/SDP operations remain unverified. Other
firmware and physical-device compatibility remain unknown.

The XML targets retain the historical `stdapis/libxml2/libxml2_*.h` spelling,
not a modern desktop libxml2 include layout. `Symbian::XmlEngine` supplies
the frozen `xmlengine.dll` import, with `XmlEngineUtils`, `XmlDom` and
`XmlSerializer` for the original utility, DOM/XPath and serializer layers.
The public XML framework remains independently available through
`Symbian::Xml`. Two source headers labeled `@publishedPartner` are staged
only as required textual support and are not selectable public APIs. A
relocated SDK compiled the reviewed XML public exports on ARMv5T and ARMv6
and converted a linked XML consumer to E32. The named RM-807 Belle Z-drive
contains the XML DLL filenames; their ordinal equivalence and runtime
behavior remain unverified.

The MMF additions expose the original server and format plug-in base classes
through `Symbian::MmfServerBase` and `Symbian::MmfFormatBase`, each with its own
complete frozen import interface. `Symbian::MediaClient` also supplies the
public GSM audio declarations. Sensor channel listener and data-definition
headers follow `Symbian::SensorNative`; vibration and power metadata follow
their existing hardware-resource targets. Speech recognition uses three
original DLL interfaces: `SpeechRecognition`, `SpeechRecognitionCommands`
and `SpeechRecognitionData`. The selected RM-807 Belle Z-drive contains the
MMF and speech DLL filenames, but header compilation and E32 conversion do
not establish that every sensor, codec or speech service is present or usable.

Messaging adds the original `msgs.dll` client/store headers and separate
`sendas2.dll`, `mtur.dll`, `schsend.dll`, BIO and WAP Push interfaces. The
`Symbian::ScheduledMessaging` target uses the V2 frozen definition selected
by its MMP, including absent ordinal slots. `Symbian::BioClient` pulls in
its BIO database, utility and transport imports. The old Send UI facility
still lacks a reviewed frozen import identity and is rejected on selection.
The selected RM-807 Belle Z-drive contains the delivered DLL filenames, but
messaging operations and compatibility with other firmware remain unverified.

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
The separate `genericopenlibs/cstdlib` manifest exports a legacy `libc/*`
tree. Direct C canaries found missing private include prerequisites and type
conflicts with the selected Open C layout, so these duplicate headers remain
unowned in the inventory pending a reviewed layout and ABI contract.
The `examples/openc_app_classic` project uses the original libc functions
directly.

`Symbian::PortableZlib` is a separate source-built zlib 1.3.1 target with
matching `<zlib.h>` and `<zconf.h>` headers. It uses the guest runtime and
Open C transitively. Link either this target or the original
`Symbian::Native_libz` device import in one binary; selecting both fails
configuration because their C symbols overlap. The portable archive has no
firmware `libz.dll` dependency. `examples/zlib_app_classic` performs a
compress/decompress/CRC round trip; it exited normally on the preserved
RM-807/Dynarmic fixture. Other firmware and physical-device execution remain
unknown. The installed manifest pins every zlib source digest and the license
notice, and bundle validation requires both architecture archives.

`Symbian::PortablePng` provides upstream libpng 1.6.53 as a separate static
dependency. It propagates `Symbian::PortableZlib`, which supplies the matching
zlib headers, library and Open C/runtime closure. The SDK ships its own
prebuilt upstream `pnglibconf.h` alongside `png.h` and `pngconf.h`; no firmware
PNG codec is imported. `examples/png_app_classic` encodes and decodes one RGBA
pixel entirely in memory. It exited normally on the preserved RM-807/Dynarmic
fixture. Full codec coverage, other firmware and physical-device behavior
remain unverified.

`Symbian::PortableJpeg` provides the original IJG libjpeg 8c source bundled
with the pinned Qt 4.8.1 release as an independent static library. It ships
matching `jpeglib.h`, `jconfig.h`, `jmorecfg.h` and `jerror.h` under
`include/portable/jpeg`, and links Open C and the guest runtime. Include
`<stddef.h>` and `<stdio.h>` before `<jpeglib.h>`, as the original header
expects `size_t` and `FILE` from its caller. `examples/jpeg_app_classic`
encodes and decodes a pixel in memory without a firmware JPEG DLL import.
That round trip exited normally on the preserved RM-807/Dynarmic fixture.
Broader codec behavior, other firmware and physical devices remain unverified.

`Symbian::PortableFreeType` ships source-built FreeType 2.13.2 with its
original `ft2build.h` and `freetype/` include layout. The FreeType License and
alternative GPL notices are preserved. Its build disables optional external
zlib, bzip2, libpng, HarfBuzz and Brotli dependencies; the SDK target still
supplies Open C and runtime links. `examples/freetype_app_classic` loads an
embedded BDF font and renders a glyph without a firmware font-service import.
The upstream `freetype/ftmac.h` is omitted from the guest payload because it
requires Classic Mac OS `Handle`, `FSSpec` and `FSRef` types; the inventory
records that unavailable header explicitly.
That bounded example exited normally on the preserved RM-807/Dynarmic fixture;
other font formats, firmware and physical-device behavior remain unverified.

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

`Symbian::Uri` supplies original URI/escape utilities and frozen
`inetprotutil.dll` imports. `Symbian::HttpNative` and `Symbian::Mime` remain
separate original targets; their representative public headers also compile
independently. `examples/uri_app_classic` parses a fixed HTTPS URI and checks
its host using `TUriParser8`. It exited normally on the named RM-807/Dynarmic
fixture without making a network request. HTTP transport, browser integration
and other firmware require separate validation.
