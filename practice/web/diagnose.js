// Diagnosis: from what you hear to the technique that fits its cause, then a small experiment checked on the
// next take. The library (problems -> causes -> techniques, the quick tests) comes from the practice taxonomy.
import { useState, useEffect, useMemo } from "./vendor/hooks.js";
import { html, api, toast, store, Field } from "./lib.js";

export const GRADE = { E: "a study (usually with musicians)", M: "meta-analysis or motor-learning research, transferred to piano",
  C: "consensus of teachers or a coaching tradition", A: "one author or an anecdote: an experiment worth trying" };
// mark types (M3) -> the problem they usually point to
export const MARK_PROBLEM = { wrong: "wrongnote", hesitation: "hitch", rhythm: "rushing", balance: "voicing", shape: "flat", pedal: "pedal", tension: "tension" };

export function useLib(version) {
  const [lib, setLib] = useState(null);
  useEffect(() => { api.get("/api/practice/techniques").then(setLib); }, [version]);
  return lib;
}
export const libIndex = (lib) => lib ? { tech: Object.fromEntries(lib.techniques.map(t => [t.id, t])), prob: Object.fromEntries(lib.problems.map(p => [p.id, p])),
  test: Object.fromEntries(lib.tests.map(t => [t.id, t])), group: Object.fromEntries(lib.groups.map(g => [g.id, g.name])) } : null;

// techniques for a cause, ranked: what has worked for you with this problem/cause first, then the taxonomy's order
export function rankedFor(lib, problem, cause) {
  const I = libIndex(lib); const P = I && I.prob[problem]; if (!P) return [];
  const C = P.causes.find(c => c.id === cause);
  const ids = C ? C.techniques : [...new Set(P.causes.flatMap(c => c.techniques))];
  const rk = (lib.ranks && (lib.ranks[`${problem}/${cause}`] || lib.ranks[problem])) || {};
  const score = (id, i) => (rk[id] && rk[id].n ? rk[id].score : 0.55) - i * 0.004;
  return ids.map((id, i) => [id, score(id, i)]).sort((a, b) => b[1] - a[1]).map(([id]) => I.tech[id] ? { ...I.tech[id], _rank: rk[id] } : null).filter(Boolean);
}

// the symptoms most often picked, one key each (D, then 1–9); the rest are in the full list
export const QUICK = [["wrongnote", "wrong notes"], ["hitch", "hesitation"], ["uneven", "uneven"], ["speed", "can't reach tempo"], ["leaps", "missed leaps"],
  ["voicing", "melody buried"], ["flat", "phrase flat"], ["slip", "memory slip"], ["tension", "tension"]];

// every technique for a problem, one list: the causes interleaved (so the first few cover different causes),
// what has worked for you first. cause (optional) narrows it to that cause.
export function cardsFor(lib, problem, cause) {
  const I = libIndex(lib); const P = I && I.prob[problem]; if (!P) return [];
  const causes = cause ? P.causes.filter(c => c.id === cause) : P.causes;
  const seen = new Map(); const order = [];
  for (let i = 0; i < 12; i++) for (const c of causes) { const id = c.techniques[i]; if (!id || !I.tech[id]) continue;
    if (seen.has(id)) { seen.get(id).causes.push(c); continue; } const x = { ...I.tech[id], causes: [c], _i: order.length }; seen.set(id, x); order.push(x); }
  for (const c of causes) for (const id of c.techniques) if (seen.has(id) && !seen.get(id).causes.includes(c)) seen.get(id).causes.push(c);
  const rk = (lib.ranks && ((cause && lib.ranks[`${problem}/${cause}`]) || lib.ranks[problem])) || {};
  for (const x of order) { x._rank = rk[x.id]; x._score = (x._rank && x._rank.n ? x._rank.score : 0.55) - x._i * 0.004; }
  return order.sort((a, b) => b._score - a._score);
}

export function DiagnosePanel({ M, p, lib, patch, marks, hist, lastTake, onLogged, Helper, helperProps, navRef }) {
  const I = useMemo(() => libIndex(lib), [lib]);
  const d = p.diag || {}; const problem = d.problem || p.problem || null; const cause = d.cause || null;
  const [fam, setFam] = useState(null); const [ti, setTi] = useState(0);
  const [pred, setPred] = useState(""); const [trying, setTrying] = useState(false);
  useEffect(() => { setTi(0); setTrying(false); setPred(""); }, [p.id, problem, cause]);
  if (!lib || !I) return html`<div class="hint">…</div>`;
  const setDiag = (o) => patch({ diag: { ...d, ...o }, ...("problem" in o ? { problem: o.problem } : {}) });
  const P = problem && I.prob[problem];

  // ---- 1. what do you hear?
  if (!P) {
    const fid = fam || (lib.families[0] && lib.families[0].id);
    const sugg = {}; for (const m of marks || []) { const pr = MARK_PROBLEM[m.kind]; if (pr && I.prob[pr]) sugg[pr] = (sugg[pr] || 0) + 1; }
    return html`<div class="diag">
      <div class="dq">What do you hear? <span class="hint">Pick the symptom, not a technique.</span></div>
      ${Object.keys(sugg).length ? html`<div class="row wrap"><span class="hint">From your marks here:</span>${Object.entries(sugg).map(([pr, n]) => html`<button class="btn small on" onClick=${() => setDiag({ problem: pr, cause: null })}>${I.prob[pr].name} (${n})</button>`)}</div>` : null}
      <div class="fams">${lib.families.map(f => html`<button class=${"chip" + (f.id === fid ? " on" : "")} onClick=${() => setFam(f.id)}>${f.name}</button>`)}</div>
      <div class="plist3">${lib.problems.filter(x => x.family === fid).map(x => html`<button class="prob" onClick=${() => setDiag({ problem: x.id, cause: null })}>
        <b>${x.name}</b><span class="hint">${x.notice}</span></button>`)}</div></div>`;
  }
  const head = html`<div class="dhead"><span class="hint">Problem</span><b class="ell">${P.name}</b><button class="btn small ghost" title="change the problem" onClick=${() => setDiag({ problem: null, cause: null })}>×</button></div>`;
  // ---- 2. which cause? each is pointed to by a quick test
  if (!cause || !P.causes.find(c => c.id === cause)) {
    return html`<div class="diag">${head}
      <div class="dq">Why? <span class="hint">Run the quick test beside a cause if you're not sure.</span></div>
      <div class="plist3">${P.causes.map(c => { const t = I.test[c.test];
        return html`<button class="prob" onClick=${() => setDiag({ cause: c.id })} title=${t ? `Test: ${t.name}\nclean now → ${t.clean}\nstill fails → ${t.fails}` : ""}>
          <b>${c.name}</b>${t ? html`<span class="hint">test: ${t.name}</span>` : null}</button>`; })}
        <button class="prob ghosted" onClick=${() => setDiag({ cause: "_any" })}><b>Not sure yet</b><span class="hint">show every technique for this problem</span></button></div></div>`;
  }
  // ---- 3. the technique, and the experiment
  const C = P.causes.find(c => c.id === cause);
  const cards = rankedFor(lib, problem, cause === "_any" ? null : cause);
  const card = cards.length ? cards[ti % cards.length] : null;
  if (navRef) navRef.current = cards.length > 1 ? (k) => setTi(i => (i + k + cards.length) % cards.length) : null;
  const exp = d.exp && d.exp.tech ? d.exp : null;
  const failedToday = (hist || []).filter(l => l.kind === "try" && l.problem === problem && l.cause === cause && (l.helped ?? 0) <= 0 && Date.now() / 1000 - l.t < 16 * 3600).length;
  const start = () => { setDiag({ exp: { tech: card.id, prediction: pred.trim(), t: Date.now() / 1000 } }); setTrying(false); toast("Now play it with the technique, then record a take (R) and say how it went"); };
  const verdict = async (v) => {
    await api.send("POST", `/api/passages/${p.id}/log`, { kind: "try", technique: exp.tech, problem, cause: cause === "_any" ? null : cause, prediction: exp.prediction, helped: v,
      take_id: lastTake && lastTake.created >= exp.t - 5 ? lastTake.id : null });
    await patch({ diag: { ...d, exp: null } });
    toast(v > 0 ? "Logged: it helped. Check it again cold tomorrow." : v === 0 ? "Logged: no change." : "Logged: worse. That rules something out.");
    onLogged && onLogged();
  };
  return html`<div class="diag">${head}
    <div class="dhead"><span class="hint">Cause</span><b class="ell">${C ? C.name : "not sure yet"}</b><button class="btn small ghost" title="change the cause" onClick=${() => setDiag({ cause: null })}>×</button></div>
    <${Field} class="sentence" placeholder="In one sentence: what exactly goes wrong, where, when?" value=${d.sentence || ""} onSave=${v => setDiag({ sentence: v })} />
    ${failedToday >= 3 ? html`<div class="row"><span class="hint warn grow1">Three experiments here haven't helped today. Park it and let it settle overnight (incubation).</span>
      ${d.park_t && Date.now() / 1000 - d.park_t < 16 * 3600 ? html`<span class="hint">parked: first tomorrow</span>` : html`<button class="btn small" onClick=${() => { setDiag({ park_t: Date.now() / 1000 }); toast("Parked: it will be first in tomorrow's plan"); }}>Park it</button>`}</div>` : null}
    ${card ? html`<div class="tech">
      <div class="row"><b class="grow1 ell" title=${card.name}>${card.name}</b>
        <span class=${"grade g" + card.grade} title=${GRADE[card.grade]}>${card.grade}</span>
        ${cards.length > 1 ? html`<button class="btn small" onClick=${() => setTi(i => (i - 1 + cards.length) % cards.length)}>‹ <kbd>[</kbd></button><span class="muted">${(ti % cards.length) + 1}/${cards.length}</span><button class="btn small" onClick=${() => setTi(i => (i + 1) % cards.length)}><kbd>]</kbd> ›</button>` : null}</div>
      <div class="how">${card.how}</div>
      <div class="listen"><b>Listen for:</b> ${card.listen}</div>
      <div class="hint">${card.yours ? "Yours · " : ""}${I.group[card.group] || ""} · ${card.src ? html`<a href=${card.src} target="_blank" rel="noopener">${card.origin}</a>` : card.origin}${card._rank && card._rank.n ? ` · helped ${Math.round(card._rank.score * 100)}% of ${card._rank.n} time${card._rank.n > 1 ? "s" : ""} for you` : ""}</div>
      ${Helper ? html`<${Helper} kind=${card.helper} tech=${card} ...${helperProps} />` : null}
    </div>` : html`<div class="hint">No techniques listed for this cause.</div>`}
    ${exp ? html`<div class="exp"><div><b>Experiment:</b> ${I.tech[exp.tech] ? I.tech[exp.tech].name : exp.tech}${exp.prediction ? html` · <span class="muted">predicted: ${exp.prediction}</span>` : null}</div>
        <div class="row"><span class="hint grow1">After a take${lastTake && lastTake.created >= exp.t - 5 ? " (recorded)" : " (R to record one)"}: compared with before, it was…</span>
          <button class="btn small" onClick=${() => verdict(1)}>better</button><button class="btn small" onClick=${() => verdict(0)}>same</button><button class="btn small" onClick=${() => verdict(-1)}>worse</button>
          <button class="btn small ghost" title="stop without logging" onClick=${() => patch({ diag: { ...d, exp: null } })}>×</button></div></div>`
      : card ? (trying ? html`<div class="exp row"><input class="grow1" ref=${el => el && !el.dataset.f && (el.dataset.f = 1, el.focus())} placeholder="I predict… (what will change on the next take)" value=${pred} onInput=${e => setPred(e.target.value)} onKeyDown=${e => { if (e.key === "Enter") start(); e.stopPropagation(); }} />
          <button class="btn small primary" onClick=${start}>Start</button><button class="btn small ghost" onClick=${() => setTrying(false)}>×</button></div>`
        : html`<div class="row"><button class="btn small primary" onClick=${() => setTrying(true)} title="try it as an experiment: predict, play, record, compare">Try it as an experiment</button><span class="hint">one change at a time; judge it on the next take</span></div>`) : null}
  </div>`;
}

// ------------------------------------------------------------------ the library, to browse
export function LibraryDialog({ lib, onClose }) {
  const I = useMemo(() => libIndex(lib), [lib]);
  const [q, setQ] = useState(""); const [view, setView] = useState(store.get("libView", "problems")); const [open, setOpen] = useState(null);
  useEffect(() => store.set("libView", view), [view]);
  if (!lib) return null;
  const match = t => !q || (t.name + " " + t.how + " " + t.origin).toLowerCase().includes(q.toLowerCase());
  const TechLine = ({ t }) => html`<div class=${"lt" + (open === t.id ? " open" : "")} onClick=${() => setOpen(open === t.id ? null : t.id)}>
    <div class="row"><b class="grow1">${t.name}${t.yours ? html` <span class="pill">yours</span>` : null}</b><span class=${"grade g" + t.grade} title=${GRADE[t.grade]}>${t.grade}</span></div>
    ${open === t.id ? html`<div class="how">${t.how}</div><div class="listen"><b>Listen for:</b> ${t.listen}</div>
      <div class="hint">${I.group[t.group]} · ${t.src ? html`<a href=${t.src} target="_blank" rel="noopener" onClick=${e => e.stopPropagation()}>${t.origin}</a>` : t.origin}</div>
      <div class="hint">For: ${(t.problems || []).map(pid => I.prob[pid] ? I.prob[pid].name : pid).join(" · ")}</div>` : null}</div>`;
  return html`<div class="scrim" onClick=${e => e.target === e.currentTarget && onClose()}><div class="dialog lib">
    <div class="row"><h3 class="grow1" style="margin:0">Practice library <span class="muted" style="font-weight:400">${lib.techniques.length} techniques</span></h3>
      <div class="seg mini"><button class=${view === "problems" ? "on" : ""} onClick=${() => setView("problems")}>By problem</button><button class=${view === "groups" ? "on" : ""} onClick=${() => setView("groups")}>By kind</button></div></div>
    <input class="libq" placeholder="Search…" value=${q} onInput=${e => setQ(e.target.value)} onKeyDown=${e => { if (e.key === "Escape") onClose(); e.stopPropagation(); }} />
    <div class="liblist">${view === "groups"
      ? lib.groups.map(g => { const ts = lib.techniques.filter(t => t.group === g.id && match(t)); return ts.length ? html`<div class="lg"><div class="lgh">${g.name}</div>${ts.map(t => html`<${TechLine} t=${t} />`)}</div>` : null; })
      : lib.families.map(f => html`<div class="lg"><div class="lgh">${f.name}</div>${lib.problems.filter(p => p.family === f.id).map(p => {
          const causes = p.causes.map(c => ({ c, ts: c.techniques.map(id => I.tech[id]).filter(t => t && match(t)) })).filter(x => x.ts.length);
          return causes.length ? html`<div class="lp"><b>${p.name}</b> <span class="hint">${p.notice}</span>${causes.map(({ c, ts }) => html`<div class="lc"><span class="muted">${c.name}${I.test[c.test] ? ` (test: ${I.test[c.test].name})` : ""}</span>${ts.map(t => html`<${TechLine} t=${t} />`)}</div>`)}</div>` : null; })}</div>`)}</div>
    <div class="actions"><span class="hint" style="margin-right:auto">Grades: E study · M research transferred · C teachers' consensus · A one author. Edit or add in data\\techniques.json.</span><button class="btn" onClick=${onClose}>Close</button></div></div></div>`;
}
