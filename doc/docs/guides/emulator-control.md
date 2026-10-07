# Native emulator capture and input

Use the patched EKA2L1 frontend to capture the guest display, send logical
pointer events and inspect native process exits. The control endpoint belongs
to a local disposable emulator session; it provides no physical-device control.

## Implementation and lifecycle

`cpp/symbian/emulator` is a separate GPL-3.0-or-later native research adapter.
`guest-control.patch` links it only into the research Qt frontend through the
injected CMake hook. The host Python extension and wheel do not link EKA2L1 or
this adapter. Its native code uses Abseil Status/StatusOr, C++20 and disabled
exceptions, with the repository's formatting and layout conventions.

Capture calls the original graphics driver's `read_bitmap` on `screen_texture`,
then encodes PNG in native Qt code. It captures the guest display, excluding the
host desktop and frontend overlays. Pointer coordinates are logical guest
coordinates; events enter the same Window Server driver queue as normal
frontend input. A `queued` response acknowledges enqueueing; check the next capture for
visible delivery. Process exit records copy scalar/string
values from the real kernel callback, avoiding dangling process pointers.

The Qt thread holds the frontend state lock while validating pointers/captures.
Kernel access uses a try-lock and reports UNAVAILABLE while busy. Capture takes
a protected screen snapshot and releases kernel/screen locks before waiting for
graphics. Pointer delivery releases its validation lock before the existing
Window Server path acquires the kernel lock internally. Teardown detaches the
callback before the OS worker destroys the kernel, saves a final report, then
stops the existing workers. Pending replies drain with at most 100 ms per
accepted socket after the Qt loop stops.

## Start a controlled disposable instance

Build the [patched frontend](../reference/emulator-source-build.md) and
prepare a disposable instance using the [firmware guide](firmware.md).
For the RM-807 profile the virtual C mapping is `data/drives/rm-807/c`.
Use `cpu: dynarmic` or `cpu: dyncom` in that copy's config.

From the repository root, after setting `SYMBIAN_GUI_INSTANCE` to that private
copy and `SYMBIAN_EMULATOR` to the built executable:

```sh
SYMBIAN_CONTROL_DIR=$(mktemp -d /tmp/symbian-control.XXXXXX)
export SYMBIAN_CONTROL_SOCKET="$SYMBIAN_CONTROL_DIR/control.sock"
printf '%s\n' "$SYMBIAN_CONTROL_SOCKET"
EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" \
EKA2L1_EXPERIMENTAL_SVC_PROFILE=rm807-113.010.1508 \
EKA2L1_RESEARCH_CONTROL_SOCKET="$SYMBIAN_CONTROL_SOCKET" \
"$SYMBIAN_EMULATOR" --device RM-807 --run 'C:\sys\bin\gui_app.exe'
```

Use a second terminal to set `SYMBIAN_CONTROL_SOCKET` to the printed path:

```sh
symbian emu status --endpoint "$SYMBIAN_CONTROL_SOCKET"
symbian emu screenshot --endpoint "$SYMBIAN_CONTROL_SOCKET" --name initial
symbian emu pointer --endpoint "$SYMBIAN_CONTROL_SOCKET" 64 575 press
symbian emu pointer --endpoint "$SYMBIAN_CONTROL_SOCKET" 96 575 move
symbian emu pointer --endpoint "$SYMBIAN_CONTROL_SOCKET" 64 575 release
symbian emu key --endpoint "$SYMBIAN_CONTROL_SOCKET" left press
symbian emu key --endpoint "$SYMBIAN_CONTROL_SOCKET" left release
symbian emu screenshot --endpoint "$SYMBIAN_CONTROL_SOCKET" --name after-tap
```

Allow guest startup/redraw to complete between commands. A busy response means
UNAVAILABLE before delivery; it can be retried. A client timeout does not cancel
native work, so do not blindly repeat a pointer request after an ambiguous
failure. The live test retries only explicit UNAVAILABLE responses.

For the current 360x640 logical layout, reset is centered at `(180,575)` and exit
at `(296,575)`. Exit is handled on press; do not send release to the closed
window. Captures are 720x1280 with this fixture's display scale two;
pointer coordinates remain 360x640. These coordinates apply to this portrait layout; adjust them for other
orientations or display scales. Current captured control colors
are green, blue and purple, reflecting the source SDK's packed TRgb ordering.
The operations are increment/reset/exit regardless of color.

After the guest exit the frontend closes its endpoint. Read its explicit saved
status with:

```sh
symbian emu status --endpoint "$SYMBIAN_CONTROL_SOCKET" --saved
```

The native report is `$SYMBIAN_CONTROL_SOCKET.status.json`; it retains the exit
record after shutdown. Without `--saved`, status requires a live endpoint and
never silently returns an old report. Keep the native report, matching PNGs,
frontend log and build/input digests together. Do not infer normal guest exit
from the host's exit code alone.

## Bounds and protocol

Control is disabled without `EKA2L1_RESEARCH_CONTROL_SOCKET`. An enabled path
must be absolute, at most 100 UTF-8 bytes and in an existing directory owned by
the current user with no group/other access. This parent-directory protection
is essential on macOS: Qt's socket access flags alone do not restrict access
there. See [QLocalServer](https://doc.qt.io/qt-6/qlocalserver.html).

One compact JSON object plus newline is accepted per connection, at most 4096
bytes. There are at most 16 active clients with a five-second idle bound. The
operations are `status`, `capture`, `pointer`, `key` and `task_close`; all others return
UNIMPLEMENTED. There is no hardware transport, guest command runner or arbitrary
filesystem path argument. Responses use `symbian.emulator-control/v1` and
canonical Abseil status codes. Status retains at most 256 process exit records.

`pointer` accepts `press`, `move` and `release` within the current logical
screen. `key` accepts `left`, `right`, `up`, `down`, `select`, `enter`, `space`,
`back` and `menu`, each with `press` or `release`. These are named guest scan
codes sent through the Window Server input queue. A queued reply confirms
submission; inspect a capture or guest event to verify delivery.

`task_close` queues AppArc's normal shutdown user event. Without a UID it
targets the focused window group; with a 32-bit application UID it finds that
application's group even when the system menu has focus. Python exposes these
as `Control.close_focused_task()` and `Control.close_task(uid)`. A queued reply
does not prove the app exited; read the saved process exit report afterward.

Captures accept only 1–64 ASCII letters, digits, hyphens or underscores as a
basename, refuse existing files/symlinks, and write into the private socket
parent. Native image dimensions are bounded at 4096 per axis. Publication uses
[QSaveFile](https://doc.qt.io/qt-6/qsavefile.html). Existing final reports also
prevent reusing an endpoint name. These are local accidental-overwrite bounds,
not an immutable archive against the owning user. A timed-out client cannot
interrupt a stalled graphics driver; the launcher must enforce a process deadline if graphics stops responding.
