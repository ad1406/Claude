// Shared bits used by every panel: rendering, API calls, formatting, the single Player.
import { h } from "./vendor/preact.js";
import { useEffect, useReducer, useState, useRef } from "./vendor/hooks.js";
import htm from "./vendor/htm.js";
import { Player } from "./player.js";

export const html = htm.bind(h);
export const api = {
  get: (u) => fetch(u).then(r => r.json()),
  send: (method, u, body) => fetch(u, { method, headers: { "Content-Type": "application/json" }, body: JSON.stringify(body || {}) }).then(r => r.json()),
  form: (u, fd) => fetch(u, { method: "POST", body: fd }).then(r => r.json()),
};
export const fmt = t => { t = Math.max(0, t || 0); const m = Math.floor(t / 60), s = (t % 60).toFixed(1).padStart(4, "0"); return `${m}:${s}`; };
export const fmtMin = s => { s = Math.max(0, Math.floor(s || 0)); return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`; };
export const ord = p => ["", "1st", "2nd", "3rd"][p] || `${p}th`;
export const store = {
  get: (k, d) => { try { const v = localStorage.getItem(k); return v == null ? d : JSON.parse(v); } catch (e) { return d; } },
  set: (k, v) => { try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) {} },
};
export const player = new Player();
window.player = player;

let toastTimer = null;
export function toast(msg) {
  const t = document.getElementById("toast"); if (!t) return;
  t.textContent = msg; t.classList.add("show"); clearTimeout(toastTimer); toastTimer = setTimeout(() => t.classList.remove("show"), 1800);
}
export function usePlayerTick() {
  const [, force] = useReducer(x => x + 1, 0);
  useEffect(() => player.on(() => force()), []);
}
export function romanOr(name, i) { const m = /^([IVXLC]+)\b/.exec(name || ""); return m ? m[1] : `${i + 1}`; }
// a range of bars as text, e.g. "m. 12–20" or "II · 3–9"
export function rangeLabel(M, a, b) {
  if (!M || !M.byId[a]) return "";
  if (!b || a === b) return M.label(a);
  const la = M.labels[a] ?? "?", lb = M.labels[b] ?? "?";
  const ma = M.mvOf[a], mb = M.mvOf[b];
  if (M.mvNames.length > 1 && ma != null) return ma === mb ? `${romanOr(M.mvNames[ma], ma)} · ${la}–${lb}` : `${M.label(a)} – ${M.label(b)}`;
  return `m. ${la}–${lb}`;
}

// A text box that keeps what you type while you type (the page re-renders every second for timers and playback,
// which would otherwise reset it to the saved value). It saves on leaving the box, or Enter for one-line boxes;
// Esc restores the saved text.
export function Field({ value, onSave, multiline, ...rest }) {
  const saved = value ?? ""; const [v, setV] = useState(saved);
  const editing = useRef(false); const cancel = useRef(false); const cur = useRef(saved);
  useEffect(() => { if (!editing.current) { setV(saved); cur.current = saved; } }, [saved]);
  const commit = () => { editing.current = false; if (cancel.current) { cancel.current = false; return; } if (cur.current !== saved) onSave(cur.current); };
  const props = { ...rest, value: v,
    onFocus: () => { editing.current = true; },
    onInput: e => { editing.current = true; cur.current = e.target.value; setV(e.target.value); },
    onBlur: commit,
    onKeyDown: e => { e.stopPropagation();
      if (e.key === "Enter" && !multiline) { e.preventDefault(); e.target.blur(); }
      else if (e.key === "Escape") { cancel.current = true; setV(saved); cur.current = saved; e.target.blur(); } } };
  return multiline ? html`<textarea ...${props}></textarea>` : html`<input ...${props} />`;
}
