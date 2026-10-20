// A Web Audio metronome with accents, regrouping, polyrhythm grids and a tempo ladder,
// plus the permutation generators used by the practice helpers. No UI here.

export class Metronome {
  constructor() {
    this.ctx = null; this.on = false; this.bpm = 80; this.beats = 4; this.sub = 1;
    this.mode = "beat";            // "beat" | "regroup" | "poly"
    this.group = 3;                // regroup: accent every `group` sub-ticks
    this.poly = { p: 3, q: 2, voice: "both", vary: 0 };   // voice: both | p | q | grid ; vary: rotate voice every n cycles (0 = off)
    this.ladder = null;            // {step, every (bars), goal}
    this.feel = "all";             // "all" | "bar" (downbeat only) | "offbeat" (only the "ands") : you supply the rest
    this.gap = null;               // {on, off}: click `on` bars, then `off` silent bars (are you still with it?)
    this.listeners = new Set(); this._timer = null; this._next = 0; this._i = 0; this._bar = 0; this.volume = 0.6;
  }
  onChange(fn) { this.listeners.add(fn); return () => this.listeners.delete(fn); }
  emit() { for (const f of this.listeners) f(this); }
  ensure() { if (!this.ctx) this.ctx = new (window.AudioContext || window.webkitAudioContext)(); if (this.ctx.state === "suspended") this.ctx.resume(); }
  set(opts) { Object.assign(this, opts); if (opts.bpm) this.bpm = Math.max(20, Math.min(320, Math.round(opts.bpm))); this.emit(); }
  start() { this.ensure(); if (this.on) return; this.on = true; this._i = 0; this._bar = 0; this._next = this.ctx.currentTime + 0.06; this._timer = setInterval(() => this._schedule(), 25); this.emit(); }
  stop() { this.on = false; clearInterval(this._timer); this._timer = null; this.emit(); }
  toggle() { this.on ? this.stop() : this.start(); }

  // ticks per cycle and seconds per tick. Poly: one cycle = max(p,q) beats, so the faster voice follows the beat.
  _grid() {
    if (this.mode === "poly") { const { p, q } = this.poly; const n = lcm(p, q); return { n, dt: 60 / this.bpm * Math.max(p, q) / n }; }
    return { n: this.beats * this.sub, dt: 60 / this.bpm / this.sub };
  }
  // level of tick i within a cycle: 0 silent, 1 faint, 2 beat, 3 accent; returns [level, pitch]
  level(i) {
    if (this.mode === "poly") {
      const { p, q } = this.poly; const n = lcm(p, q); const j = i % n; let voice = this.poly.voice;
      if (this.poly.vary) { const vs = ["both", "p", "q"]; voice = vs[Math.floor(this._bar / this.poly.vary) % vs.length]; }
      const onP = j % (n / p) === 0, onQ = j % (n / q) === 0;
      if (j === 0) return [3, 1500];
      if (voice === "grid") return [1, 900];
      if ((voice === "both" || voice === "p") && onP) return [2, 1200];
      if ((voice === "both" || voice === "q") && onQ) return [2, 800];
      return [0, 0];
    }
    const n = this.beats * this.sub; const j = i % n;
    if (this.gap && this.gap.on && (this._bar % (this.gap.on + this.gap.off)) >= this.gap.on) return [0, 0];
    if (this.mode === "beat" && this.feel === "bar") return j === 0 ? [3, 1500] : [0, 0];
    if (this.mode === "beat" && this.feel === "offbeat") { const h = Math.max(1, Math.floor(this.sub / 2)); return this.sub >= 2 && j % this.sub === h ? [2, 1100] : [0, 0]; }
    if (this.mode === "regroup") return j % this.group === 0 ? [3, 1500] : (j % this.sub === 0 ? [1, 900] : [0, 0]);
    if (j === 0) return [3, 1500];
    if (j % this.sub === 0) return [2, 1000];
    return [1, 800];
  }
  _schedule() {
    if (!this.on) return;
    const ctx = this.ctx;
    while (this._next < ctx.currentTime + 0.12) {
      const { n, dt } = this._grid(); const j = this._i % n;
      if (j === 0 && this._i > 0) this._cycle();
      const [lv, f] = this.level(this._i);
      if (lv > 0) this._click(this._next, f, [0, 0.12, 0.3, 0.55][lv] * this.volume);
      this._next += dt; this._i++;
    }
  }
  _cycle() {
    this._bar++;
    const L = this.ladder;
    if (L && L.step && L.every && this._bar % L.every === 0) {
      const goal = L.goal || 320; const nb = L.step > 0 ? Math.min(goal, this.bpm + L.step) : Math.max(goal, this.bpm + L.step);
      if (nb !== this.bpm) { this.bpm = nb; setTimeout(() => this.emit(), 0); }
    }
    setTimeout(() => this.emit(), 0);
  }
  _click(when, freq, gain) {
    const o = this.ctx.createOscillator(), g = this.ctx.createGain(); o.frequency.value = freq; o.type = "square";
    g.gain.setValueAtTime(0.0001, when); g.gain.exponentialRampToValueAtTime(gain, when + 0.002); g.gain.exponentialRampToValueAtTime(0.0001, when + 0.035);
    o.connect(g).connect(this.ctx.destination); o.start(when); o.stop(when + 0.05);
  }
}

export function gcd(a, b) { a = Math.abs(a | 0); b = Math.abs(b | 0); while (b) [a, b] = [b, a % b]; return a || 1; }
export function lcm(a, b) { return Math.abs(a * b) / gcd(a, b) || 1; }

// ---------------------------------------------------------------- permutation generators
// Burst permutations: bursts of L fast notes ending on a stop ("."), with the stop shifted so each note is once a landing point.
//   bursts(4) -> ["ffff.", "f.ffff.", "ff.ffff.", "fff.ffff."]
export function bursts(L) {
  const out = []; for (let r = 0; r < L; r++) out.push((r ? "f".repeat(r) + "." : "") + "f".repeat(L) + "."); return out;
}
// Fast/slow continuous permutations over groups of m notes: every rotation of f^k s^(m-k), k = 1..m-1.
//   fastSlow(2) -> ["fs", "sf"];  fastSlow(3) -> ["fss","ssf","sfs","ffs","fsf","sff"]
export function fastSlow(m) {
  const seen = new Set(), out = [];
  for (let k = 1; k < m; k++) { const base = "f".repeat(k) + "s".repeat(m - k); for (let r = 0; r < m; r++) { const s = base.slice(r) + base.slice(0, r); if (!seen.has(s)) { seen.add(s); out.push(s); } } }
  return out;
}
// Soft/loud permutations: one loud note per group of m, rotated (Lsss, sLss, ssLs, sssL); `two` adds two-loud patterns.
export function softLoud(m, two = false) {
  const out = []; for (let r = 0; r < m; r++) out.push("s".repeat(r) + "L" + "s".repeat(m - r - 1));
  if (two && m >= 3) for (let a = 0; a < m; a++) for (let b = a + 1; b < m; b++) { if (b - a === 1) continue; const s = [..."s".repeat(m)]; s[a] = s[b] = "L"; out.push(s.join("")); }
  return out;
}
// Chaining steps over n bars (indices into the passage): backward = [[n-1,n-1],[n-2,n-1],...]; focal alternates around f.
export function chainSteps(n, kind = "back", f = 0) {
  if (n <= 0) return [];
  if (kind === "back") { const out = []; for (let s = n - 1; s >= 0; s--) out.push([s, n - 1]); return out; }
  const out = [[f, f]]; let lo = f, hi = f, left = true;
  while (lo > 0 || hi < n - 1) { if ((left && lo > 0) || hi >= n - 1) lo--; else hi++; left = !left; out.push([lo, hi]); }
  return out;
}
