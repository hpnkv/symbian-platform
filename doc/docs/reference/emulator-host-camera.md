# EKA2L1 host camera in Camera GL

Camera GL now receives real host-camera frames through EKA2L1's Qt Multimedia
backend, the guest ECam API and the SDK GLES2 texture path. Validation on
12 October 2026 used macOS arm64, Qt 6.11.2, the named RM-807 Belle fixture and
both Dynarmic and Dyncom. The release build still pins Qt 6.8.3; its CI module
installation and bundle checks have been updated, but Linux and that Qt version
were not executed locally for this change.

## Blocking paths

| Observed problem | Fix |
| --- | --- |
| The SDK emulator build forced `EKA2L1_ENABLE_QT_CAMERA=OFF`; Qt Multimedia was also absent locally. Guest discovery returned zero and Camera GL used synthetic pixels. | Enable the backend, install Multimedia in CI, require it at configure time, document native dependencies and include runtime multimedia plugins. |
| SDK background sessions launch a symlink outside the `.app`. Qt rejected the camera permission request because that launch could not find `NSCameraUsageDescription`. | Embed the generated Info.plist in the executable, preserving the privacy declaration for both launch paths. Match the bundle identifier used by distribution packaging. |
| With cameras discovered, the guest crashed reading `0x3AC0` while EKA2L1's replacement ECam DLL left with `KErrNotSupported` from formatted viewfinder start. | Relocate the replacement DLL's exception-descriptor offset when loading it into the emulator's ROM region. The old descriptor offset was `0x3AB8`; its third word was incorrectly read as address `0x3AC0`. The SDK can now catch that leave and select its bitmap fallback. |
| Bitmap capture used `EColor16MA` (mode 12). The SDK's optimized path accepted `EColor16MU` and RGB565, so this layout called `GetPixel` for all 307,200 pixels of each 480×640 frame. | Use the same B,G,R buffer path for straight-alpha `EColor16MA`, ignoring alpha as the previous conversion did. Keep premultiplied `EColor16MAP` on its RGB recovery path. |

These changes are maintained in `distribution-camera.patch`, the emulator
build/deployment scripts and `native_camera_stream.cc`. There is no host-camera
substitute in the application or SDK. No camera device still means no real
capture; the application retains its existing synthetic discovery fallback.

## Measured evidence

The host enumerated **MacBook Pro Camera** and **iPhone Camera**. Guest camera 0
used the MacBook device and delivered **480×640** bitmaps. The original slow
conversion produced callbacks 31 and 61 at 10,961 ms and 21,184 ms after bitmap
start: 30 frames in 10,223 ms, about **2.9 callbacks/s**. With direct buffer
access, the corresponding Dynarmic interval was **998 ms**, about
**30 callbacks/s**. These diagnostic runs enabled native camera tracing and
used the same guest fixture and capture dimensions.

Final checks disabled native tracing and ran the actual deployment helper on
a separate bundle, audited its library closure and verified its code signature.
Both SDK background launches then passed:

| CPU backend | Guest interval to present 60 live frames | Exit |
| --- | ---: | --- |
| Dynarmic | 1,967 ms | Normal, reason 0 |
| Dyncom | 1,971 ms | Normal, reason 0 |

The test requires the live-frame marker and a completed GLES swap, then a
60-frame presentation report, a framebuffer capture, a close request, completed
camera shutdown and the native kernel exit record. Synthetic rendering cannot
satisfy the live-frame marker. The captured framebuffer was 720×1280, with a
360×640 guest drawing surface; those are presentation dimensions, distinct
from the 480×640 camera input.

These are emulator throughput measurements. They exclude firmware copying and
do not measure phone startup latency or establish physical-phone performance.
Raw text logs, hashes and exit metadata are under
`.symbian/host-camera-20261012/`; the research archive retains text evidence
without host-camera photographs.

## Replay

Build the emulator with the [maintained driver](emulator-source-build.md).
Allow EKA2L1 camera access when macOS requests it. Select an available default
host camera before launching Camera GL.

```sh
cmake --preset guest-probes-armv6-release-gpu \
  -DSYMBIAN_NATIVE_CONVERTER="$PWD/build/debug/cpp/symbian/tools/symbian-native"
cmake --build --preset guest-probes-armv6-release-gpu \
  --target camera_app_gles2_e32 symbian_header_canaries
SYMBIAN_CAMERA_HOST_WORKSPACE="$PWD" \
  .venv/bin/python -m pytest symbian/tests/test_camera_host_guest.py -q
```

The opt-in test requires the prepared fixture at
`.symbian/instances/delight-import-01` and the configured frontend at
`.symbian/emulator-control-build/build/bin/EKA2L1.app/Contents/MacOS/EKA2L1`.
Override these with `SYMBIAN_CAMERA_HOST_INSTANCE` and
`SYMBIAN_CAMERA_HOST_EMULATOR`. It copies the firmware into disposable instances
before launch and exercises the actual host camera on each backend in turn.

## Startup UI and full-screen geometry (12 October follow-up)

The blue startup surface came from Camera GL's explicit `(0, 0, 1)` clear
while waiting for its first frame; a trace-file failure changed it to magenta.
Camera GL now presents a dark camera glyph and three animated dots before
camera discovery and reservation, and keeps drawing it while startup is pending.
The SDK GLES rectangle batch draws directly into the EGL surface. Close/input
polling and the existing five-second camera deadline remain active.

Two independent geometry problems needed fixes:

* The application used `AspectFitViewport`, leaving bands. The measured old
  480×640 source on a 360×640 surface implies a 360×480 fitted image: 25% of
  the surface was unused. Camera GL now uses the SDK aspect-fill viewport;
  the software example borrows the SDK centered crop before resampling.
* The emulator's Qt backend used `Qt::IgnoreAspectRatio` to convert host frames
  to guest dimensions. Application geometry cannot undo that distortion.
  `camera-aspect.patch` preserves proportions, retains a complete source axis
  and crops the centered excess on the other. Rotation happens before sizing.

Portrait capture now prefers 360×640, followed by 480×640 and landscape
fallbacks. Landscape still prefers 640×360. Matching the display aspect first
avoids an unnecessary intermediate aspect change on the emulator. Native ECam
can select other sizes; presentation uses the returned frame dimensions.
For a 640×480 stream, a 640×360 screen retains the full width and rows 60–419;
a 360×640 screen retains the full height and columns 185–454. No zoom factor
is applied beyond the uniform scaling required to fill the display.

A deterministic 640×480 calibration image contains a centered 96×96 square
and coordinate gradients. The native Qt tests exercise both output shapes at
0°, 90°, 180° and 270°. All eight outputs preserve one complete source axis,
keep the square centered within one pixel and preserve its proportions within
one pixel. A separate negative control using the old stretch mode measured:

| Output, rotation | Old square bounds | Corrected square bounds |
| --- | --- | --- |
| 360×640, 0° | 54×126 | 127×126 |
| 640×360, 0° | 96×72 | 96×96 |

Bounds use a >240 brightness threshold after smooth resampling; the one-pixel
edge difference is sampling/rounding, not a second scale or zoom.

The real-camera test now captures and checks the waiting glyph, starts in both
orientations on both CPU backends, switches to the other mode and back during
capture, requires another 60-frame report after each switch, captures each
result and requires normal guest/frontend shutdown. Display changes use the
SDK emulator `Control.screen_mode` operation, exercising Window Server
notifications and EGL surface recreation rather than resizing screenshots.
The logical viewports are 360×640 and 640×360; framebuffer captures are
720×1280 and 1280×720. Final measured intervals and hashes are recorded in the
research archive under `camera-geometry-20261012`.

| Opening orientation / backend | 60-frame intervals: initial, rotated, returned |
| --- | --- |
| Portrait / dynarmic | 1973 ms, 2000 ms, 2034 ms |
| Portrait / dyncom | 1980 ms, 2000 ms, 2000 ms |
| Landscape / dynarmic | 1975 ms, 2000 ms, 2000 ms |
| Landscape / dyncom | 1980 ms, 2000 ms, 2000 ms |

All four cases passed (49.79 seconds total test duration, including fixture
setup). Host-polled waiting-to-first-frame observations were 1,052–1,109 ms;
these include capture-observation overhead and are not physical-phone startup
measurements.

Replay the calibration checks against the same patched emulator source:

```sh
cmake -S research/eka2l1/tests -B .symbian/emulator-camera-tests -G Ninja \
  -DEKA2L1_SOURCE="$PWD/.symbian/emulator-control-input-source" \
  -DEKA2L1_BUILD="$PWD/.symbian/emulator-control-build/build"
cmake --build .symbian/emulator-camera-tests --target camera_image_test
ctest --test-dir .symbian/emulator-camera-tests -R camera_image \
  --output-on-failure
```

These checks cover macOS arm64 Qt 6.11.2 and the named Belle emulator fixture.
They do not establish physical-phone camera orientation behavior or Linux
camera execution. Firmware copying is outside the observed camera interval.


## Initial rear sensor presentation and mode buttons (superseded below)

The default is now **Rear camera**, selected by the REAR overlay button.
Capture negotiates landscape dimensions once, even if opened in portrait.
The centered source crop is cached independently of display orientation.
`ViewfinderGeometry` describes its unrotated source rectangle, an orientation
and oriented dimensions for consumers; no source bytes or stride are changed
when those oriented dimensions swap. The GLES renderer compensates for the
Window Server's display rotation, maintaining the image's position on the
physical display. EGL surface refreshes still accommodate the display mode;
the camera buffer and source texture remain unchanged.

ROTATE selects the previous opening-orientation presentation policy. A mode
change may restart capture, while tapping an already selected mode does not.
Both examples draw the rounded buttons over the viewfinder and support
Left/Right selection as well as pointer input. The button group's center stays
anchored to the same physical-screen position. The rounded shapes and labels
rotate there; individual centers move inward in landscape to retain the same
small gap instead of leaving a large vertical space. Hit regions follow the
current layout. On the 360×640 RM-807 display, the group stays at physical
(180,584), with an eight-pixel gap. At 90° the buttons' logical centers are
(584,202) and (584,158), compared with (132,584) and (228,584) in portrait.
GLES rectangle batches now establish their own full-surface viewport and client-array binding, so an image's aspect-fill
viewport cannot distort the controls.

Native tests exercise a 640×480 fallback source cropped to 640×360 through all
four rotations: the crop stays at (0,60), the borrowed address stays at the same
row offset, stride stays 640 and the source bytes are unchanged. Metadata reports
360×640 for portrait and 640×360 for landscape. Button tests check all four
rotations, the fixed group anchor, an eight-pixel gap, screen bounds and hit
regions. Separate host and guest header canaries compile the presentation and
controls independently.

The opt-in real-camera test additionally switches Rear → Rotate → Rear, taps
the overlay buttons, checks selected/unselected border pixels, and rotates out
and back in each mode. Each orientation must produce another 60 live frames,
keep identical crop bounds and emit no source texture resize or capture restart.
These checks are emulator evidence; sensor mounting and camera-specific native
bitmap behavior on the physical phone still need validation.


Final mode/button verification on macOS arm64, Qt 6.11.2 and the named Belle
fixture passed all four backend/opening-orientation cases in 98.53 seconds.
Each case tests Rear → Rotate → Rear with two orientation changes per mode.
Across 36 measured intervals, 60 frames took 1977–2033 ms. Host-observed
waiting-to-first-frame intervals were 630–972 ms; these include observation
and capture overhead and are not phone startup measurements. Test duration
includes firmware fixture copying, which is outside those camera intervals.
Five native presentation tests, host/guest header canaries, both guest app
builds and style checks pass. Text logs, source snapshots and artifact hashes
are archived under `~/.symbian-dev/research/camera-modes-20261012`; camera
photographs and firmware remain outside that archive.


## Physical Nokia 808 fallback correction

The owner observed a clockwise quarter-turn in Rear mode in both orientations,
and a magnified scene in Rotate mode. The 1.0.27 phone trace establishes the
actual returned dimensions and application transforms:

| Mode | Returned bitmap | Previous source rectangle | Previous portrait presentation |
| --- | --- | --- | --- |
| Rear | 480×360 | (0,45,480,270) | Added a clockwise 90° turn, then filled 360×640 |
| Rotate | 360×270 | (104,0,151,270) | Filled 360×644, discarding 209 of the 360 source columns |

The first transform assumed that requesting landscape also described the
bitmap's mounting orientation. It does not: the phone's bitmap is aligned to
the native display. The second assumed that the phone honored a portrait
request, although its bitmap was 4:3. Filling portrait then retained only about
42% of its columns, giving roughly 2.38× magnification. Emulator tests had
returned the requested dimensions and checked metadata rather than a known
physical scene; their success did not establish the phone's orientation.

For version 1.0.28, the owner chose the complete stream with margins when the
requested full-screen shape was unavailable. That version preserved every
returned source pixel.
They compensate Window Server rotation relative to the native display, without
a fixed 90° offset or an inferred opening-orientation offset. GPU presentation
uses SDK `AspectFitViewport`; software uses SDK `CenteredAspectFit` and
`TransformFrame` into a rectangle within the existing screen bitmap. Black
margins replace the former tight crop, with no source recopy or resampling pass
on orientation changes. The image retains its proportions. The two mode buttons
keep their compact layout around the same physical-screen group anchor.

For the observed phone frames, both 480×360 and 360×270 now occupy 360×270
inside a 360×640 portrait window, with 185-pixel top and bottom margins. At a
90° display rotation they occupy 270×360 centered in the 640×360 logical
window. These are the same physical-screen rectangle. Native calibration uses
four distinct corners and a 96×96 central square in a padded 480×360 source.
It requires all corners, a 72×72 output square and identical physical-screen
pixels at all four rotations, with the source storage unchanged. Tests also
cover opening in landscape and the exact 360×270 portrait fallback.

The earlier aspect-fill and 90° reference descriptions above document the
superseded experiments. The emulator's Qt aspect correction is still required:
it preserves proportions before delivering a guest bitmap, while this fix
preserves the complete bitmap returned by ECam. The subsequent phone run confirmed correct initial content and proportions,
but also showed excessive margins and identical behavior in the two modes.
The policy below supersedes universal aspect fit.


The corrected build passed seven native presentation tests and all four real
host-camera backend/opening-orientation cases (100.22 seconds including fixture
setup). There were 36 measured 60-frame intervals of 1977–2001 ms; these are
emulator throughput measurements, not physical-phone startup times. Tests check
complete source rectangles in both modes, fitted viewport sizes, black margins,
button border pixels, mode-selection input, six rotations per case and normal
camera/guest/frontend shutdown. Both guest apps, host/guest header canaries and
style checks pass. The signed 1.0.28 phone package contains the exact E32 tested
in all four emulator cases. The old phone trace and final text/hash evidence are
retained under `~/.symbian-dev/research/camera-phone-framing-20261012`.

## Screen filling with separate Rear and Rotate policies (1.0.29)

The next phone run confirmed that 1.0.28 removed the initial distortion and
incorrect content rotation. It also showed large margins: both buttons used the
same inverse Window Server transform and aspect fit. The modes therefore had
no presentation difference even when the display orientation changed.

The renderer now fills the screen using the minimum centered crop of the actual
source dimensions. A complete source axis remains visible. Filling a portrait
screen with a landscape bitmap necessarily discards side content; retaining all
four edges would require margins. There is no additional zoom factor or inferred
sensor mounting turn.

Rear retains the confirmed native reference and inverse Window Server transform.
Its crop stays fixed in source coordinates and on the physical glass. Rotate
instead follows the logical UI, with no inverse Window Server transform, and
recomputes its crop for the current screen aspect. The modes can coincide in
portrait but differ when the phone turns. For the measured 480×360 phone source:

| Policy | Portrait source crop | Landscape source crop | Landscape presentation rotation |
| --- | --- | --- | --- |
| Rear | (139,0,202,360) | (139,0,202,360) | Counterclockwise 90°, canceling Window Server |
| Rotate | (139,0,202,360) | (0,45,480,270) | None; follows logical UI |

The software path uses SDK `CropFrameView` and `TransformFrame` directly into
screen storage. GLES samples the same source bounds and uses SDK
`AspectFillViewport` to cover integer-rounding slivers. Neither allocates a
rotated/cropped source or changes capture storage on a display event. Controls
retain the compact physical group anchor.

Eight native tests cover the actual 808 fallback sizes, landscape opening,
all four display rotations, mode differences, borrowed buffer offsets/stride,
unchanged source bytes, full-screen known-color pixels and square proportions
to one output pixel of rounding. Rear's physical calibration image remains
byte-identical through all four orientations. Rotate reveals the full source
width in landscape and keeps the calibration square proportional.

All four real-host-camera cases pass (Dynarmic/Dyncom × portrait/landscape
opening), with full-screen border content, the distinct crop/rotation policies,
selected button pixels, six display changes per case and normal shutdown.
The suite took 97.20 seconds including fixture copying. Its 36 intervals of
60 fresh frames took 1977–2017 ms; these are emulator throughput measurements.
Both guest executables, independent host/guest header checks and styles pass.
The signed 1.0.29 package contains the E32 tested in all four cases. SDK MTP
transfer and full readback verified `Installs/camera_app_gles2-d0a69eab330a.sis`
on the Nokia 808; installation and optical confirmation are still pending.
Text/hash evidence is under
`~/.symbian-dev/research/camera-phone-framing-20261012/iteration-129`.
