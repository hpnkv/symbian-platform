"""Serverless desktop shell and typed bridge behavior."""

import shutil
import subprocess
from pathlib import Path

import pytest

from symbian.console.application import read_application
from symbian.console.client import ConsoleClient
from symbian.console.models import CommandResult
from symbian.console.service import ConsoleService, create_app
from symbian.console.web_frontend.app import render_html
from symbian.console.web_frontend.bridge import ConsoleWebBridge
from symbian.console.web_frontend.models import (
    DesktopCatalog,
    FirmwareLibrary,
    FormDefaults,
    UsbInventoryItem,
)
from symbian.status import StatusException


def test_web_frontend_bundles_local_assets() -> None:
    """The renderer must boot without an HTTP server or remote assets."""
    html = render_html()
    assert "/* __CONSOLE_STYLE__ */" not in html
    assert "/* __CONSOLE_SCRIPT__ */" not in html
    assert "/* __CONSOLE_RESULTS__ */" not in html
    assert "/* __CONSOLE_LIVE__ */" not in html
    assert 'id="navigation"' in html
    assert 'id="context-panel"' in html
    assert 'id="device-status"' in html
    assert "data-optional-settings" in html
    assert "Configure more…" not in html
    assert "data-language" not in html
    assert "__NOKIA_808_IMAGE__" not in html
    assert "data:image/png;base64," in html
    assert "device-portrait" in html
    assert 'window.addEventListener("pywebviewready", boot)' in html
    assert 'data-page="application_detail"' in html
    assert 'data-application-action="build"' in html
    assert 'data-application-action="package"' in html


def test_explicit_application_workdir_opens_application_view(
    tmp_path: Path,
) -> None:
    """Only an explicit valid application workdir changes the first view."""
    project = tmp_path / "project"
    project.mkdir()
    source = Path(__file__).resolve().parents[2] / "examples/gui_app"
    (project / "symbian.toml").write_bytes(
        (source / "symbian.toml").read_bytes()
    )
    bridge = ConsoleWebBridge(workdir=project, initial_application=True)
    try:
        assert bridge.get_startup_view() == "application_detail"
    finally:
        bridge.shutdown()
    ordinary = ConsoleWebBridge(workdir=project)
    try:
        assert ordinary.get_startup_view() == "applications"
    finally:
        ordinary.shutdown()


def test_application_run_log_includes_active_emulator_output(
    tmp_path: Path,
) -> None:
    """The live view combines build diagnostics with the owned frontend log."""
    project = tmp_path / "application"
    session = project / ".symbian/runs/run-1"
    session.mkdir(parents=True)
    (session / "frontend.log").write_text("Frame presented\n")
    bridge = ConsoleWebBridge()
    try:
        bridge._run_project = project
        bridge._build_log.write_text("Compiling application\n")
        bridge._run_session.write_text(str(session))
        assert "Compiling application" in bridge.get_run_log()
        assert "Frame presented" in bridge.get_run_log()
        bridge._run_session.write_text(str(tmp_path / "unrelated"))
        assert "Frame presented" not in bridge.get_run_log()
    finally:
        bridge.shutdown()


def test_application_run_view_shows_live_output() -> None:
    """The Run button exposes its log panel during the active session."""
    if not shutil.which("node"):
        pytest.skip("Node.js is needed for the frontend render check")
    script = """
const fs = require('fs');
const vm = require('vm');
const context = vm.createContext({window: {addEventListener: () => {}}});
vm.runInContext(
  fs.readFileSync('symbian/console/web_frontend/app.js', 'utf8'),
  context
);
vm.runInContext(`
  state.context = {project: '/tmp/gui_app'};
  state.applicationOverview = {
    directory: '/tmp/gui_app', name: 'gui_app', caption: 'GUI',
    architecture: 'armv6', uid3: '0xe0000811',
    kind: 'e32-pic-experiment', artifact: null
  };
  state.applicationAction = 'run';
  state.applicationBusy = 'Run gui_app';
  state.applicationRunLog = 'Emulator started';
`, context);
const html = vm.runInContext('renderApplication()', context);
if (!html.includes('Run output') ||
    !html.includes('Emulator started')) process.exit(1);
if (!html.includes('id="application-run-log"')) process.exit(2);
"""
    result = subprocess.run(
        ["node", "-e", script],
        cwd=Path(__file__).resolve().parents[2],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr


def test_application_navigation_renders_before_context_loads() -> None:
    """The optional manifest icon never dereferences an unloaded project."""
    if not shutil.which("node"):
        pytest.skip("Node.js is needed for the frontend render check")
    script = """
const fs = require('fs');
const vm = require('vm');
const navigation = {innerHTML: ''};
const context = vm.createContext({
  document: {
    getElementById: () => navigation,
    querySelectorAll: () => [],
  },
  window: {addEventListener: () => {}},
});
const source = fs.readFileSync('symbian/console/web_frontend/app.js', 'utf8');
vm.runInContext(source, context);
vm.runInContext('renderNavigation()', context);
if (navigation.innerHTML.includes('nav-subitem')) process.exit(1);
vm.runInContext(
  'state.context = {project: "/tmp/gui_app"}; renderNavigation()',
  context,
);
if (!navigation.innerHTML.includes('nav-subitem')) process.exit(2);
vm.runInContext(
  'state.applicationOverview = {directory: "/tmp/gui_app",' +
  ' icon_data_url: "data:image/png;base64,AA=="}; renderNavigation()',
  context,
);
if (!navigation.innerHTML.includes('data:image/png;base64,AA==')) {
  process.exit(3);
}
"""
    result = subprocess.run(
        ["node", "-e", script],
        cwd=Path(__file__).resolve().parents[2],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr


def test_firmware_default_and_compact_usb_interfaces() -> None:
    """The connected model wins by default and interface details stay brief."""
    if not shutil.which("node"):
        pytest.skip("Node.js is needed for the frontend render check")
    script = """
const fs = require('fs');
const vm = require('vm');
const context = vm.createContext({window: {addEventListener: () => {}}});
vm.runInContext(
  fs.readFileSync('symbian/console/web_frontend/app.js', 'utf8'), context
);
vm.runInContext(
  fs.readFileSync('symbian/console/web_frontend/result_views.js', 'utf8'),
  context
);
const result = vm.runInContext(`
  state.context = {devices: [{selector: 'usb:808', product: '808 PureView'}]};
  state.selectedDevice = 'usb:808';
  const records = [
    {firmware: 'e6', device: {model: 'E6-00'}},
    {firmware: '808', device: {model: 'Nokia 808 PureView'}}
  ];
  selectFirmwareForPhone(records);
  const automatic = state.firmwareSelection;
  state.firmwareSelection = 'e6';
  state.firmwareManuallySelected = true;
  selectFirmwareForPhone(records);
  const manual = state.firmwareSelection;
  const html = ConsoleResultViews.render({interfaces: [
    {number: 0, function: 'Still imaging / PTP transport',
     declared_name: 'MTP', endpoint_count: 3, class_code: 6}
  ]}, 'device', escapeHtml);
  [automatic, manual, html]
`, context);
if (result[0] !== '808' || result[1] !== 'e6') process.exit(1);
if (!result[2].includes('interface-list') ||
    result[2].includes('Interfaces 1')) process.exit(2);
"""
    result = subprocess.run(
        ["node", "-e", script],
        cwd=Path(__file__).resolve().parents[2],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr


def test_application_overview_uses_manifest_icon_and_build_state(
    tmp_path: Path,
) -> None:
    """A selected TOML application shows its real identity and local icon."""
    project = tmp_path / "gui_app"
    project.mkdir()
    source = Path(__file__).resolve().parents[2] / "examples/gui_app"
    (project / "symbian.toml").write_bytes(
        (source / "symbian.toml").read_bytes()
    )
    (project / "assets").mkdir()
    (project / "assets/icon.svg").write_bytes(
        (source / "assets/icon.svg").read_bytes()
    )
    overview = read_application(project)
    assert overview.name == "gui_app"
    assert overview.uid3 == "0xe0000811"
    assert overview.caption == "Symbian GUI Counter"
    assert overview.icon_data_url.startswith("data:image/svg+xml;base64,")
    assert overview.artifact is None
    artifact = project / ".symbian/build/gui_app.exe"
    artifact.parent.mkdir(parents=True)
    artifact.write_bytes(b"test artifact")
    assert read_application(project).artifact == str(artifact)


def test_application_icon_rejects_external_content(tmp_path: Path) -> None:
    """Manifest icons cannot reach outside the project or embed scripts."""
    project = tmp_path / "app"
    project.mkdir()
    (tmp_path / "outside.svg").write_text(
        '<svg xmlns="http://www.w3.org/2000/svg"/>'
    )
    (project / "symbian.toml").write_text(
        '[project]\nname="app"\nkind="e32-pic-experiment"\nuid3=0xe0000001\n'
        '[application]\nicon="../outside.svg"\n'
    )
    assert read_application(project).icon_data_url is None
    (project / "icon.svg").write_text(
        '<svg xmlns="http://www.w3.org/2000/svg"><script/></svg>'
    )
    (project / "symbian.toml").write_text(
        (project / "symbian.toml")
        .read_text()
        .replace("../outside.svg", "icon.svg")
    )
    assert read_application(project).icon_data_url is None


def test_workdir_selects_application_and_local_sdk(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The detached process directory drives initial host discovery."""
    monkeypatch.setattr(
        "symbian.device.connection.list_devices", lambda: {"devices": []}
    )
    monkeypatch.setattr(
        "symbian.console.service.ConsoleService.usb_inventory",
        lambda _self: [],
    )
    (tmp_path / "symbian.toml").write_text(
        '[project]\nname="app"\nkind="e32-pic-experiment"\nuid3=0xe0000001\n'
    )
    sdk = tmp_path / ".symbian/app-sdk"
    sdk.mkdir(parents=True)
    (sdk / "sdk.json").write_text("{}")
    bridge = ConsoleWebBridge(workdir=tmp_path)
    try:
        snapshot = bridge.get_context()
        assert snapshot["context"]["workspace"] == str(tmp_path)
        assert snapshot["context"]["project"] == str(tmp_path)
        assert snapshot["context"]["sdk_manifest"] == str(sdk / "sdk.json")
        assert bridge.get_application_overview()["name"] == "app"
    finally:
        bridge.shutdown()


def test_web_bridge_exposes_catalog_and_resolved_defaults() -> None:
    """Every public action remains reachable through the typed bridge."""
    bridge = ConsoleWebBridge()
    try:
        catalog = DesktopCatalog.model_validate(bridge.get_catalog())
        assert len(catalog.tasks) == 38
        live = {
            task.command.path
            for task in catalog.tasks
            if task.presentation.view_mode == "live"
        }
        assert live == {
            ("doctor",),
            ("firmware", "list"),
            ("emu", "resolve"),
            ("device", "list"),
        }
        doctor = next(
            task for task in catalog.tasks if task.command.path == ("doctor",)
        )
        defaults = FormDefaults.model_validate(
            bridge.get_form_defaults(list(doctor.command.path))
        )
        assert defaults.path == ("doctor",)
        assert defaults.values == ()
        assert bridge.validate_form({"path": ["doctor"], "values": {}}) == []
    finally:
        bridge.shutdown()


def test_firmware_library_uses_typed_live_result() -> None:
    """The inventory bridge validates records before the view receives them."""
    service = ConsoleService(
        command_runner=lambda _arguments: {
            "store": "/tmp/example-store",
            "objects": [],
            "integrity_verified": False,
        }
    )
    bridge = ConsoleWebBridge(ConsoleClient(create_app(service)))
    try:
        bridge.get_catalog()
        response = bridge.run_form({"path": ["firmware", "list"], "values": {}})
        library = FirmwareLibrary.model_validate(response["firmware_library"])
        assert library.store == "/tmp/example-store"
        assert library.objects == ()
        assert not library.integrity_verified
    finally:
        bridge.shutdown()


def test_web_inventory_names_native_classes() -> None:
    """Generic descriptors retain codes alongside familiar names."""
    bridge = ConsoleWebBridge()
    try:
        inventory = [
            UsbInventoryItem.model_validate(item)
            for item in bridge.get_usb_inventory()
        ]
        assert all("0x" in item.class_name for item in inventory)
    finally:
        bridge.shutdown()


def test_host_selection_controls_context_and_child_directory(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Chosen paths affect effective defaults and the isolated CLI child."""
    monkeypatch.setattr(
        "symbian.device.connection.list_devices", lambda: {"devices": []}
    )
    monkeypatch.setattr(
        "symbian.console.service.ConsoleService.usb_inventory",
        lambda _self: [],
    )
    captured = []

    def run_cli(request):
        captured.append(request)
        return CommandResult(path=request.path, result={"ready": True})

    monkeypatch.setattr("symbian.console.runner.run_cli", run_cli)
    bridge = ConsoleWebBridge()
    try:
        bridge.get_catalog()
        workspace = tmp_path / "work"
        workspace.mkdir()
        snapshot = bridge.select_host_path(
            {"kind": "workspace", "path": str(workspace)}
        )
        assert snapshot["context"]["workspace"] == str(workspace.resolve())

        project = workspace / "app"
        project.mkdir()
        (project / "symbian-project.json").write_text("{}")
        snapshot = bridge.select_host_path(
            {"kind": "project", "path": str(project)}
        )
        assert snapshot["context"]["project"] == str(project.resolve())
        assert (
            bridge.select_host_path({"kind": "project"})["context"].get(
                "project"
            )
            is None
        )

        # Standalone source applications, including gui_app, use TOML.
        gui_app = Path(__file__).resolve().parents[2] / "examples/gui_app"
        snapshot = bridge.select_host_path(
            {"kind": "project", "path": str(gui_app)}
        )
        assert snapshot["context"]["project"] == str(gui_app.resolve())
        bridge.select_host_path({"kind": "project"})

        sdk = workspace / "sdk"
        sdk.mkdir()
        (sdk / "sdk.json").write_text("{}")
        snapshot = bridge.select_host_path({"kind": "sdk", "path": str(sdk)})
        assert snapshot["context"]["sdk_manifest"] == str(
            (sdk / "sdk.json").resolve()
        )
        bridge.run_form({"path": ["doctor"], "values": {}})
        assert captured[0].cwd == str(workspace.resolve())

        with pytest.raises(StatusException):
            bridge.select_host_path(
                {"kind": "workspace", "path": str(tmp_path / "missing")}
            )
        assert bridge.get_context()["context"]["workspace"] == str(
            workspace.resolve()
        )
    finally:
        bridge.shutdown()
