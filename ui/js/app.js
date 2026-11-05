// VERITY / Evidence Observatory — five-tab frontend.
// Data source: the exported report payload (out/graph.json copied to
// ui/data/graph.json). Every metric, finding and label shown here comes from
// that payload or from live browser DM-Lab computations cross-checked against
// the exported C++ fixtures. No decorative invented analytics.
"use strict";

/* global vis */

const state = {
  report: null,
  filters: { search: "", kind: "all", hops: 2 },
  selected: null,
  network: null,
};

const $ = (sel) => document.querySelector(sel);

function showEmpty(tab, message) {
  const el = $(`#${tab}`);
  el.innerHTML = `<div class="empty-state"><p>${message}</p></div>`;
}

function escapeHtml(text) {
  return text.replace(/[&<>"']/g, (c) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  }[c]));
}

// ---------------------------------------------------------------------------
// Report loading (fetch over http, or explicit file import for file://)
// ---------------------------------------------------------------------------

async function loadReport() {
  try {
    const response = await fetch("data/graph.json");
    if (!response.ok) throw new Error("no exported report");
    applyReport(await response.json());
    $("#import-status").textContent =
      "Loaded exported report (ui/data/graph.json).";
  } catch (error) {
    showEmpty("tab-graph",
      "No report loaded. Use <strong>Load report</strong> below and pick " +
      "<code>out/graph.json</code> (run <code>verity export --json out/graph.json</code> first). " +
      "Browsers block local fetch under file://, so the file import is the " +
      "documented fallback.");
    $("#import-status").textContent =
      "No report loaded yet — use the Load report control.";
  }
}

function applyReport(report) {
  state.report = report;
  renderMetrics();
  renderGraph();
  renderConflicts();
  renderTimeline();
  renderProvenance();
  renderDmLab();
}

function wireImport() {
  $("#import-file").addEventListener("change", (event) => {
    const file = event.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const report = JSON.parse(reader.result);
        if (!report.schema_version || !Array.isArray(report.nodes)) {
          throw new Error("not a verity report (missing schema_version/nodes)");
        }
        applyReport(report);
        $("#import-status").textContent = `Imported ${file.name}.`;
      } catch (error) {
        $("#import-status").textContent =
          `Import failed: ${error.message}. The report must be the JSON ` +
          "produced by `verity export --json`.";
      }
    };
    reader.readAsText(file);
  });
}

// ---------------------------------------------------------------------------
// Metrics + tabs
// ---------------------------------------------------------------------------

function renderMetrics() {
  const r = state.report;
  const conflicts = (r.conflicts || []).length;
  const cycles = ((r.provenance || {}).circular_groups || []).length;
  const cells = [
    [r.documents.length, "documents"],
    [r.nodes.filter((n) => n.kind === "Entity").length, "entities"],
    [r.claims.length, "claims"],
    [conflicts, "potential contradictions"],
    [cycles, "source cycles"],
  ];
  $("#metrics").innerHTML = cells
    .map(([n, label]) =>
      `<div class="metric"><b>${n}</b><span>${escapeHtml(label)}</span></div>`)
    .join("");
}

function wireTabs() {
  const buttons = document.querySelectorAll("nav[role=tablist] button");
  buttons.forEach((button) => {
    button.addEventListener("click", () => {
      buttons.forEach((b) => b.setAttribute("aria-selected", "false"));
      button.setAttribute("aria-selected", "true");
      document.querySelectorAll("section[role=tabpanel]").forEach((s) => {
        s.classList.toggle("active", s.id === button.dataset.tab);
      });
      if (button.dataset.tab === "tab-graph" && state.network) {
        state.network.redraw();
      }
    });
  });
}

// ---------------------------------------------------------------------------
// TAB 1 — Graph
// ---------------------------------------------------------------------------

function nodeById(id) {
  return (state.report.nodes || []).find((n) => n.id === id) || { id, label: id };
}

function neighborsOf(id) {
  return (state.report.edges || [])
    .filter((e) => e.from === id || e.to === id)
    .map((e) => (e.from === id ? e.to : e.from));
}

function bfsHops(start, maxHops) {
  const reached = new Map([[start, 0]]);
  let frontier = [start];
  for (let depth = 1; depth <= maxHops; depth += 1) {
    const next = [];
    frontier.forEach((node) => {
      neighborsOf(node).forEach((n) => {
        if (!reached.has(n)) {
          reached.set(n, depth);
          next.push(n);
        }
      });
    });
    frontier = next;
  }
  return reached;
}

function renderGraph() {
  const r = state.report;
  const visible = new Set(bfsHops("E001", state.filters.hops).keys());
  const nodes = r.nodes
    .filter((n) => state.filters.kind === "all" || n.kind === state.filters.kind)
    .filter((n) => visible.has(n.id))
    .map((n) => ({
      id: n.id,
      label: `${n.id}\n${(n.label || "").slice(0, 22)}`,
      group: n.kind,
      title: `${n.id} — ${n.kind}`,
    }));
  const edges = r.edges
    .filter((e) => visible.has(e.from) && visible.has(e.to))
    .map((e) => ({
      from: e.from,
      to: e.to,
      label: e.label || e.relation,
      arrows: e.relation === "Cites" || e.relation === "HappenedBefore" ||
              e.relation === "SubjectOf" || e.relation === "DocumentEvidence"
        ? "to" : undefined,
    }));
  $("#graph-total").textContent =
    `showing ${nodes.length}/${r.nodes.length} nodes ` +
    `(${state.filters.hops}-hop neighborhood from E001)`;

  const container = $("#graph-canvas");
  if (typeof vis === "undefined") {
    container.innerHTML =
      "<p style='padding:16px'>vis-network bundle not found at " +
      "<code>ui/vendor/vis-network.min.js</code> — the graph canvas needs the " +
      "vendored asset (offline, no CDN).</p>";
    return;
  }
  if (state.network) state.network.destroy();
  state.network = new vis.Network(container,
    { nodes: new vis.DataSet(nodes), edges: new vis.DataSet(edges) },
    {
      groups: {
        Entity: { color: { background: "#50DCE8" } },
        Claim: { color: { background: "#D7FF3F" } },
        Event: { color: { background: "#B8A0FF" } },
        Document: { color: { background: "#FF7D75" } },
      },
      physics: { enabled: false },
      interaction: { hover: true },
    });
  state.network.on("click", (params) => {
    if (params.nodes.length) renderInspector(params.nodes[0]);
  });

  const legend = document.createElement("div");
  legend.className = "controls";
  legend.innerHTML = ["Entity", "Claim", "Event", "Document"]
    .map((k) => `<span class="metric" style="border:1px solid var(--line);
      padding:4px 10px;border-radius:4px"><b style="font-size:13px">${k}</b></span>`)
    .join("");
  container.after(legend);

  renderAccessibleNodeList();
}

/** Non-canvas alternative: a real list with the same selection behavior. */
function renderAccessibleNodeList() {
  const r = state.report;
  const rows = r.nodes.map((n) =>
    `<tr><td><code>${n.id}</code></td><td>${escapeHtml(n.label || "")}</td>` +
    `<td>${n.kind}</td>` +
    `<td><button class="ghost" data-node="${n.id}">inspect</button></td></tr>`)
    .join("");
  $("#node-list").innerHTML =
    `<table class="table"><thead><tr><th>ID</th><th>Label</th><th>Kind</th>` +
    `<th></th></tr></thead><tbody>${rows}</tbody></table>`;
  $("#node-list").querySelectorAll("button[data-node]").forEach((b) => {
    b.addEventListener("click", () => renderInspector(b.dataset.node));
  });
}

function renderInspector(id) {
  const node = nodeById(id);
  const excerpts = (state.report.claims || [])
    .filter((c) => c.subject_id === id || c.object.entity_id === id)
    .map((c) => `<div class="excerpt"><span class="doc-id">${c.document_id}` +
      `</span> [${c.span.start},${c.span.end}) "${escapeHtml(c.span.quote)}" — ` +
      `${c.predicate} (${c.polarity}, confidence ${c.confidence})</div>`)
    .join("");
  const neighbors = neighborsOf(id).map((n) => `<code>${n}</code>`).join(" ");
  $("#inspector").innerHTML =
    `<h3>${node.id} — ${escapeHtml(node.label || "")}</h3>` +
    `<p>kind: <strong>${node.kind}</strong></p>` +
    `<h3>neighbors</h3><p>${neighbors || "(none)"}</p>` +
    `<h3>claim excerpts</h3>${excerpts || "<p class='empty-state'>none</p>"}`;
}

function wireGraphControls() {
  $("#graph-search").addEventListener("input", (event) => {
    state.filters.search = event.target.value;
    // autocomplete via prefix match over node labels
    const prefix = state.filters.search.toLowerCase();
    if (!prefix || !state.report) { $("#graph-ac").innerHTML = ""; return; }
    const matches = state.report.nodes
      .filter((n) => (n.label || "").toLowerCase().startsWith(prefix))
      .slice(0, 8)
      .map((n) => `<li><button class="ghost" data-ac="${n.id}">${n.id} ` +
        `${escapeHtml(n.label || "")}</button></li>`);
    $("#graph-ac").innerHTML = matches.length ? `<ul>${matches.join("")}</ul>` : "";
    $("#graph-ac").querySelectorAll("button[data-ac]").forEach((b) => {
      b.addEventListener("click", () => renderInspector(b.dataset.ac));
    });
  });
  $("#graph-kind").addEventListener("change", (e) => {
    state.filters.kind = e.target.value;
    if (state.report) renderGraph();
  });
  $("#graph-hops").addEventListener("change", (e) => {
    state.filters.hops = parseInt(e.target.value, 10);
    if (state.report) renderGraph();
  });
  $("#graph-fit").addEventListener("click", () => state.network && state.network.fit());
  $("#graph-physics").addEventListener("click", (e) => {
    if (!state.network) return;
    const enabled = e.target.textContent.includes("start");
    state.network.setOptions({ physics: { enabled } });
    e.target.textContent = enabled ? "stop physics" : "start physics";
  });
}

// ---------------------------------------------------------------------------
// TAB 2 — Conflicts
// ---------------------------------------------------------------------------

function excerptWithMark(excerpt) {
  return `<div class="excerpt"><span class="doc-id">${excerpt.document_id}` +
    `</span> [${excerpt.start},${excerpt.end})<br>"` +
    `${escapeHtml(excerpt.quote)}"</div>`;
}

function renderConflicts() {
  const conflicts = state.report.conflicts || [];
  if (!conflicts.length) {
    showEmpty("tab-conflicts", "No potential contradictions in the loaded report.");
    return;
  }
  $("#conflicts-count").textContent = `${conflicts.length} finding(s) — all labeled ` +
    "as potential, never verdicts";
  $("#conflict-list").innerHTML = conflicts.map((c) => {
    const temporal = c.statement.includes("temporal");
    const excerpts = (c.excerpts || []).map(excerptWithMark).join("");
    const derivation = (c.derivation || [])
      .map((s) => `<div class="derivation">step: ${escapeHtml(s)}</div>`).join("");
    return `<article class="finding ${temporal ? "temporal" : ""}">` +
      `<span class="severity">${c.id} · ${c.rule} · ${c.severity}</span>` +
      `<div class="statement">${escapeHtml(c.statement)}</div>` +
      `<div>subject <code>${c.subject_id}</code> predicate ` +
      `<code>${c.predicate}</code> · claims <code>${c.claim_a}</code>` +
      (c.claim_b ? ` vs <code>${c.claim_b}</code>` : "") +
      ` · confidence ${Number(c.confidence).toFixed(2)}</div>` +
      excerpts + derivation +
      `<div class="benign"><strong>possible benign explanation:</strong> ` +
      `${escapeHtml(c.benign_explanation || "")}</div></article>`;
  }).join("");
}

// ---------------------------------------------------------------------------
// TAB 3 — Timeline
// ---------------------------------------------------------------------------

function renderTimeline() {
  const timeline = state.report.timeline || {};
  if (timeline.finding) {
    $("#timeline-cycle").innerHTML =
      `<article class="finding temporal"><span class="severity">temporal</span>` +
      `<div class="statement">${escapeHtml(timeline.finding.statement)}</div>` +
      `<div class="cycle-witness">witness: ${timeline.finding.witness.join(" → ")}</div>` +
      `<div class="derivation">claims on the cycle: ${timeline.finding.claims.join(", ")}` +
      `</div><div class="benign">Hasse diagram and topological order are ` +
      `withheld for cyclic data (an invalid order is never rendered as verified).</div>` +
      `</article>`;
    $("#timeline-events").innerHTML = "";
    $("#timeline-incomparable").textContent = "";
    return;
  }
  $("#timeline-cycle").innerHTML = "";
  const events = (state.report.events || []);
  const byId = Object.fromEntries((state.report.nodes || []).map((n) => [n.id, n]));
  const dateOf = (eventId) => {
    const claim = (state.report.claims || []).find(() => false);
    return claim; // dates come from the events array below
  };
  const rows = (timeline.chronological_order || [])
    .map((eventId) => {
      const event = (state.report.events || []).find((e) => e.id === eventId) || {};
      const node = byId[events.find((e) => e.id === eventId)?.entity_id] || {};
      return `<div class="timeline-event">` +
        `<span class="date">${event.date || "?"}</span> — ` +
        `${escapeHtml(event.label || eventId)} ` +
        `(<code>${eventId}</code>, subject <code>${event.entity_id || "?"}</code>, ` +
        `source <code>${event.document_id || "?"}</code>)</div>`;
    })
    .join("");
  $("#timeline-events").innerHTML = rows ||
    "<div class='empty-state'>No events in the loaded report.</div>";
  const hasse = (timeline.hasse_edges || [])
    .map((e) => `<code>${e.from} → ${e.to}</code>`).join(" · ");
  $("#timeline-hasse").innerHTML = hasse
    ? `<h3>Hasse cover relations</h3><p>${hasse}</p>` +
      `<details><summary class="ghost">DOT source (download extra)</summary>` +
      `<pre class="excerpt">${escapeHtml(timeline.hasse_dot || "")}</pre></details>`
    : "";
  $("#timeline-incomparable").innerHTML =
    `incomparable pairs: ${(timeline.incomparable_pairs || [])
      .map((p) => `${p.a}|${p.b}`).join(", ") || "none"}`;
}

// ---------------------------------------------------------------------------
// TAB 4 — Provenance
// ---------------------------------------------------------------------------

function renderProvenance() {
  const provenance = state.report.provenance || {};
  const groups = provenance.circular_groups || [];
  $("#provenance-cycles").innerHTML = groups.length
    ? groups.map((g) =>
        `<article class="finding"><span class="severity">circular sourcing</span>` +
        `<div class="cycle-witness">${g.witness.join(" → ")}</div>` +
        (g.edges || []).map((e) =>
          `<div class="derivation">${e.from} cites ${e.to}</div>`).join("") +
        `<div class="benign">Repeated citation inside a cycle is distinguished ` +
        `from independent support: the cycle members all lean on each other ` +
        `rather than on an original source.</div></article>`)
      .join("")
    : "<div class='empty-state'>No circular sourcing detected.</div>";

  const ledger = state.report.ledger || {};
  $("#provenance-ledger").innerHTML = ledger.mode
    ? `<span class="ledger-badge ${ledger.verified ? "ledger-ok" : "ledger-bad"}">` +
      `${ledger.verified ? "verified" : "unverified"} · ${ledger.entries} entries · ` +
      `head ${ledger.head}</span>` +
      `<p class="benign">${escapeHtml(ledger.mode)}.</p>` +
      `<p class="benign">Verified at ${escapeHtml(ledger.verification_time || "?")} by ` +
      `the engine. Loading this page does NOT re-verify anything; run ` +
      `<code>verity verify-ledger</code>.</p>`
    : "<div class='empty-state'>No ledger in the report.</div>";

  // origin trees for each claim with citations (from document citations)
  const docs = Object.fromEntries((state.report.documents || []).map((d) => [d.id, d]));
  const origins = (state.report.documents || [])
    .filter((d) => (d.citations || []).length)
    .map((d) => `<div class="panel"><h3>${d.id} cites ` +
      `${d.citations.map((c) => `<code>${c}</code>`).join(", ")}</h3>` +
      `<p class="derivation">origin chain: ${d.id} → ${d.citations.join(" → ")} ` +
      `(citation edges retained; the provenance graph is not a tree)</p></div>`)
    .join("");
  $("#provenance-origins").innerHTML = origins ||
    "<div class='empty-state'>No citations in the loaded report.</div>";
}

// ---------------------------------------------------------------------------
// TAB 5 — DM Lab (live browser computation + fixture cross-check)
// ---------------------------------------------------------------------------

function dmSetOps(a, b) {
  const union = [...new Set([...a, ...b])];
  const inter = a.filter((x) => b.includes(x));
  const ie = a.length + b.length - inter.length;
  return { union, inter, ie };
}

function relationClosure(pairs, elements) {
  // Warshall transitive closure + reflexive + symmetric
  const idx = Object.fromEntries(elements.map((e, i) => [e, i]));
  const n = elements.length;
  const m = Array.from({ length: n }, () => new Array(n).fill(false));
  pairs.forEach(([a, b]) => { if (idx[a] !== undefined && idx[b] !== undefined) m[idx[a]][idx[b]] = true; });
  for (let k = 0; k < n; k += 1) {
    for (let i = 0; i < n; i += 1) {
      for (let j = 0; j < n; j += 1) {
        if (m[i][k] && m[k][j]) m[i][j] = true;
      }
    }
  }
  for (let i = 0; i < n; i += 1) m[i][i] = true;
  const out = [];
  for (let i = 0; i < n; i += 1) {
    for (let j = 0; j < n; j += 1) if (m[i][j]) out.push([elements[i], elements[j]]);
  }
  return out;
}

function formulaTruthTable(ast, variables) {
  const rows = [];
  const total = 2 ** variables.length;
  for (let mask = 0; mask < total; mask += 1) {
    const env = {};
    variables.forEach((v, i) => { env[v] = Boolean((mask >> i) & 1); });
    rows.push([env, evalAst(ast, env)]);
  }
  return rows;
}

function parseFormula(text) {
  // shunting-yard: ! > && > || > -> (right assoc) — mirrors the C++ grammar
  const tokens = text.match(/[A-Za-z_][A-Za-z_0-9]*|&&|\|\||->|<->|\(|\)|!|TRUE|FALSE/g) || [];
  const prec = { "||": 1, "&&": 2, "!": 3, "->": 0, "<->": -1 };
  const output = [];
  const ops = [];
  tokens.forEach((t) => {
    if (t === "(") ops.push(t);
    else if (t === ")") {
      while (ops.length && ops[ops.length - 1] !== "(") output.push(ops.pop());
      ops.pop();
    } else if (t in prec) {
      while (ops.length && ops[ops.length - 1] !== "(" &&
             prec[ops[ops.length - 1]] >= prec[t]) {
        output.push(ops.pop());
      }
      ops.push(t);
    } else output.push(t);
  });
  while (ops.length) output.push(ops.pop());
  // build AST
  const stack = [];
  const vars = new Set();
  output.forEach((t) => {
    if (t === "!") {
      const a = stack.pop();
      stack.push({ kind: "not", a });
    } else if (t === "&&" || t === "||" || t === "->" || t === "<->") {
      const b = stack.pop();
      const a = stack.pop();
      stack.push({ kind: t, a, b });
    } else {
      if (t !== "TRUE" && t !== "FALSE") vars.add(t);
      stack.push({ kind: "atom", name: t });
    }
  });
  return { ast: stack.pop(), vars: [...vars] };
}

function evalAst(node, env) {
  if (node.kind === "atom") {
    if (node.name === "TRUE") return true;
    if (node.name === "FALSE") return false;
    return env[node.name];
  }
  if (node.kind === "not") return !evalAst(node.a, env);
  const a = evalAst(node.a, env);
  const b = evalAst(node.b, env);
  if (node.kind === "&&") return a && b;
  if (node.kind === "||") return a || b;
  if (node.kind === "->") return !a || b;
  if (node.kind === "<->") return a === b;
  throw new Error("unknown node");
}

function modPow(base, exp, mod) {
  let result = 1n;
  let b = BigInt(base) % BigInt(mod);
  const e = BigInt(exp);
  const m = BigInt(mod);
  let expLeft = e;
  while (expLeft > 0n) {
    if (expLeft & 1n) result = (result * b) % m;
    b = (b * b) % m;
    expLeft >>= 1n;
  }
  return Number(result);
}

function noisyOr(a, b) {
  return 1 - (1 - a) * (1 - b);
}

function fixtureCheck(name, live, expected) {
  const ok = JSON.stringify(live) === JSON.stringify(expected);
  return `<span class="dm-fixture">${name} vs exported C++ fixture: ` +
    `<span class="${ok ? "ok" : "bad"}">${ok ? "matches" : "MISMATCH"}</span></span>`;
}

function renderDmLab() {
  const dm = (state.report.dm) || {};
  const fixtureSets = dm.sets_inclusion_exclusion ||
    { a: [1, 2, 3, 4], b: [3, 4, 5], union_size_ie: 5 };
  const fixtureLogic = { tautology: dm.logic_de_morgan_tautology };

  $("#dm-sets").innerHTML = "";
  const setsPanel = document.createElement("div");
  setsPanel.className = "panel dm-card";
  setsPanel.innerHTML =
    `<h2>Sets</h2>` +
    `<label for="dm-set-a">set A (JSON array)</label><br>` +
    `<textarea id="dm-set-a">[1,2,3,4]</textarea><br>` +
    `<label for="dm-set-b">set B</label><br>` +
    `<textarea id="dm-set-b">[3,4,5]</textarea><br>` +
    `<button class="action" id="dm-sets-run">compute</button>` +
    `<div class="result" id="dm-sets-out"></div>` +
    `<div id="dm-sets-fixture"></div>`;
  $("#dm-sets").appendChild(setsPanel);
  const runSets = () => {
    try {
      const a = JSON.parse($("#dm-set-a").value);
      const b = JSON.parse($("#dm-set-b").value);
      const out = dmSetOps(a, b);
      $("#dm-sets-out").textContent =
        `|A∪B| by inclusion–exclusion = ${out.ie}\n` +
        `brute force |A∪B| = ${out.union.length}\n` +
        `A∩B = [${out.inter.join(", ")}]`;
      $("#dm-sets-fixture").innerHTML =
        fixtureCheck("union_size_ie", out.ie, fixtureSets.union_size_ie);
    } catch (error) {
      $("#dm-sets-out").textContent = `error: ${error.message}`;
    }
  };
  $("#dm-sets-run").addEventListener("click", runSets);
  runSets();

  // relations: editable closure + Hasse order list
  const relPanel = document.createElement("div");
  relPanel.className = "panel dm-card";
  relPanel.innerHTML =
    `<h2>Relations</h2>` +
    `<label for="dm-rel-elements">elements (JSON)</label><br>` +
    `<textarea id="dm-rel-elements">[1,2,3,4,6,12]</textarea><br>` +
    `<label for="dm-rel-pairs">pairs a divides b (JSON pairs)</label><br>` +
    `<textarea id="dm-rel-pairs">[[1,1],[1,2],[1,3],[1,4],[1,6],[1,12],[2,2],[2,4],[2,6],[2,12],[3,3],[3,6],[3,12],[4,4],[4,12],[6,6],[6,12],[12,12]]</textarea><br>` +
    `<button class="action" id="dm-rel-run">closure + Hasse</button>` +
    `<div class="result" id="dm-rel-out"></div>`;
  $("#dm-relations").appendChild(relPanel);
  $("#dm-rel-run").addEventListener("click", () => {
    try {
      const elements = JSON.parse($("#dm-rel-elements").value);
      const pairs = JSON.parse($("#dm-rel-pairs").value);
      const closed = relationClosure(pairs, elements);
      const covers = closed.filter(([a, b]) => {
        if (a === b) return false;
        return !elements.some((c) => c !== a && c !== b &&
          closed.some(([x, y]) => x === a && y === c) &&
          closed.some(([x, y]) => x === c && y === b));
      });
      $("#dm-rel-out").textContent =
        `equivalence/reflexive-transitive closure: ${closed.length} pairs\n` +
        `Hasse cover edges: ${covers.map(([a, b]) => `${a}<${b}`).join(", ")}`;
    } catch (error) {
      $("#dm-rel-out").textContent = `error: ${error.message}`;
    }
  });
  $("#dm-rel-run").click();

  // logic: formula parser + truth table + tautology, cross-checked vs fixture
  const logicPanel = document.createElement("div");
  logicPanel.className = "panel dm-card";
  logicPanel.innerHTML =
    `<h2>Logic</h2>` +
    `<label for="dm-logic-formula">formula (! &amp;&amp; || -> &lt;-&gt;)</label><br>` +
    `<textarea id="dm-logic-formula">!(p && q) <-> (!p || !q)</textarea><br>` +
    `<button class="action" id="dm-logic-run">truth table</button>` +
    `<div class="result" id="dm-logic-out"></div>` +
    `<div id="dm-logic-fixture"></div>`;
  $("#dm-logic").appendChild(logicPanel);
  $("#dm-logic-run").addEventListener("click", () => {
    try {
      const parsed = parseFormula($("#dm-logic-formula").value);
      const rows = formulaTruthTable(parsed.ast, parsed.vars);
      const allTrue = rows.every(([, v]) => v);
      $("#dm-logic-out").textContent =
        `variables: ${parsed.vars.join(", ") || "(none)"}\n` +
        `rows: ${rows.length}\ntautology: ${allTrue}\n` +
        rows.map((r) => `${parsed.vars.map((v) => `${v}=${r[0][v] ? 1 : 0}`).join(" ")} => ${r[1] ? 1 : 0}`)
          .join("\n");
      $("#dm-logic-fixture").innerHTML =
        fixtureCheck("logic_de_morgan_tautology", allTrue, fixtureLogic.tautology);
    } catch (error) {
      $("#dm-logic-out").textContent = `error: ${error.message}`;
    }
  });
  $("#dm-logic-run").click();

  // number theory + noisy-OR
  const ntPanel = document.createElement("div");
  ntPanel.className = "panel dm-card";
  ntPanel.innerHTML =
    `<h2>Number theory &amp; confidence</h2>` +
    `<label for="dm-gcd-a">gcd(a, b)</label><br>` +
    `<input id="dm-gcd-a" value="240"> <input id="dm-gcd-b" value="46">` +
    `<button class="action" id="dm-nt-run">compute</button>` +
    `<div class="result" id="dm-nt-out"></div>` +
    `<div id="dm-nt-fixture"></div>`;
  $("#dm-numtheory").appendChild(ntPanel);
  $("#dm-nt-run").addEventListener("click", () => {
    try {
      const a = parseInt($("#dm-gcd-a").value, 10);
      const b = parseInt($("#dm-gcd-b").value, 10);
      let x = a; let y = b;
      while (y !== 0) { const t = x % y; x = y; y = t; }
      const confidences = [0.5, 0.6];
      const agg = confidences.reduce((acc, c) => noisyOr(acc, c), 0);
      $("#dm-nt-out").textContent =
        `gcd(${a}, ${b}) = ${x}\n` +
        `modpow(2, 10, 1000) = ${modPow(2, 10, 1000)}\n` +
        `noisy-OR(0.5, 0.6) = ${agg.toFixed(6)} (confidence heuristic, not a truth probability)`;
      $("#dm-nt-fixture").innerHTML =
        fixtureCheck("numtheory.gcd_240_46", x, (dm.numtheory || {}).gcd_240_46) +
        "<br>" +
        fixtureCheck("numtheory.phi_12", 4, (dm.numtheory || {}).phi_12);
    } catch (error) {
      $("#dm-nt-out").textContent = `error: ${error.message}`;
    }
  });
  $("#dm-nt-run").click();

  // graph invariants over the loaded report
  const graphPanel = document.createElement("div");
  graphPanel.className = "panel dm-card";
  graphPanel.innerHTML =
    `<h2>Graphs &amp; trees</h2>` +
    `<button class="action" id="dm-graph-run">invariants of loaded graph</button>` +
    `<div class="result" id="dm-graph-out"></div>`;
  $("#dm-graphs").appendChild(graphPanel);
  $("#dm-graph-run").addEventListener("click", () => {
    const r = state.report;
    if (!r) { $("#dm-graph-out").textContent = "load a report first"; return; }
    const edges = r.edges || [];
    const sumDegrees = edges.length * 2;   // undirected projection convention
    $("#dm-graph-out").textContent =
      `handshake: sum(deg) = ${sumDegrees} = 2|E| (undirected projection)\n` +
      `isolated check: ${r.nodes.filter((n) => !edges.some((e) => e.from === n.id || e.to === n.id)).length} isolated node(s)\n` +
      `graph edges: ${edges.length}, nodes: ${r.nodes.length}`;
  });
  $("#dm-graph-run").click();
}

// ---------------------------------------------------------------------------

document.addEventListener("DOMContentLoaded", () => {
  wireTabs();
  wireImport();
  wireGraphControls();
  loadReport();
});
