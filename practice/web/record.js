// Recording yourself (R) and flags dropped while listening (F).
import { useState, useEffect, useRef } from "./vendor/hooks.js";
import { html, api, fmt, fmtMin, player, toast, Field } from "./lib.js";
import { MARK, MARK_KINDS } from "./marks.js";

// ------------------------------------------------------------------ microphone recorder
export class Recorder {
  constructor() { this.mr = null; this.stream = null; this.chunks = []; this.t0 = 0; this.an = null; this.buf = null; this.listeners = new Set(); this.peak = -90; }
  onChange(f) { this.listeners.add(f); return () => this.listeners.delete(f); }
  emit() { for (const f of this.listeners) f(this); }
  get on() { return !!(this.mr && this.mr.state === "recording"); }
  elapsed() { return this.on ? (performance.now() - this.t0) / 1000 : 0; }
  async start() {
    // a piano needs the raw signal: no echo cancellation, noise suppression or automatic gain
    this.stream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false, channelCount: 1 } });
    const mime = ["audio/webm;codecs=opus", "audio/webm", "audio/ogg;codecs=opus"].find(m => window.MediaRecorder && MediaRecorder.isTypeSupported(m)) || "";
    this.mr = new MediaRecorder(this.stream, mime ? { mimeType: mime, audioBitsPerSecond: 160000 } : {});
    this.chunks = []; this.mr.ondataavailable = e => { if (e.data && e.data.size) this.chunks.push(e.data); };
    const ctx = new (window.AudioContext || window.webkitAudioContext)(); this.ctx = ctx;
    const src = ctx.createMediaStreamSource(this.stream); this.an = ctx.createAnalyser(); this.an.fftSize = 2048; src.connect(this.an);
    this.buf = new Float32Array(this.an.fftSize);
    this.mr.start(1000); this.t0 = performance.now(); this.peak = -90;
    this._iv = setInterval(() => this.emit(), 100); this.emit();
  }
  level() {
    if (!this.an) return -90; this.an.getFloatTimeDomainData(this.buf);
    let m = 0; for (const x of this.buf) m = Math.max(m, Math.abs(x));
    const db = 20 * Math.log10(m + 1e-9); this.peak = Math.max(this.peak - 0.5, db); return db;
  }
  stop() {
    return new Promise(res => {
      if (!this.mr) return res(null);
      this.mr.onstop = () => {
        const blob = new Blob(this.chunks, { type: this.mr.mimeType || "audio/webm" });
        this.stream.getTracks().forEach(t => t.stop()); clearInterval(this._iv); try { this.ctx.close(); } catch (e) {}
        this.mr = null; this.emit(); res(blob);
      };
      this.mr.stop();
    });
  }
}
export const recorder = new Recorder();

// What a take covers is fixed when recording starts: `context` is its label ("m. 4–9", or "" for the whole piece),
// opts (optional) = {sec_a, sec_b, passage_id, label, technique, piano}: a take of one section (lined up with those
// bars only, kept out of the main recording list), labelled.
let backEl = null;                                   // plays the take just recorded, straight from memory
export function playLast() {
  const L = recorder.last; if (!L) return;
  if (!backEl || backEl.src !== L.url) { if (backEl) backEl.pause(); backEl = new Audio(L.url); backEl.onended = backEl.onpause = backEl.onplay = () => recorder.emit(); }
  if (backEl.paused) { player.pause(); backEl.currentTime = backEl.ended ? 0 : backEl.currentTime; backEl.play().catch(() => {}); } else backEl.pause();
  recorder.emit();
}
export const lastPlaying = () => !!(backEl && !backEl.paused && recorder.last && backEl.src === recorder.last.url);

export async function toggleRecording(pid, context, opts) {
  if (!recorder.on) {
    if (backEl) backEl.pause();
    try { recorder.target = { context: context || "", opts: opts || null }; await recorder.start(); player.pause();
      toast(context ? `Recording ${context} only: R again to stop` : "Recording the whole piece: R again to stop (select bars first to record just those)"); }
    catch (e) { toast("Couldn't open the microphone: " + (e.message || e.name)); }
    return;
  }
  const { context: ctx, opts: o } = recorder.target || {};
  const secs = recorder.elapsed(); const blob = await recorder.stop();
  if (!blob || secs < 2) { toast("Too short: not kept"); return; }
  const d = new Date(); const when = d.toLocaleDateString(undefined, { month: "short", day: "numeric" }) + " " + d.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" });
  if (recorder.last) URL.revokeObjectURL(recorder.last.url);
  recorder.last = { url: URL.createObjectURL(blob), context: ctx, secs }; recorder.emit();
  const fd = new FormData(); fd.append("file", blob, "take.webm"); fd.append("name", `Take ${when}${ctx ? " · " + ctx : ""}`);
  fd.append("kind", "mine"); fd.append("source", "take");
  for (const [k, v] of Object.entries(o || {})) if (v != null && v !== "") fd.append(k, v);
  const r = await api.form(`/api/pieces/${pid}/recordings`, fd);
  if (r.error) toast("Saving failed: " + r.error);
  else { recorder.last.id = r.id; toast(`Take saved${ctx ? ` (${ctx})` : ""}: listen back now with Shift+R while it is lined up with the score`); }
  return r;
}

export function RecordButton({ pid, context, opts, onSaved, onToggle }) {
  const [, f] = useState(0); useEffect(() => recorder.onChange(() => f(x => x + 1)), []);
  const on = recorder.on; const lv = on ? recorder.level() : -90;
  const pct = Math.max(0, Math.min(100, (lv + 60) / 60 * 100));
  const L = recorder.last;
  return html`<span class="row recwrap">
    <button class=${"btn recbtn" + (on ? " live" : "")} title=${on ? "stop (R)" : context ? `record ${context} only (R): the take is lined up with those bars` : "record the whole piece (R); select bars first to record just those"}
      onClick=${async () => { if (onToggle) { onToggle(); return; } const r = await toggleRecording(pid, context, opts); if (r && onSaved) onSaved(r); }}>
      <span class="dot"></span>${on ? html`${fmtMin(recorder.elapsed())}<span class="lvl"><i style=${`width:${pct}%;background:${lv > -3 ? "var(--warn)" : "var(--ok)"}`}></i></span>Stop` : html`Record <span class="rctx">${context || "all"}</span>`} <kbd>R</kbd></button>
    ${L && !on ? html`<button class=${"btn lastbtn" + (lastPlaying() ? " on" : "")} title="listen to the take you just recorded (Shift+R)" onClick=${playLast}>${lastPlaying() ? "■" : "▶"} last take${L.context ? ` · ${L.context}` : ""} <kbd>⇧R</kbd></button>
      <button class="btn small ghost" title="hide" onClick=${() => { if (backEl) backEl.pause(); recorder.last = null; recorder.emit(); }}>×</button>` : null}</span>`;
}

// ------------------------------------------------------------------ flags
export async function dropFlag() {
  const r = player.rec; if (!r) { toast("Play a recording first; F marks a spot in it"); return null; }
  const p = player.pos(); const t = player.time();
  const f = await api.send("POST", `/api/recordings/${r.id}/flags`, { t, bar: p && p.k >= 0 ? p.bar : null, pass: p && p.k >= 0 ? p.pass : null, frac: p && p.k >= 0 ? Math.round(p.frac * 100) / 100 : null });
  return f;
}

export function FlagsCard({ M, data, flags, reload, makePassage }) {
  const recs = Object.fromEntries(data.recordings.map(r => [r.id, r]));
  const open = flags.filter(f => !f.done && recs[f.recording_id]);
  const go = (f) => { const res = player.select(f.recording_id, { time: Math.max(0, f.t - 2), play: true }); if (!res.ok) toast("Can't play that recording"); };
  if (!open.length) return html`<div class="hint">Press <kbd>F</kbd> while listening to mark a spot, then a number for its type (1 wrong note · 2 hesitation · 3 rhythm · 4 balance · 5 shape · 6 pedal · 7 tension · 8 note). Marks collect here, show on the score, and suggest what's wrong in the guide.</div>`;
  return html`<div class="flags">${open.map(f => html`<div class="flag">
      <button class=${"fl mk-" + (f.kind || "note")} title=${(MARK[f.kind] || MARK.note).name + ": play from just before"} onClick=${() => go(f)}>${(MARK[f.kind] || { sym: "⚑" }).sym}</button>
      <span class=${"nm" + (recs[f.recording_id].kind === "mine" ? " mine" : "")} onClick=${() => go(f)}>${recs[f.recording_id].name}</span>
      <span class="muted nowrap" onClick=${() => go(f)}>${f.bar && M.byId[f.bar] ? M.label(f.bar) : fmt(f.t)}</span>
      <${Field} class="fnote" value=${f.note || ""} placeholder="note" onSave=${async v => { await api.send("PATCH", `/api/flags/${f.id}`, { note: v }); reload(); }} />
      ${f.bar ? html`<button class="btn small" title="select these bars to work on them" onClick=${() => makePassage(f)}>→ bars</button>` : null}
      <button class="btn small" title="dismiss" onClick=${async () => { await api.send("DELETE", `/api/flags/${f.id}`); reload(); }}>×</button></div>`)}</div>`;
}

// red flag markers on the score, one per flagged bar
export function FlagMarks({ M, bars, flags, W, H, k }) {
  const on = new Set(bars.map(b => b.id)); const count = {};
  for (const f of flags || []) if (!f.done && f.bar && on.has(f.bar)) count[f.bar] = (count[f.bar] || 0) + 1;
  const kinds = {}; for (const f of flags || []) if (!f.done && f.bar && on.has(f.bar)) (kinds[f.bar] = kinds[f.bar] || []).push(f);
  return html`<g>${Object.entries(kinds).map(([id, fs]) => { const [x0, y0, x1] = M.byId[id].box;
    // one small symbol per mark along the top of the bar, right to left
    return fs.slice(0, 6).map((f, i) => { const x = x1 * W - (10 + 11 * i) * k, y = y0 * H + 2 * k; const m = MARK[f.kind];
      return m ? html`<g class=${"fmark typed mk-" + f.kind}><circle cx=${x} cy=${y + 5 * k} r=${5 * k} /><text x=${x} y=${y + 8.2 * k} text-anchor="middle" style=${`font-size:${8 * k}px`}>${m.sym}</text><title>${m.name}${f.note ? ": " + f.note : ""}</title></g>`
        : html`<g class="fmark"><path d=${`M${x} ${y}v${12 * k}M${x} ${y}h${8 * k}l${-2 * k} ${3 * k}l${2 * k} ${3 * k}h${-8 * k}`} vector-effect="non-scaling-stroke" /><title>${f.note || "flag"}</title></g>`; }); })}</g>`;
}
