// Playback engine: one <audio> per recording, bar-accurate switching, loops, click track, speed.
// Recording positions are described by occurrences: [barId, pass, tStart, tEnd, spread] in time order.

export class Player {
  constructor() {
    this.recs = new Map();      // id -> {id, audio(url), occ, name}
    this.els = new Map();       // id -> HTMLAudioElement
    this.act = null; this.prev = null;
    this.rate = 1; this.click = false; this.loop = null;   // loop: {a, b} bar ids (notated order)
    this.order = {};            // bar id -> notated index
    this.listeners = new Set(); this.actx = null; this.lastTick = -1; this._raf = null;
    this.follow = true;
  }
  on(fn) { this.listeners.add(fn); return () => this.listeners.delete(fn); }
  emit(kind) { for (const f of this.listeners) f(kind); }

  setPiece(recordings, barOrder) {
    const keep = new Set();
    this.order = barOrder || {};
    for (const r of recordings) {
      if (!r.audio) continue;
      const occ = (r.alignment && r.alignment.occ) || [];
      const old = this.recs.get(r.id);
      if (old && old.audio !== r.audio) { const e = this.els.get(r.id); if (e && e.paused) { this.els.delete(r.id); } }   // converted: fresh element when idle
      this.recs.set(r.id, { id: r.id, audio: r.audio, occ, name: r.name, kind: r.kind });
      keep.add(r.id);
    }
    for (const id of [...this.recs.keys()]) if (!keep.has(id)) { this.recs.delete(id); const e = this.els.get(id); if (e) { e.pause(); this.els.delete(id); } }
    if (this.act != null && !this.recs.has(this.act)) this.act = null;
    if (this.act == null) { const first = [...this.recs.values()].find(r => r.occ.length) || [...this.recs.values()][0]; this.act = first ? first.id : null; }
    this.emit("recs");
  }
  el(id) {
    let e = this.els.get(id);
    if (!e) {
      const r = this.recs.get(id); if (!r) return null;
      e = new Audio(r.audio); e.preload = "auto"; e.preservesPitch = true; e.playbackRate = this.rate;
      e.addEventListener("ended", () => this.emit("state"));
      this.els.set(id, e);
    }
    return e;
  }
  get cur() { return this.act != null ? this.el(this.act) : null; }
  get rec() { return this.act != null ? this.recs.get(this.act) : null; }
  time() { const e = this.cur; return e ? e.currentTime : 0; }
  playing() { const e = this.cur; return !!(e && !e.paused); }

  // ---- position mapping
  idxAt(r, t) {
    const o = r.occ; if (!o.length) return -1;
    let lo = 0, hi = o.length - 1;
    if (t < o[0][2]) return -1;
    while (lo < hi) { const m = (lo + hi + 1) >> 1; if (o[m][2] <= t) lo = m; else hi = m - 1; }
    if (t > o[lo][3] + 0.05) return -1;            // between runs (a pause, or not playing)
    return lo;
  }
  pos(id = this.act, t = null) {
    const r = this.recs.get(id); if (!r) return null;
    if (t == null) t = this.el(id).currentTime;
    const k = this.idxAt(r, t); if (k < 0) return { k: -1, t };
    const o = r.occ[k], f = Math.min(1, Math.max(0, (t - o[2]) / Math.max(1e-3, o[3] - o[2])));
    return { k, bar: o[0], pass: o[1], frac: f, t, spread: o[4] };
  }
  occOf(id, bar, pass) {
    const r = this.recs.get(id); if (!r) return -1;
    let k = r.occ.findIndex(o => o[0] === bar && o[1] === pass);
    if (k < 0) k = r.occ.findIndex(o => o[0] === bar);
    return k;
  }
  timeOf(id, bar, pass = 1, frac = 0) {
    const r = this.recs.get(id); const k = this.occOf(id, bar, pass);
    if (k < 0) return null; const o = r.occ[k]; return o[2] + frac * (o[3] - o[2]);
  }
  passes(id, bar) { const r = this.recs.get(id); return r ? r.occ.filter(o => o[0] === bar).length : 0; }

  // ---- transport
  ensureCtx() { try { if (!this.actx) this.actx = new (window.AudioContext || window.webkitAudioContext)(); if (this.actx.state === "suspended") this.actx.resume(); } catch (e) {} }
  seek(t, play) {
    const e = this.cur; if (!e) return; e.currentTime = Math.max(0, t); this.lastTick = -1;
    if (play) this.play(); this.emit("state");
  }
  play() { this.ensureCtx(); const e = this.cur; if (!e) return; e.playbackRate = this.rate; e.play().catch(() => {}); this.startLoop(); this.emit("state"); }
  pause() { const e = this.cur; if (e) e.pause(); this.emit("state"); }
  toggle() { this.playing() ? this.pause() : this.play(); }
  select(id, { time = null, play = null } = {}) {
    if (!this.recs.has(id)) return { ok: false };
    const was = this.playing(); let t = time;
    if (t == null) {
      if (id === this.act) t = this.time();
      else {
        const p = this.pos();
        if (p && p.k >= 0) { t = this.timeOf(id, p.bar, p.pass, p.frac); if (t == null) return { ok: false, bar: p.bar }; }
        else t = 0;
      }
    }
    if (id !== this.act) { if (this.cur) this.cur.pause(); this.prev = this.act; this.act = id; }
    this.seek(t, play == null ? was : play);
    return { ok: true };
  }
  // ---- a list of segments played one after the other, across recordings (ping-pong comparison)
  playSegments(segs) {
    this.segs = (segs || []).filter(s => this.recs.has(s.rid) && s.t1 > s.t0); this.si = 0; this.loop = null;
    if (!this.segs.length) { this.segs = null; return false; }
    const s = this.segs[0]; this.select(s.rid, { time: s.t0, play: true }); this.emit("state"); return true;
  }
  stopSegments() { if (this.segs) { this.segs = null; this.emit("state"); } }
  segment() { return this.segs ? this.segs[this.si] : null; }
  gotoBar(bar, pass = 1, play = true) {
    this.segs = null;
    const t = this.timeOf(this.act, bar, pass, 0);
    if (t == null) return false; this.seek(t, play); return true;
  }
  step(d) {
    const r = this.rec; if (!r || !r.occ.length) return;
    let k = this.pos()?.k; if (k == null || k < 0) { const t = this.time(); k = r.occ.findIndex(o => o[2] > t) - 1; if (k < -1) k = r.occ.length - 1; }
    const j = Math.min(r.occ.length - 1, Math.max(0, k + d)); this.seek(r.occ[j][2], this.playing());
  }
  setRate(x) { this.rate = Math.min(1.5, Math.max(0.4, Math.round(x * 100) / 100)); for (const e of this.els.values()) e.playbackRate = this.rate; this.emit("state"); }

  // ---- loop over a range of notated bars [a, b] in the active recording
  loopTimes() {
    if (!this.loop || !this.rec) return null;
    const r = this.rec, { a, b } = this.loop, oa = this.order[a], ob = this.order[b];
    // the occurrence of `a` nearest before the playhead (or the first one), then the matching `b` after it
    const t = this.time(); let ka = -1, first = -1;
    r.occ.forEach((o, k) => { if (o[0] !== a) return; if (first < 0) first = k; if (o[2] <= t + 0.05) ka = k; });
    if (ka < 0) ka = first;
    if (ka < 0) return null;
    let kb = -1;
    for (let k = ka; k < r.occ.length; k++) {
      const o = r.occ[k]; if (this.order[o[0]] < oa || this.order[o[0]] > ob) break;
      kb = k; if (o[0] === b) break;
    }
    if (kb < 0) return null;
    return [r.occ[ka][2], r.occ[kb][3]];
  }

  // ---- rAF loop: loop wrap, click track, UI ticks
  startLoop() { if (this._raf) return; const f = () => { this._raf = null; this.frame(); if (this.playing()) this._raf = requestAnimationFrame(f); else this.emit("tick"); }; this._raf = requestAnimationFrame(f); }
  frame() {
    const e = this.cur; if (!e) return;
    if (this.segs) {
      const sg = this.segs[this.si];
      if (sg && sg.rid === this.act && e.currentTime >= sg.t1 - 0.01) {
        this.si++;
        if (this.si >= this.segs.length) { this.segs = null; e.pause(); this.emit("state"); }
        else { const n = this.segs[this.si]; this.select(n.rid, { time: n.t0, play: true }); }
      }
    } else if (this.loop) {
      const lt = this.loopTimes();
      if (lt && (e.currentTime >= lt[1] - 0.02 || e.currentTime < lt[0] - 1.0)) { e.currentTime = lt[0]; this.lastTick = -1; }
    }
    if (this.click && this.actx && !e.paused) {
      const r = this.rec, t = e.currentTime; const k = this.idxAt(r, t); let nb;
      if (k >= 0) nb = r.occ[k][2] > t ? k : k + 1; else nb = r.occ.findIndex(o => o[2] > t);
      if (nb >= 0 && nb < r.occ.length) { const dt = r.occ[nb][2] - t; if (dt < 0.12 && dt >= 0 && this.lastTick !== nb) { this.lastTick = nb; this.tick(this.actx.currentTime + dt / e.playbackRate); } }
    }
    this.emit("tick");
  }
  tick(when) {
    const o = this.actx.createOscillator(), g = this.actx.createGain(); o.frequency.value = 1760;
    g.gain.setValueAtTime(0.0001, when); g.gain.exponentialRampToValueAtTime(0.45, when + 0.002); g.gain.exponentialRampToValueAtTime(0.0001, when + 0.04);
    o.connect(g).connect(this.actx.destination); o.start(when); o.stop(when + 0.05);
  }
}
