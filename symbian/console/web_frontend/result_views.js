"use strict";

/* Visual, bounded presentation of result data. Raw JSON stays available below. */
const ConsoleResultViews = (() => {
  function label(name) {
    return String(name).replace(/[_-]/g, " ").replace(/\b\w/g, match => match.toUpperCase());
  }
  function value(value) {
    if (value === null || value === undefined) return "Unknown";
    if (typeof value === "boolean") return value ? "Yes" : "No";
    if (Array.isArray(value)) return value.join(", ");
    return String(value);
  }
  function rows(entries, escapeHtml) {
    return `<div class="data-rows">${entries.map(([name, item]) =>
      `<div class="data-row"><span>${escapeHtml(label(name))}</span><span>${escapeHtml(value(item))}</span></div>`
    ).join("")}</div>`;
  }
  function facts(entries, escapeHtml) {
    return `<div class="data-facts">${entries.map(([name, item]) =>
      `<div class="data-fact"><span>${escapeHtml(label(name))}</span><strong>${escapeHtml(value(item))}</strong></div>`
    ).join("")}</div>`;
  }
  function fileIndex(files, query, escapeHtml) {
    const all = Object.entries(files || {});
    const needle = String(query || "").trim().toLowerCase();
    const matching = needle ? all.filter(([name]) => name.toLowerCase().includes(needle)) : all;
    const shown = matching.slice(0, 60);
    return `<div class="file-count">${matching.length} matching files${matching.length > shown.length ? ` · showing first ${shown.length}` : ""}</div>` +
      `<div class="file-index">${shown.map(([name, digest]) =>
        `<div class="data-row"><span title="${escapeHtml(name)}">${escapeHtml(name)}</span><span title="${escapeHtml(digest)}">${escapeHtml(String(digest).slice(0, 12))}…</span></div>`
      ).join("") || '<p class="empty-copy">No matching files.</p>'}</div>`;
  }
  function renderFiles(files, key, escapeHtml) {
    return `<section class="result-data"><h3>Files · ${Object.keys(files).length}</h3>` +
      `<input class="file-search" type="search" placeholder="Find a file" aria-label="Find a file" data-file-query="${escapeHtml(key)}">` +
      `<div data-file-list="${escapeHtml(key)}">${fileIndex(files, "", escapeHtml)}</div></section>`;
  }
  function renderList(name, items, depth, escapeHtml) {
    if (!items.length) return `<section class="result-data"><h3>${escapeHtml(label(name))}</h3><p class="empty-copy">None recorded.</p></section>`;
    if (name === "interfaces" && items.every(item => item && typeof item === "object" && !Array.isArray(item))) {
      return `<section class="result-data interface-results"><h3>Interfaces · ${items.length}</h3><div class="interface-list">${items.map((item, index) => {
        const number = item.number ?? index;
        const role = item.function || item.declared_name || "Unknown function";
        const secondary = [item.declared_name && item.declared_name !== role ? item.declared_name : null,
          item.host_driver ? `Driver: ${item.host_driver}` : null,
          item.host_serial_port ? `Port: ${item.host_serial_port}` : null].filter(Boolean).join(" · ");
        const details = Object.entries(item).filter(([key]) => !["number", "function", "declared_name", "host_driver", "host_serial_port"].includes(key));
        return `<details class="interface-item"><summary><span class="interface-number">${escapeHtml(number)}</span><span class="interface-description"><strong>${escapeHtml(role)}</strong>${secondary ? `<small>${escapeHtml(secondary)}</small>` : ""}</span><span class="interface-endpoints">${escapeHtml(item.endpoint_count ?? "?")} endpoints</span></summary>${details.length ? rows(details, escapeHtml) : ""}</details>`;
      }).join("")}</div></section>`;
    }
    if (items.every(item => item === null || typeof item !== "object")) {
      return `<section class="result-data"><h3>${escapeHtml(label(name))} · ${items.length}</h3><div class="data-rows">${items.slice(0, 40).map((item, index) =>
        `<div class="data-row"><span>${index + 1}</span><span>${escapeHtml(value(item))}</span></div>`
      ).join("")}</div>${items.length > 40 ? `<p class="file-count">Showing 40 of ${items.length}; Raw JSON contains all entries.</p>` : ""}</section>`;
    }
    return `<section class="result-data"><h3>${escapeHtml(label(name))} · ${items.length}</h3>${items.slice(0, 24).map((item, index) => {
      if (!item || typeof item !== "object") return `<div class="data-record">${escapeHtml(value(item))}</div>`;
      const title = item.model || item.product || item.name || item.firmware_code || item.firmware || `${label(name)} ${index + 1}`;
      return `<div class="data-record"><strong>${escapeHtml(title)}</strong>${renderObject(item, depth + 1, escapeHtml)}</div>`;
    }).join("")}${items.length > 24 ? `<p class="file-count">Showing 24 of ${items.length}; Raw JSON contains all entries.</p>` : ""}</section>`;
  }
  function renderObject(object, depth, escapeHtml, key = "") {
    if (!object || typeof object !== "object") return "";
    if (depth > 3) return `<span>${escapeHtml(Array.isArray(object) ? `${object.length} entries` : `${Object.keys(object).length} fields`)}</span>`;
    const entries = Object.entries(object);
    const scalar = entries.filter(([, item]) => item === null || typeof item !== "object");
    const nested = entries.filter(([, item]) => item !== null && typeof item === "object");
    let html = scalar.length ? (depth === 0 ? facts(scalar.slice(0, 16), escapeHtml) : rows(scalar.slice(0, 16), escapeHtml)) : "";
    if (scalar.length > 16) html += `<p class="file-count">${scalar.length - 16} more fields in Raw JSON.</p>`;
    for (const [name, item] of nested.slice(0, 12)) {
      if (name === "files" && !Array.isArray(item)) {
        html += renderFiles(item, key, escapeHtml);
      } else if (Array.isArray(item)) {
        html += renderList(name, item, depth, escapeHtml);
      } else {
        html += `<section class="result-data"><h3>${escapeHtml(label(name))}</h3>${renderObject(item, depth + 1, escapeHtml, key)}</section>`;
      }
    }
    if (nested.length > 12) html += `<p class="file-count">${nested.length - 12} more sections in Raw JSON.</p>`;
    return html;
  }
  function render(payload, key, escapeHtml) {
    if (payload === null || payload === undefined) return "";
    if (Array.isArray(payload)) return renderList("Results", payload, 0, escapeHtml);
    if (typeof payload === "object") return `<div class="result-data">${renderObject(payload, 0, escapeHtml, key)}</div>`;
    return `<div class="result-data">${escapeHtml(value(payload))}</div>`;
  }
  return {render, fileIndex};
})();
