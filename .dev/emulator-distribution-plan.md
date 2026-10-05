# Publishable EKA2L1 distributions

Requested 2026-10-05. This is an implementation plan, not an announcement of
published emulator packages. Continue shipping the native SDK independently
while establishing the emulator's own build and release gates.

## Outcome

A user installs `symbian-platform` from PyPI, installs a compatible native SDK,
and installs the matching emulator with one CLI command. They can then follow
Getting started, hello_time and gui_app without obtaining Qt, FFmpeg, an
emulator source checkout or a separate compiler for the emulator. Firmware is
an explicitly imported, separately preserved user input; no emulator package,
wheel or public test artifact includes ROMs, Z-drive files or device data.

The proposed interface is `symbian emulator install`, with automatic OS/CPU
selection, an explicit version option and an offline archive option. These
commands do not exist yet. Direct desktop launch must work too: the distributed
frontend remains a useful EKA2L1 application, not merely an SDK subprocess.

## Starting point and compatibility contract

Use the existing pinned EKA2L1 revision
`2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8`, recursively pinned submodules and the
maintained patches in `research/eka2l1/`. The ordered patch recipe currently
lives in [the source guide](../doc/docs/reference/emulator-source-build.md).
The public guide must eventually consume the maintained acquisition driver
rather than act as a second manually maintained build recipe.

Actual named-firmware application acceptance exists on macOS arm64 and Linux
x86_64, including the restricted Nokia 7610 EKA1 process. These are source-built
frontends, not proof of portable distributable bundles. Linux arm64 and macOS
x86_64 emulator builds and execution remain gates even though their host SDKs
and wheels already build. See [the Linux host plan](linux-host-plan.md) and
[EKA1 restrictions](EKA1.md).

SDK compatibility requires the maintained control protocol
`symbian.emulator-control/v1`, owned process startup/shutdown, isolated data
roots, firmware import helpers, framebuffer capture/input, native guest exit
records, loopback GDB and the supported firmware-specific service profiles.
An unmodified upstream emulator with the same executable name is insufficient.
Preserve upstream behavior outside the bounded patches and pursue upstreamable
changes; a distribution is not permission for an unbounded emulator fork.

Keep EKA2L1 out of the Apache SDK native libraries and Python extension. It
remains a separate GPL executable, communicated with through its existing
process/control interface. Keep native format logic in the SDK and reuse its
configuration, firmware resolver and launch supervisor.

## Versions and artifacts

Initially use root `VERSION` as the single package-version authority and the
existing `vX.Y.Z` release tag. Do not invent a second maintained version string
for Qt bundles, installers or Python configuration. Emulator package versions
can follow SDK releases without recompiling an unchanged emulator core: the
assembly step supplies the package version and filenames. If independent
emulator releases later become necessary, design that tag/version policy
explicitly; do not silently introduce another version file now.

Produce four portable emulator archives, matching the SDK host matrix:

| Host | Initial baseline | Proposed release asset |
| --- | --- | --- |
| macOS arm64 | macOS 15+ | `symbian-emulator-VERSION-macos-arm64.tar.gz` |
| macOS x86_64 | macOS 15+ | `symbian-emulator-VERSION-macos-x86_64.tar.gz` |
| Linux x86_64 | Ubuntu 24.04 / glibc 2.39+ | `symbian-emulator-VERSION-linux-x86_64.tar.gz` |
| Linux arm64 | Ubuntu 24.04 / glibc 2.39+ | `symbian-emulator-VERSION-linux-aarch64.tar.gz` |

Confirm those minimum versions with clean-host tests; do not borrow the
manylinux 2.28 wheel claim for an emulator built on Ubuntu 24.04. Use separate
macOS application bundles per CPU initially, with no Rosetta requirement.
Provide a convenient DMG containing the same tested application for each Mac
CPU once signing and notarization are configured. Linux's initial portable
archive contains a launcher and desktop/icon integration; an AppImage is an
optional subsequent delivery format, not a prerequisite for a usable archive.

Publish an accompanying emulator source archive containing the exact patched
EKA2L1 sources, recursively required dependency sources, license texts and the
build/install scripts needed to reproduce the binary. This is distinct from
the existing SDK source archive, which does not contain the ignored emulator
checkout. Include the corresponding Qt/dependency source and rebuild material
required by the actual distribution licenses. Preserve notices for EKA2L1,
Qt, FFmpeg, CPU backends and every redistributed library. Audit the chosen
FFmpeg configure options and Qt modules; do not accidentally select nonfree
FFmpeg components or depend on a commercial Qt license.

Keep only metadata required for installation and compatibility: package
version, host OS/CPU, relative frontend/import-helper locations, control
protocol and supported launch capabilities. Put immutable source pins and
ordered patch inputs in the build driver. Do not introduce digest inventories,
SBOMs, provenance reports or archaeology manifests. Download integrity can use
the release service's asset digest, with an offline caller-provided digest when
requested; this does not require a maintained per-file provenance manifest.

## Phase 1: reproducible build driver

1. Add one maintained acquisition/build entry point for the pinned upstream,
   recursive submodules, root patches and host-specific submodule patches.
   Operate in a new disposable workspace. Validate existing source states
   before reuse; never reset the owner's research checkout or firmware store.
2. Separate dependency build, emulator core/frontend build, bundle assembly
   and acceptance. Export actual CMake/Ninja presets and compilation databases.
   Keep compiler/Qt/module choices explicit and identical between local and CI
   builds. Adapt A11's dependency-prefix and reusable release-job patterns.
3. Replace the four historical FFmpeg shell recipes with one parametrized
   maintained recipe, retaining the pinned source/configuration. Establish
   Linux arm64 and Intel macOS builds; the x86 Linux script is not an ARM
   recipe. Replay the existing FFmpeg/Linux and MCL compatibility patches.
4. Pin a supported Qt 6 release and the required Widgets, Gui, Network, Svg
   and build-time translation tools. Audit the existing Qt private-header
   dependency and platform event hooks before updating Qt. Keep camera and Lua
   disabled initially as in the accepted builds; describe those limitations
   briefly where users select emulator features. Keep firmware codecs used by
   the accepted applications rather than trimming them without tests.
5. Run the upstream tests and existing independent platform harnesses on each
   host. Retain both Dynarmic and Dyncom. Do not solve an ARM-host failure by
   silently advertising an interpreter-only package as equivalent.

Gate: all four native hosts build the same supported frontend/control surface,
without guest firmware in the build inputs. Record architecture-specific
compiler, Qt, FFmpeg or graphics blockers in the research log.

## Phase 2: complete platform bundles

### macOS

Build a normal `.app` with a stable SDK-distribution bundle identifier, version,
icon, translations and resources. Audit the existing upstream/plugin copying
and `dolphin_postprocess_bundle` behavior instead of assuming it closes every
runtime dependency. Use the matching Qt deployment tool, then audit the final
Mach-O closure, framework paths and every plugin's dependent libraries.

Bundle the Cocoa platform plugin, image/SVG plugins and the other Qt plugins
actually used. Include SSL, SDL, codecs and other non-system libraries needed
by the enabled frontend. No executable or plugin may retain Homebrew, a build
workspace or Python-environment paths. Preserve framework structure and relative
rpaths; sign nested libraries/plugins before the application, after relocation.

Validate both Finder/open desktop launch and the SDK's owned direct executable
launch. The current SDK launches a symlink outside `.app/Contents/MacOS` to
avoid foreground activation: deployed Qt plugins and resources must still be
found through the resolved bundle, without global `QT_PLUGIN_PATH` or a lost
application-directory assumption. Keep normal desktop activation and SDK
background-window behavior separate. Test dragging/input and full-screen Space
placement on a real Mac desktop, not only Qt's offscreen plugin.

For stable consumer downloads, use Developer ID signing, hardened runtime
entitlements justified by actual JIT behavior, notarization and stapling of the
final deliverable. Verify Dynarmic under the signed runtime. Apple signing and
notarization credentials are an external CI configuration requirement; there
are none assumed by this plan. Ad hoc signed development archives may precede
that gate with an explicit installation caveat. Do not describe them as
notarized or bypass Gatekeeper by clearing quarantine automatically.

### Linux

Produce a self-contained relocatable prefix containing the frontend, firmware
import helpers, Qt libraries/plugins, translations, assets, required fonts and
the non-system shared-library closure. The launcher selects its own relative
library/plugin paths without requiring a virtual environment or build checkout.
Use `qt.conf`/Qt deployment support and audit `dlopen` dependencies explicitly;
`ldd` on the main executable alone misses platform and image plugins.

Exercise both X11/xcb and Wayland plugins where available. Include required
XCB/xkb/font libraries and plugin dependencies, rather than assuming the host
has developer packages. Keep the operating-system ABI, kernel interfaces and
hardware graphics drivers external: bundling a random host's Mesa/Vulkan driver
is not portable. Document the actual display, graphics and audio service
requirements, with short Ubuntu dependency sections only for genuine external
runtime needs. Check software rendering under Xvfb as well as a real desktop
with GPU-backed rendering, input and audio. A successful offscreen `--help`
invocation alone is insufficient.

Offer reversible desktop entry/icon installation; register no system-wide
services and write no preserved firmware state inside the application prefix.
A future AppImage must pass the same native-prefix checks before adding
AppImage/FUSE-specific installation behavior.

Gate: unpacked bundles run from paths with spaces after moving them, with only
OS facilities available. Audit dependencies of binaries and plugins and reject
missing resources, wrong architecture, absolute build paths or omitted notices.

## Phase 3: SDK installation and selection

1. Implement archive and released-version installation through the existing
   safe archive handling/configuration patterns. Choose host architecture
   explicitly; reject unsupported hosts, compatibility mismatches, traversal
   and incomplete payloads before changing the selected installation.
2. Stage and validate a new version, then atomically select it. Preserve older
   versions for explicit rollback. Install application files separately from
   the firmware store and writable instances. Failed downloads or acceptance
   must leave the previous emulator and firmware untouched.
3. Extend the existing resolution chain to the installed emulator, keeping
   explicit command/project/SDK/user overrides and source-checkout paths.
   Resolve the packaged firmware importer as well as the GUI executable;
   [current configuration](../symbian/emulator/configuration.py) includes a
   source-build macOS default that must not remain the installed-user default.
4. Add a bounded compatibility query/doctor check before launch. Verify the
   actual frontend's protocol/features, not merely a package filename or
   filesystem descriptor. Preserve the current schema checks and owned control
   socket. Keep GDB loopback-only and expose no dangerous device-operation API.
5. Make the installed-distribution route the default in Getting started,
   firmware onboarding and both application guides. Keep source-build steps as
   a separate route. A fresh user needs one emulator installation command and
   their own firmware import, not patch application or Homebrew Qt setup.

Gate: fresh environments on all four hosts install, select, run and roll back
an emulator from published/offline archives without source dependencies.

## Phase 4: CI flow without repeated expensive builds

Add a reusable emulator workflow with four native-host jobs. Build dependencies
once per host and emulator binaries once per actual source/configuration input
set, independent of the Python 3.11–3.14 matrix. Wheels communicate with the
same executable; no per-Python emulator compile is necessary.

Cache dependency prefixes using OS/CPU/minimum OS, compiler ABI, Qt/FFmpeg pins,
enabled modules and dependency patches. Cache the tested emulator core using
upstream/submodule pins, emulator patches, build drivers and relevant CMake
inputs. Cache assembled payloads using that core, deployment scripts, notices,
resources and package VERSION. CLI-only/documentation changes must not trigger
fresh Qt/FFmpeg/core builds. Never use an approximate cache hit to replace
required validation; rerun relocation/install acceptance on restored archives.

Use successful exact-source standalone artifacts in the release workflow, as
with the SDK. Wait for an existing producer instead of starting a duplicate
matrix. Keep nonpublishing manual/PR validation separate from protected tag
publication; do not execute untrusted PR code on private-fixture runners or
under signing credentials. Scope permissions to the publication/signing jobs.

Release audit requires all four correctly named/versioned bundles, corresponding
source material, compatibility metadata and license closure. Publish only after
that audit and the applicable runtime gates pass. Add the emulator assets to
the GitHub release, outside PyPI wheels and native SDK archives so users can
update/select the emulator without replacing their compiler SDK. Preserve
published tags/assets; repair failures before publication and use a new version
for released corrections.

## Acceptance matrix and release gates

- Every host: upstream native tests; help/compatibility query; Qt plugin/resource
  discovery; SDK control startup/status/capture; owned shutdown/cleanup; two
  isolated instances; relocation and prefix-with-spaces checks; dependency and
  license audit. Source archives must rebuild without undeclared downloads.
- Linux CI: Xvfb rendering/input smoke with Mesa software rendering. Wayland,
  real GPU and audio checks need separate desktop acceptance; report each
  tested backend explicitly. macOS CI: actual Cocoa plugin/window startup and
  control capture where the runner supports it, plus a real-desktop gate for
  focus/Spaces/signing/JIT behavior.
- Named-firmware acceptance: hello_time timer/Clear and gui_app increment/reset
  through the installed CLI, actual framebuffer pixels, native guest reason
  zero and frontend exit zero; ARMv5T/ARMv6, Dynarmic/Dyncom where the existing
  workflows support them. Replay bounded EKA1 normal/changed/import-corruption
  controls on the preserved 7610. Replay live HTTP, WebSocket, TLS 1.2/TLS 1.3
  and guest GDB stepping/library-symbol checks against the packaged frontend.
- Firmware is not available to public PR jobs. Establish trusted maintainer
  acceptance on an isolated/private runner or owner host with legally held
  fixtures, protected from untrusted code. Clone disposable state, verify the
  preserved store before/after runs, and publish only sanitized results. The
  current Linux host `helena@192.168.1.209` is a practical initial acceptance
  host, not automatically a permanently configured GitHub runner.
- Clean public-download acceptance: install PyPI wheel, published native SDK
  and published emulator with isolated HOME/config/PATH, import owner firmware,
  and execute Getting started and both application guides on every advertised
  host. Physical-device installation remains a separate SDK gate.

Do not claim all four executable packages are usable from architecture metadata
or a native compile alone. No package changes the existing EKA1/runtime/firmware
restrictions. A release candidate is ready only when its actual delivered
bundle passes the appropriate host and named-firmware gates above.

## First implementation order

1. Consolidate acquisition/build and validate the missing Intel macOS and
   Linux arm64 paths, including FFmpeg and both CPU backends.
2. Prove a fully relocated Linux x86_64 archive and macOS arm64 `.app` using
   existing named-firmware guide acceptance; carry the same recipes to the
   remaining native hosts.
3. Implement installed-emulator selection/doctor and reversible installation.
4. Add the reusable CI matrix, source/licensing closure and release audit.
5. Configure signing/notarization, finish all clean-download/desktop gates,
   publish all four assets, and replace public source-build prerequisites with
   the installed route.
