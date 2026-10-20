// The guide (left column): what you are practising right now (the selected bars), what it should sound like,
// what's wrong with it (one key), and the technique that fits, with its helper. No forms to fill in: a spot is
// saved by itself as soon as you record it, write its intent or say what's wrong; time on it is logged by itself.
import { useState, useEffect, useMemo } from "./vendor/hooks.js";
import { html, api, fmtMin, store, toast, rangeLabel, Field } from "./lib.js";
import { libIndex, cardsFor, QUICK, GRADE, MARK_PROBLEM } from "./diagnose.js";
import { Helper, MetroCard, STAGES, STATE_NAME, stageOf } from "./practice.js";

export function GuideColumn({ M, sel, spot, inside, lib, patchSpot, ensureSpot, marks, next, goNext, startTest, spotSecs, setTech, navRef, symOpen, setSymOpen, helperProps, lastTake, judge, playTake }) {
  const I = useMemo(() => libIndex(lib), [lib]);
  const d = (spot && spot.diag) || {};
  const problem = d.problem || (spot && spot.problem) || null;
  const [cause, setCause] = useState(null); const [ti, setTi] = useState(0);
  useEffect(() => { setCause(null); setTi(0); }, [spot && spot.id, problem]);
  const cards = useMemo(() => lib && problem ? cardsFor(lib, problem, cause) : [], [lib, problem, cause]);
  const card = cards.length ? cards[ti % cards.length] : null;
  useEffect(() => { setTech(card ? card.id : null); }, [card && card.id]);
  if (navRef) navRef.current = cards.length > 1 ? (k) => setTi(i => (i + k + cards.length) % cards.length) : null;
  const pick = (pid) => ensureSpot().then(sp => sp && patchSpot(sp, { problem: pid, diag: { ...((sp && sp.diag) || {}), problem: pid } }));
  // D, then 1–9 (0: the full list)
  useEffect(() => {
    if (!symOpen) return;
    const key = e => { e.preventDefault(); e.stopPropagation(); setSymOpen(false);
      const i = "123456789".indexOf(e.key);
      if (i >= 0) pick(QUICK[i][0]); else if (e.key === "0") { const s = document.querySelector(".symsel"); s && s.focus(); } };
    window.addEventListener("keydown", key, true); const t = setTimeout(() => setSymOpen(false), 4000);
    return () => { window.removeEventListener("keydown", key, true); clearTimeout(t); };
  }, [symOpen, spot && spot.id, sel && sel.a, sel && sel.b]);

  const nextBtn = next ? html`<button class="btn small" title=${`next in today's plan: ${next.name || next.label}${next.kind === "test" ? " (cold take)" : ""}`} onClick=${() => goNext(next)}>Next: ${next.name || next.label}${next.kind === "test" ? " · cold" : ""} <kbd>N</kbd></button>` : null;
  if (!sel || !M.byId[sel.a]) return html`
    <div class="card focus"><h2>Practising</h2>
      <div class="hint">Click a bar and Shift+click the last one: that is what you're practising. Everything here follows it: what it should sound like, what's wrong, the technique, your takes of it. Nothing needs saving.</div>
      <div class="row" style="margin-top:6px">${nextBtn || html`<span class="hint">Nothing due in today's plan.</span>`}</div></div>
    <div class="card grow guide"><div class="hint">Keys: <kbd>R</kbd> record these bars · <kbd>⇧R</kbd> hear the take · <kbd>,</kbd> <kbd>.</kbd> your older/newer takes · <kbd>V</kbd> take vs performer · <kbd>D</kbd> what's wrong · <kbd>[</kbd> <kbd>]</kbd> techniques · <kbd>T</kbd> cold take · <kbd>N</kbd> next spot</div></div>
    <${MetroCard} active=${null} />`;

  const st = spot ? stageOf(spot) : null;
  const setStage = async (s) => {
    const sp = await ensureSpot(); if (!sp) return;
    const late = s === "securing" || s === "performing";
    if (late && sp.state === "working") { await api.send("POST", `/api/passages/${sp.id}/log`, { kind: "graduate" }); toast("It comes back as a cold take from tomorrow"); }
    else if (!late && sp.state === "maintenance") await api.send("POST", `/api/passages/${sp.id}/log`, { kind: "unlearn" });
    patchSpot(sp, { stage: s });
  };
  const fromMarks = {}; for (const m of marks || []) { const pr = MARK_PROBLEM[m.kind]; if (pr && I && I.prob[pr]) fromMarks[pr] = (fromMarks[pr] || 0) + 1; }
  const quickIds = QUICK.map(q => q[0]);
  return html`
    <div class="card focus">
      <div class="row"><b class="flabel">${rangeLabel(M, sel.a, sel.b)}</b>
        ${spot ? html`<${Field} class="fname grow1" value=${spot.name || ""} placeholder="name this spot…" onSave=${v => patchSpot(spot, { name: v })} />`
          : html`<span class="hint grow1 ell" title="saved as a spot when you record it, write its intent or say what's wrong">${inside ? `inside “${inside.name || rangeLabel(M, inside.a, inside.b)}”` : "new spot"}</span>`}
        <span class="muted timer" title="time on this spot now (logged by itself)">${fmtMin(spotSecs)}</span></div>
      ${spot && (spot.a !== sel.a || spot.b !== sel.b) ? html`<div class="row hint">spot is ${rangeLabel(M, spot.a, spot.b)} <button class="btn small" title="make the spot these bars" onClick=${() => patchSpot(spot, { a: sel.a, b: sel.b })}>→ ${rangeLabel(M, sel.a, sel.b)}</button></div>` : null}
      <div class="row">
        <select class=${"stage-sel st-" + (st ? st.id : "learning")} value=${st ? st.id : "learning"} title=${st ? `${st.q} (${STATE_NAME[spot.state]})` : "stage"} onChange=${e => setStage(e.target.value)}>${STAGES.map(s => html`<option value=${s.id}>${s.name}</option>`)}</select>
        <span class="spacer"></span>
        <button class="btn small" title="one recorded run, cold, then rate it" onClick=${async () => { const sp = await ensureSpot(); if (sp) startTest(sp); }}>Cold take <kbd>T</kbd></button>
        ${nextBtn}</div>
      <${Field} multiline class="intent" rows="2" placeholder="What should it sound like? (shape, voicing, character, breath)" value=${(spot && spot.intent) || ""}
        onSave=${v => ensureSpot().then(sp => sp && patchSpot(sp, { intent: v }))} />
    </div>
    <div class="card grow guide">
      <h2>What's wrong? <span class="r hint" style="text-transform:none;letter-spacing:0">${symOpen ? html`<b class="warn">press 1–9 (0: all)</b>` : html`<kbd>D</kbd> then 1–9`}</span></h2>
      <div class="syms">${QUICK.map(([id, n], i) => html`<button class=${"chip" + (problem === id ? " on" : "") + (fromMarks[id] ? " marked" : "")} title=${fromMarks[id] ? `${fromMarks[id]} of your marks here` : ""} onClick=${() => pick(problem === id ? null : id)}><kbd>${i + 1}</kbd> ${n}</button>`)}
        <select class=${"mini symsel" + (problem && !quickIds.includes(problem) ? " on" : "")} value=${problem && !quickIds.includes(problem) ? problem : ""} onChange=${e => { pick(e.target.value || null); e.target.blur(); }}>
          <option value="">${problem && !quickIds.includes(problem) ? "" : "0 · more…"}</option>
          ${lib ? lib.families.map(f => html`<optgroup label=${f.name}>${lib.problems.filter(p => p.family === f.id).map(p => html`<option value=${p.id}>${p.name}</option>`)}</optgroup>`) : null}</select></div>
      ${Object.keys(fromMarks).filter(pr => !quickIds.includes(pr)).map(pr => html`<button class="chip marked" onClick=${() => pick(pr)}>${I.prob[pr].name} (${fromMarks[pr]} marks)</button>`)}
      ${card ? html`<div class="tech">
          <div class="row"><b class="grow1 tname" title=${card.name}>${card.name}${card.yours ? html` <span class="pill">yours</span>` : null}</b>
            <span class=${"grade g" + card.grade} title=${GRADE[card.grade]}>${card.grade}</span>
            ${cards.length > 1 ? html`<button class="btn small" onClick=${() => setTi(i => (i - 1 + cards.length) % cards.length)}><kbd>[</kbd></button><span class="muted">${(ti % cards.length) + 1}/${cards.length}</span><button class="btn small" onClick=${() => setTi(i => (i + 1) % cards.length)}><kbd>]</kbd></button>` : null}</div>
          <div class="causes"><span class="hint">if:</span>${cause ? html`<button class="chip on" title="show techniques for every cause" onClick=${() => setCause(null)}>${I.prob[problem].causes.find(c => c.id === cause).name} ×</button>`
            : card.causes.slice(0, 2).map(c => html`<button class="chip" title=${I.test[c.test] ? `check: ${I.test[c.test].name}\nclean then → ${I.test[c.test].clean}\nstill fails → ${I.test[c.test].fails}\n(click: only this cause)` : "only this cause"} onClick=${() => setCause(c.id)}>${c.name}</button>`)}</div>
          <div class="how">${card.how}</div>
          <div class="listen"><b>Listen for:</b> ${card.listen}</div>
          ${lastTake && lastTake.technique === card.id ? html`<div class="verdict row"><button class="btn small" title="play that take here (Shift+R right after recording)" onClick=${() => playTake(lastTake)}>▶</button>
            <span class="grow1">Your take with this: better than before?</span>
            ${[[1, "better", "↑"], [0, "same", "="], [-1, "worse", "↓"]].map(([h, n, s]) => html`<button class="btn small" title=${n} onClick=${() => judge(lastTake, h)}>${s}</button>`)}</div>` : null}
          <div class="hint">${I.group[card.group] || ""} · ${card.src ? html`<a href=${card.src} target="_blank" rel="noopener">${card.origin}</a>` : card.origin}${card._rank && card._rank.n ? ` · helped ${Math.round(card._rank.score * 100)}% of ${card._rank.n}× for you` : ""}</div>
          ${card.helper ? html`<${Helper} kind=${card.helper} tech=${card} ...${helperProps} />` : null}
        </div>`
        : html`<div class="hint">${problem ? "No techniques listed for this." : "Just practise: record (R), listen back (⇧R), compare (V). When something is wrong, say what (D): the technique that fits shows here, and your next takes are tagged with it."}</div>`}
    </div>
    <${MetroCard} active=${spot} />`;
}
