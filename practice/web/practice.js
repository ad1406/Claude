// Practice mode: today's plan and the active passage. A passage has an intent (what it should sound like), a stage,
// a diagnosis that leads to a technique tried as an experiment, its takes over time, and cold takes that are
// recorded and rated after listening. Tempo is a setting of the metronome, not a goal.
import { useState, useEffect, useRef, useMemo } from "./vendor/hooks.js";
import { html, api, fmtMin, store, player, toast, rangeLabel, Field } from "./lib.js";
import { Metronome, bursts, fastSlow, softLoud, chainSteps, lcm } from "./metronome.js";
import { Recorder } from "./record.js";
import { RATING_WORDS } from "./takes.js";
import { MARK, CUE_KINDS } from "./marks.js";

export const metro = new Metronome();
window.metro = metro;
const RATINGS = [[1, "Broke down", "stopped, or lost it"], [2, "Got through", "with slips"], [3, "As intended", "it sounded like the intent"], [4, "Better", "than intended"]];
export const STATE_NAME = { working: "daily work", maintenance: "cold takes" };
export const STAGES = [
  { id: "learning", name: "Learning", q: "Are the notes, fingering and pedal settled, played slowly and musically?" },
  { id: "shaping", name: "Shaping", q: "Did your last warm take sound like the intent?" },
  { id: "securing", name: "Securing", q: "Did a cold take sound like the intent?" },
  { id: "performing", name: "Performing", q: "Does it hold in context, cold, in one recorded take?" }];
export const stageOf = p => STAGES.find(s => s.id === (p.stage || p.auto_stage)) || STAGES[0];

function useMetro() { const [, f] = useState(0); useEffect(() => metro.onChange(() => f(x => x + 1)), []); }
export const passageName = (M, p) => p.name || rangeLabel(M, p.a, p.b) || "Passage";
const pname = (p) => p.name || p.label || "Passage";

// bars of a passage in notated order
export function passageBars(M, p) {
  if (!M || !p) return []; const oa = M.order[p.a], ob = M.order[p.b];
  if (oa == null || ob == null) return []; return M.seq.slice(Math.min(oa, ob), Math.max(oa, ob) + 1);
}

// ------------------------------------------------------------------ score overlay (inside the page SVG)
export function PassageOverlay({ M, bars, passages, active, W, H, k, onPick }) {
  const onPage = new Set(bars.map(b => b.id));
  const out = [];
  for (const p of passages || []) {
    const ids = passageBars(M, p).filter(id => onPage.has(id)); if (!ids.length) continue;
    const on = active && active.id === p.id;
    for (const id of ids) {
      const [x0, , x1, y1] = M.byId[id].box;
      out.push(html`<rect class=${"pz " + (p.state || "") + (on ? " on" : "")} x=${x0 * W + 2 * k} y=${y1 * H - (on ? 7 : 5) * k} width=${Math.max(2, (x1 - x0) * W - 4 * k)} height=${(on ? 6 : 4) * k}
        rx=${2 * k} onClick=${e => { e.stopPropagation(); onPick(p); }}><title>${p.name || "passage"} (${stageOf(p).name})</title></rect>`);
    }
  }
  return html`<g>${out}</g>`;
}
export function HideOverlay({ M, bars, hide, W, H, k }) {
  if (!hide) return null; const ids = new Set(passageBars(M, hide));
  return html`<g>${bars.filter(b => ids.has(b.id)).map(b => { const [x0, y0, x1, y1] = b.box;
    return html`<rect class="hid" x=${x0 * W} y=${y0 * H - 4 * k} width=${(x1 - x0) * W} height=${(y1 - y0) * H + 8 * k} />`; })}</g>`;
}

// ------------------------------------------------------------------ cold take: record one run, listen, then rate
const coldRec = new Recorder();
export function TestDialog({ M, pid, passage, onClose, onDone, setHide }) {
  const [hideScore, setHideScore] = useState(store.get("testHide", false));
  const [step, setStep] = useState("rec");          // rec | rate (ready only if the microphone fails)
  const [url, setUrl] = useState(null); const [err, setErr] = useState(null); const [, tick] = useState(0);
  const upload = useRef(null); const audio = useRef(null);
  useEffect(() => { if (hideScore) setHide({ a: passage.a, b: passage.b }); return () => setHide(null); }, [hideScore]);
  useEffect(() => coldRec.onChange(() => tick(x => x + 1)), []);
  useEffect(() => () => { if (coldRec.on) coldRec.stop(); if (url) URL.revokeObjectURL(url); }, []);
  useEffect(() => { start(); }, []);                 // T starts recording at once: play when ready, Space or T to stop
  const start = async () => {
    player.pause(); metro.stop();
    try { await coldRec.start(); setStep("rec"); } catch (e) { setErr("Couldn't open the microphone (" + (e.message || e.name) + "). You can still rate from memory."); setStep("rate"); }
  };
  const stop = async () => {
    const secs = coldRec.elapsed(); const blob = await coldRec.stop();
    if (!blob || secs < 1.5) { setStep("ready"); toast("Too short: not kept"); return; }
    setUrl(URL.createObjectURL(blob)); setStep("rate");
    const fd = new FormData(); fd.append("file", blob, "take.webm"); fd.append("name", `Cold take · ${passageName(M, passage)}`);
    fd.append("kind", "mine"); fd.append("source", "take"); fd.append("passage_id", passage.id); fd.append("label", "cold");
    const piano = store.get("piano", ""); if (piano) fd.append("piano", piano);
    upload.current = api.form(`/api/pieces/${pid}/recordings`, fd);
    setTimeout(() => audio.current && audio.current.play().catch(() => {}), 150);
  };
  const rate = async (r) => {
    const up = upload.current ? await upload.current : null;
    const v = await api.send("POST", `/api/passages/${passage.id}/log`, { kind: "test", rating: r, take_id: up && up.id });
    toast(r === 1 ? `${pname(passage)}: back to daily work. Say what went wrong in the guide (D).`
      : v.state === "maintenance" ? `${pname(passage)}: ${RATINGS[r - 1][1]} · next cold take ${new Date(v.due * 1000).toLocaleDateString(undefined, { month: "short", day: "numeric" })}`
      : `${pname(passage)}: ${RATINGS[r - 1][1]} (logged; it stays in daily work until you mark it ready)`);
    onDone(v, r === 1); onClose();
  };
  useEffect(() => {
    const key = e => {
      if (e.target && (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA")) return;
      if (step === "ready" && (e.key === " " || e.key === "r" || e.key === "R" || e.key === "t" || e.key === "T")) { e.preventDefault(); e.stopPropagation(); start(); }
      else if (step === "rec" && (e.key === " " || e.key === "r" || e.key === "R" || e.key === "t" || e.key === "T" || e.key === "Enter")) { e.preventDefault(); e.stopPropagation(); stop(); }
      else if (step === "rate" && /^[1-4]$/.test(e.key)) { e.preventDefault(); e.stopPropagation(); rate(+e.key); }
      else if (step === "rate" && e.key === " " && audio.current) { e.preventDefault(); e.stopPropagation(); audio.current.paused ? audio.current.play() : audio.current.pause(); }
      else if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); if (coldRec.on) coldRec.stop(); onClose(); }
    };
    window.addEventListener("keydown", key, true); return () => window.removeEventListener("keydown", key, true);
  }, [step, url]);
  const lv = coldRec.on ? coldRec.level() : -90; const pct = Math.max(0, Math.min(100, (lv + 60) / 60 * 100));
  return html`<div class="scrim clear" onClick=${e => e.target === e.currentTarget && step !== "rec" && onClose()}><div class="dialog">
    <h3>Cold take · ${passageName(M, passage)} <span class="muted" style="font-weight:400">${rangeLabel(M, passage.a, passage.b)}</span></h3>
    ${passage.intent ? html`<div class="intent-read"><span class="hint">Intent</span> ${passage.intent}</div>` : html`<div class="hint">No intent written for this passage yet: say to yourself what it should sound like first.</div>`}
    ${step === "ready" ? html`
      <p class="hint">Couldn't start recording. Play it once, without warming up first, then rate it.</p>
      <label class="hint" style="display:flex;align-items:center;gap:6px"><input type="checkbox" checked=${hideScore} onChange=${e => { setHideScore(e.target.checked); store.set("testHide", e.target.checked); }} /> hide the passage on the score</label>
      <div class="row" style="margin-top:10px"><button class="btn primary" onClick=${start}>● Record the take <kbd>Space</kbd></button><span class="spacer"></span>
        <button class="btn" onClick=${() => setStep("rate")} title="no recording: rate from memory">Rate without recording</button></div>`
    : step === "rec" ? html`<p class="hint" style="margin:0 0 4px">Recording. Play it once, entering from the bar before, without warming up first; keep going through slips as on stage.</p>
      <label class="hint" style="display:flex;align-items:center;gap:6px"><input type="checkbox" checked=${hideScore} onChange=${e => { setHideScore(e.target.checked); store.set("testHide", e.target.checked); }} /> hide the passage on the score</label>
      <div class="row" style="margin-top:8px"><span class="recdot"></span><b>${fmtMin(coldRec.elapsed())}</b><span class="lvl big"><i style=${`width:${pct}%;background:${lv > -3 ? "var(--warn)" : "var(--ok)"}`}></i></span>
        <span class="spacer"></span><button class="btn primary" onClick=${stop}>■ Stop <kbd>Space</kbd></button></div>`
    : html`${err ? html`<p class="hint warn">${err}</p>` : null}
      ${url ? html`<p class="hint">Listen back first, then rate what you hear against the intent: not what it felt like.</p><audio ref=${audio} src=${url} controls style="width:100%"></audio>` : null}
      <div class="ratings">${RATINGS.map(([n, name, d]) => html`<button class=${"btn rate r" + n} onClick=${() => rate(n)}><kbd>${n}</kbd><b>${name}</b><span>${d}</span></button>`)}</div>`}
    <div class="actions"><button class="btn" onClick=${() => { if (coldRec.on) coldRec.stop(); onClose(); }}>Cancel <kbd>Esc</kbd></button></div></div></div>`;
}

export function Suggestions({ plan, openPassage }) {
  const [gone, setGone] = useState(store.get("dismissedJoins", []));
  const list = (plan.suggestions || []).filter(x => !gone.includes(x.ids.join("+")));
  if (!list.length) return null;
  const join = async (x) => { const p = await api.send("POST", `/api/pieces/${x.piece_id}/passages`, { a: x.a, b: x.b, name: x.name });
    if (!p.error) { toast("Joined: practise them as one passage now"); openPassage(p); } };
  const dismiss = (x) => { const g = [...gone, x.ids.join("+")]; setGone(g); store.set("dismissedJoins", g); };
  return html`${list.map(x => html`<div class="qrow sugg" title="both hold in cold takes: time to play them as one">
    <span class="kind join">Join</span><span class="nm">${x.name}</span><span class="muted ell">${x.label || ""}</span>
    <span class="row"><button class="btn small" onClick=${() => join(x)}>Join</button><button class="btn small ghost" onClick=${() => dismiss(x)}>×</button></span></div>`)}`;
}

export function Today({ M, plan, active, openPassage, startTest }) {
  if (!plan) return html`<div class="hint">…</div>`;
  const items = plan.items; const inb = items.filter(i => i.in_budget); const later = items.length - inb.length;
  if (!items.length) return html`<div class="hint">${plan.done.length ? `Done for today: ${plan.done.length} passage${plan.done.length > 1 ? "s" : ""}. ` : ""}${statLine(plan.stats) ? statLine(plan.stats) + ". " : ""}Nothing else is due.
    Select bars on the score and press <kbd>P</kbd> to make a passage to work on.</div><${Suggestions} plan=${plan} openPassage=${openPassage} />`;
  const [next, ...rest] = inb;
  const go = (it) => { openPassage(it); if (it.kind === "test") setTimeout(() => startTest(it), 60); };
  const kindName = it => it.kind === "test" ? "Cold" : "Work";
  return html`
    <div class=${"next" + (active && active.id === next.id ? " on" : "")} onClick=${() => go(next)}>
      <span class=${"kind " + next.kind}>${kindName(next)}</span>
      <div class="grow1"><b>${pname(next)}</b> <span class="muted">${next.piece_title}</span><div class="hint">${next.kind === "test" ? next.reason.replace("cold test", "cold take") : next.reason}</div></div>
      <button class="btn primary small" onClick=${e => { e.stopPropagation(); go(next); }}>Start</button></div>
    ${rest.slice(0, 4).map(it => html`<div class=${"qrow" + (active && active.id === it.id ? " on" : "")} onClick=${() => go(it)}>
      <span class=${"kind " + it.kind}>${kindName(it)}</span><span class="nm">${pname(it)}</span><span class="muted ell">${it.piece_title}</span><span class="muted">${Math.round(it.est)}′</span></div>`)}
    <${Suggestions} plan=${plan} openPassage=${openPassage} />
    <div class="hint" style="margin-top:3px">${[rest.length > 4 ? `+${rest.length - 4} more` : "", later ? `${later} past your time today` : "",
      plan.done.length ? `${plan.done.length} done` : "", plan.rested.length ? `${plan.rested.length} resting (theme rotation)` : "", statLine(plan.stats)].filter(Boolean).join(" · ")}</div>`;
}

export function statLine(st) {
  if (!st || !(st.today || st.week)) return "";
  const hm = m => m >= 60 ? `${Math.floor(m / 60)} h ${m % 60 ? (m % 60) + " min" : ""}`.trim() : `${m} min`;
  return [`today ${hm(st.today)}`, `this week ${hm(st.week)}`, st.streak > 1 ? `${st.streak} days in a row` : ""].filter(Boolean).join(", ");
}

export function PieceList({ M, data, passages, active, openPassage, onPerf }) {
  const [perf, setPerf] = useState(data.piece.perf_date || "");
  useEffect(() => setPerf(data.piece.perf_date || ""), [data.piece.id]);
  const savePerf = async (v) => { setPerf(v); await api.send("PATCH", `/api/pieces/${data.piece.id}`, { perf_date: v || null }); onPerf(); };
  const perfRow = html`<label class="hint row" style="margin-bottom:4px" title="raises priority and target retention as the date approaches">Performance date
    <input type="date" class="mini" value=${perf} onChange=${e => savePerf(e.target.value)} />${perf ? html`<button class="btn small" onClick=${() => savePerf("")}>×</button>` : null}</label>`;
  if (!passages.length) return html`${perfRow}<div class="hint">No passages in this piece yet. Select bars (click, Shift+click) and press <kbd>P</kbd>.</div>`;
  return html`${perfRow}<div class="plist2">${passages.map(p => html`<div class=${"qrow" + (active && active.id === p.id ? " on" : "")} onClick=${() => openPassage(p)}>
    <span class=${"kind " + (p.state === "maintenance" ? "test" : "work")}>${stageOf(p).name.slice(0, 5)}</span><span class="nm">${passageName(M, p)}</span>
    <span class="muted ell">${rangeLabel(M, p.a, p.b)}</span><span class="muted">${p.worked_today || p.tested_today ? "✓" : ""}</span></div>`)}</div>`;
}

export function NotesTab({ M, p, patch, hist, lib, cues, onCue, marks, onDeleted }) {
  const day = t => new Date(t * 1000).toLocaleDateString(undefined, { month: "short", day: "numeric" });
  const tname = id => { const t = lib && lib.techniques.find(x => x.id === id); return t ? t.name : id; };
  const line = l => l.kind === "test" ? `cold take: ${RATING_WORDS[l.rating] || l.rating}`
    : l.kind === "work" ? `worked ${l.minutes ? Math.round(l.minutes) + " min" : ""}${l.technique ? " · " + tname(l.technique) : ""}${l.helped === 1 ? " · helped" : l.helped === -1 ? " · didn't help" : ""}`
    : l.kind === "try" ? `tried ${tname(l.technique)}: ${l.helped > 0 ? "better" : l.helped < 0 ? "worse" : "same"}${l.prediction ? ` (predicted: ${l.prediction})` : ""}`
    : l.kind === "graduate" ? "moved to cold takes" : l.kind === "unlearn" ? "back to daily work" : l.kind === "stage" ? `stage: ${l.note}` : l.kind;
  const del = async (l) => { if (!confirm(`Delete "${line(l)}" (${day(l.t)})? ${l.kind === "test" ? "The spacing of cold takes is recalculated as if it never happened." : ""}`)) return;
    await api.send("DELETE", `/api/log/${l.id}`); onDeleted(); };
  return html`<div class="notes-hist">
    <${Field} multiline rows="3" placeholder="Notes, fingering, the story of this passage…" value=${p.notes || ""} onSave=${v => patch({ notes: v })} />
    <label class="hint row" title="passages with the same theme are rotated: one per day">Theme <${Field} class="grow1" value=${p.theme || ""} placeholder="—" onSave=${v => patch({ theme: v.trim() || null })} /></label>
    ${cues.length || marks.length ? html`<div class="cuelist">${cues.map(c => html`<span class=${"cuechip cue-" + c.kind} onClick=${() => onCue(c)} title=${c.kind}>${M.labels[c.bar] ?? ""} ${c.text || c.kind}</span>`)}
      ${marks.map(f => html`<span class=${"cuechip mk mk-" + f.kind} title=${(MARK[f.kind] || {}).name}>${M.labels[f.bar] ?? ""} ${(MARK[f.kind] || {}).sym} ${f.note || (MARK[f.kind] || {}).name}</span>`)}</div>` : html`<div class="hint">Cues: select a bar and press <kbd>Q</kbd> to add a performance cue.</div>`}
    <div class="hist">${hist.length ? hist.slice(0, 40).map(l => html`<div class="hrow"><span class="muted">${day(l.t)}</span><span class="grow1 ell" title=${line(l)}>${line(l)}</span>
        <button class="btn small ghost" title="delete this entry (a false test, a mistake)" onClick=${() => del(l)}>×</button></div>`) : html`<span class="hint">No practice logged yet.</span>`}</div></div>`;
}

// ------------------------------------------------------------------ helpers on the technique card
export function Helper({ kind, tech, M, p, patch, setSel, hide, setHide }) {
  const hs = p.helper_state || {}; const st = hs[tech.id] || {};
  const save = (v) => patch({ helper_state: { ...hs, [tech.id]: { ...st, ...v } } });
  if (kind === "bursts" || kind === "fastslow" || kind === "softloud") {
    const m = st.m || (kind === "fastslow" ? 3 : 4); const done = new Set(st.done || []);
    const pats = kind === "bursts" ? bursts(m) : kind === "fastslow" ? fastSlow(m) : softLoud(m, !!st.two);
    const toggle = s => { const d = new Set(done); d.has(s) ? d.delete(s) : d.add(s); save({ done: [...d] }); };
    return html`<div class="helper"><div class="row hint">${kind === "bursts" ? "burst length" : "notes per group"}
        <select class="mini" value=${m} onChange=${e => save({ m: +e.target.value, done: [] })}>${[2, 3, 4, 5, 6, 8].map(x => html`<option value=${x}>${x}</option>`)}</select>
        ${kind === "softloud" ? html`<label class="row"><input type="checkbox" checked=${!!st.two} onChange=${e => save({ two: e.target.checked })} /> two loud</label>` : null}
        <span class="spacer"></span>${done.size}/${pats.length} done ${done.size ? html`<button class="btn small" onClick=${() => save({ done: [] })}>reset</button>` : null}</div>
      <div class="pats">${pats.map(s => html`<button class=${"pat" + (done.has(s) ? " done" : "")} onClick=${() => toggle(s)}>${s}</button>`)}</div></div>`;
  }
  if (kind === "chain-back" || kind === "chain-focal") {
    const bars = passageBars(M, p); const n = bars.length;
    const focus = Math.min(n - 1, st.focus ?? Math.floor(n / 2));
    const steps = chainSteps(n, kind === "chain-back" ? "back" : "focal", focus); const i = Math.min(steps.length - 1, st.step || 0);
    const [lo, hi] = steps[i] || [0, 0];
    const show = (j) => { const [a, b] = steps[j]; setSel({ a: bars[a], b: bars[b], anchor: bars[a] }); save({ step: j }); };
    if (n < 2) return html`<div class="helper hint">Make the passage at least two bars long to chain it.</div>`;
    return html`<div class="helper"><div class="row"><span>Step ${i + 1} of ${steps.length}: <b>${rangeLabel(M, bars[lo], bars[hi])}</b></span><span class="spacer"></span>
        <button class="btn small" onClick=${() => show(Math.max(0, i - 1))}>‹</button><button class="btn small primary" onClick=${() => show(Math.min(steps.length - 1, i + 1))}>secure → next</button></div>
      <div class="chain">${bars.map((id, j) => html`<i class=${(j >= lo && j <= hi ? "in" : "") + (kind === "chain-focal" && j === focus ? " focus" : "")}
          title=${M.label(id)} onClick=${() => kind === "chain-focal" ? save({ focus: j, step: 0 }) : null}></i>`)}</div>
      ${kind === "chain-focal" ? html`<div class="hint">Click a cell to set the focus bar.</div>` : null}</div>`;
  }
  if (kind === "landing") {
    const nb = M.seq[M.order[p.b] + 1];
    return html`<div class="helper row">${nb ? html`<button class="btn small" onClick=${() => setSel({ a: p.a, b: nb, anchor: p.a })}>Select through ${M.label(nb)} (the landing bar)</button>` : html`<span class="hint">This passage ends the piece.</span>`}</div>`;
  }
  if (kind === "hide") return html`<div class="helper row"><button class=${"btn small" + (hide ? " on" : "")} onClick=${() => setHide(hide ? null : { a: p.a, b: p.b })}>${hide ? "Show the passage" : "Hide the passage on the score"} <kbd>H</kbd></button></div>`;
  if (kind === "counter") {
    const c = st.count || 0;
    return html`<div class="helper row"><span>Notes built: <b>${c}</b></span><button class="btn small" onClick=${() => save({ count: c + 1 })}>+1</button><button class="btn small" onClick=${() => save({ count: Math.max(0, c - 1) })}>−1</button><button class="btn small" onClick=${() => save({ count: 0 })}>reset</button></div>`;
  }
  if (kind === "story") return html`<div class="helper"><${Field} multiline rows="2" placeholder="The story or picture for this passage…" value=${p.notes || ""} onSave=${v => patch({ notes: v })} /></div>`;
  if (kind === "theme") return html`<div class="helper hint">Give each instance of the theme the same Theme name (Notes tab).</div>`;
  if (kind === "regroup") {
    const g = st.group || 2, sub = st.sub || 2;
    const apply = (ng, ns) => { save({ group: ng, sub: ns }); metro.set({ mode: "regroup", group: ng, sub: ns }); };
    return html`<div class="helper row">Accent every <select class="mini" value=${g} onChange=${e => apply(+e.target.value, sub)}>${[2, 3, 4, 5, 6, 7].map(x => html`<option value=${x}>${x}</option>`)}</select>
      of <select class="mini" value=${sub} onChange=${e => apply(g, +e.target.value)}>${[1, 2, 3, 4, 6].map(x => html`<option value=${x}>${x}</option>`)}</select> notes per beat
      <button class=${"btn small" + (metro.on && metro.mode === "regroup" ? " on" : "")} onClick=${() => { apply(g, sub); metro.toggle(); }}>${metro.on && metro.mode === "regroup" ? "Stop" : "Start"} <kbd>M</kbd></button></div>`;
  }
  if (kind === "poly") return html`<${PolyHelper} st=${st} save=${save} />`;
  if (kind === "metronome") return html`<${MetroHelper} tech=${tech} p=${p} />`;
  return null;
}

// quick metronome settings for the technique on the card (the tempo is the passage's last one)
function MetroHelper({ tech, p }) {
  useMetro();
  const b = p.bpm || metro.bpm;
  const go = (o) => { metro.set({ mode: "beat", feel: "all", gap: null, ladder: null, ...o }); if (!metro.on) metro.start(); };
  const presets = {
    gapclick: [["2 on, 2 silent", { gap: { on: 2, off: 2 } }], ["4 on, 4 silent", { gap: { on: 4, off: 4 } }]],
    sparseclick: [["only the bar line", { feel: "bar" }]],
    offbeat: [["only the off-beats", { feel: "offbeat", sub: 2 }]],
    gridclick: [["16ths", { sub: 4 }], ["triplets", { sub: 3 }], ["slow 16ths", { sub: 4, bpm: Math.round(b * 0.6) }]],
    oneclick: [["one click per note (÷4)", { sub: 4 }]],
    ladder: [["ladder +2 every 4 bars", { ladder: { step: 2, every: 4, goal: Math.round(b * 1.3) } }]],
    kalams: [["slow pulse", { bpm: Math.round(b / 2), sub: 1 }], ["same pulse ÷2", { bpm: Math.round(b / 2), sub: 2 }], ["÷4", { bpm: Math.round(b / 2), sub: 4 }]],
  }[tech.id] || [["−30%", { bpm: Math.round(b * 0.7) }], ["−15%", { bpm: Math.round(b * 0.85) }], [`♩ ${Math.round(b)}`, { bpm: b }], ["+5%", { bpm: Math.round(b * 1.05) }]];
  return html`<div class="helper row">${presets.map(([n, o]) => html`<button class="btn small" onClick=${() => go(o)}>${n}</button>`)}
    ${metro.on ? html`<button class="btn small on" onClick=${() => metro.stop()}>■ <kbd>M</kbd></button>` : null}<span class="hint">tempo is a setting, not a score</span></div>`;
}

function PolyHelper({ st, save }) {
  useMetro();
  const p = st.p || 3, q = st.q || 2, voice = st.voice || "both", vary = st.vary || 0; const n = lcm(p, q);
  const apply = (o) => { const s = { p, q, voice, vary, ...o }; save(s); metro.set({ mode: "poly", poly: { p: s.p, q: s.q, voice: s.voice, vary: s.vary } }); };
  const on = metro.on && metro.mode === "poly";
  return html`<div class="helper">
    <div class="row"><select class="mini" value=${p} onChange=${e => apply({ p: +e.target.value })}>${[2, 3, 4, 5, 6, 7, 8, 9].map(x => html`<option value=${x}>${x}</option>`)}</select> against
      <select class="mini" value=${q} onChange=${e => apply({ q: +e.target.value })}>${[2, 3, 4, 5, 6, 7, 8].map(x => html`<option value=${x}>${x}</option>`)}</select>
      <select class="mini" value=${voice} onChange=${e => apply({ voice: e.target.value })}><option value="both">click both</option><option value="p">only ${p}</option><option value="q">only ${q}</option><option value="grid">the grid</option></select>
      <label class="row hint" title="rotate what the metronome clicks every 2 cycles"><input type="checkbox" checked=${!!vary} onChange=${e => apply({ vary: e.target.checked ? 2 : 0 })} /> vary</label>
      <button class=${"btn small" + (on ? " on" : "")} onClick=${() => { apply({}); metro.toggle(); }}>${on ? "Stop" : "Start"}</button></div>
    <div class="grid" style=${`grid-template-columns:repeat(${n},1fr)`}>
      ${Array.from({ length: n }, (_, j) => html`<i class=${j % (n / p) === 0 ? "on" : ""}>${j % (n / p) === 0 ? j / (n / p) + 1 : ""}</i>`)}
      ${Array.from({ length: n }, (_, j) => html`<i class=${j % (n / q) === 0 ? "on q" : ""}>${j % (n / q) === 0 ? j / (n / q) + 1 : ""}</i>`)}
    </div><div class="hint">Count the ${n} grid cells; top row = ${p}, bottom row = ${q}.</div></div>`;
}

// ------------------------------------------------------------------ metronome strip
export function MetroCard({ active }) {
  useMetro();
  const modeName = { beat: "", regroup: `accent every ${metro.group}`, poly: `${metro.poly.p}:${metro.poly.q}` }[metro.mode];
  const feel = metro.gap ? "gap" : metro.feel;
  const setFeel = v => metro.set(v === "gap" ? { mode: "beat", feel: "all", gap: { on: 2, off: 2 } } : { mode: "beat", feel: v, gap: null, ...(v === "offbeat" && metro.sub < 2 ? { sub: 2 } : {}) });
  return html`<div class="card metro">
    <div class="row">
      <button class=${"btn" + (metro.on ? " on" : "")} onClick=${() => metro.toggle()}>${metro.on ? "■" : "▶"} <kbd>M</kbd></button>
      <span class="bpm" title="the tempo is remembered for the open passage">♩ = <input type="number" value=${metro.bpm} onChange=${e => metro.set({ bpm: +e.target.value })} /></span>
      ${metro.mode !== "poly" ? html`<select class="mini" value=${metro.beats} title="beats per bar" onChange=${e => metro.set({ beats: +e.target.value })}>${[1, 2, 3, 4, 5, 6, 7, 9, 12].map(x => html`<option value=${x}>${x}/bar</option>`)}</select>
        <select class="mini" value=${metro.sub} title="clicks per beat" onChange=${e => metro.set({ sub: +e.target.value })}>${[1, 2, 3, 4, 6].map(x => html`<option value=${x}>${x === 1 ? "no sub" : "÷" + x}</option>`)}</select>
        <select class="mini" value=${feel} title="what the click leaves to you" onChange=${e => setFeel(e.target.value)}><option value="all">every beat</option><option value="bar">bar line</option><option value="offbeat">off-beats</option><option value="gap">2 on, 2 off</option></select>` : null}
      ${modeName ? html`<button class="btn small on" title="back to a plain beat" onClick=${() => metro.set({ mode: "beat" })}>${modeName} ×</button>` : null}
      ${metro.ladder ? html`<button class="btn small on" title="stop the tempo ladder" onClick=${() => metro.set({ ladder: null })}>ladder +${metro.ladder.step}/${metro.ladder.every} bars ×</button>` : null}
    </div>
  </div>`;
}
