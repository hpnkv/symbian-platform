# Symbian SDK 0.2.0

This release broadens the original native Symbian API surface available to
ARMv5T and ARMv6 applications. CMake targets supply public headers, frozen
DLL import interfaces and reviewed target dependencies; the SDK retains
original include spellings and export ordinals, including holes. Firmware DLL
implementations are not redistributed.

The source-backed native inventory covers 2,694 public export records and 213
facility records. It stages 187 frozen import interfaces. Added families span
MDA/MMF audio and video, bitmap and image services, fonts, S60 application
frameworks, Bluetooth and network services, sensors and hardware resources,
contacts/calendar/messaging, HTTP and URI utilities, XML, Versit/vCard/vCalendar,
CryptoSPI, RemCon, location, backup and other public utilities. The installed
`share/symbian/native/inventory.json` maps headers and DLLs to `Symbian::`
targets, source revisions, licenses, dependencies and availability.

The SDK also includes Open C facilities, Qt 4.8.1 and available Qt Mobility
modules, plus separately identified source-built zlib, libpng, libjpeg and
FreeType dependencies. The native examples remain available in the main CMake
project. `examples/linking_app` demonstrates SDK-managed static and dynamic
library linking and explicit DLL packaging selection.

Local validation compiled 1,775 independently selected public-header objects
for each ARM profile, checked the staged frozen-import payload and passed 56
focused inventory/surface tests. Earlier slices linked and converted
representative installed-SDK consumers to E32. The release workflow builds
and audits host and native archives for macOS and Linux.

**Coverage remains incomplete.** The inventory records 952 public export
records without reviewed delivery/target ownership. These include 320
historical STLport/Open C++ records whose runtime ABI is unverified with the
selected Clang/libc++ toolchain, 65 conflicting legacy C-header exports,
62 exports with unresolved include-root layout, 16 differing source variants,
one absent source file and 488 further ownership cases. `Gsm` and `Sms`
selection remains blocked by a partner-only ETel Multimode prerequisite.
Portable SDL is not included. Header compilation, import linking and E32
conversion do not establish that an API executes on Belle or another firmware.
New breadth-pass calls, firmware ordinal equivalence and physical-device
compatibility remain unverified; no physical hardware was operated.

**Post-publication findings (2026-10-07).** On the preserved RM-807 emulator
fixture, the shipped bitmap, image-conversion and FreeType examples displayed
their output and exited normally. The shipped vibration example panics with
`E32USER-CBase` reason 44 when Run is pressed because its calling thread lacks
an active scheduler; the fix is on `main` after this tag. The audio-stream
example panics with reason 46 on both tested emulator CPU backends. A separate
MDA `KeepOpenAtEnd()` contract fix is on `main`, but the audio panic persists.
These findings do not establish behavior on physical devices.
