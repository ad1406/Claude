// Takes of a passage: your recordings of it over time, the performers at the same bars, and ways to compare them
// (play the span, ping-pong bar by bar, timing and dynamic shape, your takes over time). Measured, never judged.
import { useState, useEffect, useMemo } from "./vendor/hooks.js";
import { html, api, fmt, store, player, toast, usePlayerTick, rangeLabel } from "./lib.js";

export const RATING_WORDS = { 1: "broke down", 2: "got through", 3: "as intended", 4: "better than intended" };
export const LABELS = { warm: "warm", cold: "cold", stage: "stage" };
const day = t => new Date(t * 1000).toLocaleDateString(undefined, { month: "short", day: "numeric" });
const clock = t => new Date(t * 1000).toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" });

// bars of a range in notated order
export function spanIds(M, a, b) { const oa = M.order[a], ob = M.order[b]; if (oa == null || ob == null) return []; return M.seq.slice(Math.min(oa, ob), Math.max(oa, ob) + 1); }

// where a recording plays the span [a..b]: the first occurrence of a, extended while the bars stay in the span
export function spanTimes(M, r, a, b) {
  const occ = (r.alignment && r.alignment.occ) || []; const oa = M.order[a], ob = M.order[b];
  const inSpan = id => { const o = M.order[id]; return o != null && o >= oa && o <= ob; };
  let k0 = occ.findIndex(o => o[0] === a);
  if (k0 < 0) k0 = occ.findIndex(o => inSpan(o[0]));
  if (k0 < 0) return null;
  let k1 = k0; while (k1 + 1 < occ.length && inSpan(occ[k1 + 1][0]) && occ[k1 + 1][2] - occ[k1][3] < 8) k1++;
  return { t0: occ[k0][2], t1: occ[k1][3], k0, k1 };
}

// stops (a new run starting inside the span) and repeated bars in a recording's pass over the span
export function flowOf(M, r, a, b) {
  const st = spanTimes(M, r, a, b); if (!st) return null;
  const occ = r.alignment.occ; const runs = r.alignment.runs || [];
  let stops = 0; for (const rn of runs) if (rn[0] > st.k0 && rn[0] <= st.k1) stops++;
  const seen = {}; let reps = 0; for (let k = st.k0; k <= st.k1; k++) { const id = occ[k][0]; if (seen[id]) reps++; seen[id] = 1; }
  return { stops, reps, dur: st.t1 - st.t0 };
}

// a take of a few bars (recorded with a selection or a passage open): lined up with those bars only
export const isSectionTake = r => !!(r.sec_a || r.passage_id);
export const overlaps = (M, r, a, b) => { if (!r.sec_a) return false; const x0 = M.order[r.sec_a], x1 = M.order[r.sec_b || r.sec_a], y0 = M.order[a], y1 = M.order[b];
  return x0 != null && x1 != null && y0 != null && y1 != null && Math.min(x1, y1) >= Math.max(x0, y0); };

export function takesOf(M, data, p) {
  const mine = [], refs = [];
  for (const r of data.recordings) {
    const covers = !!spanTimes(M, r, p.a, p.b);
    if (r.passage_id === p.id || overlaps(M, r, p.a, p.b) || ((r.kind === "mine" || r.source === "take") && covers && !r.passage_id)) mine.push(r);
    else if (covers) refs.push(r);
  }
  mine.sort((x, y) => y.created - x.created);
  return { mine, refs };
}

export function playSpan(M, r, a, b) {
  const st = spanTimes(M, r, a, b);
  if (!st) {                                         // not lined up yet (or not at all): play the whole take from the start
    if (!r.audio) { toast("This take is still being saved"); return; }
    const res = player.select(r.id, { time: 0, play: true });
    if (!res.ok) toast("Can't play that take yet"); else if (r.status !== "ready") toast("Playing the whole take: it is still being lined up with the score");
    return;
  }
  player.playSegments([{ rid: r.id, t0: Math.max(0, st.t0 - 0.25), t1: st.t1 }]);
}

export function pingPong(M, A, B, a, b) {
  const ids = spanIds(M, a, b); const segs = [];
  const first = r => { const m = {}; for (const o of (r.alignment && r.alignment.occ) || []) if (!m[o[0]]) m[o[0]] = o; return m; };
  const fa = first(A), fb = first(B);
  for (const id of ids) { if (fa[id]) segs.push({ rid: A.id, t0: fa[id][2], t1: fa[id][3] }); if (fb[id]) segs.push({ rid: B.id, t0: fb[id][2], t1: fb[id][3] }); }
  if (!player.playSegments(segs)) toast("Neither recording plays these bars");
}
// the whole span in A, then in B
export function aThenB(M, A, B, a, b) {
  const sa = spanTimes(M, A, a, b), sb = spanTimes(M, B, a, b);
  if (!player.playSegments([sa && { rid: A.id, t0: Math.max(0, sa.t0 - 0.25), t1: sa.t1 }, sb && { rid: B.id, t0: Math.max(0, sb.t0 - 0.25), t1: sb.t1 }].filter(Boolean))) toast("Neither recording plays these bars");
}

// ------------------------------------------------------------------ the tab
export function TakesPanel({ M, data, p, reload, tname = (x) => x }) {
  usePlayerTick();
  const { mine, refs } = useMemo(() => takesOf(M, data, p), [data, p.id, p.a, p.b]);
  const all = [...mine, ...refs];
  const [A, setA] = useState(null); const [B, setB] = useState(null);
  const a = all.find(r => r.id === A) || mine[0] || all[0]; const b = all.find(r => r.id === B) || refs[0] || all.find(r => r !== a);
  const [chart, setChart] = useState(store.get("takesChart", "timing")); useEffect(() => store.set("takesChart", chart), [chart]);
  const [overTime, setOverTime] = useState(false);
  const del = async (r) => { if (!confirm(`Delete this take (${r.name})?`)) return; await api.send("DELETE", `/api/recordings/${r.id}`); reload(); };
  const seg = player.segment();
  return html`<div class="takes">
    <div class="tlist">
      ${mine.length ? mine.map(r => html`<${TakeRow} M=${M} r=${r} p=${p} onDel=${() => del(r)} reload=${reload} playing=${seg && seg.rid === r.id} tname=${tname} />`)
        : html`<div class="hint">No takes of these bars yet. Press <kbd>R</kbd> to record them (again to stop); the take is lined up with just these bars.</div>`}
      ${refs.length ? html`<div class="hint sep">Performers here</div>${refs.map(r => html`<div class="take ref">
          <button class=${"btn small" + (seg && seg.rid === r.id ? " on" : "")} onClick=${() => playSpan(M, r, p.a, p.b)}>▶</button>
          <span class="nm ell">${r.name}</span><span class="muted">${(() => { const f = flowOf(M, r, p.a, p.b); return f ? fmt(f.dur) : ""; })()}</span></div>`)}` : null}
    </div>
    ${all.length >= 2 ? html`<div class="cmp row">
        <select class="mini" value=${a && a.id} onChange=${e => setA(+e.target.value)}>${all.map(r => html`<option value=${r.id}>${shortName(r)}</option>`)}</select>
        <span class="muted">vs</span>
        <select class="mini" value=${b && b.id} onChange=${e => setB(+e.target.value)}>${all.map(r => html`<option value=${r.id}>${shortName(r)}</option>`)}</select>
        <button class="btn small" title="alternate bar by bar: A, B, A, B…" onClick=${() => pingPong(M, a, b, p.a, p.b)}>Ping-pong</button>
        <button class="btn small" title="the whole passage in A, then in B" onClick=${() => { const sa = spanTimes(M, a, p.a, p.b), sb = spanTimes(M, b, p.a, p.b);
          player.playSegments([sa && { rid: a.id, t0: sa.t0, t1: sa.t1 }, sb && { rid: b.id, t0: sb.t0, t1: sb.t1 }].filter(Boolean)); }}>A then B</button>
        ${seg ? html`<button class="btn small" onClick=${() => { player.stopSegments(); player.pause(); }}>■</button>` : null}</div>` : null}
    ${all.length ? html`<div class="row"><div class="seg mini"><button class=${chart === "timing" ? "on" : ""} onClick=${() => setChart("timing")}>Timing</button><button class=${chart === "dynamics" ? "on" : ""} onClick=${() => setChart("dynamics")}>Dynamics</button></div>
        <label class="hint row" title="only your takes, oldest faded"><input type="checkbox" checked=${overTime} onChange=${e => setOverTime(e.target.checked)} /> your takes over time</label></div>
      <${ShapeChart} M=${M} data=${data} ids=${spanIds(M, p.a, p.b)} mode=${chart} recs=${overTime ? mine : [...refs, ...mine]} overTime=${overTime} hi=${[a && a.id, b && b.id]} />` : null}
  </div>`;
}

const shortName = r => r.source === "take" && r.created ? `${day(r.created)} ${clock(r.created)}${r.label ? " · " + r.label : ""}` : r.name;

function TakeRow({ M, r, p, onDel, reload, playing, tname }) {
  const f = flowOf(M, r, p.a, p.b);
  const status = r.status !== "ready" ? ({ queued: "queued", processing: "analysing…", aligning: "lining up…", waiting: "waiting", failed: "failed" }[r.status] || r.status) : (r.alignment && r.alignment.stale ? "re-aligning…" : "");
  const [lab, setLab] = useState(null);            // shown at once; saved in the background
  useEffect(() => setLab(null), [r.label]);
  const setLabel = async (label) => { setLab(label); await api.send("PATCH", `/api/recordings/${r.id}`, { label }); reload(); };
  return html`<div class="take">
    <button class=${"btn small" + (playing ? " on" : "")} disabled=${!r.audio} title=${f ? "play these bars in this take" : "not lined up yet: plays the whole take"} onClick=${() => playSpan(M, r, p.a, p.b)}>▶</button>
    <span class="nm ell" title=${r.name}>${r.created ? `${day(r.created)} ${clock(r.created)}` : r.name}</span>
    <select class="mini lab" value=${lab || r.label || "warm"} onChange=${e => setLabel(e.target.value)} title="warm: after practising it · cold: first time today · stage: one take, as in performance">
      ${Object.entries(LABELS).map(([k, v]) => html`<option value=${k}>${v}</option>`)}</select>
    <span class="muted ell">${status || [r.rating ? RATING_WORDS[r.rating] : "", f && f.stops ? `${f.stops} stop${f.stops > 1 ? "s" : ""}` : "", f && f.reps ? `${f.reps} repeat${f.reps > 1 ? "s" : ""}` : "", r.technique ? tname(r.technique) : ""].filter(Boolean).join(" · ")}</span>
    <button class="btn small ghost" title="delete this take" onClick=${onDel}>×</button>
    ${r.rating && !r.rerating && Date.now() / 1000 - r.created > 18 * 3600 && f ? html`<div class="rerate"><span class="hint">A day later: listen as if someone else played it, and rate again</span>
      ${[1, 2, 3, 4].map(n => html`<button class="btn small" title=${RATING_WORDS[n]} onClick=${async () => { await api.send("PATCH", `/api/recordings/${r.id}`, { rerating: n }); reload(); toast(n === r.rating ? "Same as you rated it then" : n < r.rating ? "Lower than you rated it right after playing" : "Higher than you rated it right after playing"); }}>${n}</button>`)}</div>` : null}
    ${r.rating && r.rerating ? html`<div class="rerate hint">rated ${r.rating} right after, ${r.rerating} a day later</div>` : null}</div>`;
}

// ------------------------------------------------------------------ shape charts
const dynCache = new Map();
function useDynamics(pid, ids, on, ver) {
  const key = `${pid}:${ids.join(",")}:${ver}`; const [d, setD] = useState(dynCache.get(key) || null);
  useEffect(() => { if (!on || !ids.length) return; if (dynCache.has(key)) { setD(dynCache.get(key)); return; }
    api.get(`/api/pieces/${pid}/dynamics?bars=${ids.join(",")}`).then(x => { dynCache.set(key, x); setD(x); }); }, [key, on]);
  return d;
}

function firstDur(r, ids) {
  const occ = (r.alignment && r.alignment.occ) || []; const first = {};
  for (const o of occ) if (first[o[0]] == null) first[o[0]] = o[3] - o[2];
  return ids.map(id => (first[id] > 0.05 ? first[id] : null));
}

export function ShapeChart({ M, data, ids, mode, recs, overTime, hi }) {
  const [hover, setHover] = useState(null);
  const ver = data.recordings.map(r => r.id + ":" + (r.alignment ? r.alignment.occ.length : 0)).join("|");
  const dyn = useDynamics(data.piece.id, ids, mode === "dynamics", ver);
  if (!ids.length) return null;
  let series = [];
  if (mode === "timing") {
    // tempo per bar relative to the median of all the recordings that play it (100% = typical)
    const all = data.recordings.map(r => ({ r, d: firstDur(r, ids) })).filter(x => x.d.filter(v => v != null).length >= 2);
    const med = ids.map((_, i) => { const v = all.map(x => x.d[i]).filter(v => v != null).sort((a, b) => a - b); return v.length ? v[Math.floor(v.length / 2)] : null; });
    series = recs.map(r => { const d = firstDur(r, ids); return { r, y: d.map((v, i) => (v != null && med[i] ? 100 * med[i] / v : null)) }; });
  } else {
    if (!dyn) return html`<div class="hint">Measuring loudness…</div>`;
    // loudness per bar in dB, each recording centred on its own median over these bars: only the shape counts
    series = recs.map(r => { const v = dyn[String(r.id)]; if (!v) return { r, y: ids.map(() => null) };
      const ok = v.filter(x => x != null).sort((a, b) => a - b); const m = ok.length ? ok[Math.floor(ok.length / 2)] : 0;
      return { r, y: v.map(x => x == null ? null : x - m) }; });
  }
  series = series.filter(s => s.y.filter(v => v != null).length >= 2);
  if (!series.length) return html`<div class="hint">No recording here is lined up with these bars yet.</div>`;
  const vals = series.flatMap(s => s.y).filter(v => v != null);
  const base = mode === "timing" ? 100 : 0;
  let lo = Math.min(...vals), hiV = Math.max(...vals);
  if (mode === "timing") { lo = Math.max(40, Math.min(85, lo) - 5); hiV = Math.min(250, Math.max(115, hiV) + 5); } else { lo = Math.min(-6, lo) - 1; hiV = Math.max(6, hiV) + 1; }
  const W = 360, H = 118, L = 30, R = 6, T = 6, B = 16;
  const X = i => L + (ids.length === 1 ? 0 : i * (W - L - R) / (ids.length - 1));
  const Y = v => T + (hiV - v) / (hiV - lo) * (H - T - B);
  const path = ys => { let d = "", pen = false; ys.forEach((v, i) => { if (v == null) { pen = false; return; } d += (pen ? "L" : "M") + X(i).toFixed(1) + " " + Y(v).toFixed(1); pen = true; }); return d; };
  const unit = mode === "timing" ? "%" : " dB";
  const ticks = [lo, base, hiV].map(v => Math.round(v)).filter((v, i, a) => a.indexOf(v) === i);
  const step = Math.max(1, Math.ceil(ids.length / 8));
  const mineSorted = series.filter(s => s.r.kind === "mine" || s.r.source === "take").sort((a, b) => a.r.created - b.r.created);
  const age = s => { const i = mineSorted.indexOf(s); return i < 0 ? 1 : 0.25 + 0.75 * (i + 1) / mineSorted.length; };
  const order = [...series].sort((a, b) => (hi.includes(a.r.id) - hi.includes(b.r.id)) || ((a.r.kind === "mine") - (b.r.kind === "mine")));
  const move = e => { const bx = e.currentTarget.getBoundingClientRect(); const x = (e.clientX - bx.left) / bx.width * W;
    const i = Math.round((x - L) / ((W - L - R) / Math.max(1, ids.length - 1))); setHover(i >= 0 && i < ids.length ? i : null); };
  const tip = hover != null ? series.map(s => [s.r, s.y[hover]]).filter(x => x[1] != null).sort((a, b) => b[1] - a[1]) : null;
  const nm = r => r.source === "take" && r.created ? `${day(r.created)}${r.label ? " " + r.label : ""}` : r.name;
  const fmtV = v => mode === "timing" ? `${Math.round(v)}%` : `${v > 0 ? "+" : ""}${v.toFixed(1)} dB`;
  return html`<div class="timing">
    <svg viewBox=${`0 0 ${W} ${H}`} class="tl" onMouseMove=${move} onMouseLeave=${() => setHover(null)}>
      ${ticks.map(v => html`<g><line x1=${L} x2=${W - R} y1=${Y(v)} y2=${Y(v)} class=${v === base ? "base" : "grid"} /><text x=${L - 4} y=${Y(v) + 3} class="ax" text-anchor="end">${v}${unit}</text></g>`)}
      ${ids.map((id, i) => i % step === 0 ? html`<text x=${X(i)} y=${H - 3} class="ax" text-anchor="middle">${M.labels[id] ?? ""}</text>` : null)}
      ${hover != null ? html`<line x1=${X(hover)} x2=${X(hover)} y1=${T} y2=${H - B} class="cross" />` : null}
      ${order.map(s => html`<path d=${path(s.y)} style=${overTime ? `opacity:${age(s)}` : ""} class=${"ln" + (s.r.kind === "mine" || s.r.source === "take" ? " mine" : "") + (hi.includes(s.r.id) ? " act" : "")}
          onClick=${() => player.select(s.r.id)}><title>${nm(s.r)}</title></path>`)}
    </svg>
    <div class="tl-legend">${tip ? html`<b>${M.label(ids[hover])}</b> ${tip.slice(0, 6).map(([r, v]) => html`<span class=${r.kind === "mine" ? "mine" : ""}>${nm(r)} ${fmtV(v)}</span>`)}`
      : html`<span class="muted">${mode === "timing" ? "tempo per bar vs. the typical recording (higher = faster)" : "loudness per bar, each recording against its own median (shape only)"}${overTime ? "; older takes fainter" : ""}</span>`}</div>
  </div>`;
}

// ------------------------------------------------------------------ Listen: every section take of the piece
export function SectionTakes({ M, data, sel, reload, makePassage }) {
  usePlayerTick();
  const takes = data.recordings.filter(isSectionTake);
  if (!takes.length) return html`<div class="hint">Select a few bars (click, then Shift+click) and press <kbd>R</kbd> to record just those bars. The take is lined up with them only and kept here, out of the recordings list; listen back right away with <kbd>Shift+R</kbd>.</div>`;
  const here = r => sel && overlaps(M, r, sel.a, sel.b);
  const list = [...takes].sort((x, y) => (here(y) - here(x)) || (y.created - x.created));
  const cur = player.act; const playing = player.playing();
  const del = async (r) => { if (!confirm(`Delete this take (${r.name})?`)) return; await api.send("DELETE", `/api/recordings/${r.id}`); reload(); };
  const passName = id => { const x = (data.passages || []).find(q => q.id === id); return x ? (x.name || "passage") : "passage"; };
  return html`<div class="stakes">${list.map(r => { const a = r.sec_a, b = r.sec_b || r.sec_a; const ok = a && M.byId[a];
    const status = r.status !== "ready" ? ({ queued: "queued", processing: "analysing…", aligning: "lining up…", waiting: "waiting", failed: "failed" }[r.status] || r.status) : "";
    return html`<div class=${"stake" + (here(r) ? " here" : "")}>
      <button class=${"btn small" + (cur === r.id && playing ? " on" : "")} disabled=${!r.audio} onClick=${() => { if (cur === r.id && playing) player.pause(); else ok ? playSpan(M, r, a, b) : player.select(r.id, { time: 0, play: true }); }}>${cur === r.id && playing ? "■" : "▶"}</button>
      <span class="sec" title=${r.name}>${ok ? rangeLabel(M, a, b) : r.name}</span>
      <span class="muted nowrap">${r.created ? `${day(r.created)} ${clock(r.created)}` : ""}${r.label && r.label !== "warm" ? " · " + r.label : ""}</span>
      ${status ? html`<span class="muted">${status}</span>` : r.passage_id ? html`<button class="btn small" title="select these bars" onClick=${() => makePassage(a, b)}>→ bars</button>`
        : ok ? html`<button class="btn small" title="select these bars" onClick=${() => makePassage(a, b)}>→ bars</button>` : html`<span></span>`}
      <button class="btn small ghost" title="delete this take" onClick=${() => del(r)}>×</button></div>`; })}</div>`;
}
