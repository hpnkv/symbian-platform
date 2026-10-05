"use strict";

const pageInfo = {
  applications: ["Applications", "Create, build, run and package Symbian applications."],
  application_detail: ["Application", "Identity, build output and emulator controls."],
  firmware: ["Firmware", "Browse, import and export local firmware content."],
  signing: ["Signing", "Manage local signing identities and sign SIS applications."],
  emulator: ["Emulator", "Configure, inspect and control emulator sessions."],
  sdk_setup: ["SDK setup", "Check this computer and prepare development tools."],
  sdk_inspection: ["Artifact inspection", "Examine and verify native outputs."],
  sdk_preservation: ["Preservation", "Create and verify local preservation records."],
  device_actions: ["Device actions", "Inspect phones and stage applications with guided steps."],
  development_agents: ["Development Agents", "Build an agent package and prepare a connected phone."],
  usb: ["USB inspector", "Explore host descriptors and interpreted phone interfaces."],
  protocols: ["Device protocols", "Read phone information through bounded native protocol probes."],
  activity: ["Activity", "Requests completed during this local console session."],
};
const groupPages = {
  "Applications": "applications", "Firmware": "firmware", "Emulator": "emulator", "Signing": "signing",
  "Getting started": "sdk_setup", "SDK and toolchain": "sdk_setup",
  "Inspection": "sdk_inspection", "Preservation": "sdk_preservation",
  "Devices": "device_actions",
};
const pathNames = new Set([
  "destination", "project", "sdk", "store", "workspace", "output",
  "artifact", "package", "executable", "source", "rom", "vpl",
  "instance", "bundle", "rpkg", "z_drive", "archive", "ticket",
  "headers", "sources_root", "oracles_build", "definition", "elf",
  "import_proxy", "root", "firmware", "manifest",
  "identities", "certificate", "private_key", "signing_certificate", "signing_key",
]);
const folderNames = new Set([
  "project", "store", "workspace", "output", "archive", "headers",
  "sources_root", "oracles_build", "root", "instance", "identities",
]);
const advancedNames = new Set([
  "at_status", "backend", "clear_firmware", "compiler", "emulator",
  "gdb", "headers", "import_proxy", "importer", "language", "linker",
  "mtp", "mtp_list", "no_build", "no_protocol", "non_interactive",
  "oracles_build", "output", "portable_runtime", "profile",
  "replace_alias", "root", "rpkg", "saved", "store", "timeout",
  "unset", "usb_map", "variant", "volume", "z_drive",
]);
const navSections = [
  ["applications", "Applications", "app"],
  ["firmware", "Firmware", "firmware"],
  ["emulator", "Emulator", "screen"],
  ["signing", "Signing", "shield"],
  ["sdk_setup", "SDK tools", "tools"],
  ["device_actions", "Devices", "phone"],
  ["development_agents", "Development Agents", "connection"],
  ["activity", "Activity", "clock"],
];
const subpages = {
  sdk_setup: [["sdk_setup", "Setup"], ["sdk_inspection", "Inspection"], ["sdk_preservation", "Preservation"]],
  device_actions: [["device_actions", "Actions"], ["usb", "USB inspector"], ["protocols", "Protocols"]],
};
const liveWorkspaces = {
  firmware: "firmware list",
  emulator: "emu resolve",
  signing: "signing list",
  sdk_setup: "doctor",
  device_actions: "device list",
};
const refreshLiveAfter = new Set([
  "firmware import", "emu configure", "emu configure-ide",
  "prepare-app-sdk", "sdk install", "toolchain prepare-gui-sdk",
  "device install", "signing create", "signing import", "signing archive",
]);
const icons = {
  app: '<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>',
  firmware: '<path d="M4 6h16v12H4z"/><path d="M8 10h8M8 14h5"/>',
  screen: '<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4"/>',
  tools: '<path d="M14 6a5 5 0 0 0-6 6l-5 5a2 2 0 0 0 3 3l5-5a5 5 0 0 0 6-6l-3 3-3-3z"/>',
  phone: '<rect x="7" y="2" width="10" height="20" rx="2"/><path d="M11 18h2"/>',
  clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l4 2"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  build: '<path d="M4 19h16M8 19v-6l4-8 4 8v6M9 14h6"/>',
  play: '<path d="m8 5 11 7-11 7z"/>',
  package: '<path d="m12 3 9 5-9 5-9-5 9-5zM3 8v9l9 5 9-5V8M12 13v9"/>',
  sliders: '<path d="M4 7h16M4 17h16M9 4v6M15 14v6"/>',
  search: '<circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/>',
  download: '<path d="M12 3v13m-5-5 5 5 5-5M4 20h16"/>',
  upload: '<path d="M12 17V4m-5 5 5-5 5 5M4 20h16"/>',
  shield: '<path d="m12 2 8 4v6c0 5-3 8-8 10-5-2-8-5-8-10V6z"/><path d="m8 12 3 3 5-6"/>',
  file: '<path d="M5 2h9l5 5v15H5zM14 2v5h5M8 12h8M8 16h8"/>',
  connection: '<path d="M7 8a4 4 0 0 0 0 8h3M17 8a4 4 0 0 1 0 8h-3M9 12h6"/>',
};

const state = {
  page: "applications", tasks: [], context: null, selectedDevice: null,
  initialApplicationView: false,
  deviceStatus: null, localPending: null, workStatus: "Connecting to the local SDK…",
  selectedTask: {}, step: {}, optionalOpen: {}, advanced: {}, drafts: {},
  defaults: {}, errors: {}, outcomes: {}, detailsOpen: {}, taskBusy: {},
  inventory: [], inventoryLoading: false, inventorySelection: null,
  inspection: {}, lastInspection: {}, activity: [], pageScroll: {},
  contextLoading: false, mtpLimit: 8,
  liveBusy: {}, liveUpdated: {}, liveErrors: {}, liveApplied: {},
  firmwareSelection: null, firmwareManuallySelected: false, firmwareSearch: "", inspectorAdvanced: {},
  firmwareInspections: {}, firmwareInspectionBusy: {}, firmwareInspectionErrors: {}, firmwareFileSearch: {},
  applicationOverview: null, applicationLoading: false, applicationError: "",
  agentProject: null, agentBusy: "", agentError: "", agentPackage: null,
  agentPackages: {}, agentIdentities: {}, agentLive: {}, agentLogs: {}, agentPairConfirmed: {},
  agentOutcome: null, agentStaged: {}, agentObservations: {},
  applicationFirmware: null, applicationFirmwareBusy: false, applicationFirmwareError: "",
  applicationSelectedFirmware: "", applicationBusy: "", applicationAction: "", applicationOutcome: null, applicationBuildLog: "", applicationRunLog: "",
  usbTopologySignature: null, usbTopologyRevision: 0, usbContextRevision: 0,
  usbTopologyDevices: null, usbPollBusy: false, topologyMissingSelected: false,
};

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, character => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  })[character]);
}
function icon(name) {
  return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${icons[name]}</svg>`;
}
function taskKey(task) { return task.command.path.join(" "); }
function taskIcon(task) {
  const path = task.command.path.join(" ");
  if (/\b(init|create|prepare)\b/.test(path)) return "plus";
  if (/\b(build|compile|convert|package)\b/.test(path)) return "build";
  if (/\b(run|pointer|launch)\b/.test(path)) return "play";
  if (/\b(install|import)\b/.test(path)) return "download";
  if (/\b(export|screenshot)\b/.test(path)) return "upload";
  if (/\b(verify|preserve|doctor)\b/.test(path)) return "shield";
  if (/\b(configure|settings)\b/.test(path)) return "sliders";
  if (/\b(device|mode)\b/.test(path)) return "phone";
  if (/\b(inspect|probe|info|status|list)\b/.test(path)) return "search";
  return "file";
}
function selectedTask() {
  const matches = state.tasks.filter(task => groupPages[task.presentation.group] === state.page);
  if (!matches.length) return null;
  const remembered = state.selectedTask[state.page];
  const preferred = liveWorkspaces[state.page] || null;
  return matches.find(task => taskKey(task) === remembered)
    || matches.find(task => taskKey(task) === preferred) || matches[0];
}
function taskSteps(task) {
  const primary = task.steps.filter(step => !step.optional);
  return primary.length ? primary : task.steps;
}
function deviceName(device) {
  return `${device.product} · ${device.vendor_id.toString(16).padStart(4,"0")}:${device.product_id.toString(16).padStart(4,"0")}`;
}
function phoneOptions(selected) {
  const devices = state.context?.devices || [];
  if (!devices.length) return '<option value="">No connected phones</option>';
  return devices.map(device => `<option value="${escapeHtml(device.selector)}" ${device.selector === selected ? "selected" : ""}>${escapeHtml(deviceName(device))}</option>`).join("");
}
function pageGroup(page) {
  if (page === "application_detail") return "applications";
  if (["sdk_setup", "sdk_inspection", "sdk_preservation"].includes(page)) return "sdk_setup";
  if (["device_actions", "usb", "protocols"].includes(page)) return "device_actions";
  return page;
}
function setPage(page) {
  if (!pageInfo[page]) return;
  state.pageScroll[state.page] = document.getElementById("main").scrollTop;
  state.page = page;
  document.getElementById("main").scrollTop = 0;
  renderNavigation();
  renderMain();
  renderStatus();
  const base = liveBaseTask(page);
  if (base) {
    fetchDefaults(base).then(() => {
      if (state.page === page) runLive(base);
    });
  }
  const task = selectedTask();
  if (task && task !== base) {
    fetchDefaults(task);
  }
  if (page === "usb" && !state.inventory.length) refreshInventory();
  if (page === "application_detail") {
    loadApplication(true);
    loadApplicationFirmware(true);
  }
  if (["device_actions", "usb", "protocols", "development_agents"].includes(page)) refreshContext();
  if (page === "development_agents") loadAgentProject();
}
function renderNavigation() {
  const group = pageGroup(state.page);
  document.getElementById("navigation").innerHTML =
    navSections.map(([key, label, symbol]) => {
      const active = group === key;
      const image = state.applicationOverview && state.applicationOverview.directory === state.context?.project
        ? state.applicationOverview.icon_data_url : null;
      const appIcon = image ? `<img src="${escapeHtml(image)}" alt="">` : icon("file");
      const application = key === "applications" && state.context?.project
        ? `<button class="nav-subitem ${state.page === "application_detail" ? "active" : ""}" data-page="application_detail" title="${escapeHtml(state.context.project)}">${appIcon}<span>${escapeHtml(state.context.project.split(/[\\/]/).filter(Boolean).pop())}</span></button>` : "";
      const devices = key === "device_actions" ? (state.context?.devices || []).map(device =>
        `<button class="nav-subitem ${state.selectedDevice === device.selector && state.page === "device_actions" ? "active" : ""}" data-nav-device="${escapeHtml(device.selector)}" title="${escapeHtml(deviceName(device))}">${icon("phone")}<span>${escapeHtml(device.product)}</span></button>`).join("") : "";
      return `<button class="nav-item ${active ? "active" : ""}" data-page="${key}" ${active ? 'aria-current="page"' : ""}>${icon(symbol)}<span>${label}</span></button>${application}${devices}`;
    }).join("");
  document.querySelectorAll("[data-page]").forEach(button => button.addEventListener("click", () => setPage(button.dataset.page)));
  document.querySelectorAll("[data-nav-device]").forEach(button => button.addEventListener("click", async () => {
    await selectPhone(button.dataset.navDevice); setPage("device_actions");
  }));
}
function renderSelection() {
  const context = state.context;
  const selected = context?.devices?.find(device => device.selector === state.selectedDevice);
  const pathRow = (label, value, kind) => `<div class="selection-row"><dt>${label}</dt><dd><button class="selection-value ${value ? "" : "selection-empty"}" data-select-path="${kind}" title="${escapeHtml(value || `Choose ${label.toLowerCase()}`)}">${escapeHtml(value || `Choose ${label.toLowerCase()}…`)}</button>${kind === "project" && value ? '<button class="selection-clear" data-clear-path="project" title="Clear application selection" aria-label="Clear application selection">×</button>' : ""}</dd></div>`;
  const phones = context?.devices || [];
  const phoneOptions = phones.map(device => `<option value="${escapeHtml(device.selector)}" ${device.selector === state.selectedDevice ? "selected" : ""}>${escapeHtml(deviceName(device))}</option>`).join("");
  document.getElementById("selection").innerHTML = `
    <div class="selection-title">Current selection</div>
    <dl>${pathRow("Working directory", context?.workspace, "workspace")}${pathRow("Application", context?.project, "project")}${pathRow("SDK", context?.sdk_manifest, "sdk")}<div class="selection-row"><dt><label for="selection-phone">Phone</label></dt><dd><select id="selection-phone"><option value="">${phones.length ? "No phone selected" : "No connected phones"}</option>${phoneOptions}</select></dd></div>${selected ? `<div class="selection-row"><dt>USB profile</dt><dd>${escapeHtml(selected.interface_profile)}</dd></div>` : ""}</dl>`;
  document.querySelectorAll("#selection [data-select-path]").forEach(button => button.addEventListener("click", () => chooseHostPath(button.dataset.selectPath)));
  document.querySelectorAll("#selection [data-clear-path]").forEach(button => button.addEventListener("click", () => updateHostPath(button.dataset.clearPath, "")));
  document.getElementById("selection-phone")?.addEventListener("change", event => selectPhone(event.target.value));
}
function renderStatus() {
  const liveKey = Object.keys(state.liveBusy).find(key => state.liveBusy[key]);
  const base = liveBaseTask(state.page);
  const waitingLive = base && !state.outcomes[taskKey(base)] && !state.liveErrors[taskKey(base)];
  const inspecting = state.firmwareInspectionBusy[state.firmwareSelection] || Object.values(state.firmwareInspectionBusy).some(Boolean);
  const background = liveKey ? `Refreshing · ${state.tasks.find(task => taskKey(task) === liveKey)?.presentation.title || liveKey}`
    : state.applicationBusy ? `Working · ${state.applicationBusy}`
    : state.applicationLoading ? "Reading application…"
    : state.applicationFirmwareBusy ? "Reading available firmware…"
    : inspecting ? "Reading firmware details…"
    : state.inventoryLoading ? "Refreshing USB inventory…"
    : state.contextLoading && state.usbTopologyRevision > state.usbContextRevision ? "Checking USB connection…"
    : waitingLive ? `Loading · ${base.presentation.title}` : null;
  document.getElementById("work-status").textContent = state.workStatus === "Ready" ? background || "Ready" : state.workStatus;
  const observed = state.localPending || state.deviceStatus;
  const status = state.topologyMissingSelected && observed?.tone === "connected" ? null : observed;
  const text = state.topologyMissingSelected && status?.tone === "pending" ? `${status.text} · USB connection changed` : status?.text;
  const modePending = status?.text?.includes("USB mode verification pending");
  document.getElementById("device-status").innerHTML = status ?
    `<span class="status-device ${status.tone === "pending" ? "pending" : ""}" title="${escapeHtml(text)}"><span class="status-dot"></span><span>${escapeHtml(text)}</span></span>${modePending ? '<button class="status-dismiss" id="clear-pending">Dismiss</button>' : ""}` : "";
  document.getElementById("clear-pending")?.addEventListener("click", async () => {
    try {
      state.deviceStatus = await window.pywebview.api.clear_pending_device_status();
      renderStatus();
    } catch (error) { showError(error); }
  });
}
function setWork(text) { state.workStatus = text; renderStatus(); }
function appendActivity(action, outcome, details = null) {
  state.activity.unshift({time: new Date().toLocaleTimeString(), action, outcome, details});
  state.activity = state.activity.slice(0, 100);
  if (state.page === "activity") renderMain();
}
async function refreshContext() {
  if (state.contextLoading) return;
  state.contextLoading = true;
  renderStatus();
  const revision = state.usbTopologyRevision;
  try {
    const snapshot = await window.pywebview.api.get_context();
    applyContext(snapshot, revision);
  } catch (error) {
    setWork(`Could not refresh the current selections: ${error.message || error}`);
    await refreshStatus();
  } finally { state.contextLoading = false; renderStatus(); }
}
function applyContext(snapshot, revision) {
  const changed = state.selectedDevice !== snapshot.selected_device;
  const previous = state.context;
  state.context = snapshot.context;
  state.agentObservations = snapshot.agent_observations || {};
  state.agentIdentities = snapshot.agent_identities || {};
  state.selectedDevice = snapshot.selected_device || null;
  state.deviceStatus = snapshot.device_status || null;
  if (changed) {
    state.firmwareManuallySelected = false;
    selectFirmwareForPhone(state.outcomes["firmware list"]?.firmware_library?.objects || []);
  }
  state.usbContextRevision = revision;
  if (revision === state.usbTopologyRevision) state.topologyMissingSelected = selectedPhoneMissing();
  renderSelection(); renderStatus();
  renderNavigation();
  const hostChanged = ["workspace", "project", "sdk_manifest", "firmware_store"].some(name => previous?.[name] !== snapshot.context[name]);
  const devicesChanged = JSON.stringify((previous?.devices || []).map(device => device.selector).sort()) !==
    JSON.stringify((snapshot.context.devices || []).map(device => device.selector).sort());
  if (changed || hostChanged || devicesChanged) {
    state.defaults = {};
    if (previous?.project !== snapshot.context.project) {
      state.applicationOverview = null;
      state.applicationFirmware = null;
      state.applicationSelectedFirmware = "";
      state.applicationOutcome = null;
      state.applicationAction = "";
      state.applicationBuildLog = "";
      renderNavigation();
      if (snapshot.context.project) loadApplication();
      if (state.page === "application_detail") loadApplicationFirmware();
    }
    if (selectedTask()) fetchDefaults(selectedTask());
    if (previous?.sdk_manifest !== snapshot.context.sdk_manifest) {
      state.agentPackage = null;
      state.agentOutcome = null;
      if (state.page === "development_agents") loadAgentProject();
    }
    if (["device_actions", "usb", "protocols", "application_detail", "development_agents"].includes(state.page)) renderMain();
  }
}
function selectedPhoneMissing() {
  const selected = state.context?.devices?.find(device => device.selector === state.selectedDevice);
  return Boolean(selected && state.usbTopologyDevices && !state.usbTopologyDevices.some(device => device.vendor_id === selected.vendor_id && device.product_id === selected.product_id));
}
async function pollUsbTopology() {
  if (state.usbPollBusy) return;
  state.usbPollBusy = true;
  try {
    const topology = await window.pywebview.api.get_usb_topology();
    const devices = topology.devices || [];
    state.usbTopologyDevices = devices;
    const signature = JSON.stringify(devices.map(device => [device.vendor_id, device.product_id, device.bus, device.address, device.ports]).sort());
    if (state.usbTopologySignature !== null && signature !== state.usbTopologySignature) {
      state.usbTopologyRevision++;
    }
    const missing = selectedPhoneMissing();
    if (missing !== state.topologyMissingSelected) { state.topologyMissingSelected = missing; renderStatus(); }
    state.usbTopologySignature = signature;
    if (state.usbTopologyRevision > state.usbContextRevision && !state.contextLoading) refreshContext();
  } catch (_) { /* Periodic context refresh remains the fallback. */ }
  finally { state.usbPollBusy = false; }
}
async function updateHostPath(kind, path) {
  setWork("Updating current selection…");
  try {
    const snapshot = await window.pywebview.api.select_host_path({kind, path});
    applyContext(snapshot, state.usbTopologyRevision);
    const base = liveBaseTask(state.page);
    if (base) runLive(base);
    else if (selectedTask()) fetchDefaults(selectedTask());
    setWork("Ready");
  } catch (error) { showError(error); }
}
async function chooseHostPath(kind) {
  const context = state.context;
  const current = kind === "workspace" ? context?.workspace : kind === "project" ? context?.project || context?.workspace : context?.sdk_manifest || "";
  try {
    const path = await window.pywebview.api.choose_path("folder", current || "");
    if (path) await updateHostPath(kind, path);
  } catch (error) { showError(error); }
}
async function refreshStatus() {
  try { state.deviceStatus = await window.pywebview.api.get_device_status(); renderStatus(); }
  catch (_) { /* The next context observation will reconcile the indicator. */ }
}
async function selectPhone(selector) {
  state.selectedDevice = selector || null;
  state.firmwareManuallySelected = false;
  selectFirmwareForPhone(state.outcomes["firmware list"]?.firmware_library?.objects || []);
  try { state.deviceStatus = await window.pywebview.api.select_device(selector); }
  catch (error) { setWork(String(error.message || error)); }
  renderSelection(); renderNavigation(); renderStatus(); renderMain();
  if (selectedTask()) fetchDefaults(selectedTask());
}
function mainHeader(title, description) {
  const tabs = subpages[pageGroup(state.page)];
  const tabMarkup = tabs ? `<nav class="tab-strip" role="tablist" aria-label="${escapeHtml(pageGroup(state.page) === "sdk_setup" ? "SDK tools" : "Devices")}">${tabs.map(([key, label]) => `<button class="tab ${state.page === key ? "active" : ""}" role="tab" aria-selected="${state.page === key}" data-page="${key}">${escapeHtml(label)}</button>`).join("")}</nav>` : "";
  return `<header class="page-header"><h1>${escapeHtml(title)}</h1><p>${escapeHtml(description)}</p></header>${tabMarkup}`;
}
function renderMain() {
  const container = document.getElementById("main");
  const previousScroll = container.scrollTop;
  let body;
  if (state.page === "usb") body = renderUsb();
  else if (state.page === "application_detail") body = renderApplication();
  else if (state.page === "development_agents") body = renderAgents();
  else if (state.page === "protocols") body = renderProtocols();
  else if (state.page === "activity") body = renderActivity();
  else body = renderActions();
  container.innerHTML = `<div class="main-inner">${body}</div>`;
  const task = liveBaseTask(state.page);
  const inspector = task ? ConsoleLiveViews.inspector(task, state) : "";
  const panel = document.getElementById("context-panel");
  panel.innerHTML = inspector;
  panel.hidden = !inspector;
  document.getElementById("app").classList.toggle("has-inspector", Boolean(inspector));
  bindPage();
  bindInspector();
  container.scrollTop = previousScroll || state.pageScroll[state.page] || 0;
}
function renderActions() {
  const [title, description] = pageInfo[state.page];
  const tasks = state.tasks.filter(task => groupPages[task.presentation.group] === state.page);
  const current = selectedTask();
  if (!current) return mainHeader(title, description) + '<p class="empty-copy">Loading actions…</p>';
  const base = liveBaseTask(state.page);
  if (base) {
    const operation = current !== base ? current : null;
    const actionButtons = tasks.filter(task => task !== base && !(state.page === "firmware" && taskKey(task) === "firmware inspect")).map(task => `<button class="button ${operation === task ? "selected-action" : ""}" ${state.page === "firmware" ? "data-open-task" : "data-task"}="${escapeHtml(taskKey(task))}" title="${escapeHtml(task.presentation.summary)}">${icon(taskIcon(task))}${escapeHtml(task.presentation.title)}</button>`).join("");
    const operationContent = operation ? `<div class="operation-head"><button class="tiny-button" data-close-operation="true">Close action</button></div>` +
      (renderTask(operation) + renderOutcome(state.outcomes[taskKey(operation)], `task:${taskKey(operation)}`)) : "";
    return mainHeader(title, description) + `<div class="workspace-actions">${actionButtons}</div>` +
      `<div class="workspace-live">${ConsoleLiveViews.render(base, state)}</div>` + operationContent;
  }
  const activeKey = taskKey(current);
  const choices = tasks.map(task => `<button class="action-choice ${taskKey(task) === activeKey ? "active" : ""}" data-task="${escapeHtml(taskKey(task))}" title="${escapeHtml(task.presentation.summary)}" ${taskKey(task) === activeKey ? 'aria-current="true"' : ""}><span class="action-icon">${icon(taskIcon(task))}</span><strong>${escapeHtml(task.presentation.title)}</strong></button>`).join("");
  const content = current.presentation.view_mode === "live"
    ? ConsoleLiveViews.render(current, state)
    : renderTask(current) + renderOutcome(state.outcomes[activeKey], `task:${activeKey}`);
  return mainHeader(title, description) + `<div class="action-grid">${choices}</div>${content}`;
}
function liveBaseTask(page) {
  const key = liveWorkspaces[page];
  return key ? state.tasks.find(task => taskKey(task) === key) || null : null;
}
function formValues(task) {
  const key = taskKey(task);
  const draft = state.drafts[key] || {};
  const defaults = state.defaults[key] || {};
  const values = {};
  for (const argument of task.command.arguments) {
    values[argument.name] = Object.prototype.hasOwnProperty.call(draft, argument.name) ? draft[argument.name]
      : argument.kind === "flag" ? false : defaults[argument.name] ?? argument.default ?? "";
  }
  if (task.command.path[0] === "signing" && key !== "signing list" && !values.identities) {
    values.identities = state.outcomes["signing list"]?.result?.result?.directory || "";
  }
  return values;
}
function fieldMarkup(argument, value, issues, binding = "data-field", prefix = "field") {
  const name = argument.name;
  const help = argument.help || (argument.repeatable ? "Separate multiple values with spaces; use quotes when needed." : "");
  const issue = issues.find(item => item.field === name);
  const id = `${prefix}-${name}`;
  const label = `<label for="${id}">${escapeHtml(argument.label)}${argument.required ? '<span class="required">Required</span>' : ""}</label>`;
  let control;
  if (argument.kind === "flag") {
    control = `<div class="check-row"><input id="${id}" type="checkbox" ${binding}="${name}" ${value ? "checked" : ""}><span>${escapeHtml(help || "Enable this option")}</span></div>`;
  } else if (name === "identity" && ["signing sign", "signing archive"].includes(taskKey(selectedTask() || {command: {path: []}}))) {
    const identities = state.outcomes["signing list"]?.result?.result?.identities || [];
    control = `<div class="field-control"><select id="${id}" ${binding}="${name}" class="${issue ? "invalid" : ""}"><option value="">Choose a signing identity</option>${identities.map(identity => `<option value="${escapeHtml(identity.name)}" ${identity.name === value ? "selected" : ""}>${escapeHtml(identity.name)} · ${escapeHtml(identity.subject)}</option>`).join("")}</select></div>`;
  } else if (name === "device") {
    control = `<div class="field-control"><select id="${id}" ${binding}="${name}" class="${issue ? "invalid" : ""}"><option value="">Choose a connected phone</option>${phoneOptions(value)}</select></div>`;
  } else if (argument.choices?.length) {
    control = `<div class="field-control"><select id="${id}" ${binding}="${name}" class="${issue ? "invalid" : ""}"><option value="">Use SDK default</option>${argument.choices.map(choice => `<option value="${escapeHtml(choice)}" ${choice === value ? "selected" : ""}>${escapeHtml(choice)}</option>`).join("")}</select></div>`;
  } else {
    const browse = pathNames.has(name) ? `<button class="browse-button" type="button" ${binding === "data-live-field" ? "data-live-browse" : "data-browse"}="${name}">Browse…</button>` : "";
    control = `<div class="field-control"><input id="${id}" type="text" ${binding}="${name}" value="${escapeHtml(value)}" class="${issue ? "invalid" : ""}" autocomplete="off">${browse}</div>`;
  }
  const hint = help && argument.kind !== "flag" ? `<p class="field-help">${escapeHtml(help)}</p>` : "";
  return `<div class="field ${pathNames.has(name) || argument.repeatable ? "wide" : ""}">${label}${control}${hint}${issue ? `<p class="field-error">${escapeHtml(issue.message)}</p>` : ""}</div>`;
}
function renderTask(task) {
  const key = taskKey(task);
  const steps = taskSteps(task);
  const index = Math.min(state.step[key] || 0, steps.length);
  state.step[key] = index;
  const isReview = index === steps.length;
  const noInputs = task.command.arguments.length === 0;
  const values = formValues(task);
  const issues = state.errors[key] || [];
  const step = steps[index];
  let content;
  if (isReview) {
    const entered = task.command.arguments.filter(argument => values[argument.name] !== "" && values[argument.name] !== false && values[argument.name] != null);
    content = `<div class="task-top"><div><h2>Review ${escapeHtml(task.presentation.title.toLowerCase())}</h2><p>Check these values before continuing.</p></div></div>` +
      (entered.length ? `<dl class="review-list">${entered.map(argument => `<dt>${escapeHtml(argument.label)}</dt><dd>${escapeHtml(values[argument.name])}</dd>`).join("")}</dl>` : '<p class="empty-copy">The SDK defaults will be used.</p>');
  } else {
    const visible = step.arguments.filter(argument => argument.required || !advancedNames.has(argument.name) || state.advanced[key]);
    const hidden = step.arguments.length - visible.length;
    const optionalSteps = task.steps.filter(item => item.optional && !steps.includes(item));
    const optionalMarkup = optionalSteps.length ? `<details class="optional-settings" data-optional-settings="${escapeHtml(key)}" ${state.optionalOpen[key] ? "open" : ""}><summary>Optional settings</summary>${optionalSteps.map(item => `<div class="optional-group"><h3>${escapeHtml(item.title)}</h3><div class="form-grid">${item.arguments.map(argument => fieldMarkup(argument, values[argument.name], issues)).join("")}</div></div>`).join("")}</details>` : "";
    content = `<div class="task-top"><div><h2>${escapeHtml(task.presentation.title)}</h2><p>${escapeHtml(task.presentation.summary)}</p></div>${steps.length > 1 ? `<span class="step-indicator">Step ${index + 1} of ${steps.length}</span>` : ""}</div>` +
      (visible.length ? `<div class="form-grid">${visible.map(argument => fieldMarkup(argument, values[argument.name], issues)).join("")}</div>` : "") +
      (hidden || state.advanced[key] ? `<button class="option-toggle" data-advanced="${escapeHtml(key)}">${state.advanced[key] ? "Hide additional settings" : `Show additional settings (${hidden})`}</button>` : "") + optionalMarkup;
  }
  const forward = noInputs || isReview ? task.presentation.action
    : index === steps.length - 1
      ? (task.presentation.review_required ? "Review" : task.presentation.action)
      : "Continue";
  const shownArguments = [
    ...(step?.arguments || []),
    ...(state.optionalOpen[key] ? task.steps.filter(item => item.optional && !steps.includes(item)).flatMap(item => item.arguments) : []),
  ];
  const errorBanner = issues.find(issue => !shownArguments.some(argument => argument.name === issue.field));
  return `<section class="panel task-panel" aria-label="Selected action">${content}${errorBanner ? `<div class="inline-alert">${escapeHtml(errorBanner.message)}</div>` : ""}<div class="form-actions">${index > 0 ? '<button class="button" data-back="true">Back</button>' : ""}<span class="spacer"></span><button class="button primary" data-forward="true" ${state.taskBusy[key] ? "disabled" : ""}>${escapeHtml(state.taskBusy[key] ? "Working…" : forward)}</button></div></section>`;
}
async function fetchDefaults(task) {
  const key = taskKey(task);
  try {
    const response = await window.pywebview.api.get_form_defaults(task.command.path);
    state.defaults[key] = Object.fromEntries(response.values.map(item => [item.name, item.value]));
    if (selectedTask() && taskKey(selectedTask()) === key) renderMain();
  } catch (_) { /* Static defaults remain usable until context is ready. */ }
}
async function advanceTask(task) {
  const key = taskKey(task);
  const steps = taskSteps(task);
  const index = state.step[key] || 0;
  const values = formValues(task);
  if (index < steps.length && task.command.arguments.length) {
    try {
      const issues = await window.pywebview.api.validate_form({path: task.command.path, values});
      const optionalNames = task.steps.filter(step => step.optional && !steps.includes(step))
        .flatMap(step => step.arguments.map(argument => argument.name));
      const hasOptionalDraft = optionalNames.some(name => Object.prototype.hasOwnProperty.call(state.drafts[key] || {}, name));
      const current = new Set([
        ...steps[index].arguments.map(argument => argument.name),
        ...(state.optionalOpen[key] || hasOptionalDraft ? optionalNames : []),
      ]);
      const relevant = issues.filter(issue => current.has(issue.field));
      state.errors[key] = relevant;
      if (relevant.length) {
        if (optionalNames.includes(relevant[0].field)) state.optionalOpen[key] = true;
        renderMain(); document.getElementById(`field-${relevant[0].field}`)?.focus(); return;
      }
      if (index === steps.length - 1 && !task.presentation.review_required) {
        await runTask(task, values); return;
      }
      state.step[key] = index + 1;
      renderMain(); return;
    } catch (error) { showError(error); return; }
  }
  await runTask(task, values);
}
async function runTask(task, values) {
  const key = taskKey(task);
  const label = task.presentation.title;
  if (state.taskBusy[key]) return;
  state.taskBusy[key] = true;
  setWork(`Working · ${label}`);
  renderMain();
  if (task.command.path[0] === "device") setPending(label);
  try {
    const outcome = await window.pywebview.api.run_form({path: task.command.path, values});
    state.outcomes[key] = outcome;
    state.errors[key] = [];
    appendActivity(label, "Completed", outcome);
    setWork("Ready");
  } catch (error) {
    state.outcomes[key] = {summary: {title: "Task failed", message: String(error.message || error), facts: []}, result: {result: null}};
    appendActivity(label, `Error: ${error.message || error}`);
    setWork(`Error · ${label}`);
  } finally {
    state.taskBusy[key] = false;
    state.localPending = null;
    await refreshStatus();
    refreshContext();
    const base = liveBaseTask(state.page);
    if (base && base !== task && refreshLiveAfter.has(key)) runLive(base);
    renderMain();
  }
}
function selectFirmwareForPhone(records) {
  if (state.firmwareManuallySelected || !records.length) return;
  const phone = state.context?.devices?.find(device => device.selector === state.selectedDevice);
  if (!phone) return;
  const model = String(phone.product || "").toLowerCase().replace(/^nokia\s+/, "").replace(/[^a-z0-9]/g, "");
  if (!model) return;
  const match = records.find(record => String(record.device?.model || "").toLowerCase()
    .replace(/^nokia\s+/, "").replace(/[^a-z0-9]/g, "") === model);
  if (match) state.firmwareSelection = match.firmware;
}
async function runLive(task) {
  const key = taskKey(task);
  if (state.liveBusy[key]) return;
  state.liveBusy[key] = true;
  state.liveErrors[key] = "";
  const submittedValues = formValues(task);
  setWork(`Refreshing · ${task.presentation.title}`);
  if (liveBaseTask(state.page) === task) renderMain();
  try {
    const outcome = await window.pywebview.api.run_form({path: task.command.path, values: submittedValues});
    state.outcomes[key] = outcome;
    state.liveApplied[key] = submittedValues;
    state.liveUpdated[key] = new Date().toLocaleTimeString([], {hour: "2-digit", minute: "2-digit"});
    if (key === "firmware list") {
      const records = outcome.firmware_library?.objects || [];
      if (!records.some(record => record.firmware === state.firmwareSelection)) {
        state.firmwareSelection = null;
        state.firmwareManuallySelected = false;
      }
      selectFirmwareForPhone(records);
      if (!state.firmwareSelection) state.firmwareSelection = records[0]?.firmware || null;
      state.outcomes["firmware inspect"] = state.firmwareInspections[state.firmwareSelection] || null;
    }
    appendActivity(task.presentation.title, "Updated", outcome);
    setWork("Ready");
  } catch (error) {
    state.liveErrors[key] = String(error.message || error);
    setWork(`Could not refresh · ${task.presentation.title}`);
  } finally {
    state.liveBusy[key] = false;
    renderStatus();
    if (liveBaseTask(state.page) === task) renderMain();
    if (key === "firmware list" && state.firmwareSelection) loadFirmwareInspection(state.firmwareSelection);
  }
}
async function loadFirmwareInspection(identity) {
  if (!identity || state.firmwareInspections[identity] || state.firmwareInspectionBusy[identity]) return;
  const record = state.outcomes["firmware list"]?.firmware_library?.objects?.find(item => item.firmware === identity);
  const task = state.tasks.find(item => taskKey(item) === "firmware inspect");
  if (!record || !task) return;
  const accepted = new Set(task.command.arguments.map(argument => argument.name));
  const applied = state.liveApplied["firmware list"] || {};
  const values = Object.fromEntries(Object.entries(applied).filter(([name, value]) => accepted.has(name) && value));
  const store = state.outcomes["firmware list"]?.firmware_library?.store;
  if (store) values.store = store;
  values.reference = record.aliases[0] || identity;
  state.firmwareInspectionBusy[identity] = true;
  state.firmwareInspectionErrors[identity] = "";
  renderStatus();
  if (state.page === "firmware" && state.firmwareSelection === identity) renderMain();
  try {
    state.firmwareInspections[identity] = await window.pywebview.api.run_form({path: task.command.path, values});
  } catch (error) {
    state.firmwareInspectionErrors[identity] = String(error.message || error);
  } finally {
    state.firmwareInspectionBusy[identity] = false;
    renderStatus();
    if (state.firmwareSelection === identity) state.outcomes["firmware inspect"] = state.firmwareInspections[identity] || null;
    if (state.page === "firmware" && state.firmwareSelection === identity) renderMain();
  }
}
function setPending(label) {
  const device = state.context?.devices?.find(item => item.selector === state.selectedDevice);
  state.localPending = {tone: "pending", text: `${device?.product || "Phone"} · ${label} (working)`};
  renderStatus();
}
function renderOutcome(outcome, key) {
  if (!outcome) return "";
  const summary = outcome.summary || {};
  const payload = outcomePayload(outcome);
  const facts = summary.facts || [];
  return `<section class="panel result-panel"><h2>${escapeHtml(summary.title || "Result")}</h2><p>${escapeHtml(summary.message || "")}</p>${facts.length ? `<div class="fact-grid">${facts.map(fact => `<div class="fact"><span>${escapeHtml(fact.label)}</span><strong>${escapeHtml(fact.value)}</strong></div>`).join("")}</div>` : ""}${ConsoleResultViews.render(payload, key, escapeHtml)}</section>${renderRawDetails(outcome, key)}`;
}
function outcomePayload(outcome) {
  return outcome?.result && Object.prototype.hasOwnProperty.call(outcome.result, "result")
    ? outcome.result.result : outcome?.result;
}
function renderRawDetails(outcome, key) {
  if (!outcome) return "";
  const open = Boolean(state.detailsOpen[key]);
  const payload = outcomePayload(outcome);
  const rendered = open ? JSON.stringify(payload ?? {}, null, 2) : "";
  return `<section class="raw-details"><div class="details-toolbar"><button class="button quiet" data-details="${escapeHtml(key)}">${open ? "Hide" : "Show"} raw JSON</button>${open ? `<span class="spacer"></span><button class="button" data-copy="${escapeHtml(key)}">Copy</button>` : ""}</div>${open ? `<pre class="code-view" data-code="${escapeHtml(key)}">${escapeHtml(rendered.slice(0, 262144))}${rendered.length > 262144 ? "\n… preview truncated; Copy includes the complete result." : ""}</pre>` : ""}</section>`;
}
async function highlightVisible() {
  const code = document.querySelector("[data-code]");
  if (!code) return;
  const key = code.dataset.code;
  const outcome = key.startsWith("task:") ? state.outcomes[key.slice(5)] : state.inspection[key];
  if (!outcome) return;
  const payload = outcomePayload(outcome);
  const rendered = JSON.stringify(payload ?? {}, null, 2);
  const preview = rendered.slice(0, 262144);
  try {
    const spans = await window.pywebview.api.highlight(preview, "JSON");
    if (code.isConnected && code.dataset.code === key) code.innerHTML = spans.map(span => `<span class="token-${span.category}">${escapeHtml(span.text)}</span>`).join("") + (rendered.length > preview.length ? "\n… preview truncated; Copy includes the complete result." : "");
  } catch (_) { /* The plain, escaped text remains readable. */ }
}
function renderUsb() {
  const [title, description] = pageInfo.usb;
  const selected = state.inventorySelection;
  const rows = state.inventory.map((device,index) => {
    const key = usbKey(device);
    return `<tr data-usb-row="${index}" class="${key === selected ? "selected" : ""}"><td>0x${device.vendor_id.toString(16).padStart(4,"0")}</td><td>0x${device.product_id.toString(16).padStart(4,"0")}</td><td>${device.bus} / ${device.address} / ${escapeHtml((device.ports || []).join("."))}</td><td>${escapeHtml(device.class_name)}</td><td>${device.configuration_count}</td></tr>`;
  }).join("");
  const descriptor = state.inventory.find(item => usbKey(item) === selected);
  const inspectKey = `map:${state.selectedDevice || ""}`;
  const inspected = state.inspection[inspectKey];
  const interfaces = inspected?.result?.device?.interfaces || [];
  const interfaceRows = interfaces.map(item => `<tr><td>${item.number}</td><td>${escapeHtml(item.function)}</td><td>${escapeHtml(item.declared_name || "Unknown")}</td><td>${escapeHtml(item.host_driver || "Unknown")}</td><td>${item.endpoint_count ?? "Unknown"}</td><td>${escapeHtml(item.host_serial_port || "—")}</td></tr>`).join("");
  return mainHeader(title, description, "Devices") +
    `<div class="toolbar"><button class="button" id="refresh-usb">${state.inventoryLoading ? "Refreshing…" : "Refresh inventory"}</button><label for="usb-phone">Phone</label><select id="usb-phone"><option value="">Choose a connected phone</option>${phoneOptions(state.selectedDevice)}</select><button class="button primary" id="inspect-usb" ${state.selectedDevice ? "" : "disabled"}>Inspect phone</button></div>` +
    `<div class="panel table-panel"><div class="table-scroll"><table class="data-table"><thead><tr><th>Vendor</th><th>Product</th><th>Bus / address / port</th><th>Class</th><th>Configs</th></tr></thead><tbody>${rows || '<tr><td colspan="5">No USB descriptors observed yet.</td></tr>'}</tbody></table></div></div>` +
    (descriptor ? `<section class="panel result-panel"><h2>USB ${descriptor.vendor_id.toString(16).padStart(4,"0")}:${descriptor.product_id.toString(16).padStart(4,"0")}</h2><p>Host descriptor identity and physical location.</p><div class="fact-grid"><div class="fact"><span>Class</span><strong>${escapeHtml(descriptor.class_name)}</strong></div><div class="fact"><span>Bus / address</span><strong>${descriptor.bus} / ${descriptor.address}</strong></div><div class="fact"><span>Port path</span><strong>${escapeHtml((descriptor.ports || []).join("."))}</strong></div><div class="fact"><span>Configurations</span><strong>${descriptor.configuration_count}</strong></div></div></section>` : "") +
    (inspected && interfaces.length ? `<section class="panel interface-panel"><h2>Phone interfaces</h2><p>${escapeHtml(inspected.result.device.product)} · ${escapeHtml(inspected.result.device.interface_profile)} · ${interfaces.length} observed</p><div class="table-scroll"><table class="data-table"><thead><tr><th>No.</th><th>Role</th><th>Device name</th><th>Host driver</th><th>Endpoints</th><th>Serial port</th></tr></thead><tbody>${interfaceRows}</tbody></table></div></section>` : "") + renderOutcome(inspected, inspectKey);
}
function usbKey(device) { return `${device.vendor_id}:${device.product_id}:${device.bus}:${(device.ports || []).join(".")}`; }
async function refreshInventory() {
  if (state.inventoryLoading) return;
  state.inventoryLoading = true;
  renderStatus();
  if (state.page === "usb") renderMain();
  try {
    const inventory = await window.pywebview.api.get_usb_inventory();
    state.inventory = inventory;
    if (!inventory.some(item => usbKey(item) === state.inventorySelection)) state.inventorySelection = null;
    appendActivity("Refresh USB inventory", `${inventory.length} devices`);
  } catch (error) { setWork(`USB inventory unavailable: ${error.message || error}`); }
  finally { state.inventoryLoading = false; renderStatus(); if (state.page === "usb") renderMain(); }
}
function renderProtocols() {
  const [title, description] = pageInfo.protocols;
  const key = state.lastInspection[state.selectedDevice || ""];
  const outcome = key ? state.inspection[key] : null;
  return mainHeader(title, description, "Devices") +
    `<div class="toolbar"><label for="protocol-phone">Phone</label><select id="protocol-phone"><option value="">Choose a connected phone</option>${phoneOptions(state.selectedDevice)}</select><button class="button" id="refresh-phones">Refresh phones</button></div>` +
    `<div class="protocol-grid"><section class="panel protocol-card"><h2><span class="protocol-icon">${icon("connection")}</span>AT modem</h2><p>Identity, firmware revision, battery and signal codes reported by the phone.</p><button class="button" data-protocol="at-identity" ${state.selectedDevice ? "" : "disabled"}>Read identity</button><button class="button" data-protocol="at-status" ${state.selectedDevice ? "" : "disabled"}>Read status</button></section>` +
    `<section class="panel protocol-card"><h2><span class="protocol-icon">${icon("package")}</span>MTP / PTP</h2><p>Device metadata and a bounded root object listing.</p><button class="button" data-protocol="mtp" ${state.selectedDevice ? "" : "disabled"}>Read metadata</button><button class="button" data-protocol="mtp-list" ${state.selectedDevice ? "" : "disabled"}>List root objects</button><label for="mtp-limit">Items per storage</label> <input class="small-input" id="mtp-limit" type="number" min="0" max="128" value="${state.mtpLimit}" style="width:60px"></section>` +
    `<section class="panel protocol-card obex"><h2><span class="protocol-icon">${icon("connection")}</span>PC Suite OBEX</h2><p>Attempt a Connect/Disconnect exchange. Browsing and transfer are not part of this probe.</p><button class="button" data-protocol="obex" ${state.selectedDevice ? "" : "disabled"}>Connect and disconnect</button></section></div>` + renderOutcome(outcome, key || "");
}
async function inspectPhone(operation) {
  const selector = state.selectedDevice;
  if (!selector) return;
  const label = operation === "map" ? "Inspect USB map" : `Read ${operation.replace("-", " ")}`;
  setPending(label); setWork(`Working · ${label}`);
  const key = `${operation}:${selector}`;
  try {
    const outcome = await window.pywebview.api.inspect_device({selector, operation, limit: state.mtpLimit});
    state.inspection[key] = outcome;
    state.lastInspection[selector] = key;
    appendActivity(label, "Completed", outcome);
    setWork("Ready");
  } catch (error) {
    state.inspection[key] = {summary: {title: "Probe failed", message: String(error.message || error), facts: []}, result: {}};
    state.lastInspection[selector] = key;
    appendActivity(label, `Error: ${error.message || error}`);
    setWork(`Error · ${label}`);
  } finally {
    state.localPending = null;
    await refreshStatus();
    refreshContext();
    renderMain();
  }
}
function renderActivity() {
  const [title, description] = pageInfo.activity;
  const rows = state.activity.map(entry => `<tr><td>${escapeHtml(entry.time)}</td><td>${escapeHtml(entry.action)}</td><td>${escapeHtml(entry.outcome)}</td></tr>`).join("");
  return mainHeader(title, description) + `<div class="panel table-panel"><div class="table-scroll"><table class="data-table"><thead><tr><th>Time</th><th>Action</th><th>Outcome</th></tr></thead><tbody>${rows || '<tr><td colspan="3">No requests in this session yet.</td></tr>'}</tbody></table></div></div>`;
}
async function loadAgentProject() {
  try { state.agentProject = await window.pywebview.api.get_agent_project(); }
  catch (error) { state.agentError = String(error.message || error); }
  if (state.page === "development_agents") renderMain();
}
function renderAgents() {
  const phones = state.context?.devices || [];
  const project = state.agentProject?.project;
  const available = Boolean(project && state.agentProject?.compiler && state.agentProject?.linker);
  const cards = phones.map(phone => {
    const mtpCandidate = phone.identity_basis === "usb-serial" && (phone.interfaces || []).some(item =>
      item.class_code === 6 && item.subclass_code === 1 && item.protocol_code === 1);
    const hasTransfer = (phone.capabilities || []).includes("stage-sis") || mtpCandidate;
    const prepared = state.agentPackages[phone.selector] || state.agentIdentities[phone.selector];
    const canStage = hasTransfer && Boolean(prepared?.package);
    const staged = state.agentStaged[phone.selector];
    const reported = state.agentObservations[phone.selector];
    const live = state.agentLive[phone.selector];
    const logNames = {1: "Authenticated", 2: "Status read", 3: "Frame rejected", 4: "Session closed"};
    const logSnapshot = state.agentLogs[phone.selector];
    const logRows = (logSnapshot?.logs?.records || []).map(record => `<tr><td>${escapeHtml(record.sequence)}</td><td>${escapeHtml(logNames[record.code] || `Event ${record.code}`)}</td><td>${escapeHtml((Number(record.elapsed_us || 0) / 1000000).toFixed(2))} s</td></tr>`).join("");
    const logs = logSnapshot ? `<div class="agent-event-log"><h3>Service events</h3><p>Recent events from this agent process. Times are elapsed since it started.</p>${logSnapshot.logs.gap ? '<p class="muted">Older events were overwritten.</p>' : ''}<div class="table-scroll"><table class="data-table"><thead><tr><th>#</th><th>Event</th><th>Elapsed</th></tr></thead><tbody>${logRows || '<tr><td colspan="3">No events retained.</td></tr>'}</tbody></table></div></div>` : "";
    const fresh = live && Date.now() - Date.parse(live.checked_at) < 30000;
    const checkedTime = live ? new Date(live.checked_at).toLocaleString(undefined, {dateStyle: "medium", timeStyle: "short"}) : "";
    const stateLabel = fresh ? `Verified live · ${live.status.state}` : live ? "Previously verified · check again" : reported ? "Running reported · live status unchecked" : staged ? "Package staged · installation unverified" : "Agent installation unknown";
    return `<section class="panel agent-card"><div class="agent-card-head"><span class="action-icon">${icon("phone")}</span><div><h2>${escapeHtml(phone.product)}</h2><p>${escapeHtml(phone.interface_profile)} · ${escapeHtml(phone.selector)}</p></div><span class="agent-badge${fresh ? " verified" : ""}">${escapeHtml(stateLabel)}</span></div>` +
      (live ? `<p>Authenticated ${escapeHtml(checkedTime)} over Wi-Fi (${escapeHtml(live.host)}). USB identifies this phone and stages packages; check again for a current reading.</p>` : `<p>USB discovery identifies the phone and can stage its package. The phone initiates a Wi-Fi connection to this Mac; the private pairing key authenticates the reply.</p>`) +
      (!reported && !live ? `<ol><li>Build a phone-specific agent package.</li><li>Stage the SIS through PC Suite MTP or a writable USB storage volume.</li><li>Install and open it on the phone. Compare the pairing code on its panel, then check live status on the same Wi-Fi.</li></ol>` : "") +
      `<div class="agent-controls"><button class="button" data-agent-build="${escapeHtml(phone.selector)}" ${available && !state.agentBusy ? "" : "disabled"}>${icon("build")} Build for this phone</button><button class="button" data-agent-stage="${escapeHtml(phone.selector)}" ${canStage && !state.agentBusy ? "" : "disabled"}>${icon("upload")} Stage agent package</button>${!hasTransfer ? '<small>Connect in PC Suite mode or mount a writable USB storage volume.</small>' : !prepared ? '<small>Build a phone-specific package first.</small>' : mtpCandidate && !(phone.capabilities || []).includes("stage-sis") ? '<small>The writable MTP Installs folder is checked before transfer.</small>' : ""}</div>` +
      (prepared ? `<p>Pairing code: <strong>${escapeHtml(prepared.pairing_code)}</strong>. Check that this code appears on the agent panel on this phone. The phone discovers the console automatically on local Wi-Fi.</p><label><input type="checkbox" data-agent-pair="${escapeHtml(phone.selector)}" ${state.agentPairConfirmed[phone.selector] ? "checked" : ""}> The phone shows this code</label><div class="agent-controls"><button class="button" data-agent-verify="${escapeHtml(phone.selector)}" ${state.agentPairConfirmed[phone.selector] && !state.agentBusy ? "" : "disabled"}>Check live status</button>${live ? `<button class="button" data-agent-logs="${escapeHtml(phone.selector)}" ${state.agentBusy ? "disabled" : ""}>Read service events</button>` : ""}</div>` : "") +
      `<div class="agent-controls"><button class="button" data-agent-report="${escapeHtml(phone.selector)}" ${state.agentBusy ? "disabled" : ""}>${reported ? "Clear running report" : "I see the agent running"}</button><small>${reported ? "Clearing only changes this computer's record." : "Records your observation on this computer; no phone operation is sent."}</small></div>` +
      (staged ? `<p class="muted">${escapeHtml(staged.next_action || "Finish installation on the phone.")}</p>` : "") + logs + `</section>`;
  }).join("");
  const build = !project || !available ? `<section class="panel agent-build"><h2>Build availability</h2><p>${!project ? 'The agent source project is unavailable in this SDK installation.' : 'Select an active SDK in the sidebar to build.'}</p></section>` : "";
  return mainHeader("Development Agents", "Build a phone-specific agent, stage its SIS over USB, then verify status over Wi-Fi.") +
    (state.agentError ? `<div class="inline-alert">${escapeHtml(state.agentError)}</div>` : "") +
    build + `<h2 class="agent-section-title">Connected devices</h2>` +
    (cards || '<div class="panel empty-library">Connect a supported phone to see its installation steps.</div>') +
    (state.agentOutcome ? renderOutcome(state.agentOutcome, "agent-action") : "");
}
async function buildAgent(selector) {
  if (!state.agentProject?.project || state.agentBusy) return;
  state.agentBusy = "build"; state.agentError = "";
  delete state.agentPackages[selector]; delete state.agentLive[selector]; delete state.agentLogs[selector];
  setWork("Building development agent…"); renderMain();
  try {
    const prepared = await window.pywebview.api.build_phone_agent(selector);
    state.agentPackages[selector] = prepared;
    state.agentIdentities[selector] = prepared;
    appendActivity("Build development agent", "Phone-specific package prepared", prepared);
    setWork("Ready");
  } catch (error) { state.agentError = String(error.message || error); setWork("Agent build failed"); }
  finally { state.agentBusy = ""; renderMain(); }
}
async function stageAgent(selector) {
  const prepared = state.agentPackages[selector] || state.agentIdentities[selector];
  if (!prepared?.package || state.agentBusy) return;
  state.agentBusy = "stage"; state.agentError = "";
  setWork("Staging development agent SIS…"); renderMain();
  try {
    const outcome = await window.pywebview.api.run_form({path: ["device", "install"], values: {
      project: state.agentProject.project, device: selector, package: prepared.package,
    }});
    state.agentStaged[selector] = outcome.result?.result || {};
    state.agentOutcome = outcome;
    appendActivity("Stage development agent SIS", "Awaiting phone installer", outcome);
    setWork("Ready");
  } catch (error) {
    state.agentError = String(error.message || error);
    appendActivity("Stage development agent SIS", state.agentError, {error: state.agentError});
    setWork(`Agent staging failed: ${state.agentError}`);
  }
  finally { state.agentBusy = ""; renderMain(); }
}
async function verifyAgent(selector) {
  if (!state.agentPairConfirmed[selector] || state.agentBusy) return;
  state.agentBusy = "verify"; state.agentError = ""; delete state.agentLive[selector];
  setWork("Checking agent over Wi-Fi…"); renderMain();
  try {
    const result = await window.pywebview.api.verify_agent_status(selector);
    state.agentLive[selector] = result;
    appendActivity("Check development agent", "Authenticated live status", result);
    setWork("Ready");
  } catch (error) { state.agentError = String(error.message || error); setWork("Agent status check failed"); }
  finally { state.agentBusy = ""; renderMain(); }
}
async function readAgentLogs(selector) {
  if (state.agentBusy || !state.agentLive[selector]) return;
  state.agentBusy = "logs"; state.agentError = "";
  setWork("Reading agent events…"); renderMain();
  try {
    const result = await window.pywebview.api.read_agent_logs(selector);
    state.agentLogs[selector] = result;
    appendActivity("Read development agent events", "Authenticated event snapshot", result);
    setWork("Ready");
  } catch (error) { state.agentError = String(error.message || error); setWork("Agent event read failed"); }
  finally { state.agentBusy = ""; renderMain(); }
}
async function setAgentReport(selector) {
  if (state.agentBusy) return;
  const reported = Boolean(state.agentObservations[selector]);
  state.agentBusy = "report"; state.agentError = ""; renderMain();
  try {
    if (reported) {
      await window.pywebview.api.clear_agent_report(selector);
      delete state.agentObservations[selector];
    } else {
      state.agentObservations[selector] = await window.pywebview.api.report_agent_running(selector);
    }
    appendActivity("Agent observation", reported ? "Local report cleared" : "Owner reported agent running");
  } catch (error) { state.agentError = String(error.message || error); }
  finally { state.agentBusy = ""; renderMain(); }
}
function renderApplication() {
  const project = state.context?.project;
  if (!project) return state.initialApplicationView && !state.context
    ? '<div class="application-suspense"><span class="spinner" aria-hidden="true"></span><span>Opening application…</span></div>'
    : mainHeader("Application", "Choose an application folder in the sidebar.") +
      '<p class="empty-copy">No application selected.</p>';
  const app = state.applicationOverview;
  if (!app || app.directory !== project) return mainHeader("Application", project) +
    (state.applicationError ? `<div class="inline-alert">${escapeHtml(state.applicationError)}</div>` : '<div class="loading-state"><span class="spinner" aria-hidden="true"></span>Reading application…</div>');
  const records = state.applicationFirmware?.objects || [];
  const firmwareOptions = records.map(record => {
    const reference = record.aliases[0] || record.firmware;
    const compatible = record.device.kernel === "eka2";
    return `<option value="${escapeHtml(reference)}" ${state.applicationSelectedFirmware === reference ? "selected" : ""} ${compatible ? "" : "disabled"}>${escapeHtml(record.device.model)} · ${compatible ? escapeHtml(reference) : "Unavailable · EKA1 ABI"}</option>`;
  }).join("");
  const isRunnable = ["e32-pic", "e32-import", "e32-pic-experiment", "e32-import-experiment"].includes(app.kind);
  const chosen = state.applicationSelectedFirmware || app.firmware;
  const selectedRecord = records.find(record => record.firmware === chosen || (record.aliases || []).includes(chosen));
  const firmwareCompatible = !selectedRecord || selectedRecord.device.kernel === "eka2";
  const firmwareHint = state.applicationFirmwareError ? `<span class="inline-alert">${escapeHtml(state.applicationFirmwareError)}</span>`
    : state.applicationFirmwareBusy ? '<span class="muted">Reading imported firmware…</span>'
    : !records.length ? '<span class="muted">Import a firmware in Firmware before running.</span>' : "";
  const incompatibleHint = firmwareCompatible ? "" : '<span class="muted">EKA1 firmware is unavailable for this EKA2 application.</span>';
  const facts = [["Target", app.architecture], ["UID3", app.uid3], ["Executable", app.artifact ? app.artifact.split(/[\\/]/).pop() : "Build needed"], ["Package", app.package_name || "Not configured"]];
  return mainHeader("Application", app.directory) +
    `<section class="panel application-panel"><div class="application-heading"><span class="action-icon">${app.icon_data_url ? `<img src="${escapeHtml(app.icon_data_url)}" alt="">` : icon("app")}</span><div class="application-identity"><h2>${escapeHtml(app.caption)}</h2><p>${escapeHtml(app.name)} · ${escapeHtml(app.generated ? "SDK project" : "Standalone CMake")}</p></div><dl class="application-facts">${facts.map(([label, value]) => `<div><dt>${escapeHtml(label)}</dt><dd>${escapeHtml(value)}</dd></div>`).join("")}</dl></div>` +
    `<div class="application-operations"><section><h3>Build</h3><p>Compile and verify the executable for the selected target.</p><button class="button" data-application-action="build" ${state.applicationBusy ? "disabled" : ""}>${icon("tools")} Build application</button></section>` +
    `<section><h3>Run in emulator</h3><p>Build, launch and supervise this application using imported firmware.</p><label for="application-firmware">Firmware</label><select id="application-firmware" ${state.applicationBusy || !isRunnable ? "disabled" : ""}><option value="" ${!firmwareCompatible && !state.applicationSelectedFirmware ? "disabled" : ""}>${escapeHtml(app.firmware ? `Current · ${app.firmware}` : "Choose imported firmware")}${!firmwareCompatible && !state.applicationSelectedFirmware ? " · Unavailable" : ""}</option>${firmwareOptions}</select>${firmwareHint}${incompatibleHint}<button class="button primary" data-application-action="run" ${state.applicationBusy || !isRunnable || !chosen || !firmwareCompatible ? "disabled" : ""}>${icon("play")} Run application</button></section>` +
    `<section><h3>Package</h3><p>Create an unsigned SIS package from the current executable.</p><button class="button" data-application-action="package" ${state.applicationBusy || !app.artifact || !app.package_name ? "disabled" : ""}>${icon("package")} Package application</button>${!app.artifact ? '<small>Build the application first.</small>' : ""}</section></div></section>` +
    (state.applicationAction === "build" && (state.applicationBusy || state.applicationBuildLog) ? `<details class="panel build-log-panel" ${state.applicationBusy ? "open" : ""}><summary>Build output</summary><pre id="application-build-log" class="build-log">${escapeHtml(state.applicationBuildLog || "Starting build…")}</pre></details>` : "") +
    (state.applicationAction === "run" && (state.applicationBusy || state.applicationRunLog) ? `<details class="panel build-log-panel" ${state.applicationBusy ? "open" : ""}><summary>Run output</summary><pre id="application-run-log" class="build-log">${escapeHtml(state.applicationRunLog || "Preparing emulator…")}</pre></details>` : "") +
    (state.applicationOutcome ? state.applicationAction === "build" ? renderBuildOutcome(state.applicationOutcome) : renderOutcome(state.applicationOutcome, "application-action") : "");
}
function renderBuildOutcome(outcome) {
  const data = outcomePayload(outcome);
  if (!data) return renderOutcome(outcome, "application-action");
  const groups = data.build_system?.compile_groups || [];
  const commands = groups.map(group => {
    const fragments = (group.compileCommandFragments || []).map(item => item.fragment).join(" ");
    return `<div class="compile-command"><strong>${escapeHtml(group.language || "Compile")}</strong><code>${escapeHtml(fragments)}</code></div>`;
  }).join("");
  return `<section class="panel result-panel build-summary"><h2>Build complete</h2><div class="data-rows">` +
    `<div class="data-row"><span>Executable</span><span>${escapeHtml(data.artifact || "Unknown")}</span></div>` +
    `<div class="data-row"><span>Reproducible</span><span>${data.reproducible ? "Yes" : "Not verified"}</span></div>` +
    `<div class="data-row"><span>SHA-256</span><span>${escapeHtml(data.sha256 || "Unknown")}</span></div></div>` +
    (commands ? `<details class="compile-commands"><summary>Compile commands · ${groups.length} languages</summary>${commands}</details>` : "") +
    `</section>${renderRawDetails(outcome, "application-action")}`;
}
async function loadApplication(force = false) {
  const project = state.context?.project;
  if (!project || state.applicationLoading || (!force && state.applicationOverview?.directory === project)) return;
  state.applicationLoading = true;
  state.applicationError = "";
  renderStatus();
  try {
    const overview = await window.pywebview.api.get_application_overview();
    if (state.context?.project === project) {
      state.applicationOverview = overview;
      renderNavigation();
    }
  } catch (error) {
    if (state.context?.project === project) state.applicationError = String(error.message || error);
  } finally {
    state.applicationLoading = false;
    renderStatus();
    if (state.context?.project !== project && state.context?.project) loadApplication();
    if (state.page === "application_detail") renderMain();
  }
}
async function loadApplicationFirmware(force = false) {
  const project = state.context?.project;
  if (!project || state.applicationFirmwareBusy || (!force && state.applicationFirmware)) return;
  state.applicationFirmwareBusy = true;
  state.applicationFirmwareError = "";
  renderStatus();
  try {
    const outcome = await window.pywebview.api.run_form({path: ["firmware", "list"], values: {project, root: state.context.workspace}});
    if (state.context?.project === project) state.applicationFirmware = outcome.firmware_library;
  } catch (error) {
    if (state.context?.project === project) state.applicationFirmwareError = String(error.message || error);
  } finally {
    state.applicationFirmwareBusy = false;
    renderStatus();
    if (state.context?.project !== project && state.context?.project) loadApplicationFirmware();
    if (state.page === "application_detail") renderMain();
  }
}
async function runApplicationAction(action) {
  const app = state.applicationOverview;
  if (!app || app.directory !== state.context?.project || state.applicationBusy) return;
  const path = action === "build" ? (app.generated ? ["app", "build"] : ["build"])
    : action === "run" ? ["app", "run"] : ["package"];
  const values = {project: app.directory};
  if (action === "build" && !app.generated) values.output = `${app.directory}/.symbian/build`;
  if (action === "run") {
    const firmware = state.applicationSelectedFirmware || app.firmware;
    if (!firmware) return;
    values.firmware = firmware;
  }
  if (action === "package") {
    if (!app.artifact || !app.package_name) return;
    values.artifact = app.artifact;
    values.output = `${app.directory}/.symbian/package`;
  }
  const label = `${action[0].toUpperCase()}${action.slice(1)} ${app.name}`;
  state.applicationBusy = label;
  state.applicationAction = action;
  state.applicationOutcome = null;
  if (action === "build") state.applicationBuildLog = "";
  if (action === "run") state.applicationRunLog = "";
  setWork(`Working · ${label}`);
  renderMain();
  const refreshApplicationLog = async () => {
    try {
      const content = action === "run"
        ? await window.pywebview.api.get_run_log()
        : await window.pywebview.api.get_build_log();
      if (action === "run") state.applicationRunLog = content;
      else state.applicationBuildLog = content;
      const view = document.getElementById(action === "run" ? "application-run-log" : "application-build-log");
      if (view) {
        const follow = view.scrollTop + view.clientHeight >= view.scrollHeight - 40;
        view.textContent = content || (action === "run" ? "Preparing emulator…" : "Starting build…");
        if (follow) view.scrollTop = view.scrollHeight;
      }
    } catch (_) { /* The final result still reports action errors. */ }
  };
  const logTimer = action === "build" || action === "run"
    ? setInterval(refreshApplicationLog, action === "run" ? 400 : 250) : null;
  try {
    const outcome = await window.pywebview.api.run_form({path, values});
    if (state.context?.project === app.directory) {
      state.applicationOutcome = outcome;
      state.outcomes["application-action"] = outcome;
    }
    appendActivity(label, "Completed", outcome);
    await loadApplication(true);
    setWork("Ready");
  } catch (error) {
    const message = String(error.message || error);
    if (state.context?.project === app.directory) {
      state.applicationOutcome = {summary: {title: `${action} failed`, message, facts: []}, result: {result: null}};
      state.outcomes["application-action"] = state.applicationOutcome;
    }
    appendActivity(label, `Error: ${message}`);
    setWork(`Error · ${label}`);
  } finally {
    if (logTimer) {
      clearInterval(logTimer);
      await refreshApplicationLog();
    }
    state.applicationBusy = "";
    renderMain();
  }
}
function bindPage() {
  document.querySelectorAll("[data-agent-build]").forEach(button => button.addEventListener("click", () => buildAgent(button.dataset.agentBuild)));
  document.querySelectorAll("[data-agent-stage]").forEach(button => button.addEventListener("click", () => stageAgent(button.dataset.agentStage)));
  document.querySelectorAll("[data-agent-pair]").forEach(input => input.addEventListener("change", () => { state.agentPairConfirmed[input.dataset.agentPair] = input.checked; renderMain(); }));
  document.querySelectorAll("[data-agent-verify]").forEach(button => button.addEventListener("click", () => verifyAgent(button.dataset.agentVerify)));
  document.querySelectorAll("[data-agent-logs]").forEach(button => button.addEventListener("click", () => readAgentLogs(button.dataset.agentLogs)));
  document.querySelectorAll("[data-agent-report]").forEach(button => button.addEventListener("click", () => setAgentReport(button.dataset.agentReport)));
  document.getElementById("application-firmware")?.addEventListener("change", event => {
    state.applicationSelectedFirmware = event.target.value;
    renderMain();
  });
  document.querySelectorAll("[data-application-action]").forEach(button => button.addEventListener("click", () => runApplicationAction(button.dataset.applicationAction)));
  document.querySelectorAll(".tab-strip [data-page]").forEach(button =>
    button.addEventListener("click", () => setPage(button.dataset.page)));
  document.querySelectorAll("[data-task]").forEach(button => button.addEventListener("click", () => {
    activateTask(button.dataset.task);
  }));
  document.querySelectorAll("#main [data-field]").forEach(control => {
    const task = selectedTask(); if (!task) return;
    const key = taskKey(task);
    const update = () => {
      state.drafts[key] ||= {};
      state.drafts[key][control.dataset.field] = control.type === "checkbox" ? control.checked : control.value;
      if (control.dataset.field === "device") selectPhone(control.value);
    };
    control.addEventListener(control.tagName === "SELECT" ? "change" : "input", update);
  });
  document.querySelectorAll("#main [data-live-field]").forEach(control => {
    const update = () => {
      state.drafts["firmware list"] ||= {};
      state.drafts["firmware list"][control.dataset.liveField] = control.type === "checkbox" ? control.checked : control.value;
    };
    control.addEventListener(control.tagName === "SELECT" ? "change" : "input", update);
  });
  document.querySelectorAll("#main [data-live-browse]").forEach(button => button.addEventListener("click", async () => {
    const name = button.dataset.liveBrowse;
    const current = document.getElementById(`live-${name}`)?.value || "";
    try {
      const path = await window.pywebview.api.choose_path(folderNames.has(name) ? "folder" : "file", current);
      if (path) { state.drafts["firmware list"] ||= {}; state.drafts["firmware list"][name] = path; renderMain(); }
    } catch (error) { showError(error); }
  }));
  document.querySelector("#main [data-live-apply]")?.addEventListener("click", () => {
    const task = liveBaseTask("firmware");
    if (task) runLive(task);
  });
  document.querySelector("[data-live-source]")?.addEventListener("toggle", event => { state.inspectorAdvanced["firmware source"] = event.target.open; });
  document.querySelector("[data-live-source-extra]")?.addEventListener("toggle", event => { state.inspectorAdvanced["firmware extras"] = event.target.open; });
  document.querySelectorAll("#main [data-browse]").forEach(button => button.addEventListener("click", async () => {
    const task = selectedTask(); if (!task) return;
    const name = button.dataset.browse;
    const current = document.getElementById(`field-${name}`)?.value || "";
    const kind = folderNames.has(name) ? "folder" : name === "destination" || name === "ticket" ? "save" : "file";
    try {
      const path = await window.pywebview.api.choose_path(kind, current);
      if (path) { state.drafts[taskKey(task)] ||= {}; state.drafts[taskKey(task)][name] = path; renderMain(); }
    } catch (error) { showError(error); }
  }));
  document.querySelector("[data-forward]")?.addEventListener("click", () => { const task = selectedTask(); if (task) advanceTask(task); });
  document.querySelector("[data-back]")?.addEventListener("click", () => { const task = selectedTask(); if (task) { state.step[taskKey(task)]--; renderMain(); } });
  document.querySelector("[data-optional-settings]")?.addEventListener("toggle", event => {
    state.optionalOpen[event.target.dataset.optionalSettings] = event.target.open;
  });
  document.querySelector("[data-advanced]")?.addEventListener("click", () => { const task = selectedTask(); if (task) { state.advanced[taskKey(task)] = !state.advanced[taskKey(task)]; renderMain(); } });
  document.querySelectorAll("[data-details]").forEach(button => button.addEventListener("click", () => { const key = button.dataset.details; state.detailsOpen[key] = !state.detailsOpen[key]; renderMain(); if (state.detailsOpen[key]) highlightVisible(); }));
  document.querySelectorAll("[data-copy]").forEach(button => button.addEventListener("click", async () => { const key = button.dataset.copy; const outcome = key.startsWith("task:") ? state.outcomes[key.slice(5)] : state.inspection[key]; await copyText(JSON.stringify(outcomePayload(outcome) ?? {}, null, 2)); }));
  document.querySelectorAll("[data-file-query]").forEach(input => input.addEventListener("input", () => {
    const key = input.dataset.fileQuery;
    if (key === "task:firmware inspect" && state.firmwareSelection) state.firmwareFileSearch[state.firmwareSelection] = input.value;
    const outcome = key.startsWith("task:") ? state.outcomes[key.slice(5)] : state.inspection[key];
    const files = outcome?.result?.result?.files;
    const list = document.querySelector(`[data-file-list="${CSS.escape(key)}"]`);
    if (list && files) list.innerHTML = ConsoleResultViews.fileIndex(files, input.value, escapeHtml);
  }));
  document.querySelector("[data-close-operation]")?.addEventListener("click", () => {
    const base = liveBaseTask(state.page);
    if (base) { state.selectedTask[state.page] = taskKey(base); renderMain(); }
  });
  document.querySelectorAll("#main [data-open-task]").forEach(button => button.addEventListener("click", () => openRelatedTask(button.dataset.openTask)));
  document.querySelectorAll("[data-open-usb]").forEach(button => button.addEventListener("click", async () => {
    await selectPhone(button.dataset.openUsb); setPage("usb"); inspectPhone("map");
  }));
  document.querySelectorAll("[data-live-refresh]").forEach(button => button.addEventListener("click", () => {
    const task = state.tasks.find(item => taskKey(item) === button.dataset.liveRefresh);
    if (task) runLive(task);
  }));
  document.getElementById("firmware-search")?.addEventListener("input", event => {
    state.firmwareSearch = event.target.value;
    const records = state.outcomes["firmware list"]?.firmware_library?.objects || [];
    document.querySelectorAll(".firmware-row").forEach(row => {
      const record = records.find(item => item.firmware === row.dataset.firmware);
      const text = record ? [record.device.model, record.device.firmware_code, record.device.manufacturer, ...record.aliases].join(" ").toLowerCase() : "";
      row.hidden = !text.includes(state.firmwareSearch.toLowerCase());
    });
  });
  document.querySelectorAll("[data-firmware]").forEach(row => row.addEventListener("click", () => {
    state.firmwareSelection = row.dataset.firmware;
    state.firmwareManuallySelected = true;
    const records = state.outcomes["firmware list"]?.firmware_library?.objects || [];
    const record = records.find(item => item.firmware === state.firmwareSelection);
    const reference = record?.aliases[0] || record?.firmware;
    if (reference) for (const key of ["firmware inspect", "firmware export"]) {
      state.drafts[key] ||= {}; state.drafts[key].reference = reference;
    }
    state.outcomes["firmware inspect"] = state.firmwareInspections[state.firmwareSelection] || null;
    state.detailsOpen["task:firmware inspect"] = false;
    renderMain();
    loadFirmwareInspection(state.firmwareSelection);
  }));
  document.querySelectorAll("[data-retry-firmware]").forEach(button => button.addEventListener("click", () => loadFirmwareInspection(button.dataset.retryFirmware)));
  document.getElementById("refresh-usb")?.addEventListener("click", refreshInventory);
  document.getElementById("inspect-usb")?.addEventListener("click", () => inspectPhone("map"));
  document.getElementById("refresh-phones")?.addEventListener("click", refreshContext);
  document.getElementById("usb-phone")?.addEventListener("change", event => selectPhone(event.target.value));
  document.getElementById("protocol-phone")?.addEventListener("change", event => selectPhone(event.target.value));
  document.getElementById("mtp-limit")?.addEventListener("input", event => {
    state.mtpLimit = Math.max(0, Math.min(128, Number(event.target.value || 0)));
  });
  document.querySelectorAll("[data-protocol]").forEach(button => button.addEventListener("click", () => inspectPhone(button.dataset.protocol)));
  document.querySelectorAll("[data-usb-row]").forEach(row => row.addEventListener("click", () => { state.inventorySelection = usbKey(state.inventory[Number(row.dataset.usbRow)]); renderMain(); }));
}
function activateTask(key, prefills = {}) {
  const task = state.tasks.find(item => taskKey(item) === key);
  if (!task) return;
  const page = groupPages[task.presentation.group];
  if (page && page !== state.page) setPage(page);
  if (key === "emu configure") {
    const settings = state.outcomes["emu resolve"]?.result?.result?.settings || {};
    prefills = {...settings, ...prefills};
  }
  state.selectedTask[state.page] = key;
  state.drafts[key] = {...(state.drafts[key] || {}), ...prefills};
  renderMain();
  fetchDefaults(task);
  document.querySelector(".operation-head, .task-panel")?.scrollIntoView({behavior: "smooth", block: "start"});
}
function openRelatedTask(key) {
  if (key === "firmware inspect") {
    loadFirmwareInspection(state.firmwareSelection);
    return;
  }
  const records = state.outcomes["firmware list"]?.firmware_library?.objects || [];
  const record = records.find(item => item.firmware === state.firmwareSelection);
  const reference = record?.aliases[0] || record?.firmware;
  const target = state.tasks.find(item => taskKey(item) === key);
  const accepted = new Set(target?.command.arguments.map(argument => argument.name) || []);
  const applied = state.liveApplied["firmware list"] || {};
  const shared = Object.fromEntries(
    Object.entries(applied).filter(([name, value]) => accepted.has(name) && value)
  );
  const store = state.outcomes["firmware list"]?.firmware_library?.store;
  if (store && accepted.has("store")) shared.store = store;
  if (reference && accepted.has("reference")) shared.reference = reference;
  activateTask(key, shared);
}
function bindInspector() {
  const panel = document.getElementById("context-panel");
  const task = liveBaseTask(state.page);
  if (!task || panel.hidden) return;
  const key = taskKey(task);
  panel.querySelectorAll("[data-field]").forEach(control => {
    const update = () => {
      state.drafts[key] ||= {};
      state.drafts[key][control.dataset.field] = control.type === "checkbox" ? control.checked : control.value;
    };
    control.addEventListener(control.tagName === "SELECT" ? "change" : "input", update);
  });
  panel.querySelectorAll("[data-browse]").forEach(button => button.addEventListener("click", async () => {
    const name = button.dataset.browse;
    const current = panel.querySelector(`#field-${name}`)?.value || "";
    try {
      const path = await window.pywebview.api.choose_path(folderNames.has(name) ? "folder" : "file", current);
      if (path) { state.drafts[key] ||= {}; state.drafts[key][name] = path; renderMain(); }
    } catch (error) { showError(error); }
  }));
  panel.querySelector("[data-live-apply]")?.addEventListener("click", () => runLive(task));
  panel.querySelector("[data-inspector-more]")?.addEventListener("toggle", event => { state.inspectorAdvanced[key] = event.target.open; });
  panel.querySelectorAll("[data-open-task]").forEach(button => button.addEventListener("click", () => openRelatedTask(button.dataset.openTask)));
}
async function copyText(text) {
  try { await navigator.clipboard.writeText(text); }
  catch (_) {
    const input = document.createElement("textarea"); input.value = text; document.body.appendChild(input);
    input.select(); document.execCommand("copy"); input.remove();
  }
}
function showError(error) { setWork(`Error · ${error.message || error}`); }
async function boot() {
  try {
    const [catalog, startupView] = await Promise.all([
      window.pywebview.api.get_catalog(),
      window.pywebview.api.get_startup_view(),
    ]);
    state.tasks = catalog.tasks;
    state.initialApplicationView = startupView === "application_detail";
    if (state.initialApplicationView) state.page = "application_detail";
    renderNavigation(); renderSelection(); renderMain();
    const task = selectedTask(); if (task) fetchDefaults(task);
    await pollUsbTopology();
    await refreshContext();
    if (task) await fetchDefaults(task);
    setWork("Ready");
    setInterval(pollUsbTopology, 750);
    setInterval(refreshContext, 2500);
  } catch (error) {
    setWork(`Console unavailable: ${error.message || error}`);
    document.getElementById("main").innerHTML = `<div class="main-inner"><div class="inline-alert">${escapeHtml(error.message || error)}</div></div>`;
  }
}
window.addEventListener("pywebviewready", boot);
