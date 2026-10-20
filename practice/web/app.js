import { h, render } from "./vendor/preact.js";
import { useState, useEffect, useRef, useMemo, useCallback, useReducer } from "./vendor/hooks.js";
import { html, api, fmt, ord, store, player, toast, usePlayerTick, romanOr, rangeLabel, Field } from "./lib.js";
import { PassageOverlay, HideOverlay, TestDialog, Today, PieceList, NotesTab, passageBars, metro } from "./practice.js";
import { GuideColumn } from "./guide.js";
import { LibraryDialog, useLib } from "./diagnose.js";
import { MarkChooser, CueDialog, CueMarks } from "./marks.js";
import { TimingLane } from "./timing.js";
import { SpotCheck } from "./spotcheck.js";
import { RecordButton, toggleRecording, recorder, dropFlag, FlagsCard, FlagMarks, playLast } from "./record.js";
import { SectionTakes, TakesPanel, isSectionTake, takesOf, playSpan, spanTimes, pingPong, aThenB } from "./takes.js";
const MCOLS = ["var(--m0)", "var(--m1)", "var(--m2)", "var(--m3)", "var(--m4)", "var(--m5)"];

// ------------------------------------------------------------------ derived piece model
function derive(data) {
  if (!data || !data.score) return null;
  const bars = data.score.bars, byId = {}, order = {}, pageBars = {};
  const mvOf = data.movement_of || {};
  const deleted = new Set((data.structure && data.structure.deleted) || []);
  let k = 0;
  for (const b of bars) { byId[b.id] = b; (pageBars[b.page] = pageBars[b.page] || []).push(b); if (!deleted.has(b.id)) order[b.id] = k++; }
  const seq = bars.filter(b => !deleted.has(b.id)).map(b => b.id);
  const labels = data.labels || {};
  const mvNames = ((data.structure && data.structure.movements) || []).map((m, i) => m.name || `Movement ${i + 1}`);
  const label = id => { const m = mvOf[id]; const n = labels[id] ?? "?"; return mvNames.length > 1 && m != null ? `${romanOr(mvNames[m], m)} · ${n}` : `m. ${n}`; };
  return { bars, byId, order, seq, pageBars, labels, label, mvOf, mvNames, deleted, npages: data.score.pages.length, pages: data.score.pages };
}

// ------------------------------------------------------------------ app
// One practice screen. The selected bars are what you are working on ("the focus"): the guide on the left says what
// it should sound like, what's wrong and what to try; the score is in the middle; on the right are the recordings
// (click a bar or a recording to hear it), your takes of these bars and ways to compare them. A spot (a saved
// passage) is made by itself as soon as you record the bars, write their intent or say what's wrong, and time on it
// is logged by itself. Structure (Tab) is for fixing the score's bars and repeats.
const IDLE_MS = 8 * 60 * 1000;                       // no input for this long (and nothing playing or recording): not practising
function App() {
  const [pieces, setPieces] = useState([]);
  const [pid, setPid] = useState(store.get("pid", null));
  const [data, setData] = useState(null);
  const [mode, setMode] = useState("practice");       // practice | structure
  const [page, setPage] = useState(1);
  const [sel, setSel] = useState(null);          // {a, b} bar ids (notated order), a <= b
  const [cursor, setCursor] = useState(null);    // bar id (for stepping when not playing)
  const [dialog, setDialog] = useState(null);    // 'palette' | 'import' | 'addrec' | 'help' | ...
  const [jobsList, setJobs] = useState([]);
  const [editSystem, setEditSystem] = useState(null);
  const [passages, setPassages] = useState([]);
  const [hide, setHide] = useState(null);          // {a, b}: bars covered on the score
  const [testing, setTesting] = useState(null);    // spot being cold-tested
  const [pver, setPver] = useState(0);             // bump to refresh the technique ranks
  const [flags, setFlags] = useState([]);
  const [cues, setCues] = useState([]);
  const [markFlag, setMarkFlag] = useState(null);  // a mark just dropped: waiting for its type
  const [cueEdit, setCueEdit] = useState(null);    // {bar} (new cue) or {cue}
  const [addLine, setAddLine] = useState(false);   // structure: drawing a box around a missed line
  const [plan, setPlan] = useState(null);
  const [guideOn, setGuideOn] = useState(store.get("guideOn", true)); useEffect(() => store.set("guideOn", guideOn), [guideOn]);
  const [tech, setTech] = useState(null);          // the technique card showing in the guide
  const [symOpen, setSymOpen] = useState(false);   // D pressed: waiting for 1–9
  const [hist, setHist] = useState([]);
  const [, tick] = useState(0);
  const lib = useLib(pver);
  const M = useMemo(() => derive(data), [data]);
  usePlayerTick();

  const loadPieces = useCallback(() => api.get("/api/pieces").then(ps => { setPieces(ps); if (pid == null && ps.length) setPid(ps[0].id); return ps; }), [pid]);
  const loadPiece = useCallback(() => { if (pid == null) return; api.get(`/api/pieces/${pid}`).then(d => { if (d.error) { setPid(null); return; } setData(d); }); }, [pid]);
  useEffect(() => { loadPieces(); }, []);
  const loadPassages = useCallback(() => { if (pid == null) return Promise.resolve(); return api.get(`/api/pieces/${pid}/passages`).then(ps => { if (Array.isArray(ps)) setPassages(ps); }); }, [pid]);
  const loadFlags = useCallback(() => { if (pid == null) return; api.get(`/api/pieces/${pid}/flags`).then(fs => { if (Array.isArray(fs)) setFlags(fs); }); }, [pid]);
  const loadCues = useCallback(() => { if (pid == null) return; api.get(`/api/pieces/${pid}/cues`).then(cs => { if (Array.isArray(cs)) setCues(cs); }); }, [pid]);
  const loadPlan = useCallback((b) => api.get("/api/practice/today" + (b ? `?budget=${b}` : "")).then(x => { if (x && x.items) setPlan(x); }), []);
  useEffect(() => { store.set("pid", pid); setData(null); setSel(null); setCursor(null); setPage(1); setHide(null); loadPiece(); setPassages([]); loadPassages(); setFlags([]); loadFlags(); setCues([]); loadCues(); loadPlan(); }, [pid]);
  useEffect(() => { if (M && data) player.setPiece(data.recordings, M.order); }, [data]);

  // job polling: refresh the piece when something relevant finishes
  const lastJobs = useRef("");
  useEffect(() => {
    let alive = true;
    const poll = async () => {
      if (!alive) return;
      try {
        const js = await api.get("/api/jobs"); setJobs(js);
        const sig = JSON.stringify(js.filter(j => j.piece_id === pid).map(j => [j.id, j.status]));
        if (sig !== lastJobs.current) { const had = lastJobs.current; lastJobs.current = sig; if (had) { loadPiece(); loadPieces(); } }
      } catch (e) {}
      setTimeout(poll, 1500);
    };
    poll(); return () => { alive = false; };
  }, [pid]);

  // follow playback: current bar -> page
  const p = player.pos();
  const curBar = p && p.k >= 0 ? p.bar : null;
  const pageShows = (id) => { const pg = M.byId[id].page; const two = document.querySelectorAll(".page").length > 1; return two ? (pg === page || pg === page + 1) : pg === page; };
  useEffect(() => { if (!(curBar && M && player.playing() && M.byId[curBar])) return; if (!pageShows(curBar)) setPage(M.byId[curBar].page); }, [curBar]);

  // ---------------- the focus: the selected bars, and the spot (saved passage) they are
  const { spot, inside } = useMemo(() => {
    if (!M || !sel || M.order[sel.a] == null) return { spot: null, inside: null };
    const a0 = M.order[sel.a], b0 = M.order[sel.b]; let best = null, bj = 0, ins = null, insLen = 1e9;
    for (const x of passages) {
      const pa = M.order[x.a], pb = M.order[x.b]; if (pa == null || pb == null) continue;
      const inter = Math.max(0, Math.min(b0, pb) - Math.max(a0, pa) + 1), uni = Math.max(b0, pb) - Math.min(a0, pa) + 1;
      if (inter / uni > bj) { bj = inter / uni; best = x; }
      if (pa <= a0 && pb >= b0 && pb - pa < insLen) { ins = x; insLen = pb - pa; }
    }
    return bj >= 0.6 ? { spot: best, inside: null } : { spot: null, inside: ins };
  }, [M, sel && sel.a, sel && sel.b, passages]);
  const multi = sel && M && M.byId[sel.a] && sel.a !== sel.b;
  const making = useRef(null);
  // the spot for the selected bars, made now if there isn't one yet
  const ensureSpot = () => {
    if (spot) return Promise.resolve(spot);
    if (!sel || !M || !M.byId[sel.a]) { toast("Select bars first: click a bar, then Shift+click the last one"); return Promise.resolve(null); }
    const key = sel.a + ":" + sel.b;
    if (making.current && making.current.key === key) return making.current.pr;
    const pr = api.send("POST", `/api/pieces/${pid}/passages`, { a: sel.a, b: sel.b }).then(x => {
      if (x.error) { toast(x.error); return null; }
      setPassages(ps => [...ps, x]); loadPlan(); return x; });
    making.current = { key, pr }; pr.then(() => { making.current = null; });
    return pr;
  };
  // edits show at once, then the server's view replaces them
  const patchSpot = async (sp, body) => {
    if (!sp) return;
    setPassages(ps => ps.map(x => x.id === sp.id ? { ...x, ...body } : x));
    const v = await api.send("PATCH", `/api/passages/${sp.id}`, body);
    if (v && v.id) setPassages(ps => ps.map(x => x.id === v.id ? v : x));
    if ("stage" in body || "a" in body || "b" in body || "problem" in body) loadPlan();
    return v;
  };
  const loadHist = () => spot ? api.get(`/api/passages/${spot.id}/history`).then(h => Array.isArray(h) && setHist(h)) : setHist([]);
  useEffect(() => { loadHist(); }, [spot && spot.id, spot && spot.last_worked, spot && spot.last_tested, spot && spot.state]);

  // the metronome remembers the tempo used for each spot (a setting, not a goal)
  useEffect(() => { if (spot && spot.bpm) metro.set({ bpm: spot.bpm }); }, [spot && spot.id]);
  const bpmTimer = useRef(null);
  useEffect(() => metro.onChange(() => {
    if (!spot) return; clearTimeout(bpmTimer.current);
    bpmTimer.current = setTimeout(() => { if (metro.bpm !== spot.bpm) patchSpot(spot, { bpm: metro.bpm }); }, 1500);
  }), [spot && spot.id, spot && spot.bpm]);

  // ---------------- time on the spot, logged by itself: while the window is open on it and you are at it
  const lastInput = useRef(Date.now());
  useEffect(() => { const f = () => { lastInput.current = Date.now(); };
    const evs = ["keydown", "mousedown", "wheel"]; evs.forEach(ev => window.addEventListener(ev, f, true));
    return () => evs.forEach(ev => window.removeEventListener(ev, f, true)); }, []);
  const acc = useRef({ id: null, secs: 0, shown: 0 });
  const techRef = useRef(null); techRef.current = tech;
  const passRef = useRef([]); passRef.current = passages;
  const flush = () => {
    const a = acc.current; const secs = a.secs; a.secs = 0;
    if (!a.id || secs < 45) return;
    const sp = passRef.current.find(x => x.id === a.id);
    const body = { kind: "work", minutes: Math.round(secs / 6) / 10, technique: techRef.current, problem: sp ? ((sp.diag && sp.diag.problem) || sp.problem) : null };
    fetch(`/api/passages/${a.id}/log`, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body), keepalive: true })
      .then(() => { loadPlan(); }).catch(() => {});
  };
  useEffect(() => { flush(); acc.current = { id: spot ? spot.id : null, secs: 0, shown: 0 }; }, [spot && spot.id]);
  useEffect(() => {
    const iv = setInterval(() => {
      const a = acc.current;
      if (a.id && document.visibilityState === "visible" && (Date.now() - lastInput.current < IDLE_MS || recorder.on || metro.on || player.playing())) {
        a.secs++; a.shown++; if (a.secs >= 300) flush(); }
      tick(x => x + 1);
    }, 1000);
    const bye = () => flush(); window.addEventListener("pagehide", bye);
    return () => { clearInterval(iv); window.removeEventListener("pagehide", bye); };
  }, []);

  // ---------------- selection
  const selectBar = (id, extend) => {
    if (!M) return;
    if (extend && sel) { const o = M.order[id]; const anchor = sel.anchor || sel.a; const oan = M.order[anchor];
      setSel(o < oan ? { a: id, b: anchor, anchor } : { a: anchor, b: id, anchor }); }
    else setSel({ a: id, b: id, anchor: id });
    setCursor(id);
  };
  const selectRange = (a, b) => { if (!M || !M.byId[a]) return; setSel({ a, b, anchor: a }); setCursor(a); if (!pageShows(a)) setPage(M.byId[a].page); };
  const inSel = id => sel && M.order[id] != null && M.order[id] >= M.order[sel.a] && M.order[id] <= M.order[sel.b];
  const seek = (id, pass, play) => { if (!player.rec) return; if (!player.gotoBar(id, pass, play)) toast(`${player.rec.name} doesn't play ${M.label(id)}`); };
  // a click selects the bar and moves playback there (still playing if it was); inside a selection of several bars
  // it only moves playback, so the focus stays; Shift+click extends; double-click plays from the bar
  const clickBar = (id, e) => {
    if (mode === "structure") { selectBar(id, e.shiftKey); return; }
    if (e.shiftKey && sel) { selectBar(id, true); return; }
    if (!(multi && inSel(id))) selectBar(id, false); else setCursor(id);
    seek(id, e.altKey ? 2 : 1, player.playing());
  };
  const dblBar = (id) => { if (mode !== "structure") seek(id, 1, true); };

  // ---------------- plan
  const pending = useRef(null);
  const goTo = (it) => { if (it.piece_id !== pid) { pending.current = it; setPid(it.piece_id); return; } selectRange(it.a, it.b); if (M && M.byId[it.a]) setPage(M.byId[it.a].page); };
  useEffect(() => { if (M && pending.current && pending.current.piece_id === pid) { const it = pending.current; pending.current = null; selectRange(it.a, it.b); setPage(M.byId[it.a] ? M.byId[it.a].page : 1); } }, [M]);
  const startTest = (sp) => { setMode("practice"); setTesting(sp); };
  const goNext = (it) => { goTo(it); if (it.kind === "test") setTimeout(() => startTest(it), 80); };
  const next = plan ? plan.items.find(it => it.in_budget && (!spot || it.id !== spot.id)) : null;

  // ---------------- takes of the focus
  const here = useMemo(() => M && data && sel && M.byId[sel.a] ? takesOf(M, data, spot || { id: -1, a: sel.a, b: sel.b }) : { mine: [], refs: [] }, [M, data, sel && sel.a, sel && sel.b, spot && spot.id]);
  const takeIdx = useRef(-1);
  useEffect(() => { takeIdx.current = -1; }, [sel && sel.a, sel && sel.b]);
  const playTake = (r) => { if (!sel) return; playSpan(M, r, r.sec_a && !multi ? r.sec_a : sel.a, r.sec_a && !multi ? (r.sec_b || r.sec_a) : sel.b); };
  const stepTake = (d) => {        // , and . : older / newer take of these bars
    const list = here.mine.filter(r => r.audio); if (!list.length) { toast(sel ? "No takes of these bars yet: R records one" : "Select bars first"); return; }
    takeIdx.current = Math.max(0, Math.min(list.length - 1, takeIdx.current + d));
    const r = list[takeIdx.current]; playTake(r);
    toast(`${takeIdx.current === 0 ? "Newest take" : `${takeIdx.current + 1} takes back`}: ${r.name.replace(/^Take /, "")}`);
  };
  const refFor = () => { const cur = data.recordings.find(r => r.id === player.act);
    return cur && !isSectionTake(cur) && here.refs.includes(cur) ? cur : here.refs[0]; };
  const compare = (seq) => {       // V: your newest take vs a performer, bar by bar (Shift+V: all of A, then all of B)
    if (!sel) { toast("Select bars first"); return; }
    const A = here.mine.find(r => r.audio && spanTimes(M, r, sel.a, sel.b)); const B = refFor();
    if (!A && !B) { toast("Nothing plays these bars yet"); return; }
    if (!A) { toast("No take of these bars lined up yet: R records one"); return; }
    if (!B) { toast("No other recording plays these bars"); return; }
    (seq ? aThenB : pingPong)(M, A, B, sel.a, sel.b); toast(`${seq ? "A then B" : "Bar by bar"}: your take, then ${B.name}`);
  };
  const lastTake = here.mine[0] && here.mine[0].technique && Date.now() / 1000 - here.mine[0].created < 3600 && !store.get("judged", []).includes(here.mine[0].id) ? here.mine[0] : null;
  const judge = async (r, helped) => {
    if (!spot) return; store.set("judged", [...store.get("judged", []).slice(-200), r.id]);
    await api.send("POST", `/api/passages/${spot.id}/log`, { kind: "try", technique: r.technique, problem: (spot.diag && spot.diag.problem) || spot.problem, cause: spot.diag && spot.diag.cause, helped, take_id: r.id });
    setPver(v => v + 1); loadHist(); toast(helped > 0 ? "Noted: it helped. It ranks higher next time" : helped < 0 ? "Noted: worse. Try the next card ( ] )" : "Noted: no change"); tick(x => x + 1);
  };

  // ---------------- recording: the spot, or the selected bars (which become a spot), else the whole piece
  const recContext = !M || mode === "structure" ? "" : spot ? (spot.name || rangeLabel(M, spot.a, spot.b)) : multi ? rangeLabel(M, sel.a, sel.b) : "";
  const record = async () => {
    if (recorder.on) { const r = await toggleRecording(pid); if (r && r.id) loadPiece(); return; }
    let opts = null, ctx = "";
    if (spot || multi) {
      const sp = await ensureSpot(); if (!sp) return;
      ctx = sp.name || rangeLabel(M, sp.a, sp.b);
      opts = { sec_a: sp.a, sec_b: sp.b, passage_id: sp.id, label: "warm", technique: tech, piano: store.get("piano", "") };
    }
    toggleRecording(pid, ctx, opts);
  };

  // tap A exactly on a downbeat while listening: that bar starts here (a hard anchor), and the recording is re-aligned
  const anchorHere = async () => {
    const r = player.rec; if (!r || !r.occ.length) { toast("Play an aligned recording, then tap Shift+A on a downbeat"); return; }
    const t = Math.max(0, player.time() - 0.05);
    let best = -1, bd = 1.5; r.occ.forEach((o, k) => { const d = Math.abs(o[2] - t); if (d < bd) { bd = d; best = k; } });
    if (best < 0) { toast("No bar starts near here"); return; }
    const o = r.occ[best];
    await api.send("POST", `/api/recordings/${r.id}/anchors`, { bar: o[0], pass: o[1], t });
    toast(`${M.label(o[0])} starts at ${fmt(t)} in ${r.name} (was ${fmt(o[2])}): re-aligning`);
  };
  const setLoop = (on) => { if (on && sel) { player.loop = { a: sel.a, b: sel.b }; player.gotoBar(sel.a, 1, true); } else player.loop = null; player.emit("state"); };
  // a recording picked (click or number key): same spot if playing; paused with bars selected: those bars in it
  const pickRec = (r) => {
    if (!r || !r.audio) return;
    if (!player.playing() && multi && spanTimes(M, r, sel.a, sel.b) && !player.loop) { playSpan(M, r, sel.a, sel.b); return; }
    const res = player.select(r.id); if (!res.ok) toast(`${r.name} doesn't play ${res.bar ? M.label(res.bar) : "this bar"}`);
  };
  const nav = useRef(null);
  const nameRef = () => { const f = document.querySelector(".fname"); if (f) f.focus(); };

  // ---------------- keyboard
  useEffect(() => {
    const onKey = (e) => {
      if (e.target && (e.target.tagName === "INPUT" || e.target.tagName === "SELECT" || e.target.tagName === "TEXTAREA")) return;
      const k = e.key;
      if ((e.ctrlKey || e.metaKey) && k.toLowerCase() === "k") { e.preventDefault(); setDialog("palette"); return; }
      if (e.ctrlKey || e.metaKey || e.altKey) return;
      if (dialog) { if (k === "Escape") setDialog(null); return; }
      if (testing || cueEdit || markFlag) { if (k === "Escape") { setTesting(null); setCueEdit(null); } return; }
      if (k === "?") { setDialog("help"); return; }
      if (k === "Tab") { e.preventDefault(); setMode(m => m === "structure" ? "practice" : "structure"); return; }
      if (!M) return;
      if (k === "PageDown" || k === "j" || k === "J") { setPage(x => Math.min(M.npages, x + 1)); return; }
      if (k === "PageUp" || k === "k" || k === "K") { setPage(x => Math.max(1, x - 1)); return; }
      if (k === "Escape") { setSel(null); setEditSystem(null); setHide(null); setAddLine(false); if (player.loop) setLoop(false); player.stopSegments(); return; }
      if (mode === "practice") {
        const lk = k.length === 1 ? k.toLowerCase() : k;
        if (k === "R" && e.shiftKey) { if (recorder.last) playLast(); else if (here.mine[0]) playTake(here.mine[0]); else toast("No take recorded yet"); return; }
        if (lk === "r") { record(); return; }
        if (lk === "f") { dropFlag().then(f => { if (f && f.id) { loadFlags(); setMarkFlag(f); } }); return; }
        if (lk === "q") { const b = (sel && sel.a) || cursor || curBar; if (!b) { toast("Select a bar first, then Q for a performance cue"); return; } setCueEdit({ bar: b }); return; }
        if (lk === "m") { metro.toggle(); return; }
        if (lk === "h") { const r = sel || spot; setHide(hide ? null : (r ? { a: r.a, b: r.b } : null)); if (!r) toast("Select the bars to hide first"); return; }
        if (lk === "g") { setGuideOn(g => !g); return; }
        if (lk === "d") { if (!sel) { toast("Select the bars first"); return; } if (!guideOn) setGuideOn(true); setSymOpen(true); return; }
        if (lk === "t") { ensureSpot().then(sp => sp && startTest(sp)); return; }
        if (lk === "n") { if (next) goNext(next); else toast("Nothing else due in today's plan"); return; }
        if (lk === "p") { if (!guideOn) setGuideOn(true); ensureSpot().then(sp => { if (sp) setTimeout(nameRef, 60); }); return; }
        if (k === "[" || k === "]") { if (nav.current) nav.current(k === "]" ? 1 : -1); return; }
        if (k === "," || k === "<") { stepTake(1); return; }
        if (k === "." || k === ">") { stepTake(-1); return; }
        if (lk === "v") { compare(e.shiftKey); return; }
      }
      if (k === " ") { e.preventDefault();
        if (!player.playing() && sel && player.rec && !(curBar && inSel(curBar))) player.gotoBar(sel.a, 1, true); else player.toggle(); return; }
      const dig = /^Digit([1-9])$/.exec(e.code || "");
      if (dig && mode === "practice" && (e.shiftKey || /^[1-9]$/.test(k))) {    // 1–9, and Shift+1–9 for recordings 10–18
        const recs = data.recordings.filter(r => r.audio && !isSectionTake(r)); pickRec(recs[+dig[1] - 1 + (e.shiftKey ? 9 : 0)]); return; }
      if (k === "`") { if (player.prev != null) player.select(player.prev); return; }
      if (k === "ArrowRight" || k === "ArrowLeft") {
        e.preventDefault(); const d = k === "ArrowRight" ? 1 : -1;
        if (e.shiftKey) { const base = sel ? (M.order[sel.b] === M.order[cursor] ? sel.b : sel.a) : (curBar || cursor || M.seq[0]); const i = Math.min(M.seq.length - 1, Math.max(0, M.order[cursor || base] + d)); selectBar(M.seq[i], true); return; }
        if (mode === "practice" && player.rec && player.rec.occ.length && player.playing()) { player.step(d); return; }
        const i = Math.min(M.seq.length - 1, Math.max(0, (M.order[cursor] ?? -1) + d)); const id = M.seq[i]; selectBar(id, false); if (!pageShows(id)) setPage(M.byId[id].page);
        if (mode === "practice" && player.rec) player.gotoBar(id, 1, false); return; }
      if (k === "ArrowUp" || k === "ArrowDown") {     // the bar above / below: previous / next line of music
        e.preventDefault(); const from = cursor || curBar || (sel && sel.a) || M.seq[0]; const b = M.byId[from]; if (!b) return;
        const lines = []; const seen = new Set();
        for (const id of M.seq) { const x = M.byId[id]; const key = x.page * 1000 + x.system; if (!seen.has(key)) { seen.add(key); lines.push(key); } }
        const li = lines.indexOf(b.page * 1000 + b.system) + (k === "ArrowDown" ? 1 : -1); if (li < 0 || li >= lines.length) return;
        const cx = (b.box[0] + b.box[2]) / 2;
        const cand = M.seq.filter(id => M.byId[id].page * 1000 + M.byId[id].system === lines[li]);
        const id = cand.reduce((best, x) => Math.abs((M.byId[x].box[0] + M.byId[x].box[2]) / 2 - cx) < Math.abs((M.byId[best].box[0] + M.byId[best].box[2]) / 2 - cx) ? x : best, cand[0]);
        selectBar(id, e.shiftKey); if (!pageShows(id)) setPage(M.byId[id].page);
        if (mode === "practice" && !e.shiftKey && player.rec) player.gotoBar(id, 1, player.playing());
        return; }
      if (k === "Enter") { e.preventDefault(); const id = sel ? sel.a : (curBar || cursor); if (id) seek(id, 1, true); return; }
      if (mode === "practice") {
        if (k === "l" || k === "L") { if (!sel && curBar) { selectBar(curBar); } setLoop(!player.loop); return; }
        if (k === "-" || k === "_") { player.setRate(player.rate - 0.05); return; }
        if (k === "=" || k === "+") { player.setRate(player.rate + 0.05); return; }
        if (k === "0") { player.setRate(1); return; }
        if (k === "c" || k === "C") { player.ensureCtx(); player.click = !player.click; player.emit("state"); return; }
        if (k === "a" || k === "A") { if (e.shiftKey) anchorHere(); else if (player.rec) setDialog("spot"); else toast("Choose a recording first"); return; }
      }
      if (mode === "structure" && structureKey.current) structureKey.current(e);
    };
    window.addEventListener("keydown", onKey); return () => window.removeEventListener("keydown", onKey);
  });
  const structureKey = useRef(null);

  const piece = data && data.piece;
  const marksHere = sel && M ? (flags || []).filter(f => !f.done && f.kind && f.bar && inSel(f.bar)) : [];
  const practice = mode === "practice";

  return html`
    <div class="top">
      <span class="title" onClick=${() => setDialog("palette")} title="Library (Ctrl+K)">${piece ? (piece.title || "Untitled") : "Music Practice"}</span>
      ${piece && piece.composer ? html`<span class="sub">${piece.composer}</span>` : null}
      ${piece ? html`<button class="btn small ghost" title="rename, performance date, delete" onClick=${() => setDialog("piece")}>✎</button>` : null}
      ${M ? html`<div class="seg"><button class=${practice ? "on" : ""} onClick=${() => setMode("practice")}>Practice</button><button class=${!practice ? "on" : ""} onClick=${() => setMode("structure")}>Structure</button></div>` : null}
      ${M && practice ? html`<button class=${"btn small" + (guideOn ? " on" : "")} title="the guide: what it should sound like, what's wrong, what to try (G)" onClick=${() => setGuideOn(!guideOn)}>Guide <kbd>G</kbd></button>` : null}
      <span class="spacer"></span>
      <${Jobs} jobs=${jobsList} data=${data} />
      ${M && practice ? html`<${RecordButton} pid=${pid} context=${recContext} onToggle=${record} />` : null}
      <button class="btn" onClick=${() => setDialog("palette")}>Library <kbd>Ctrl K</kbd></button>
      <button class="btn" onClick=${() => setDialog("help")}>?</button>
    </div>
    ${!pieces.length && !piece ? html`<${Welcome} onImport=${() => setDialog("import")} />` :
      !M ? html`<div class="empty">${piece && piece.status !== "ready" ? html`<div class="row" style="justify-content:center"><span class="spin"></span> Reading the score… ${piece.message || ""}</div><div class="hint" style="margin-top:8px">The first reading of a score takes a few minutes (about half a minute per page). Recordings are prepared at the same time.</div>` : "Loading…"}</div>` :
      html`<div class=${"main" + (practice && guideOn ? " with-guide" : "")}>
        ${practice && guideOn ? html`<div class="side guidecol">
          <${GuideColumn} M=${M} sel=${sel} spot=${spot} inside=${inside} lib=${lib} patchSpot=${patchSpot} ensureSpot=${ensureSpot} marks=${marksHere}
            next=${next} goNext=${goNext} startTest=${startTest} spotSecs=${acc.current.shown} setTech=${setTech} navRef=${nav} symOpen=${symOpen} setSymOpen=${setSymOpen}
            lastTake=${lastTake} judge=${judge} playTake=${playTake}
            helperProps=${{ M, p: spot || {}, patch: (body) => patchSpot(spot, body), setSel: (s) => s && selectRange(s.a, s.b), hide, setHide }} /></div>` : null}
        <${Score} M=${M} data=${data} page=${page} setPage=${setPage} sel=${sel} curBar=${curBar} mode=${mode} onBar=${clickBar} onBarDbl=${dblBar}
                  editSystem=${editSystem} setEditSystem=${setEditSystem} reload=${loadPiece}
                  passages=${passages} active=${spot} onPassage=${(x) => selectRange(x.a, x.b)} hide=${hide} flags=${flags}
                  cues=${cues} onCue=${(c) => setCueEdit({ cue: c })} addLine=${addLine} setAddLine=${setAddLine} />
        <div class="side">
          ${practice
            ? html`<${RightColumn} M=${M} data=${data} sel=${sel} multi=${multi} spot=${spot} curBar=${curBar} here=${here} plan=${plan} loadPlan=${loadPlan}
                     passages=${passages} hist=${hist} lib=${lib} flags=${flags} cues=${cues} reload=${loadPiece} reloadFlags=${loadFlags}
                     onAdd=${() => setDialog("addrec")} setLoop=${setLoop} onCheck=${() => setDialog("spot")} pickRec=${pickRec} compare=${compare}
                     goTo=${goTo} goNext=${goNext} startTest=${startTest} selectRange=${selectRange} patchSpot=${patchSpot} loadHist=${loadHist}
                     onCue=${(c) => setCueEdit({ cue: c })} deleteSpot=${async () => { if (!spot || !confirm(`Delete the spot ${spot.name || rangeLabel(M, spot.a, spot.b)} and its practice history? (Its takes are kept.)`)) return;
                       await api.send("DELETE", `/api/passages/${spot.id}`); acc.current.id = null; setPassages(ps => ps.filter(x => x.id !== spot.id)); loadPlan(); }}
                     flagToSpot=${(f) => { const a = sel && inSel(f.bar) ? sel.a : f.bar, b = sel && inSel(f.bar) ? sel.b : f.bar; selectRange(a, b); api.send("PATCH", `/api/flags/${f.id}`, { done: 1 }).then(loadFlags); }} />`
            : html`<${StructurePanel} M=${M} data=${data} sel=${sel} reload=${loadPiece} keyRef=${structureKey} editSystem=${editSystem} setEditSystem=${setEditSystem}
                     addLine=${addLine} setAddLine=${setAddLine}
                     goLine=${(pg, sy) => { const id = M.seq.find(x => M.byId[x].page === pg && M.byId[x].system === sy); if (id) selectBar(id); setPage(pg); setEditSystem({ page: pg, idx: sy }); }} />`}
        </div>
      </div>`}
    ${dialog === "palette" ? html`<${Palette} pieces=${pieces} pid=${pid} onPick=${(id) => { setPid(id); setDialog(null); }} onImport=${() => setDialog("import")} onClose=${() => setDialog(null)} reload=${loadPieces} onSettings=${() => setDialog("settings")} onLibrary=${() => setDialog("library")} />` : null}
    ${dialog === "settings" ? html`<${SettingsDialog} onClose=${() => { setDialog(null); loadPiece(); }} />` : null}
    ${dialog === "import" ? html`<${ImportDialog} onClose=${() => setDialog(null)} onDone=${(id) => { setDialog(null); loadPieces(); setPid(id); }} />` : null}
    ${dialog === "addrec" ? html`<${AddRecording} pid=${pid} onClose=${() => setDialog(null)} onDone=${() => { setDialog(null); loadPiece(); }} />` : null}
    ${dialog === "help" ? html`<${Help} onClose=${() => setDialog(null)} onLibrary=${() => setDialog("library")} />` : null}
    ${dialog === "library" ? html`<${LibraryDialog} lib=${lib} onClose=${() => setDialog(null)} />` : null}
    ${cueEdit && M ? html`<${CueDialog} M=${M} pid=${pid} bar=${cueEdit.bar} cue=${cueEdit.cue} onClose=${() => setCueEdit(null)} onSaved=${loadCues} />` : null}
    ${markFlag ? html`<${MarkChooser} flag=${markFlag} onDone=${(k) => { setMarkFlag(null); loadFlags(); if (k) toast(`Marked ${markFlag.bar && M.byId[markFlag.bar] ? M.label(markFlag.bar) : fmt(markFlag.t)}: ${k}`); }} />` : null}
    ${dialog === "spot" && M && player.rec ? html`<${SpotCheck} M=${M} rec=${player.rec} onClose=${() => setDialog(null)} onDone=${loadPiece}
        onShow=${(id) => { if (!M.byId[id]) return; setSel({ a: id, b: id, anchor: id }); setCursor(id); if (!pageShows(id)) setPage(M.byId[id].page); }} />` : null}
    ${dialog === "piece" && piece ? html`<${PieceDialog} piece=${piece} onClose=${() => setDialog(null)}
        onSaved=${() => { setDialog(null); loadPiece(); loadPieces(); loadPlan(); }}
        onDeleted=${() => { setDialog(null); setData(null); setPid(null); loadPieces().then(ps => setPid(ps.length ? ps[0].id : null)); }} />` : null}
    ${testing && M ? html`<${TestDialog} M=${M} pid=${pid} passage=${testing} setHide=${setHide} onClose=${() => setTesting(null)} onDone=${() => { loadPassages(); loadPiece(); loadPlan(); setPver(v => v + 1); }} />` : null}
    <div class="toast" id="toast"></div>`;
}

// ------------------------------------------------------------------ top-level pieces
function Jobs({ jobs, data }) {
  const running = jobs.filter(j => j.status === "running")[0]; const queued = jobs.filter(j => j.status === "queued").length;
  const failed = jobs.filter(j => j.status === "failed");
  if (!running && !queued && !failed.length) return null;
  const names = { score: "Reading score", redetect: "Finding barlines", addline: "Reading the new line", check: "Checking bar numbers", download: "Downloading", features: "Analysing audio", align: "Aligning", consensus: "Cross-checking recordings" };
  const recName = id => { const r = data && data.recordings.find(r => r.id === id); return r ? r.name : ""; };
  return html`<span class="jobs" title=${failed.map(f => `${names[f.kind] || f.kind} failed: ${f.error}`).join("\n")}>
    ${running ? html`<span class="spin"></span>${names[running.kind] || running.kind} ${recName(running.recording_id)} ${running.progress ? Math.round(running.progress * 100) + "%" : ""}` : null}
    ${queued ? html`<span class="pill">+${queued} queued</span>` : null}
    ${failed.length ? html`<span class="pill status-bad">${failed.length} failed</span>` : null}</span>`;
}

function Welcome({ onImport }) {
  return html`<div class="welcome"><h1>Music Practice</h1>
    <p class="muted">Start with a score: a PDF from IMSLP works well. The tool reads it, finds the bars, and lines up any recordings you add.</p>
    <button class="btn primary" onClick=${onImport}>Import a score (PDF)</button></div>`;
}

function Palette({ pieces, pid, onPick, onImport, onClose, reload, onSettings, onLibrary }) {
  const [q, setQ] = useState(""); const [i, setI] = useState(0); const inp = useRef();
  useEffect(() => { inp.current && inp.current.focus(); reload(); }, []);
  const items = pieces.filter(p => (p.title + " " + (p.composer || "")).toLowerCase().includes(q.toLowerCase()));
  const key = e => { if (e.key === "ArrowDown") { e.preventDefault(); setI(x => Math.min(items.length - 1, x + 1)); } if (e.key === "ArrowUp") { e.preventDefault(); setI(x => Math.max(0, x - 1)); }
    if (e.key === "Enter" && items[i]) onPick(items[i].id); if (e.key === "Escape") onClose(); };
  return html`<div class="scrim" onClick=${e => e.target === e.currentTarget && onClose()}><div class="dialog palette">
    <input ref=${inp} placeholder="Find a piece…" value=${q} onInput=${e => { setQ(e.target.value); setI(0); }} onKeyDown=${key} />
    <div class="plist">${items.map((p, k) => html`<div class=${"pitem" + (k === i ? " on" : "")} onClick=${() => onPick(p.id)}>
        <b>${p.title}</b><span class="muted">${p.composer || ""}</span><span class="meta">${p.status !== "ready" ? p.status + " · " : ""}${p.recordings} recording${p.recordings === 1 ? "" : "s"}</span></div>`)}
      ${!items.length ? html`<div class="empty">No pieces yet.</div>` : null}</div>
    <div class="actions"><button class="btn" style="margin-right:auto" onClick=${onSettings}>Settings…</button><button class="btn" onClick=${onLibrary}>Practice library…</button><button class="btn" onClick=${onImport}>Import a score (PDF)…</button></div></div></div>`;
}

function SettingsDialog({ onClose }) {
  const [st, setSt] = useState(null); const [key, setKey] = useState(""); const [model, setModel] = useState("");
  useEffect(() => { api.get("/api/settings").then(s => { setSt(s); setModel(s.deepseek_model || ""); }); }, []);
  const save = async () => { const body = { deepseek_model: model.trim() || null }; if (key.trim()) body.deepseek_key = key.trim(); await api.send("PUT", "/api/settings", body); onClose(); };
  const forget = async () => { await api.send("PUT", "/api/settings", { deepseek_key: "" }); setSt({ ...st, deepseek_key: null }); };
  return html`<div class="scrim" onClick=${e => e.target === e.currentTarget && onClose()}><div class="dialog"><h3>Settings</h3>
    <div class="fields">
      <label>DeepSeek API key (optional: checks bar numbers against the printed ones, a fraction of a cent per page)
        <input type="password" autocomplete="off" value=${key} placeholder=${st && st.deepseek_key ? "saved: type to replace" : "sk-…"} onInput=${e => setKey(e.target.value)} /></label>
      ${st && st.deepseek_key ? html`<div class="row hint">A key is saved on this computer only. <button class="btn small" onClick=${forget}>Forget it</button></div>` : null}
      <label>Model<input value=${model} placeholder="deepseek-flash" onInput=${e => setModel(e.target.value)} /></label>
    </div>
    <div class="actions"><button class="btn" onClick=${onClose}>Cancel</button><button class="btn primary" onClick=${save}>Save</button></div></div></div>`;
}

function PieceDialog({ piece, onClose, onSaved, onDeleted }) {
  const [title, setTitle] = useState(piece.title || ""); const [composer, setComposer] = useState(piece.composer || "");
  const [perf, setPerf] = useState(piece.perf_date || "");
  const save = async () => { await api.send("PATCH", `/api/pieces/${piece.id}`, { title, composer, perf_date: perf || null }); onSaved(); };
  const del = async () => { if (!confirm(`Delete "${piece.title}" with its recordings, passages and practice history? Audio files you added from your own folders are not touched.`)) return;
    await api.send("DELETE", `/api/pieces/${piece.id}`); onDeleted(); };
  return html`<div class="scrim" onClick=${e => e.target === e.currentTarget && onClose()}><div class="dialog"><h3>This piece</h3>
    <div class="fields">
      <label>Title<input value=${title} onInput=${e => setTitle(e.target.value)} /></label>
      <label>Composer<input value=${composer} onInput=${e => setComposer(e.target.value)} /></label>
      <label>Performance date (optional: raises its priority as the date approaches)<input type="date" value=${perf} onInput=${e => setPerf(e.target.value)} /></label>
    </div>
    <div class="actions"><button class="btn" style="margin-right:auto;color:var(--warn)" onClick=${del}>Delete piece</button>
      <button class="btn" onClick=${onClose}>Cancel</button><button class="btn primary" onClick=${save}>Save</button></div></div></div>`;
}

function ImportDialog({ onClose, onDone }) {
  const [file, setFile] = useState(null); const [title, setTitle] = useState(""); const [composer, setComposer] = useState("");
  const [from, setFrom] = useState(""); const [to, setTo] = useState(""); const [busy, setBusy] = useState(false);
  const go = async () => {
    if (!file) return; setBusy(true);
    const fd = new FormData(); fd.append("pdf", file); fd.append("title", title); fd.append("composer", composer);
    if (from) fd.append("page_from", from); if (to) fd.append("page_to", to);
    const r = await api.form("/api/pieces", fd);
    onDone(r.id);
  };
  return html`<div class="scrim" onClick=${e => e.target === e.currentTarget && onClose()}><div class="dialog"><h3>Import a score</h3>
    <div class="fields">
      <label>PDF file<input type="file" accept="application/pdf" onChange=${e => { const f = e.target.files[0]; setFile(f); if (f && !title) setTitle(f.name.replace(/\.pdf$/i, "").replace(/^IMSLP\d+-PMLP\d+-/, "").replace(/_/g, " ")); }} /></label>
      <label>Title<input value=${title} onInput=${e => setTitle(e.target.value)} /></label>
      <label>Composer<input value=${composer} onInput=${e => setComposer(e.target.value)} /></label>
      <div class="row hint">Only pages <input style="width:60px" placeholder="first" value=${from} onInput=${e => setFrom(e.target.value)} /> to <input style="width:60px" placeholder="last" value=${to} onInput=${e => setTo(e.target.value)} /> (optional: e.g. one movement of a long PDF)</div>
    </div>
    <p class="hint">Reading takes about half a minute per page. You can keep using the app meanwhile.</p>
    <div class="actions"><button class="btn" onClick=${onClose}>Cancel</button><button class="btn primary" disabled=${!file || busy} onClick=${go}>${busy ? "Uploading…" : "Import"}</button></div></div></div>`;
}

function AddRecording({ pid, onClose, onDone }) {
  const [tab, setTab] = useState("youtube"); const [url, setUrl] = useState(""); const [file, setFile] = useState(null);
  const [name, setName] = useState(""); const [kind, setKind] = useState("reference"); const [busy, setBusy] = useState(false);
  const go = async () => {
    setBusy(true);
    if (tab === "youtube") await api.send("POST", `/api/pieces/${pid}/recordings`, { url, name, kind });
    else { const fd = new FormData(); fd.append("file", file); fd.append("name", name); fd.append("kind", kind); await api.form(`/api/pieces/${pid}/recordings`, fd); }
    onDone();
  };
  return html`<div class="scrim" onClick=${e => e.target === e.currentTarget && onClose()}><div class="dialog"><h3>Add a recording</h3>
    <div class="seg" style="width:max-content;margin-bottom:10px"><button class=${tab === "youtube" ? "on" : ""} onClick=${() => setTab("youtube")}>YouTube link</button><button class=${tab === "file" ? "on" : ""} onClick=${() => setTab("file")}>Audio file</button></div>
    <div class="fields">
      ${tab === "youtube" ? html`<label>Link<input placeholder="https://www.youtube.com/watch?v=…" value=${url} onInput=${e => setUrl(e.target.value)} /></label>`
        : html`<label>File (mp3, m4a, wav, webm…)<input type="file" accept="audio/*,video/*" onChange=${e => { const f = e.target.files[0]; setFile(f); if (f && !name) setName(f.name.replace(/\.[^.]+$/, "")); }} /></label>`}
      <label>Name (performer)<input value=${name} placeholder=${tab === "youtube" ? "from the video title if empty" : ""} onInput=${e => setName(e.target.value)} /></label>
      <label>Whose recording?<select value=${kind} onChange=${e => setKind(e.target.value)}><option value="reference">A performer (reference)</option><option value="mine">Mine</option></select></label>
    </div>
    <p class="hint">It is downloaded, analysed, and lined up with the score automatically, including which repeats were taken.</p>
    <div class="actions"><button class="btn" onClick=${onClose}>Cancel</button><button class="btn primary" disabled=${busy || (tab === "youtube" ? !url : !file)} onClick=${go}>${busy ? "Adding…" : "Add"}</button></div></div></div>`;
}

function Help({ onClose, onLibrary }) {
  const rows = [
    ["", "The selected bars are what you are working on"],
    ["click a bar", "select it and move playback there (still playing if it was); inside a selection, only move playback"], ["double-click", "play from that bar"],
    ["Shift+click, Shift+← →", "select a range of bars"], ["R", "record: the selected bars (they become a spot, and the take is lined up with just them); one bar or none: the whole piece"],
    ["Shift+R", "hear the take you just recorded, straight away"], [",  .", "your older / newer takes of the selected bars"],
    ["V", "your newest take vs a performer, bar by bar (Shift+V: all of one, then the other)"],
    ["D, then 1–9", "what's wrong (0: the full list): the technique that fits shows in the guide, and your next takes are tagged with it"],
    ["[  ]", "other techniques for it"], ["T", "cold take: record once, listen, rate 1–4"], ["N", "next in today's plan"], ["P", "name the spot"], ["G", "show / hide the guide"],
    ["", "Listening"],
    ["Space", "play / pause (from the selection)"], ["L", "loop the selection"], ["1 – 9, Shift+1 – 9", "switch recording at the same spot; paused with bars selected: play them in it"],
    ["`", "back to the previous recording"], ["−  =  0", "slower / faster / normal speed (pitch stays)"], ["← →  ↑ ↓", "previous / next bar, line above / below"], ["Enter", "play from the selection"],
    ["F, then 1–8", "mark this spot in the recording: wrong note, hesitation, rhythm, balance, shape, pedal, tension, note"], ["Q", "performance cue on the selected bar"],
    ["C", "click on every downbeat"], ["A", "check timing where it is least certain"], ["Shift+A", "tap exactly on a downbeat to pin it"],
    ["", "Everywhere"],
    ["M", "metronome on / off"], ["H", "hide the selected bars on the score (memory)"], ["J K, PgUp PgDn", "turn pages"],
    ["Tab", "Practice ↔ Structure"], ["Esc", "clear selection, stop looping, show hidden bars"], ["Ctrl+K", "library of pieces"]];
  return html`<div class="scrim" onClick=${e => e.target === e.currentTarget && onClose()}><div class="dialog helpd"><h3>Keys</h3>
    <div class="help">${rows.map(([k, v]) => k ? html`<span><kbd>${k}</kbd></span><span>${v}</span>` : html`<b class="hsec">${v}</b>`)}</div>
    <p class="hint">Time on a spot is logged by itself while it is selected. Structure mode: click a bar, then use the buttons (or keys shown on them) to mark repeats, endings, D.C., new movements, meter changes, or to fix barlines and whole lines.</p>
    <div class="actions"><button class="btn" style="margin-right:auto" onClick=${onLibrary}>Practice library…</button><button class="btn" onClick=${onClose}>Close</button></div></div></div>`;
}

// ------------------------------------------------------------------ score view
// Page images are decoded before they are shown, so a page turn swaps one finished picture for another instead of
// flashing the empty page first. Neighbouring pages are decoded ahead of time.
const decoded = new Set(), decoding = new Map(), keepImgs = new Map();
function decodePage(src) {
  if (decoded.has(src)) return Promise.resolve();
  if (decoding.has(src)) return decoding.get(src);
  const im = new Image(); im.decoding = "async"; im.src = src;
  const pr = (im.decode ? im.decode() : new Promise(r => { im.onload = r; im.onerror = r; })).catch(() => {})
    .then(() => { decoded.add(src); keepImgs.set(src, im); decoding.delete(src); if (keepImgs.size > 24) { const k = keepImgs.keys().next().value; keepImgs.delete(k); decoded.delete(k); } });
  decoding.set(src, pr); return pr;
}
const pageSrc = (pid, n) => `/api/pieces/${pid}/pages/${n}`;

function Score(props) {
  const { M, data, page, setPage, curBar } = props;
  const wrap = useRef(); const [box, setBox] = useState({ w: 800, h: 700 });
  useEffect(() => { const ro = new ResizeObserver(es => { const r = es[0].contentRect; setBox({ w: r.width, h: r.height }); }); ro.observe(wrap.current); return () => ro.disconnect(); }, []);
  const pid = data.piece.id;
  const pg = M.pages[page - 1] || M.pages[0]; const aspect = pg.w / pg.h;
  const pw = box.h * aspect; const two = M.npages > 1 && box.w >= 2 * pw + 8;
  // in a two-page spread, show (page, page+1); keep the current bar visible
  const want = two ? Math.min(page, M.npages - 1) : page;
  const [left, setLeft] = useState(want);
  useEffect(() => {
    const need = two ? [want, want + 1] : [want]; let alive = true;
    if (need.every(n => decoded.has(pageSrc(pid, n)))) setLeft(want);
    else Promise.all(need.map(n => decodePage(pageSrc(pid, n)))).then(() => { if (alive) setLeft(want); });
    for (const n of [want - 2, want - 1, want + 1, want + 2, want + 3]) if (n >= 1 && n <= M.npages) decodePage(pageSrc(pid, n));
    return () => { alive = false; };
  }, [want, two, pid]);
  const L = Math.min(left, two ? M.npages - 1 : M.npages);
  const shown = two ? [L, L + 1] : [L];
  return html`<div class="scorewrap" ref=${wrap}>
    ${shown.map((n, slot) => html`<${Page} key=${"slot" + slot} ...${props} n=${n} dispW=${Math.min(pw, two ? (box.w - 8) / 2 : box.w)} />`)}
    <div class="pagenav" style="position:absolute;left:8px;bottom:6px;z-index:2"><button onClick=${() => setPage(Math.max(1, page - (two ? 2 : 1)))}>‹</button><button onClick=${() => setPage(Math.min(M.npages, page + (two ? 2 : 1)))}>›</button></div>
  </div>`;
}

function Page({ M, data, n, dispW, sel, curBar, mode, onBar, onBarDbl, editSystem, setEditSystem, reload, passages, active, onPassage, hide, flags, cues, onCue, addLine, setAddLine }) {
  const pg = M.pages[n - 1]; const bars = M.pageBars[n] || [];
  const pid = data.piece.id; const W = pg.w, H = pg.h; const k = W / Math.max(100, dispW);
  const oa = sel ? M.order[sel.a] : null, ob = sel ? M.order[sel.b] : null;
  const mk = (data.structure && data.structure.markup) || {};
  const mvStarts = new Set(((data.structure && data.structure.movements) || []).map(m => m.start));
  const spreadOf = useMemo(() => {   // bars where the recordings disagree (active recording)
    const out = {}; const r = player.rec; if (!r) return out;
    for (const o of r.occ) { if (o[4] != null && o[4] > 0.12 * Math.max(0.05, o[3] - o[2])) out[o[0]] = 1; }
    return out; }, [data, player.act]);
  const svgRef = useRef();
  const es = editSystem && editSystem.page === n ? data.score.systems.find(s => s.page === editSystem.page && s.idx === editSystem.idx) : null;
  const [drag, setDrag] = useState(null);
  const [lines, setLines] = useState(null);
  const [ext, setExt] = useState(null); const [dragY, setDragY] = useState(null);   // the line's top / bottom
  const [boxDraw, setBoxDraw] = useState(null);                                       // drawing a missed line
  useEffect(() => { setLines(es ? [...es.barlines] : null); setExt(es ? { y0: es.y0, y1: es.y1 } : null); }, [editSystem, data]);
  const toX = (e) => { const r = svgRef.current.getBoundingClientRect(); return (e.clientX - r.left) / r.width; };
  const toY = (e) => { const r = svgRef.current.getBoundingClientRect(); return (e.clientY - r.top) / r.height; };
  const saveExt = async (x) => { await api.send("PUT", `/api/pieces/${pid}/line/extent`, { page: es.page, idx: es.idx, y0: x.y0, y1: x.y1 }); reload(); };
  const finishBox = async (b) => { setBoxDraw(null); const box = [Math.min(b.x0, b.x1), Math.min(b.y0, b.y1), Math.max(b.x0, b.x1), Math.max(b.y0, b.y1)];
    if (box[2] - box[0] < 0.1 || box[3] - box[1] < 0.02) { toast("Draw the box around the whole line, both staves"); return; }
    const r = await api.send("POST", `/api/pieces/${pid}/line/add`, { page: n, box }); setAddLine(false);
    toast(r.error ? r.error : "Reading the new line (about half a minute)…"); reload(); };
  const saveLines = async (ls) => { await api.send("PUT", `/api/pieces/${pid}/barlines`, { page: es.page, idx: es.idx, barlines: ls }); reload(); };
  const fs = 11 * k;
  return html`<div class="page" style=${`aspect-ratio:${W}/${H}`}>
      <img src=${pageSrc(pid, n)} draggable="false" decoding="sync" />
      <svg ref=${svgRef} viewBox=${`0 0 ${W} ${H}`} preserveAspectRatio="none" class=${addLine ? "drawing" : ""}
        onMouseDown=${e => { if (addLine) { e.preventDefault(); const x = toX(e), y = toY(e); setBoxDraw({ x0: x, y0: y, x1: x, y1: y }); } }}
        onMouseMove=${e => { if (boxDraw) { setBoxDraw({ ...boxDraw, x1: toX(e), y1: toY(e) }); return; }
          if (dragY && ext) { const y = Math.min(0.999, Math.max(0.001, toY(e))); setExt({ ...ext, [dragY]: y }); return; }
          if (drag != null && lines) { const x = Math.min(0.999, Math.max(0.001, toX(e))); const ls = [...lines]; ls[drag] = x; setLines(ls); } }}
        onMouseUp=${() => { if (boxDraw) { finishBox(boxDraw); return; } if (dragY && ext) { setDragY(null); saveExt(ext); return; } if (drag != null && lines) { setDrag(null); saveLines(lines); } }}>
        ${bars.map(b => {
          const [x0, y0, x1, y1] = b.box; const o = M.order[b.id];
          const deleted = M.deleted.has(b.id);
          const cls = ["bar", mode === "structure" ? "edit" : "", o != null && sel && o >= oa && o <= ob ? "sel" : "", b.id === curBar ? "cur" : "",
                       deleted ? "deleted" : "", mode === "structure" && mvStarts.has(b.id) ? "mvstart" : "", mode === "practice" && spreadOf[b.id] ? "low" : ""].join(" ");
          return html`<rect class=${cls} vector-effect="non-scaling-stroke" x=${x0 * W + 2 * k} y=${y0 * H} width=${Math.max(2, (x1 - x0) * W - 4 * k)} height=${(y1 - y0) * H} rx=${4 * k}
                        onClick=${e => { if (addLine) return; if (es && es.idx === b.system) { const ls = [...lines, toX(e)].sort((a, c) => a - c); setLines(ls); saveLines(ls); return; } onBar(b.id, e); }}
                        onDblClick=${() => { if (mode === "structure") setEditSystem({ page: b.page, idx: b.system }); else if (onBarDbl) onBarDbl(b.id); }}>
                        <title>${deleted ? "not a bar (removed)" : M.label(b.id)}</title></rect>`;
        })}
        ${bars.map(b => {
          const [x0, y0, x1, y1] = b.box; const showLbl = mode === "structure" || b.idx === 0 || b.id === curBar;
          const m = mk[b.id] || {}; const marks = [];
          if (m.repeat_start) marks.push("|:"); if (m.repeat_end) marks.push(":|"); if (m.repeat_to) marks.push("(→" + (M.labels[m.repeat_to] ?? "?") + ")"); if (m.ending) marks.push(m.ending + "."); if (m.fine) marks.push("Fine");
          if (m.dc) marks.push("D.C."); if (m.ds) marks.push("D.S."); if (m.segno) marks.push("segno"); if (m.coda) marks.push("coda"); if (m.to_coda) marks.push("to coda"); if (m.meter) marks.push(m.meter);
          return html`<g>${showLbl && M.labels[b.id] != null && !M.deleted.has(b.id) ? html`<text class="lbl" style=${`font-size:${fs}px`} x=${x0 * W + 5 * k} y=${y0 * H + fs * 1.05}>${M.labels[b.id]}</text>` : null}
            ${mode === "structure" && marks.length ? html`<text class="mk" style=${`font-size:${fs * 1.1}px`} x=${x0 * W + 5 * k} y=${y1 * H - 4 * k}>${marks.join(" ")}</text>` : null}</g>`;
        })}
        ${mode === "practice" ? html`<${PassageOverlay} M=${M} bars=${bars} passages=${passages} active=${active} W=${W} H=${H} k=${k} onPick=${onPassage} />` : null}
        <${HideOverlay} M=${M} bars=${bars} hide=${hide} W=${W} H=${H} k=${k} />
        ${mode === "structure" && data.check ? data.check.flags.filter(f => f.page === n).map(f => { const sy = data.score.systems.find(x => x.page === n && x.idx === f.system); if (!sy) return null;
            return html`<rect class="checkline" vector-effect="non-scaling-stroke" x=${(sy.x_left - 0.01) * W} y=${sy.y0 * H - 6 * k} width=${(0.985 - sy.x_left) * W} height=${(sy.y1 - sy.y0) * H + 12 * k} rx=${6 * k}><title>${f.msg}</title></rect>`; }) : null}
        ${mode !== "structure" ? html`<${FlagMarks} M=${M} bars=${bars} flags=${flags} W=${W} H=${H} k=${k} />` : null}
        ${mode !== "structure" ? html`<${CueMarks} M=${M} bars=${bars} cues=${cues} W=${W} H=${H} k=${k} showText=${mode === "practice"} onEdit=${onCue} />` : null}
        ${es && ext ? ["y0", "y1"].map(key => html`<line class="ext-handle" vector-effect="non-scaling-stroke" x1=${es.x_left * W} x2=${0.99 * W} y1=${ext[key] * H} y2=${ext[key] * H}
            onMouseDown=${e => { e.stopPropagation(); setDragY(key); }}><title>drag to move the line's ${key === "y0" ? "top" : "bottom"}</title></line>`) : null}
        ${boxDraw ? html`<rect class="drawbox" vector-effect="non-scaling-stroke" x=${Math.min(boxDraw.x0, boxDraw.x1) * W} y=${Math.min(boxDraw.y0, boxDraw.y1) * H} width=${Math.abs(boxDraw.x1 - boxDraw.x0) * W} height=${Math.abs(boxDraw.y1 - boxDraw.y0) * H} />` : null}
        ${es && lines ? lines.map((x, j) => html`<line class="bl-handle" vector-effect="non-scaling-stroke" x1=${x * W} x2=${x * W} y1=${es.y0 * H} y2=${es.y1 * H}
            onMouseDown=${e => { e.stopPropagation(); setDrag(j); }}
            onContextMenu=${e => { e.preventDefault(); const ls = lines.filter((_, i) => i !== j); setLines(ls); saveLines(ls); }}><title>drag to move · right-click to remove</title></line>`) : null}
      </svg>
      <span class="pg">p. ${n} / ${M.npages}</span>
    </div>`;
}

// ------------------------------------------------------------------ recordings
function formSummary(r, M) {
  const occ = r.alignment ? r.alignment.occ : []; if (!occ.length) return "";
  // performance order as spans of consecutive bars, per movement
  const spans = []; let cur = null;
  for (const o of occ) {
    const i = M.order[o[0]]; if (i == null) continue;
    if (cur && i === cur.bi + 1) { cur.b = o[0]; cur.bi = i; } else { cur = { a: o[0], b: o[0], ai: i, bi: i }; spans.push(cur); }
  }
  const lab = id => M.labels[id] ?? "?";
  const mvs = [];
  for (const s of spans) { const m = M.mvOf[s.a]; const name = M.mvNames.length > 1 ? romanOr(M.mvNames[m], m) + ": " : ""; mvs.push(name + (s.a === s.b ? lab(s.a) : `${lab(s.a)}–${lab(s.b)}`)); }
  return mvs.join(" · ");
}

function RecStrip({ r, M, active, flags }) {
  const occ = r.alignment ? r.alignment.occ : []; const T = Math.max(1, r.duration || (occ.length ? occ[occ.length - 1][3] + 2 : 1));
  const t = active ? player.time() : null;
  const seek = (e) => { e.stopPropagation(); if (!r.audio) return; const b = e.currentTarget.getBoundingClientRect();
    const res = player.select(r.id, { time: Math.max(0, (e.clientX - b.left) / b.width * T), play: true }); if (!res.ok) toast("Can't play that recording"); };
  return html`<div class="strip" title="click to play from there" onClick=${seek}>${occ.map((o, k) => {
      const w = Math.min(20, o[3] - o[2]); if (w <= 0) return null;
      return html`<i class=${o[1] > 1 ? "p2" : ""} style=${`left:${100 * o[2] / T}%;width:${100 * w / T + 0.05}%;background:${MCOLS[(M.mvOf[o[0]] || 0) % MCOLS.length]}`}></i>`; })}
    ${(flags || []).filter(f => f.recording_id === r.id && !f.done).map(f => html`<b class="ft" style=${`left:${100 * f.t / T}%`}></b>`)}
    ${t != null ? html`<div class="ph" style=${`left:${100 * t / T}%`}></div>` : null}</div>`;
}

// the right column: recordings (the listening), transport, and tabs about the selected bars and the day
function RightColumn({ M, data, sel, multi, spot, curBar, plan, loadPlan, passages, hist, lib, flags, cues, reload, reloadFlags, onAdd, setLoop, onCheck, pickRec, compare,
                       goTo, startTest, patchSpot, loadHist, onCue, deleteSpot, flagToSpot, selectRange }) {
  const recs = data.recordings.filter(r => !isSectionTake(r)); const playable = recs.filter(r => r.audio);   // section takes live with their bars
  const p = player.pos(); const r = player.rec; const bar = curBar;
  const twice = bar && r ? player.passes(r.id, bar) > 1 : false;
  const [menu, setMenu] = useState(null);
  const [tab, setTab] = useState(store.get("rtab", "here")); useEffect(() => store.set("rtab", tab), [tab]);
  const [plist, setPlist] = useState("today");
  const statusText = rr => ({ queued: "queued", downloading: "downloading…", processing: "analysing…", aligning: "aligning…", waiting: "waiting to align", failed: "failed", new: "waiting" }[rr.status] || "");
  const inRange = (id, a, b) => M.order[id] != null && M.order[id] >= M.order[a] && M.order[id] <= M.order[b];
  const nFlags = flags.filter(f => !f.done).length; const nTakes = data.recordings.filter(isSectionTake).length;
  const todo = plan ? plan.items.filter(i => i.in_budget).length : 0;
  const tabs = [["here", "Here", "your takes of the selected bars, the performers there, and their shape"], ["today", `Today${todo ? ` (${todo})` : ""}`, "today's plan, and the spots in this piece"],
    ["marks", `Marks${nFlags ? ` (${nFlags})` : ""}`, "spots marked with F while listening"], ["timing", "Timing", "the recordings' timing, bar by bar"],
    ["takes", `Takes${nTakes ? ` (${nTakes})` : ""}`, "every take of a few bars, in this piece"], ["notes", "Notes", "notes, cues and history of the spot"]];
  const bars = spot ? new Set(passageBars(M, spot)) : new Set();
  return html`
    <div class="card"><h2>Recordings${recs.length > 9 ? html` <span class="muted" style="text-transform:none;letter-spacing:0">(${recs.length}; Shift+number for 10–18)</span>` : null}<span class="r"><button class="btn small" onClick=${onAdd}>+ Add</button></span></h2>
      <div class="reclist">${recs.length ? recs.map((rr) => { const k = playable.indexOf(rr);
        return html`<div class=${"rec" + (player.act === rr.id ? " on" : "")} onClick=${() => pickRec(rr)}
            onContextMenu=${e => { e.preventDefault(); setMenu(menu === rr.id ? null : rr.id); }} title=${formSummary(rr, M) + "\n(right-click for options)"}>
          <span class="key" title=${k >= 9 && k < 18 ? `Shift+${k - 8}` : ""}>${k < 0 ? "·" : k < 9 ? k + 1 : k < 18 ? "⇧" + (k - 8) : "·"}</span>
          <span class=${"nm" + (rr.kind === "mine" ? " mine" : "")}>${rr.name || "…"}</span>
          ${rr.alignment && rr.alignment.occ.length ? html`<${RecStrip} r=${rr} M=${M} active=${player.act === rr.id} flags=${flags} />` : html`<span class=${"st" + (rr.status === "failed" ? " status-bad" : "")} title=${rr.message || ""}>${statusText(rr) || (rr.status === "ready" ? "not in this piece?" : "")}</span>`}
          <span class="st">${rr.alignment && rr.alignment.stale ? "re-aligning…" : ""}</span>
        </div>
        ${menu === rr.id ? html`<${RecMenu} r=${rr} onDone=${() => { setMenu(null); reload(); }} />` : null}`; })
      : html`<div class="hint">No recordings yet. Add a YouTube link or an audio file, yours or a performer's.</div>`}</div>
    </div>
    <div class="card now">
      <div class="big">${bar ? M.label(bar) : (sel ? M.label(sel.a) : "—")}${twice ? html`<small>${ord(p.pass)} time</small>` : null}<small>${fmt(player.time())}</small>${r ? html`<small class="ell nowrap" style="max-width:120px;display:inline-block;vertical-align:bottom">${r.name}</small>` : null}</div>
      <div class="controls">
        <button class=${"btn" + (player.playing() ? " on" : "")} onClick=${() => { if (!player.playing() && sel && !(bar && inRange(bar, sel.a, sel.b))) player.gotoBar(sel.a, 1, true); else player.toggle(); }}>${player.playing() ? "Pause" : "Play"} <kbd>Space</kbd></button>
        <button class=${"btn" + (player.loop ? " on" : "")} disabled=${!sel} onClick=${() => setLoop(!player.loop)}>Loop <kbd>L</kbd></button>
        <button class="btn" onClick=${() => player.setRate(player.rate - 0.05)}>−</button><span class="speed">${Math.round(player.rate * 100)}%</span><button class="btn" onClick=${() => player.setRate(player.rate + 0.05)}>+</button>
        <button class=${"btn" + (player.click ? " on" : "")} title="click on every downbeat (to check the alignment)" onClick=${() => { player.ensureCtx(); player.click = !player.click; player.emit("state"); }}>Click <kbd>C</kbd></button>
        <button class="btn" disabled=${!r} title="check the alignment where it is least certain" onClick=${() => onCheck()}>Check <kbd>A</kbd></button>
        <button class="btn" disabled=${!sel} title="your newest take of these bars against a performer, bar by bar (Shift+V: all of one, then all of the other)" onClick=${e => compare(e.shiftKey)}>Take vs rec <kbd>V</kbd></button>
        ${player.segment() ? html`<button class="btn" onClick=${() => { player.stopSegments(); player.pause(); }}>■</button>` : null}
      </div>
    </div>
    <div class="card grow"><h2><div class="seg mini tabs">${tabs.map(([id, n, t]) => html`<button class=${tab === id ? "on" : ""} title=${t} onClick=${() => setTab(id)}>${n}</button>`)}</div></h2>
      <div class="tabbody">
      ${tab === "here" ? (sel && M.byId[sel.a] ? html`<${TakesPanel} M=${M} data=${data} p=${spot || { id: -1, a: sel.a, b: sel.b }} reload=${reload} tname=${(id) => { const x = lib && lib.techniques.find(q => q.id === id); return x ? x.name : id; }} />`
          : html`<div class="hint">Select bars (click, then Shift+click) to see your takes of them next to the performers', and their timing and dynamic shape. <kbd>,</kbd> <kbd>.</kbd> play your older / newer takes; <kbd>V</kbd> compares your newest with a performer bar by bar.</div>`)
      : tab === "today" ? html`<div class="row" style="margin-bottom:4px"><div class="seg mini"><button class=${plist === "today" ? "on" : ""} onClick=${() => setPlist("today")}>Today</button><button class=${plist === "piece" ? "on" : ""} onClick=${() => setPlist("piece")}>This piece${passages.length ? ` (${passages.length})` : ""}</button></div>
            <span class="spacer"></span>${plan && plist === "today" ? html`<span class="muted">${Math.round(plan.total)} min ·</span><select class="mini" value=${plan.budget} onChange=${e => loadPlan(e.target.value)} title="how long you have today">${[15, 20, 30, 40, 60, 90, 120, 180].map(b => html`<option value=${b}>${b} min</option>`)}</select>` : null}</div>
          <div class="scrolly">${plist === "today" ? html`<${Today} M=${M} plan=${plan} active=${spot} openPassage=${goTo} startTest=${startTest} />`
            : html`<${PieceList} M=${M} data=${data} passages=${passages} active=${spot} openPassage=${goTo} onPerf=${() => loadPlan()} />`}</div>`
      : tab === "marks" ? html`<div class="form-summary" title="the form this recording takes">${r ? html`<b>${r.name}:</b> ${formSummary(data.recordings.find(x => x.id === r.id) || {}, M) || "not aligned yet"}` : "Add a recording to hear the score."}</div>
          <div class="scrolly"><${FlagsCard} M=${M} data=${data} flags=${flags} reload=${reloadFlags} makePassage=${flagToSpot} /></div>`
      : tab === "takes" ? html`<div class="scrolly"><${SectionTakes} M=${M} data=${data} sel=${sel} reload=${reload} makePassage=${(a, b) => selectRange(a, b)} /></div>`
      : tab === "timing" ? html`<${TimingLane} M=${M} data=${data} range=${sel || (bar ? { a: bar, b: bar } : null)} />`
      : spot ? html`<div class="row"><b class="grow1 ell">${spot.name || rangeLabel(M, spot.a, spot.b)}</b><span class="muted">${spot.sessions || 0} day${spot.sessions === 1 ? "" : "s"} worked</span>
            <button class="btn small ghost" title="delete this spot and its history (its takes are kept)" onClick=${deleteSpot}>Delete spot</button></div>
          <${NotesTab} M=${M} p=${spot} patch=${(body) => patchSpot(spot, body)} hist=${hist} lib=${lib} cues=${(cues || []).filter(c => bars.has(c.bar))} onCue=${onCue}
            marks=${(flags || []).filter(f => !f.done && f.kind && bars.has(f.bar))} onDeleted=${() => { loadHist(); loadPlan(); }} />`
      : html`<div class="hint">Notes and history belong to a spot. The selected bars become one as soon as you record them, write their intent or say what's wrong (or press <kbd>P</kbd>).</div>`}
      </div>
    </div>`;
}

function RecMenu({ r, onDone }) {
  const [name, setName] = useState(r.name || ""); const [a, setA] = useState(r.range_start ?? ""); const [b, setB] = useState(r.range_end ?? "");
  const save = async () => { await api.send("PATCH", `/api/recordings/${r.id}`, { name, range_start: a === "" ? null : +a, range_end: b === "" ? null : +b }); onDone(); };
  return html`<div class="card" style="margin:4px 0 6px 26px;padding:8px">
    <div class="fields" style="display:grid;gap:6px">
      <label class="hint">Name <input value=${name} onInput=${e => setName(e.target.value)} /></label>
      <div class="row hint">Only use from <input style="width:70px" placeholder="0" value=${a} onInput=${e => setA(e.target.value)} /> to <input style="width:70px" placeholder="end" value=${b} onInput=${e => setB(e.target.value)} /> seconds</div>
      ${r.chapters && r.chapters.length ? html`<div class="hint">Chapters: ${r.chapters.map(c => html`<button class="btn small" onClick=${() => { setA(Math.round(c.start_time)); setB(Math.round(c.end_time)); }}>${c.title}</button> `)}</div>` : null}
    </div>
    <div class="row" style="margin-top:6px"><button class="btn small primary" onClick=${save}>Save</button>
      <button class="btn small" onClick=${async () => { await api.send("POST", `/api/recordings/${r.id}/realign`); onDone(); }}>Re-align</button>
      ${r.alignment && r.alignment.anchors ? html`<button class="btn small" title="forget the downbeats you tapped with A" onClick=${async () => { await api.send("DELETE", `/api/recordings/${r.id}/anchors`); onDone(); }}>Clear ${r.alignment.anchors} anchor${r.alignment.anchors > 1 ? "s" : ""}</button>` : null}
      <button class="btn small" title="move up (number key)" onClick=${async () => { await api.send("POST", `/api/recordings/${r.id}/move`, { d: -1 }); onDone(); }}>↑</button>
      <button class="btn small" title="move down" onClick=${async () => { await api.send("POST", `/api/recordings/${r.id}/move`, { d: 1 }); onDone(); }}>↓</button>
      <span class="spacer"></span><button class="btn small" onClick=${async () => { if (confirm(`Remove "${r.name}" from this piece?`)) { await api.send("DELETE", `/api/recordings/${r.id}`); onDone(); } }}>Remove</button></div></div>`;
}

// ------------------------------------------------------------------ structure panel
function CheckList({ M, data, reload, setEditSystem, goLine }) {
  const pid = data.piece.id; const ck = data.check; const [busy, setBusy] = useState(false);
  const run = async () => { setBusy(true); const r = await api.send("POST", `/api/pieces/${pid}/check`); setBusy(false); if (r.error) toast(r.error); else toast("Checking bar numbers in the background…"); reload(); };
  if (!data.vision) return html`<div class="hint" style="margin-top:8px">Optional: with a DeepSeek API key (Library → Settings), the printed bar numbers are read and compared with the bars found here, so you only check the lines that disagree.</div>`;
  return html`<div class="checks">
    <div class="row"><button class="btn small" disabled=${busy} onClick=${run}>${ck ? "Check bar numbers again" : "Check bar numbers"}</button>
      ${ck ? html`<span class="hint">${ck.reliable === false ? "this edition doesn't seem to print bar numbers (the numbers read don't fit the lines), so nothing is flagged"
        : ck.read ? `${ck.read} of ${ck.lines} line numbers read${ck.flags.length ? "" : " · all agree"}` : "no printed bar numbers found in this edition: nothing to compare"}</span>` : null}</div>
    ${ck && ck.flags.length ? html`<div class="flags">${ck.flags.map(f => html`<div class="checkrow" onClick=${() => goLine(f.page, f.system)}>
        <b>p. ${f.page}, line ${f.system + 1}</b> <span class="hint">${f.msg}</span></div>`)}</div>` : null}</div>`;
}

function StructurePanel({ M, data, sel, reload, keyRef, editSystem, setEditSystem, goLine, addLine, setAddLine }) {
  const pid = data.piece.id; const st = data.structure || { movements: [], markup: {}, deleted: [] };
  const [dirty, setDirty] = useState(null);
  const s = dirty || st;
  const save = async (ns) => { setDirty(ns); await api.send("PUT", `/api/pieces/${pid}/structure`, ns); setDirty(null); reload(); };
  const bar = sel ? sel.a : null; const m = (bar && s.markup[bar]) || {};
  const setMark = (key, val) => { const mk = { ...s.markup }; const cur = { ...(mk[bar] || {}) };
    if (val == null || val === 0 || val === "" || val === false) delete cur[key]; else cur[key] = val;
    if (Object.keys(cur).length) mk[bar] = cur; else delete mk[bar]; save({ ...s, markup: mk }); };
  const toggle = (key, val = 1) => setMark(key, m[key] ? null : val);
  // "or back to...": the next bar clicked becomes an alternative repeat target of this repeat sign
  const [pickAlt, setPickAlt] = useState(null);
  useEffect(() => { if (!pickAlt || !sel || sel.a === pickAlt) return;
    const mk = { ...s.markup }; mk[pickAlt] = { ...(mk[pickAlt] || {}), repeat_to: sel.a }; save({ ...s, markup: mk }); setPickAlt(null); }, [sel && sel.a]);
  const newMovement = () => { if (!bar || s.movements.some(x => x.start === bar)) return;
    const mv = [...s.movements, { name: "", start: bar, meter: (s.movements[0] && s.movements[0].meter) || "4/4", pickup: false }];
    mv.sort((a, b) => M.order[a.start] - M.order[b.start]); save({ ...s, movements: mv }); };
  const delBar = () => { const d = new Set(s.deleted || []); d.has(bar) ? d.delete(bar) : d.add(bar); save({ ...s, deleted: [...d] }); };
  keyRef.current = (e) => {
    if (!bar) return; const k = e.key.toLowerCase();
    if (k === "[") toggle("repeat_start"); else if (k === "]") toggle("repeat_end"); else if (k === "1") setMark("ending", m.ending === 1 ? null : 1);
    else if (k === "2") setMark("ending", m.ending === 2 ? null : 2); else if (k === "f") toggle("fine"); else if (k === "d") toggle("dc");
    else if (k === "n") newMovement(); else if (k === "delete" || k === "backspace") delBar();
    else if (k === "b") { const b = M.byId[bar]; setEditSystem(editSystem ? null : { page: b.page, idx: b.system }); }
  };
  return html`
    <div class="card"><h2>Movements</h2>
      ${s.movements.map((mv, i) => html`<div class="mvrow">
        <${Field} value=${mv.name} placeholder=${`Movement ${i + 1}`} onSave=${v => { const x = s.movements.map((y, j) => j === i ? { ...y, name: v } : y); save({ ...s, movements: x }); }} />
        <${Field} value=${mv.meter} title="time signature" onSave=${v => { const x = s.movements.map((y, j) => j === i ? { ...y, meter: v } : y); save({ ...s, movements: x }); }} />
        <label class="hint" title="the first bar is a pickup (upbeat)"><input type="checkbox" checked=${!!mv.pickup} onChange=${e => { const x = s.movements.map((y, j) => j === i ? { ...y, pickup: e.target.checked } : y); save({ ...s, movements: x }); }} /> pickup</label>
        ${i > 0 ? html`<button class="btn small" title="merge into the previous movement" onClick=${() => save({ ...s, movements: s.movements.filter((_, j) => j !== i) })}>×</button>` : html`<span></span>`}
      </div>`)}
      <div class="hint">Starts at the green-outlined bars. Meter like 3/4, 6/8, C = 4/4.</div>
    </div>
    <div class="card"><h2>Selected bar ${bar ? html`<span class="pill">${M.label(bar)}</span>` : null}</h2>
      ${bar ? html`<div class="tools">
          <button class=${"btn" + (m.repeat_start ? " on" : "")} onClick=${() => toggle("repeat_start")}>𝄆 repeat from here <kbd>[</kbd></button>
          <button class=${"btn" + (m.repeat_end ? " on" : "")} onClick=${() => toggle("repeat_end")}>𝄇 repeat back <kbd>]</kbd></button>
          ${m.repeat_end ? html`<button class=${"btn" + (m.repeat_to || pickAlt ? " on" : "")} title="some performers repeat to a different bar (e.g. Chopin op. 35: m. 5 as printed, or m. 1)"
              onClick=${() => { if (m.repeat_to) setMark("repeat_to", null); else setPickAlt(pickAlt ? null : bar); }}>${m.repeat_to ? `or back to ${M.label(m.repeat_to)} ×` : pickAlt ? "click the other target bar…" : "or back to…"}</button>` : null}
          <button class=${"btn" + (m.ending === 1 ? " on" : "")} onClick=${() => setMark("ending", m.ending === 1 ? null : 1)}>1st ending <kbd>1</kbd></button>
          <button class=${"btn" + (m.ending === 2 ? " on" : "")} onClick=${() => setMark("ending", m.ending === 2 ? null : 2)}>2nd ending <kbd>2</kbd></button>
          <button class=${"btn" + (m.fine ? " on" : "")} onClick=${() => toggle("fine")}>Fine <kbd>F</kbd></button>
          <button class=${"btn" + (m.dc ? " on" : "")} onClick=${() => toggle("dc")}>D.C. <kbd>D</kbd></button>
          <button class=${"btn" + (m.ds ? " on" : "")} onClick=${() => toggle("ds")}>D.S.</button>
          <button class=${"btn" + (m.segno ? " on" : "")} onClick=${() => toggle("segno")}>𝄋 segno</button>
          <button class=${"btn" + (m.to_coda ? " on" : "")} onClick=${() => toggle("to_coda")}>to coda</button>
          <button class=${"btn" + (m.coda ? " on" : "")} onClick=${() => toggle("coda")}>𝄌 coda</button>
          <button class="btn" onClick=${newMovement}>New movement here <kbd>N</kbd></button>
          <button class=${"btn" + (M.deleted.has(bar) ? " on" : "")} onClick=${delBar} title="for something that was mistaken for a bar">Not a bar <kbd>Del</kbd></button>
          <button class=${"btn" + (editSystem ? " on" : "")} onClick=${() => { const b = M.byId[bar]; setEditSystem(editSystem ? null : { page: b.page, idx: b.system }); }}>Fix barlines in this line <kbd>B</kbd></button>
          <label class="hint row">Meter change here <${Field} style="width:60px" value=${m.meter || ""} placeholder="—" onSave=${v => setMark("meter", v.trim())} /></label>
        </div>
        ${editSystem ? html`<p class="hint">Barline editing: click inside the line to add a barline, drag a red line to move it, right-click it to remove it. Drag the blue lines to fix the line's top and bottom. Esc when done.</p>` : null}
        <div class="row" style="margin-top:6px">${(() => { const b = M.byId[bar]; const ids = M.bars.filter(x => x.page === b.page && x.system === b.system).map(x => x.id); const allGone = ids.every(i => M.deleted.has(i));
            return html`<button class="btn small" title="for a line that isn't music (a heading, a stray staff): all its bars become 'not a bar'" onClick=${async () => { const r = await api.send("POST", `/api/pieces/${pid}/line/remove`, { page: b.page, idx: b.system }); toast(r.removed ? "Line removed (the same button restores it)" : "Line restored"); reload(); }}>${allGone ? "Restore this line" : "Remove this line"}</button>`; })()}
          <span class="spacer"></span></div>`
        : html`<div class="hint">Click a bar on the score. Marks are applied to the selected bar: "repeat back" and endings go on the last bar of a section.</div>`}
    </div>
    <div class="card"><h2>Lines</h2><button class=${"btn small" + (addLine ? " on" : "")} onClick=${() => setAddLine(!addLine)}>${addLine ? "Drawing: drag a box around the line… (Esc)" : "Add a missed line"}</button>
      <div class="hint" style="margin-top:4px">If the reader skipped a whole line (or read one badly), draw a box around it, both staves: it is read on its own and replaces what was there.</div></div>
    <div class="card grow"><h2>Checks</h2><div class="hint">Every change re-aligns this piece's recordings in the background, so each recording's repeats, D.C. and cuts are found from the audio itself.
      ${dirty ? html`<div class="row" style="margin-top:6px"><span class="spin"></span> saving…</div>` : null}</div>
      <${CheckList} M=${M} data=${data} reload=${reload} setEditSystem=${setEditSystem} goLine=${goLine} />
      <div class="row" style="margin-top:auto"><button class="btn small" onClick=${async () => { if (confirm("Read the score again from scratch? Structure marks will be reset.")) { await api.send("POST", `/api/pieces/${pid}/reread`); reload(); } }}>Read the score again</button></div>
    </div>`;
}

render(html`<${App} />`, document.getElementById("app"));
