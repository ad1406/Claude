// Per-bar timing of every recording over a range of bars: a measurement, not a judgment.
// y = tempo relative to the median of all recordings in that bar (100% = typical; higher = faster).
// Your takes are drawn in the accent colour, performers in grey; the active recording is thicker.
import { useState } from "./vendor/hooks.js";
import { html, player } from "./lib.js";

function durations(r, ids) {
  const occ = (r.alignment && r.alignment.occ) || []; const first = {};
  for (const o of occ) if (first[o[0]] == null) first[o[0]] = o[3] - o[2];     // first time through
  return ids.map(id => (first[id] > 0.05 ? first[id] : null));
}

export function TimingLane({ M, data, range }) {
  const [hover, setHover] = useState(null);
  if (!range) return html`<div class="hint">Select bars (click, Shift+click) to compare how each recording times them.</div>`;
  const oa = M.order[range.a], ob = M.order[range.b];
  let ids = M.seq.slice(Math.min(oa, ob), Math.max(oa, ob) + 1);
  if (ids.length < 2) {                      // a single bar: show its line of music for context
    const b = M.byId[range.a]; ids = M.seq.filter(id => M.byId[id].page === b.page && M.byId[id].system === b.system);
  }
  if (ids.length > 60) ids = ids.slice(0, 60);
  const recs = data.recordings.filter(r => r.alignment && r.alignment.occ && r.alignment.occ.length);
  const D = recs.map(r => ({ r, d: durations(r, ids) })).filter(x => x.d.filter(v => v != null).length >= 2);
  if (!D.length) return html`<div class="hint">No aligned recording plays these bars yet.</div>`;
  const med = ids.map((_, i) => { const v = D.map(x => x.d[i]).filter(v => v != null).sort((a, b) => a - b); return v.length ? v[Math.floor(v.length / 2)] : null; });
  const series = D.map(x => ({ r: x.r, y: x.d.map((v, i) => (v != null && med[i] ? 100 * med[i] / v : null)) }));
  const all = series.flatMap(s => s.y).filter(v => v != null);
  const lo = Math.max(40, Math.min(85, ...all) - 5), hi = Math.min(250, Math.max(115, ...all) + 5);
  const W = 360, H = 120, L = 30, R = 6, T = 6, B = 16;
  const X = i => L + (ids.length === 1 ? 0 : i * (W - L - R) / (ids.length - 1));
  const Y = v => T + (hi - v) / (hi - lo) * (H - T - B);
  const path = ys => { let d = "", pen = false; ys.forEach((v, i) => { if (v == null) { pen = false; return; } d += (pen ? "L" : "M") + X(i).toFixed(1) + " " + Y(v).toFixed(1); pen = true; }); return d; };
  const ticks = [lo, 100, hi].map(v => Math.round(v)).filter((v, i, a) => a.indexOf(v) === i);
  const step = Math.max(1, Math.ceil(ids.length / 8));
  const order = [...series].sort((a, b) => (a.r.id === player.act) - (b.r.id === player.act) || (a.r.kind === "mine") - (b.r.kind === "mine"));
  const move = e => { const bx = e.currentTarget.getBoundingClientRect(); const x = (e.clientX - bx.left) / bx.width * W;
    const i = Math.round((x - L) / ((W - L - R) / Math.max(1, ids.length - 1))); setHover(i >= 0 && i < ids.length ? i : null); };
  const tip = hover != null ? series.map(s => [s.r, s.y[hover]]).filter(x => x[1] != null).sort((a, b) => b[1] - a[1]) : null;
  return html`<div class="timing">
    <svg viewBox=${`0 0 ${W} ${H}`} class="tl" onMouseMove=${move} onMouseLeave=${() => setHover(null)}>
      ${ticks.map(v => html`<g><line x1=${L} x2=${W - R} y1=${Y(v)} y2=${Y(v)} class=${v === 100 ? "base" : "grid"} /><text x=${L - 4} y=${Y(v) + 3} class="ax" text-anchor="end">${v}%</text></g>`)}
      ${ids.map((id, i) => i % step === 0 ? html`<text x=${X(i)} y=${H - 3} class="ax" text-anchor="middle">${M.labels[id] ?? ""}</text>` : null)}
      ${hover != null ? html`<line x1=${X(hover)} x2=${X(hover)} y1=${T} y2=${H - B} class="cross" />` : null}
      ${order.map(s => html`<path d=${path(s.y)} class=${"ln" + (s.r.kind === "mine" ? " mine" : "") + (s.r.id === player.act ? " act" : "")}
          onClick=${() => player.select(s.r.id)}><title>${s.r.name}</title></path>`)}
      ${hover != null ? order.map(s => s.y[hover] != null ? html`<circle cx=${X(hover)} cy=${Y(s.y[hover])} r="2.6" class=${"pt" + (s.r.kind === "mine" ? " mine" : "") + (s.r.id === player.act ? " act" : "")} />` : null) : null}
    </svg>
    <div class="tl-legend">${tip ? html`<b>${M.label(ids[hover])}</b> ${tip.map(([r, v]) => html`<span class=${r.kind === "mine" ? "mine" : ""}>${r.name} ${Math.round(v)}%</span>`)}`
      : html`<span><i class="sw mine"></i>you</span><span><i class="sw"></i>performers</span><span><i class="sw act"></i>playing now</span><span class="muted">tempo vs. the typical recording, per bar (first time through)</span>`}</div>
  </div>`;
}
