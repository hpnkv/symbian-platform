# Application startup profile, 12 October 2026

The strongest measured application improvement is the failure view's background
painting: a diagnostic row-copy implementation reduced guest startup work by
about **25%**, while retaining the same panel pixels. Arkanoid spends most of
its guest startup work building and drawing its first scene. Camera GL's tested
synthetic path spends most of its guest work in common logging initialization.

An emulator font-cache defect substantially inflated the original failure-view
profile. Its improvement must not be presented as a phone optimization.

## Scope and measurement

These experiments ran actual ARMv6 executables against the preserved RM-807
Belle fixture in EKA2L1. The source revision was
`8d5ed757a3824c8f26bfd908d63cf6226092d39c`; application builds used
`guest-probes-armv6-release-gpu` (`RelWithDebInfo`, application optimization
`-O2`). Arkanoid used SDL2's GLES2 renderer; the captured frame reports `GPU`.

The measurement interval starts at the primary `SymbianRunThread` entry and
ends when the first application presentation returns: `SDL_RenderPresent`,
failure-view `WindowSurface::Present`, or Camera GL `Swap`. Subsequent native
screen captures verify the output. This measures guest initialization through
presentation submission, not display scanout. It excludes firmware copying,
host builds, emulator boot and host-side executable loading. Phone executable
loading, flash latency and demand paging are not measured by this interval.

Two complementary measurements were used:

* Dynarmic wall-clock checkpoints, three starts per application, measured on
  the host with a monotonic clock. Debug output was drained continuously;
  tracing was limited to errors and guest debug prints. No debugger was
  attached.
* Dyncom guest instruction-dispatch counting, with per-phase 64-byte PC
  histograms. The private instrumentation starts at guest thread entry and
  stops at the first presentation marker. Its bulk-loop accelerator was
  disabled so copies and fills execute guest instructions. Fused compare and
  branch dispatches retain both operations. Counts include instrumentation and
  dispatch-boundary overhead; they are workload measurements, not retired
  hardware instructions or cycle estimates.

Counts are a better CPU-work proxy than a fast host's JIT timings, but have
limits: EKA2L1 implements services and graphics operations on the host. Their
internal CPU work is absent from the guest counts. GPU compilation, driver
behavior, camera hardware, service startup and physical storage latency are not
calibrated to a Nokia 808. These results do not establish phone startup times.

The fixture identity is
`e4502051aee69bb1b7060f47c4e5d5dbf63db3277f8ea918a81076b43ab47e8d`,
with executive profile `rm807-113.010.1508`. The host was an Apple M5 Max running
macOS on arm64. Only one app process ran in each disposable emulator instance;
the desktop host was not isolated from other work.

## Measured results

| Application | Original Dynarmic guest interval, median [range] | Dyncom guest dispatches to first presentation |
| --- | --- | --- |
| SDL2 Arkanoid | 104 ms [100–110] | 2,778,767–2,778,864, two runs |
| Failure Handler Demo | 143 ms [130–146] | 48,670,497–48,692,706, two runs; font-cache distortion below |
| Camera GL | 109 ms [93–138] | 603,969–604,037, two runs; synthetic frames only |

The raw wall times illustrate emulator behavior; they must not be read as
predictions for phone launch latency. In particular, logging initialization
cost about 20–27 ms under Dynarmic but approximately 4–5 ms under Dyncom.
That backend sensitivity makes the original wall-time ranking an unreliable
guide to phone CPU cost.

### Failure handler: distinguish the emulator defect from app work

The original profile attributed approximately 39 million dispatches to the
ROM's `fntstr.dll`, mainly its session-glyph-cache search. A second diagnostic
recorded **3,017 rasterization requests for 71 distinct character codes**:
1,082 requests during wrapping and 1,935 during drawing. Repeated spaces alone
caused 399 requests. This was not simply a cold font cache.

Source inspection identified a concrete address error in EKA2L1's
`services/src/fbs/impls/font.cpp`, in `MAKE_CACHE_ENTRY`. In its offset-based
branch, it subtracts the cache-entry address directly from the bitmap font's
tagged `openfont` field. The same emulator's `fill_bitmap_information` stores
that field as a relative offset with bit zero set. Original Symbian
`CBitmapFont::OpenFont` resolves it by clearing the tag and adding the bitmap
font's address. The diagnostic correction applied that resolution before
forming the glyph entry's font offset.

With the exact same guest executable and corrected emulator cache:

| Measurement | Original cache | Corrected cache |
| --- | --- | --- |
| Guest dispatches to first presentation | 48.67–48.69M | 11.186–11.220M, three runs |
| Rasterization requests | 3,017 in the diagnostic run | 128–130 |
| Wrapping phase | 15.60–15.61M dispatches | about 2.50M |
| Initial drawing phase | 32.27–32.28M dispatches | about 7.90M |

This removes about **77%** of the original guest work. It establishes a
profiling distortion and a candidate emulator fix, not a 77% phone speedup.
The correction was experimental; broader firmware and font-cache compatibility
were not tested.

The app-side background fill remained expensive after the cache correction.
`failure_handler.cc::Fill` writes two bytes per pixel in nested loops and
repaints overlapping background, panel and button rectangles. A prototype
filled each rectangle's first row and copied that row to the remaining rows.

Across three runs with captures and expected failure-demo exit evidence, the
prototype executed **8.348–8.383M** dispatches, versus **11.186–11.220M** for
the corrected-cache control: approximately **25% less total startup work**.
The corresponding guest elapsed times were 83–92 ms versus 100–104 ms on the
instrumented Dyncom backend; instruction reduction is the more portable result.
One additional prototype trial reached its presentation marker but its capture
timed out; it is excluded from the visually verified comparison.

All 270,848 compared pixels in the header, footer and outer margins were
identical between a control and prototype capture. The report body contains
changing timestamps and thread IDs, so this is a region comparison, not a
whole-screen equivalence claim. The demo retained its expected process exit
reason 1 after closing the intentional CHECK report.

Recommended application work:

1. Replace repeated per-pixel solid fills with row copies or a native solid
   rectangle operation, preserving the single offscreen composition/blit.
   The row-copy experiment provides measured evidence for this change.
2. Retain the same font owner through wrapping and drawing, and investigate
   wrapping only the initial viewport before the first presentation. Whole
   report copying and subsequent scrolling must remain complete. The corrected
   wrapping phase still costs about 2.50M dispatches, but the savings from these
   changes have not been measured.
3. Land and regression-test the emulator cache correction before using this
   UI as a phone-performance proxy. Do not optimize the application around
   thousands of cache misses caused by the emulator.

### SDL2 Arkanoid: first-scene preparation and drawing

The two instruction-count runs agree closely:

| Phase | Guest dispatches |
| --- | ---: |
| Thread entry to `main` | about 543K |
| `main` to renderer ready | about 134K |
| Game construction | 1.3K |
| MIDI/vibration owner creation | 5.4K |
| First scene draw and presentation | **2.094M**, about **75%** of startup |

The first-draw histogram attributes approximately 473K dispatches to
`BlockAtlas::Open` and its inlined procedural tile painting. Other hot areas
include GLES geometry queuing and software floating-point helpers. Attribution
uses 64-byte buckets, so individual function totals are approximate.

`assets/text.cc::DrawText` submits a filled SDL rectangle for each lit pixel in
each 5×7 glyph. This repeatedly enters SDL's geometry and coordinate-conversion
path. The block atlas separately paints all 60 procedural variations at launch.

Recommended work:

1. Put bitmap glyphs in a texture atlas, or batch the glyph rectangles, so text
   does not submit one SDL draw operation per lit pixel. Measure the resulting
   first-draw instruction count and check the same frame visually.
2. Precompute the block atlas during the build, or simplify/batch its initial
   painting. Approximately 473K dispatches, about 17% of the measured total,
   occur in this area; that is an observed cost, not a promised 17% speedup.
3. Keep actual MIDI/vibration service startup after the first presentation,
   as the current code already does. Moving it later again cannot explain or
   remove the measured first-frame work. Game construction is also too small
   to be a useful first target.

### Camera GL: common initialization dominates the tested CPU path

The two counted runs executed about 604K dispatches, of which approximately
548K occurred before `main`. The automatic logging constructor accounted for
about **439K**, roughly **73%** of the total. Its histogram includes Abseil's
low-level allocator and time-zone/calendar setup. This identifies a possible
shared SDK improvement: investigate deferring expensive logging/time-format
initialization until required, while preserving automatic sinks and complete
first-LOG/CHECK failure reporting. Simply deleting the constructor would change
behavior and is not an acceptable optimization.

Only about 56K dispatches remained between `main` and the first synthetic
presentation. Camera discovery reported zero cameras, so the measured path
did **not** reserve or power a camera, negotiate a real capture stream, or
wait for its first sensor frame. The red/blue/white diagnostic pixels and
synthetic-state strip were captured successfully.

For actual camera startup, the code currently opens the stream before creating
the EGL context. Showing a useful window first and overlapping asynchronous
camera reservation/power-on with graphics setup is a candidate, but this
experiment supplies no measured saving for that path. The example also flushes
each trace message synchronously when its E-drive trace file opens; batching or
disabling diagnostic file tracing merits a separate storage-backed experiment.
Neither real camera startup nor physical E-drive flush cost was measured here.

## Evidence and replay

Private raw evidence is retained at
`.symbian/startup-profile-20261012/`: timestamped logs, phase histograms,
`analysis.json`, captures, executable/emulator digests, build logs and diagnostic
patches. A research archive copy of the measurements and scripts is stored at
`~/.symbian-dev/research/startup-profile-20261012/`. No firmware, upstream source
checkout or build product is added to version control.

`run.py` owns each emulator subprocess and records native process exits. Its
`--marker` option delays capture until the guest has submitted its first frame,
avoiding capture during graphics initialization. Early exploratory polling
included a camera stall, and several SDL trials rendered but required frontend
termination after their close request. These are recorded; they are not normal
exit evidence or phone-startup measurements.

Replay uses the saved `instrument.py`, application instrumentation patch,
private emulator patches, `--backend dyncom --instructions`, and the named
fixture. The root source build needed experiment-local logging link dependencies
and the repository's existing `symbian-log-no-weak-gcov.patch`; those setup steps
are retained with the build logs. Compare guest intervals and phase counts,
never the firmware-copy or emulator-process-start columns in the raw summaries.

The application sources, private emulator sources and configured emulator
executable were restored after the experiments. The restored executable matches
SHA-256 `221fcff863e42db7e060b42d65483cd6da93d2e0a1e239471e7a379fe1591ca5`,
and its app bundle passes strict deep code-signature verification. The row-copy
and font-cache changes remain diagnostic patches rather than shipped changes.

## Failure-handler lifecycle follow-up

The handler now starts presentation and persistence on separate SDK worker
executors, with a fiber for each task. Its immutable report snapshot outlives
both. A presentation error does not cancel persistence; closing the view
destroys its native window before the writer finishes. The calling fiber parks
until both tasks settle, and a fatal CHECK terminates the process afterwards.
File creation, writes and flushing use SDK storage utilities. A small native
leave/cleanup boundary remains necessary for the Window Server view thread.

The maintained `failure_handler_probe` and opt-in
`symbian/tests/test_failure_handler_guest.py` exercise the production task runner
against the same named Belle fixture on Dynarmic and Dyncom. All four probe/demo
cases pass. After an injected UI failure, the probe verifies the saved bytes.
After closing a real window, Window Server can no longer find its application
task while the process remains alive and a deliberately delayed writer drains
for three seconds. The final file is then flushed and the process exits with
reason zero. The real CHECK demo renders, accepts Exit, saves the intentional
failure and exits with reason one on both backends.

This establishes independent lifetimes and removes a synchronous write/flush
barrier before presentation. It does not measure physical flash latency or
establish a new phone startup time. No diagnostic font-cache or row-fill patch
was shipped as part of this lifecycle change. Raw follow-up evidence is in
`.symbian/failure-async-emulator-tests/` and `.symbian/failure-async-*.log`.
