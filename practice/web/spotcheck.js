// "Check timing": the app picks the few bar starts it is least sure about (and that matter most) and asks which
// of a few clicks lands on the downbeat. Listening and comparing, like an eye test, instead of tapping in time.
import { useState, useEffect, useRef } from "./vendor/hooks.js";
import { html, api, fmt, player, toast } from "./lib.js";

let actx = null;
const ctx = () => { if (!actx) actx = new (window.AudioContext || window.webkitAudioContext)(); if (actx.state === "suspended") actx.resume(); return actx; };
const PRE = 1.8, POST = 1.0;                         // seconds of music before / after the candidate downbeat

async function loadSnippet(rid, t0, t1) {
  const r = await fetch(`/api/recordings/${rid}/snippet?t0=${t0.toFixed(3)}&t1=${t1.toFixed(3)}`);
  if (!r.ok) throw new Error("no audio");
  return await ctx().decodeAudioData(await r.arrayBuffer());
}
// a copy of the snippet with a click mixed in at `at` seconds
function withClick(buf, at) {
  const c = ctx(), out = c.createBuffer(1, buf.length, buf.sampleRate), src = buf.getChannelData(0), dst = out.getChannelData(0);
  dst.set(src); const sr = buf.sampleRate, i0 = Math.round(at * sr), n = Math.round(0.035 * sr);
  let peak = 0; for (let i = 0; i < src.length; i++) peak = Math.max(peak, Math.abs(src[i]));
  const g = Math.max(0.25, Math.min(0.9, peak * 0.9));
  for (let i = 0; i < n && i0 + i < dst.length; i++) { if (i0 + i < 0) continue; dst[i0 + i] += g * Math.sin(2 * Math.PI * 1760 * i / sr) * Math.exp(-i / (0.008 * sr)); }
  return out;
}

export function SpotCheck({ M, rec, onClose, onDone, onShow }) {
  const [spots, setSpots] = useState(null); const [i, setI] = useState(0);
  const [buf, setBuf] = useState(null); const [played, setPlayed] = useState(null); const [slow, setSlow] = useState(false);
  const [answered, setAnswered] = useState(0); const src = useRef(null);
  const [err, setErr] = useState(null);
  useEffect(() => { player.pause(); api.get(`/api/recordings/${rec.id}/spotchecks?n=6`).then(s => { if (s && s.error) { setErr(s.error); setSpots([]); } else setSpots(Array.isArray(s) ? s : []); }); }, []);
  const spot = spots && spots[i];
  const t0 = spot ? Math.max(0, Math.min(...spot.cands) - PRE) : 0;
  useEffect(() => { setBuf(null); setPlayed(null); if (!spot) return; onShow && onShow(spot.bar);
    loadSnippet(rec.id, t0, Math.max(...spot.cands) + POST).then(setBuf).catch(() => toast("Couldn't load the audio")); }, [spot && spot.k]);
  const stop = () => { try { src.current && src.current.stop(); } catch (e) {} src.current = null; };
  const play = (j) => {
    if (!buf || !spot) return; stop(); const c = ctx(); const at = spot.cands[j] - t0;
    const b = withClick(buf, at); const s = c.createBufferSource(); s.buffer = b; s.playbackRate.value = slow ? 0.7 : 1; s.connect(c.destination);
    const from = Math.max(0, at - PRE), dur = PRE + POST; s.start(0, from, dur); src.current = s; setPlayed(j);
  };
  const answer = async (skip) => {
    stop(); const body = skip ? { bar: spot.bar, pass: spot.pass, skip: true } : { bar: spot.bar, pass: spot.pass, t: spot.cands[played] };
    await api.send("POST", `/api/recordings/${rec.id}/spotchecks`, body); if (!skip) setAnswered(a => a + 1);
    if (i + 1 < spots.length) setI(i + 1); else finish(answered + (skip ? 0 : 1));
  };
  const finish = async (n) => { stop(); if (n) { await api.send("POST", `/api/recordings/${rec.id}/spotchecks`, { done: true }); toast(`Thanks: ${rec.name} is being re-aligned around ${n} checked downbeat${n > 1 ? "s" : ""}`); onDone && onDone(); } onClose(); };
  useEffect(() => {
    const key = e => {
      if (!spot) { if (e.key === "Escape") onClose(); return; }
      if (/^[1-9]$/.test(e.key) && +e.key <= spot.cands.length) { e.preventDefault(); e.stopPropagation(); play(+e.key - 1); }
      else if (e.key === " ") { e.preventDefault(); e.stopPropagation(); play(played ?? spot.cands.indexOf(spot.t)); }
      else if (e.key === "Enter" && played != null) { e.preventDefault(); e.stopPropagation(); answer(false); }
      else if (e.key === "n" || e.key === "N") { e.preventDefault(); e.stopPropagation(); answer(true); }
      else if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); finish(answered); }
    };
    window.addEventListener("keydown", key, true); return () => window.removeEventListener("keydown", key, true);
  }, [spot, buf, played, slow, answered, i]);
  useEffect(() => stop, []);
  const cur = spot ? spot.cands.reduce((b, c, j) => Math.abs(c - spot.t) < 0.1 && (b < 0 || Math.abs(c - spot.t) < Math.abs(spot.cands[b] - spot.t)) ? j : b, -1) : -1;
  return html`<div class="scrim clear" onClick=${e => e.target === e.currentTarget && finish(answered)}><div class="dialog spot">
    <h3>Check timing · ${rec.name}</h3>
    ${!spots ? html`<div class="row"><span class="spin"></span> Finding the least certain places…</div>`
    : err ? html`<p class="hint">${err}</p>`
    : !spots.length ? html`<p class="hint">Nothing uncertain enough to be worth your time in this recording. (Checked places and anchors are not asked again.)</p>`
    : html`<div class="row" style="justify-content:space-between"><b style="font-size:16px">${M.label(spot.bar)}${spot.pass > 1 ? ` (${spot.pass}${["", "st", "nd", "rd"][spot.pass] || "th"} time)` : ""}</b><span class="muted">${i + 1} of ${spots.length} · at ${fmt(spot.t)}</span></div>
      <p class="hint" style="margin:4px 0 10px">Why here: ${spot.why}. Play each click and pick the one that lands on the first beat of ${M.label(spot.bar)}.</p>
      <div class="cands">${spot.cands.map((c, j) => html`<button class=${"btn cand" + (played === j ? " on" : "")} disabled=${!buf} onClick=${() => play(j)}>
          <kbd>${j + 1}</kbd><span>${j === cur ? "current" : (c - spot.t > 0 ? "+" : "−") + Math.abs(c - spot.t).toFixed(2) + " s"}</span></button>`)}</div>
      <div class="row" style="margin-top:10px"><label class="hint" style="display:flex;align-items:center;gap:6px"><input type="checkbox" checked=${slow} onChange=${e => setSlow(e.target.checked)} /> slower (70%)</label>
        <span class="spacer"></span><button class="btn" onClick=${() => answer(true)}>None / can't tell <kbd>N</kbd></button>
        <button class="btn primary" disabled=${played == null} onClick=${() => answer(false)}>This one <kbd>Enter</kbd></button></div>`}
    <div class="actions"><span class="hint" style="margin-right:auto">${answered ? `${answered} answered` : "Each answer pins that downbeat; the rest is re-fitted around it."}</span>
      <button class="btn" onClick=${() => finish(answered)}>${answered ? "Done" : "Close"} <kbd>Esc</kbd></button></div>
  </div></div>`;
}
