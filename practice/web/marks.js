// Close annotation: typed marks dropped while a recording plays (F, then a number for the type), and performance
// cues drawn on the score (Q): basic, interpretive, expressive, structural (Chaffin & Imreh).
import { useState, useEffect, useRef } from "./vendor/hooks.js";
import { html, api, toast } from "./lib.js";

export const MARK_KINDS = [["wrong", "wrong note", "✕"], ["hesitation", "hesitation", "‖"], ["rhythm", "rhythm", "♩"], ["balance", "balance", "⚖"],
  ["shape", "shape", "⌒"], ["pedal", "pedal", "𝆮"], ["tension", "tension", "!"], ["note", "note", "•"]];
export const MARK = Object.fromEntries(MARK_KINDS.map(([k, n, s]) => [k, { name: n, sym: s }]));
export const CUE_KINDS = [["expressive", "Expressive", "the character or feeling here"], ["interpretive", "Interpretive", "phrasing, dynamics, timing, pedal"],
  ["structural", "Structural", "a section boundary, a switch between similar passages"], ["basic", "Basic", "fingering, a leap, a technical corner"]];

// after F: a small bar that takes the next number key as the mark's type (for 4 seconds)
export function MarkChooser({ flag, onDone }) {
  const [left, setLeft] = useState(4);
  const set = async (k) => { await api.send("PATCH", `/api/flags/${flag.id}`, { kind: k }); onDone(k); };
  useEffect(() => {
    const key = e => { const i = "12345678".indexOf(e.key); if (i >= 0) { e.preventDefault(); e.stopPropagation(); set(MARK_KINDS[i][0]); } else if (e.key === "Escape") { e.stopPropagation(); onDone(null); } };
    window.addEventListener("keydown", key, true);
    const iv = setInterval(() => setLeft(x => { if (x <= 1) { clearInterval(iv); onDone(null); } return x - 1; }), 1000);
    return () => { window.removeEventListener("keydown", key, true); clearInterval(iv); };
  }, [flag.id]);
  return html`<div class="markbar"><span class="muted">Mark type</span>${MARK_KINDS.map(([k, n, s], i) => html`<button class="btn small" onClick=${() => set(k)}><kbd>${i + 1}</kbd> ${s} ${n}</button>`)}<span class="muted">${left}s</span></div>`;
}

export function CueDialog({ M, bar, cue, pid, onClose, onSaved }) {
  const [kind, setKind] = useState(cue ? cue.kind : "expressive"); const [text, setText] = useState(cue ? cue.text : "");
  const inp = useRef(); useEffect(() => { inp.current && inp.current.focus(); }, [kind]);
  const save = async () => {
    if (cue) await api.send("PATCH", `/api/cues/${cue.id}`, { kind, text });
    else await api.send("POST", `/api/pieces/${pid}/cues`, { bar, kind, text });
    onSaved(); onClose();
  };
  const del = async () => { await api.send("DELETE", `/api/cues/${cue.id}`); onSaved(); onClose(); };
  return html`<div class="scrim" onClick=${e => e.target === e.currentTarget && onClose()}><div class="dialog">
    <h3>Performance cue · ${M.label(cue ? cue.bar : bar)}</h3>
    <p class="hint" style="margin-top:0">A landmark to think of at this spot when you play: it becomes a place you can start from and a memory anchor. Expressive cues hold up best.</p>
    <div class="tools">${CUE_KINDS.map(([k, n, d]) => html`<button class=${"btn cue-" + k + (kind === k ? " on" : "")} title=${d} onClick=${() => setKind(k)}>${n}</button>`)}</div>
    <div class="fields" style="margin-top:8px"><input ref=${inp} placeholder=${{ expressive: "e.g. the question, left hanging", interpretive: "e.g. breathe before the E; half pedal", structural: "e.g. 2nd time goes to the Neapolitan", basic: "e.g. thumb on A, look at the landing" }[kind]}
      value=${text} onInput=${e => setText(e.target.value)} onKeyDown=${e => { if (e.key === "Enter") save(); if (e.key === "Escape") onClose(); e.stopPropagation(); }} /></div>
    <div class="actions">${cue ? html`<button class="btn" style="margin-right:auto;color:var(--warn)" onClick=${del}>Delete</button>` : null}<button class="btn" onClick=${onClose}>Cancel</button><button class="btn primary" onClick=${save}>Save <kbd>Enter</kbd></button></div></div></div>`;
}

// cues on the score: a coloured tag at the top-left of the bar, the text on hover (and shown in practice mode)
export function CueMarks({ M, bars, cues, W, H, k, showText, onEdit }) {
  const on = new Set(bars.map(b => b.id)); const per = {};
  for (const c of cues || []) if (on.has(c.bar)) (per[c.bar] = per[c.bar] || []).push(c);
  return html`<g>${Object.entries(per).map(([id, cs]) => { const [x0, y0] = M.byId[id].box;
    return cs.map((c, i) => { const x = x0 * W + 4 * k, y = y0 * H - (8 + 12 * i) * k;
      return html`<g class=${"cue cue-" + c.kind} onClick=${e => { e.stopPropagation(); onEdit && onEdit(c); }}>
        <rect x=${x} y=${y} width=${showText && c.text ? Math.min(220, 8 + c.text.length * 5.6) * k : 10 * k} height=${10 * k} rx=${3 * k} />
        ${showText && c.text ? html`<text x=${x + 4 * k} y=${y + 7.8 * k} style=${`font-size:${8.5 * k}px`}>${c.text.length > 38 ? c.text.slice(0, 37) + "…" : c.text}</text>` : null}
        <title>${c.kind}: ${c.text}</title></g>`; }); })}</g>`;
}
