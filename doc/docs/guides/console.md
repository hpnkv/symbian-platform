# Symbian Console

Symbian Console is the desktop view of the same SDK commands used in a
terminal. Use it to create a project, build an E32 executable, choose imported
firmware, run in the emulator and prepare a SIS package. The screenshots below
use the actual frontend with sample paths and no connected phone.

## Create and select an application

1. Start the Console with `uv run symbian console` from the repository root.
2. Choose **Applications** and **Create an application**. Enter a new folder,
   name and application UID3, then review the proposed settings.
3. Select the created project in the sidebar. Its application page collects
   **Build**, **Run in emulator** and **Package** in one place.

![The Applications page showing Create, Build, Run and Package choices and the Create form.](../assets/screenshots/console-actions.png)

*Applications page, captured from the current frontend with a sample SDK path.*

![A selected Counter project with Build, Run and Package controls.](../assets/screenshots/console-application.png)

*Selected project before its first build. Import compatible firmware before
running; Package becomes available after Build.*

## Launch and navigate

Run `symbian console` (or `uv run symbian console`). Pass
`--workdir /path/to/application` to start in a chosen directory; its
`symbian.toml` is selected automatically and a local
`.symbian/app-sdk/sdk.json` is preferred when present. The command starts a
separate desktop process and returns the terminal immediately. For startup
diagnostics, run `python -m symbian.console.web_frontend.app` in a terminal.
The desktop shell uses pywebview: WKWebView on macOS and WebView2 on Windows.
The Linux package includes the PySide6 renderer; the launcher can use the
legacy Tk frontend if pywebview is absent. The HTML, CSS and JavaScript are
bundled locally. No browser tab, listening port or remote content is needed.

The light interface groups all 38 public SDK actions by purpose:

- **Applications:** create, build, run, configure and package.
- **Firmware:** import, inspect, browse and export local content.
- **Emulator:** configuration, IDE integration and session controls.
- **SDK tools:** setup, inspection and preservation.
- **Devices:** guided actions, USB inspection and bounded protocol probes.
- **Activity:** requests completed in this console session.

Firmware inventory, effective emulator settings, host readiness and connected
phones appear when their section opens. They refresh in the background while
the previous result stays visible. The firmware view has a searchable catalog
beside an inspection pane. Selecting an identity reads its manifest in the
background and shows device paths, a searchable file index and import
provenance; repeat selections reuse the cached result. Import and Export remain
available in the same workspace. Library source settings expand beneath the
catalog. The emulator sidebar holds optional source settings.

Other actions use compact guided forms with actual known paths and devices.
Required fields are validated. Optional settings expand within the same form,
so Review stays available without a separate settings page.
Simple actions execute from their input page; consequential changes retain a
review step. The full content area scrolls with a touchpad. Results display
typed values, grouped fields and bounded lists as the primary view. Large
firmware manifests have a searchable file index. Raw JSON can still be opened,
copied and highlighted as JSON for exact evidence. Navigation retains cached
results and draft inputs while context refreshes in the background.

The USB inspector lists generic host descriptors with named USB classes. Select
a supported phone and choose **Inspect phone** to see interpreted interface
roles, host drivers, endpoint counts and serial port paths. Protocols offers
bounded AT identity/status, MTP metadata/root listing, and PC Suite OBEX
Connect/Disconnect probes. These probes run only when selected explicitly.
Connected-device cards keep a compact portrait beside their information. An
identified Nokia 808 PureView uses a transparent front portrait cropped from
the user's supplied image; other
models retain the generic phone icon.

The sidebar lets you choose the working directory, application, SDK and phone.
Folder buttons show the effective paths; the phone menu lists connected
handsets. Changing the working directory also changes where isolated CLI
commands run. Application selection accepts a project folder containing
`symbian.toml` or `symbian-project.json`, including `examples/gui_app` and
projects created by `symbian init`. The sidebar refreshes automatically and
has no Refresh button.
With `symbian console --workdir PATH`, a valid application at `PATH` opens in
the application view on startup; the view shows a loading state until its
identity and build state arrive. On macOS, the detached console opens on the
display containing the invoking terminal window.

When an application is selected, an indented entry below **Applications**
opens its project view. Its name, caption, UID3, architecture, package name,
manifest icon and current executable come from the selected directory.
**Build** creates a reproducible E32 executable. **Run** uses the current
emulator firmware or an imported identity selected in that view; the emulator
session stays supervised until it exits. **Package** creates an unsigned SIS
from the current executable and becomes available after a successful build.
For a standalone TOML project such as `examples/gui_app`, build output is
`<project>/.symbian/build`; generated applications use their SDK build action.
Builds show live tool output, collapse that output on completion, and summarize
the produced executable. Compile
options remain available in one compact disclosure. Imported EKA1 firmware
appears unavailable for EKA2 application runs. A run started from the console
opens its emulator window on the same display, above the console. The Run
output panel shows build and launcher messages followed by the emulator's live
frontend log. It collapses when the run ends and remains available to inspect.

The status bar shows a green bullet for a connected selected phone. A native
USB observation runs every 750 ms; when the selected handset disappears, it
clears the idle indicator while full device reconciliation runs in the
background. Active phone requests show an amber bullet until completion, even
if the phone temporarily disappears. A USB mode baseline remains amber until
verification or **Dismiss** in the status bar. Selection follows a
serial-backed redacted identity across mode changes; a port-only identity
clears after an observed disconnect. The status bar also reports background
firmware, USB inventory and live-view loads until they finish.

`symbian/console/web_frontend/` contains the desktop shell, typed bridge and
local UI assets. It calls `ConsoleClient`, which uses HTTPX `ASGITransport` to
invoke FastAPI in the same process. The service owns command and device policy;
the command catalog comes from the CLI parser. SDK failures retain `Status`
and `StatusException`. A worker runs SDK requests so the window's UI thread
handles only presentation. CLI tasks run in children because some SDK commands
replace their process with a tool.

The console exposes existing public SDK workflows and implemented USB, AT,
MTP and OBEX probes. OBEX currently performs a bounded Connect/Disconnect
exchange; browsing, file transfer and SyncML remain open.
