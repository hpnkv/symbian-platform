# Symbian SDK 0.3.0

This release adds an installed ARMv5T older-EKA2 C++ runtime profile alongside
the existing EKA2 runtime and streams profiles. Select
`SYMBIAN_RUNTIME_LEGACY_EKA2=ON` when configuring an ARMv5T application. The
`Symbian::LegacyEka2` target supplies bounded C, pthread, math, UTF-8,
formatting, calendar, file and time compatibility without requiring Open C
`libc.dll`, `libpthread.dll` or `libm.dll`. It uses checked ESTLIB ordinals and
the shared RHeap backend. The build exports this profile reproducibly and
checks it in the installed SDK release gate.

Portable SDL2 2.30.11 and SDL3 3.2.22, Window Server display/input, optional
GLES2 rendering and software fallback, task and focus handling, and the game
feedback API are included. The GPU archives require EGL/GLES2 at load time;
use the software archives on older ROMs. The SDK also exposes move-only file
and directory owners, native timer/frame pacing APIs, and strict portable
UTF-8/UTF-16 conversion. The text helpers accept supplementary scalars and
return `InvalidArgument` for malformed input.

Source-built SDL2 and SDL3 images passed the executable firmware matrix on
E71/RM-346 and 6120c/RM-243 with ARMv5T software rendering, and on
C7/RM-675, E6/RM-609 and Nokia 808/RM-807 with ARMv6 GPU rendering. Each run
recorded direct imports, fixture and image digests, loader acceptance, a frame
capture, and normal guest exit after AppArc close. The RM-807 executive profile
is restricted to its exact verified ROM/EUSER pair. The installed-SDK ARMv5T
local-static/thread/streams/UTF-8 consumer and both SDL applications also
built and exited normally on E71 and 6120c under EKA2L1. The installed
consumer exercised `absl::StrFormat` and strict UTF-8 conversion. These
installed runs are recorded separately from the source-workspace matrix.
An ARMv5T local-static/thread/C compatibility probe imported only EUSER,
ESTLIB and DRTAEABI and exited normally on both older ROM fixtures. Its
two workers observed one local-static construction; recursive or failed
initialization is not covered.

**Coverage remains incomplete.** The C++ runtime still uses disabled
exceptions and the SDK's explicit
`absl::Status`/`StatusOr` policy. General ELF `thread_local`, DLL TLS teardown,
arbitrary locale, full POSIX `errno`/pthread semantics and general DLL lifetime
ordering are not supported. Ordinary allocation failure exits with
`KErrNoMemory`; nothrow allocation returns null. Native timers must be cancelled
and drained before their buffers or owners are destroyed. SDL audio and haptic
device backends are dummy; SDK MIDI/tactile services are optional and may fail.
The E71 emulator's digitiser report is inaccurate, so Back/right softkey is
the reliable menu route there.

All firmware results above are emulator results. No 0.3.0 package has been
installed or run on a physical device. Firmware, private device data, emulator
state and upstream checkouts are not part of the release archive.
