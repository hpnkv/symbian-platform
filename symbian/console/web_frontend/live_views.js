"use strict";

/* Read-only pages lead with current information, not a command form. */
const ConsoleLiveViews = (() => {
  const nokia808Image = "__NOKIA_808_IMAGE__";
  const settings = {
    "firmware list": ["store", "project", "sdk", "root", "emulator", "importer", "backend", "language", "profile"],
    "emu resolve": ["project", "sdk", "firmware", "store", "root", "emulator", "importer", "backend", "language", "profile"],
  };
  function outcomeData(state, key) {
    return state.outcomes[key]?.result?.result || null;
  }
  function status(state, key) {
    const working = Boolean(state.liveBusy[key]);
    const updated = state.liveUpdated[key];
    return `<div class="live-head"><span class="muted">${working ? "Refreshing…" : updated ? `Updated ${escapeHtml(updated)}` : state.liveErrors[key] ? "Unavailable" : "Loading…"}</span><button class="button" data-live-refresh="${escapeHtml(key)}" ${working ? "disabled" : ""}>Refresh</button></div>` +
      (state.liveErrors[key] ? `<div class="inline-alert live-error">${escapeHtml(state.liveErrors[key])}</div>` : "");
  }
  function firmware(state) {
    const key = "firmware list";
    const library = state.outcomes[key]?.firmware_library;
    const records = library?.objects || [];
    const selected = state.firmwareSelection;
    const filtered = records.filter(record => {
      const search = state.firmwareSearch.toLowerCase();
      return !search || [record.device.model, record.device.firmware_code, record.device.manufacturer, ...record.aliases].some(value => String(value).toLowerCase().includes(search));
    });
    const rows = filtered.map(record => `<button class="firmware-row ${record.firmware === selected ? "active" : ""}" data-firmware="${escapeHtml(record.firmware)}"><span class="action-icon">${icon("phone")}</span><span class="firmware-main"><strong>${escapeHtml(record.device.model)}</strong><small>${escapeHtml(record.aliases.join(", ") || record.device.manufacturer)} · ${escapeHtml(record.device.symbian_version)} · ${escapeHtml(record.device.kernel.toUpperCase())}</small></span><span class="firmware-code">${escapeHtml(record.device.firmware_code)}</span></button>`).join("");
    const record = records.find(item => item.firmware === selected);
    return status(state, key) + `<div class="firmware-browser"><section class="firmware-catalog"><div class="library-toolbar"><strong>${records.length} imported ${records.length === 1 ? "identity" : "identities"}</strong><input class="library-search" type="search" id="firmware-search" placeholder="Find model or alias" value="${escapeHtml(state.firmwareSearch)}" aria-label="Find imported firmware"></div>` +
      `<div class="panel library-list">${rows || `<div class="empty-library"><p>${library ? "No firmware matches this search." : "Loading imported firmware…"}</p>${library && !records.length ? '<button class="button primary" data-open-task="firmware import">Import firmware</button>' : ""}</div>`}</div>${firmwareSource(state, library)}</section>` +
      `<section class="panel firmware-inspection">${record ? firmwareDetails(record, state, library) : '<div class="empty-library">Select an imported firmware identity to inspect it.</div>'}</section></div>`;
  }
  function firmwareSource(state, library) {
    const task = state.tasks.find(item => item.command.path.join(" ") === "firmware list");
    if (!task) return "";
    const values = formValues(task);
    if (!values.store) values.store = library?.store || "";
    const byName = new Map(task.command.arguments.map(argument => [argument.name, argument]));
    const renderFields = names => names.map(name => byName.has(name) ? fieldMarkup(byName.get(name), values[name], [], "data-live-field", "live") : "").join("");
    return `<details class="library-source" data-live-source ${state.inspectorAdvanced["firmware source"] ? "open" : ""}><summary>Library source</summary><div class="form-grid">${renderFields(["store"])}</div>` +
      `<details data-live-source-extra ${state.inspectorAdvanced["firmware extras"] ? "open" : ""}><summary>More source options</summary><div class="form-grid">${renderFields(settings["firmware list"].filter(name => name !== "store"))}</div></details>` +
      '<button class="button" data-live-apply="firmware list">Apply and refresh</button></details>';
  }
  function firmwareDetails(record, state, library) {
    const identity = record.firmware;
    const outcome = state.firmwareInspections[identity];
    const data = outcome?.result?.result;
    const device = data?.device || record.device;
    const facts = [["Manufacturer", device.manufacturer], ["Symbian", device.symbian_version], ["Kernel", device.kernel?.toUpperCase()], ["Machine UID", device.machine_uid], ["ROM", device.rom], ["Drive Z", device.z_drive], ["Drive C", device.c_drive]];
    const rows = facts.filter(([, value]) => value !== undefined && value !== null && value !== "").map(([label, value]) => `<div class="data-row"><span>${escapeHtml(label)}</span><span>${escapeHtml(value)}</span></div>`).join("");
    const error = state.firmwareInspectionErrors[identity];
    const query = state.firmwareFileSearch[identity] || "";
    const files = data?.files || {};
    return `<div class="firmware-inspection-head"><div><h2>${escapeHtml(device.model)}</h2><p>${escapeHtml(device.firmware_code)} · ${escapeHtml(record.aliases.join(", ") || "No alias")}</p></div><button class="button" data-open-task="firmware export">Export…</button></div>` +
      `<div class="inspection-summary"><span>${library?.integrity_verified ? "File bytes verified" : "Catalog metadata; file bytes unverified"}</span><span>${data ? `${Object.keys(files).length} files indexed` : state.firmwareInspectionBusy[identity] ? "Reading manifest…" : "Manifest not loaded"}</span></div>` +
      `<div class="data-rows">${rows}</div>` +
      (error ? `<div class="inline-alert">${escapeHtml(error)} <button class="tiny-button" data-retry-firmware="${escapeHtml(identity)}">Retry</button></div>` : "") +
      (data ? `<section class="result-data"><div class="inspection-section-head"><h3>Files</h3><input class="file-search" type="search" placeholder="Find a file" aria-label="Find a firmware file" data-file-query="task:firmware inspect" value="${escapeHtml(query)}"></div><div data-file-list="task:firmware inspect">${ConsoleResultViews.fileIndex(files, query, escapeHtml)}</div></section>` +
        `<details class="inspection-provenance"><summary>Import provenance</summary>${ConsoleResultViews.render(data.provenance || {}, "task:firmware inspect", escapeHtml)}</details>` +
        `<div class="inspection-raw">${renderRawDetails(outcome, "task:firmware inspect")}</div>` : state.firmwareInspectionBusy[identity] ? '<div class="loading-state"><span class="spinner" aria-hidden="true"></span>Reading imported firmware…</div>' : "");
  }
  function emulator(state) {
    const key = "emu resolve";
    const data = outcomeData(state, key);
    if (!data) return status(state, key) + '<div class="loading-state">Reading effective settings…</div>';
    const settingRows = Object.entries(data.settings || {}).map(([name, value]) => `<div class="data-row"><span>${escapeHtml(name.replace(/_/g, " "))}</span><span>${escapeHtml(value ?? "Not set")}</span></div>`).join("");
    const toolCards = Object.entries(data.host_tools || {}).map(([name, value]) => `<div class="readiness-card ${value?.available ? "" : "unavailable"}"><strong><span class="state-dot"></span>${escapeHtml(name)}</strong><small>${escapeHtml(value?.path || "Unavailable")}</small></div>`).join("");
    return status(state, key) + `<div class="readiness-grid"><div class="readiness-card"><strong>${escapeHtml(data.device?.model || "No device selected")}</strong><small>${escapeHtml(data.device?.firmware_code || "Firmware not selected")}</small></div><div class="readiness-card"><strong>${data.integrity_verified ? "Firmware verified" : "Verification unknown"}</strong><small>${escapeHtml(data.firmware || "No firmware identity")}</small></div><div class="readiness-card"><strong>${escapeHtml(data.settings?.backend || "Backend unknown")}</strong><small>CPU backend</small></div></div>` +
      `<section class="panel result-panel"><h2>Effective settings</h2><div class="data-rows">${settingRows || '<p class="empty-copy">No settings resolved.</p>'}</div></section>` +
      `<section class="result-data"><h3>Host tools</h3><div class="readiness-grid">${toolCards || '<p class="empty-copy">No host tools reported.</p>'}</div></section>` +
      `<section class="result-data"><h3>Settings sources</h3>${ConsoleResultViews.render({origins: data.origins, layers: data.layers}, `task:${key}`, escapeHtml)}</section>` +
      renderRawDetails(state.outcomes[key], `task:${key}`);
  }
  function doctor(state) {
    const key = "doctor";
    const data = outcomeData(state, key);
    if (!data) return status(state, key) + '<div class="loading-state">Checking host tools…</div>';
    const tools = Object.entries(data.tools || {}).map(([name, path]) => `<div class="readiness-card ${path ? "" : "unavailable"}"><strong><span class="state-dot"></span>${escapeHtml(name)}</strong><small>${escapeHtml(path || "Not found")}</small></div>`).join("");
    return status(state, key) + `<div class="readiness-grid"><div class="readiness-card ${data.native_analysis ? "" : "unavailable"}"><strong><span class="state-dot"></span>Native inspection</strong><small>${data.native_analysis ? "Available" : "Unavailable"}</small></div><div class="readiness-card"><strong>${escapeHtml(data.host?.system || "Unknown host")}</strong><small>${escapeHtml(data.host?.machine || "Architecture unknown")}</small></div></div>` +
      `<section class="result-data"><h3>Host tools</h3><div class="readiness-grid">${tools}</div></section>` +
      `<section class="result-data"><h3>Target evidence</h3>${ConsoleResultViews.render(data.target || {}, `task:${key}`, escapeHtml)}</section>` +
      renderRawDetails(state.outcomes[key], `task:${key}`);
  }
  function devices(state) {
    const key = "device list";
    const data = outcomeData(state, key);
    if (!data) return status(state, key) + '<div class="loading-state">Looking for connected phones…</div>';
    const phones = data.devices || [];
    const cards = phones.map(phone => {
      const isNokia808 = phone.vendor_id === 0x0421 && /\b808\s+pureview\b/i.test(phone.product || "");
      const portrait = isNokia808
        ? `<img src="${nokia808Image}" alt="Nokia 808 PureView" width="40" height="68">`
        : icon("phone");
      const interfaces = (phone.interfaces || []).slice(0, 8).map(item => `<div class="data-row"><span>Interface ${item.number}</span><span>${escapeHtml(item.function)}</span></div>`).join("");
      return `<section class="panel result-panel device-card"><div class="device-card-row"><div class="device-portrait ${isNokia808 ? "photo" : "generic"}">${portrait}</div><div class="device-card-content"><h2>${escapeHtml(phone.product)}</h2><p>${escapeHtml(phone.manufacturer)} · ${phone.vendor_id.toString(16).padStart(4,"0")}:${phone.product_id.toString(16).padStart(4,"0")} · ${escapeHtml(phone.interface_profile)}</p><div class="data-rows">${interfaces}</div><button class="button" data-open-usb="${escapeHtml(phone.selector)}">Inspect USB interfaces</button></div></div></section>`;
    }).join("");
    return status(state, key) + (cards || '<div class="panel empty-library">No supported phones are connected.</div>') + renderRawDetails(state.outcomes[key], `task:${key}`);
  }
  function render(task, state) {
    const key = task.command.path.join(" ");
    if (key === "firmware list") return firmware(state);
    if (key === "emu resolve") return emulator(state);
    if (key === "doctor") return doctor(state);
    if (key === "device list") return devices(state);
    return "";
  }
  function inspector(task, state) {
    const key = task.command.path.join(" ");
    if (key === "firmware list") return "";
    const names = settings[key];
    if (!names) return "";
    const values = formValues(task);
    const byName = new Map(task.command.arguments.map(argument => [argument.name, argument]));
    const mainNames = key === "firmware list" ? ["store"] : ["project", "sdk", "firmware", "store"];
    const extraNames = names.filter(name => !mainNames.includes(name));
    const renderFields = names => names.map(name => byName.has(name) ? fieldMarkup(byName.get(name), values[name], []) : "").join("");
    return `<h2>Resolve with</h2>${renderFields(mainNames)}` +
      `<details data-inspector-more="${escapeHtml(key)}" ${state.inspectorAdvanced[key] ? "open" : ""}><summary>More source options</summary>${renderFields(extraNames)}</details><button class="button primary" data-live-apply="${escapeHtml(key)}">Apply and refresh</button>`;
  }
  return {render, inspector};
})();
