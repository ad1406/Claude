(() => {
const D = window.MEDTNER_DATA.pieces, T = window.MEDTNER_TEXT;
const $ = s => document.querySelector(s), $$ = s => [...document.querySelectorAll(s)];
const audio = $("#audio");
const st = { key: "nw", page: 1, bar: null, open: null, sel: null, shown: {}, stopAt: null, pending: null };
const P = () => D[st.key];
const fmt = t => { t = Math.max(0, t || 0); const m = Math.floor(t / 60), s = Math.floor(t % 60); return `${m}:${String(s).padStart(2, "0")}`; };
const idx = b => b - P().first;
const barStart = b => P().times[Math.min(Math.max(idx(b), 0), P().times.length - 1)];
const barEnd = b => P().times[Math.min(idx(b) + 1, P().times.length - 1)];
function barAt(t) {
  const ts = P().times; let lo = 0, hi = ts.length - 2;
  if (t < ts[0]) return P().first;
  while (lo < hi) { const m = (lo + hi + 1) >> 1; if (ts[m] <= t + 1e-3) lo = m; else hi = m - 1; }
  return lo + P().first;
}
const sections = k => T[k].sections.map(([a, b, l, title, mark, key, text]) => ({ a, b, l, title, mark, key, text }));
function sectionAt(b, k = st.key) {
  const s = sections(k).filter(x => x.a <= b && b <= x.b); return s.sort((x, y) => y.l - x.l || (x.b - x.a) - (y.b - y.a))[0];
}
const leaves = k => { const ss = sections(k); return ss.filter(s => s.l === 2 || !ss.some(o => o.l === 2 && o.a >= s.a && o.b <= s.b)); };
const motif = (id, k = st.key) => D[k].motifs.find(m => m.id === id);

// ---------- text markup ----------
function rich(html, k) {
  return html
    .replace(/\{m:(\w+)\}/g, (_, id) => { const m = motif(id, k); return m ? `<span class="mc" data-motif="${id}" data-piece="${k}" style="background:${m.color}">${m.name}</span>` : id; })
    .replace(/\{(?:(nw|mi):)?(\d+)(?:-(\d+))?\}/g, (_, pk, a, b) => {
      const piece = pk || k, lab = (pk && pk !== k ? (pk === "nw" ? "NW " : "Min. ") : "") + (b ? `${a}–${b}` : a);
      return `<span class="bl" data-piece="${piece}" data-a="${a}" data-b="${b || a}">${b ? "bb." : "b."} ${lab}</span>`;
    });
}
document.addEventListener("click", e => {
  const bl = e.target.closest(".bl");
  if (bl) { go(bl.dataset.piece, +bl.dataset.a, +bl.dataset.b); return; }
  const mc = e.target.closest(".mc");
  if (mc) { if (mc.dataset.piece !== st.key) setPiece(mc.dataset.piece); showMotif(mc.dataset.motif); }
});

// ---------- piece switching ----------
function setTab(tab) {
  $$("#tabs button").forEach(b => b.classList.toggle("on", b.dataset.tab === tab));
  const cmp = tab === "compare";
  $("#piece").hidden = cmp; $("#compare").hidden = !cmp; $("#piece").style.display = cmp ? "none" : "";
  if (cmp) { drawCompare(); return; }
  setPiece(tab);
}
function setPiece(k) {
  $$("#tabs button").forEach(b => b.classList.toggle("on", b.dataset.tab === k));
  $("#piece").hidden = false; $("#piece").style.display = ""; $("#compare").hidden = true;
  if (st.key === k && audio.src) return;
  st.key = k; st.bar = null; st.sel = null; st.open = null;
  if (!st.shown[k]) { st.shown[k] = {}; P().motifs.forEach(m => st.shown[k][m.id] = !["turn", "lament", "ddrhythm", "knock", "descent"].includes(m.id)); }
  audio.src = P().audio.split("/").map(encodeURIComponent).join("/").replace(/^\.\./, "..");
  $("#gotobar").min = P().first; $("#gotobar").max = P().first + P().bars.length - 1;
  renderForm(); renderMotifs(); renderEssays(); showPage(1); drawTimeline(); update(true);
}
$$("#tabs button").forEach(b => b.onclick = () => setTab(b.dataset.tab));
$$("#subtabs button").forEach(b => b.onclick = () => sub(b.dataset.sub));
function sub(name) {
  $$("#subtabs button").forEach(b => b.classList.toggle("on", b.dataset.sub === name));
  $$(".sub").forEach(s => s.classList.toggle("on", s.id === "sub-" + name));
}

// ---------- seeking ----------
function seek(t, play = true) {
  st.stopAt = null;
  const doit = () => { audio.currentTime = Math.max(0, t); if (play) audio.play().catch(() => {}); update(true); };
  if (audio.readyState >= 1) doit(); else { st.pending = doit; audio.load(); }
}
audio.addEventListener("loadedmetadata", () => { if (st.pending) { const f = st.pending; st.pending = null; f(); } $("#dur").textContent = fmt(audio.duration); });
function go(k, a, b) {
  if (k !== st.key) setPiece(k);
  st.sel = [a, b || a]; seek(barStart(a) - 0.05); showBar(a); renderOverlay();
}
function playSolo(h) { st.sel = [h.bar, h.bar_end]; seek(h.a0 - 0.15); st.stopAt = h.a1 + 0.5; showBar(h.bar); renderOverlay(); }

// ---------- score ----------
function pageOfBar(b) { return P().bars[Math.max(0, Math.min(idx(b), P().bars.length - 1))][0]; }
function showPage(p) {
  const n = P().pages.length; p = Math.max(1, Math.min(n, p));
  if (p === st.page && $("#pageimg").dataset.k === st.key) return;
  st.page = p; const pg = P().pages[p - 1];
  $("#pageimg").src = pg.src; $("#pageimg").dataset.k = st.key;
  const bars = P().bars.map((x, i) => i).filter(i => P().bars[i][0] === p);
  $("#pagelabel").textContent = `page ${p} / ${n}` + (bars.length ? ` · bars ${bars[0] + P().first}–${bars[bars.length - 1] + P().first}` : "");
  renderOverlay();
}
$("#prevpage").onclick = () => { $("#follow").checked = false; showPage(st.page - 1); };
$("#nextpage").onclick = () => { $("#follow").checked = false; showPage(st.page + 1); };
function hitsByBar() {
  const map = {}; const k = st.key;
  P().motifs.forEach((m, mi) => { if (!st.shown[k][m.id] && st.open !== m.id) return;
    m.hits.forEach(h => { for (let b = h.bar; b <= h.bar_end; b++) (map[b] = map[b] || new Map()).set(m.id, m); }); });
  return map;
}
function renderOverlay() {
  const ov = $("#overlay"); ov.innerHTML = ""; const img = $("#pageimg");
  const W = img.clientWidth, H = img.clientHeight; if (!W) return;
  ov.style.width = W + "px"; ov.style.height = H + "px";
  const hb = hitsByBar();
  P().bars.forEach((bx, i) => {
    if (bx[0] !== st.page) return;
    const b = i + P().first, d = document.createElement("div");
    d.className = "bx" + (b === st.bar ? " now" : "") + (st.sel && b >= st.sel[0] && b <= st.sel[1] ? " sel" : "");
    Object.assign(d.style, { left: bx[1] * W + "px", top: bx[2] * H + "px", width: (bx[3] - bx[1]) * W + "px", height: (bx[4] - bx[2]) * H + "px" });
    d.dataset.bar = b;
    const ms = hb[b] ? [...hb[b].values()] : [];
    d.title = `bar ${b}` + (ms.length ? " · " + ms.map(m => m.name).join(", ") : "");
    d.innerHTML = `<span class="num">${b}</span><div class="strips">${ms.map(m => `<div class="strip" style="background:${m.color}"></div>`).join("")}</div>`;
    d.onclick = () => { st.sel = null; seek(barStart(b) - 0.05); showBar(b); };
    ov.appendChild(d);
  });
}
$("#pageimg").addEventListener("load", renderOverlay);
window.addEventListener("resize", () => { renderOverlay(); drawTimeline(); if (!$("#compare").hidden) drawCompare(); });

// ---------- panels ----------
function renderForm() {
  const t = T[st.key];
  $("#sub-form").innerHTML = `<p class="lede">${rich(t.lede, st.key)}</p>` + sections(st.key).map((s, i) =>
    `<div class="sec l${s.l}" data-i="${i}"><${s.l === 1 ? "h3" : "h4"} data-a="${s.a}">${s.title}</${s.l === 1 ? "h3" : "h4"}>
     <div class="meta">bb. ${s.a}–${s.b}${s.mark ? " · " + s.mark : ""}${s.key ? " · " + s.key : ""}</div>
     <div>${rich(s.text, st.key)}</div></div>`).join("");
  $$("#sub-form .sec h3, #sub-form .sec h4").forEach(h => h.onclick = () => go(st.key, +h.dataset.a));
}
function renderEssays() {
  $("#sub-essays").innerHTML = T[st.key].essays.map(([h, x]) => `<div class="essay"><h3>${h}</h3>${rich(x, st.key)}</div>`).join("");
}
function renderMotifs() {
  const k = st.key;
  $("#sub-motifs").innerHTML = `<div class="tools"><button id="mall">show all on score</button><button id="mnone">hide all</button>
    <span class="lede">Coloured strips under the bars mark motif statements. Found automatically in the MIDI (prime, inversion, transposition, augmentation).</span></div>` +
    P().motifs.map(m => `<div class="motif" id="mot-${m.id}"><header><input type="checkbox" ${st.shown[k][m.id] ? "checked" : ""} data-m="${m.id}" title="show on score">
      <span class="sw" style="background:${m.color}"></span><span class="nm">${m.name}</span><span class="ct">${m.hits.length} statements · ${m.kind}</span></header>
      <p>${rich(m.desc, k)}</p><div class="tools"><button data-list="${m.id}">list all statements</button></div><div class="hits" hidden></div></div>`).join("");
  $$("#sub-motifs input[type=checkbox]").forEach(c => c.onchange = () => { st.shown[k][c.dataset.m] = c.checked; renderOverlay(); drawTimeline(); });
  $("#mall").onclick = () => { P().motifs.forEach(m => st.shown[k][m.id] = true); renderMotifs(); renderOverlay(); drawTimeline(); };
  $("#mnone").onclick = () => { P().motifs.forEach(m => st.shown[k][m.id] = false); renderMotifs(); renderOverlay(); drawTimeline(); };
  $$("#sub-motifs [data-list]").forEach(b => b.onclick = () => showMotif(b.dataset.list, false));
}
function showMotif(id, jump = true) {
  sub("motifs"); st.open = id; const m = motif(id); if (!m) return;
  $$(".motif").forEach(x => { x.classList.toggle("open", x.id === "mot-" + id); x.querySelector(".hits").hidden = x.id !== "mot-" + id; });
  const box = $(`#mot-${id} .hits`);
  box.innerHTML = m.hits.map((h, i) => `<div class="hit" data-i="${i}"><span>b. ${h.bar}${h.bar_end !== h.bar ? "–" + h.bar_end : ""}</span><span>${h.hand}</span>
    <span>${h.form}${h.aug ? " · " + h.aug.slice(0, 3) + "." : ""}${h.q === "tonal" ? "*" : ""}</span><span class="ps">${h.pitches.join(" ")}</span>
    <button class="solo" title="play just this statement">▶</button></div>`).join("") +
    `<p class="lede" style="font-size:12px">* intervals match by size class (diatonic or chromatic variant), not exactly.</p>`;
  $$(`#mot-${id} .hit`).forEach(r => {
    const h = m.hits[+r.dataset.i];
    r.onclick = e => { if (e.target.classList.contains("solo")) playSolo(h); else { st.sel = [h.bar, h.bar_end]; seek(h.a0 - 0.15); showBar(h.bar); renderOverlay(); } };
  });
  document.getElementById("mot-" + id).scrollIntoView({ block: "start" });
  renderOverlay(); drawTimeline();
  if (jump && m.hits.length) showPage(pageOfBar(m.hits[0].bar));
}
function showBar(b) {
  const s = sections(st.key).filter(x => x.a <= b && b <= x.b);
  const ms = P().motifs.map(m => [m, m.hits.filter(h => h.bar <= b && b <= h.bar_end)]).filter(x => x[1].length);
  const t0 = barStart(b), t1 = barEnd(b), tempo = P().tempo[idx(b)];
  $("#sub-bar").innerHTML = `<h3>Bar ${b}</h3><dl>
    <dt>recording</dt><dd>${fmt(t0)}–${fmt(t1)} (${(t1 - t0).toFixed(2)} s)${tempo ? ` · ${tempo >= 1 ? "faster" : "slower"} than the MIDI tempo ×${tempo.toFixed(2)}` : ""}</dd>
    <dt>score</dt><dd>page ${pageOfBar(b)}</dd>
    <dt>section</dt><dd>${s.map(x => `<span class="bl" data-piece="${st.key}" data-a="${x.a}" data-b="${x.b}">${x.title}</span>`).join(" › ") || "—"}</dd>
    <dt>chromatic sat.</dt><dd>${P().chroma[idx(b)]}</dd>
    <dt>motifs</dt><dd>${ms.map(([m, hs]) => `<span class="mc" data-motif="${m.id}" data-piece="${st.key}" style="background:${m.color}">${m.name}</span> ` +
      hs.map(h => `${h.hand} ${h.form} ${h.pitches.join("–")}`).join("; ")).join("<br>") || "—"}</dd></dl>`;
}

// ---------- timeline ----------
function cssv(n) { return getComputedStyle(document.documentElement).getPropertyValue(n).trim(); }
function drawTimeline() {
  const c = $("#timeline"), r = devicePixelRatio || 1, W = c.clientWidth, H = 96; c.width = W * r; c.height = H * r;
  const g = c.getContext("2d"); g.scale(r, r); g.clearRect(0, 0, W, H);
  const p = P(), T0 = 0, T1 = p.duration || p.times[p.times.length - 1], X = t => (t - T0) / (T1 - T0) * W;
  const secs = sections(st.key), pal = ["#e9dccb", "#d8e2dc", "#e4d6e8", "#dbe6ef", "#efe0d0", "#e2e8d0"];
  const dark = matchMedia("(prefers-color-scheme: dark)").matches && document.documentElement.dataset.theme !== "light";
  let j = 0;
  leaves(st.key).forEach(s => {
    const x0 = X(barStart(s.a)), x1 = X(barEnd(s.b)); g.fillStyle = pal[j++ % pal.length]; g.globalAlpha = dark ? .22 : .9; g.fillRect(x0, 0, x1 - x0, 22); g.globalAlpha = 1;
    g.fillStyle = cssv("--ink"); g.font = "11px system-ui, sans-serif"; g.save(); g.beginPath(); g.rect(x0, 0, x1 - x0 - 2, 22); g.clip(); g.fillText(s.title, x0 + 4, 15); g.restore();
  });
  secs.filter(s => s.l === 1).forEach(s => { g.fillStyle = cssv("--accent"); g.fillRect(X(barStart(s.a)), 0, 2, H); });
  // tempo (recording vs MIDI), log scale, smoothed
  const tp = p.tempo.map(x => x ? Math.log2(x) : 0), sm = tp.map((_, i) => { let s = 0, n = 0; for (let k = i - 3; k <= i + 3; k++) if (k >= 0 && k < tp.length) { s += tp[k]; n++; } return s / n; });
  g.strokeStyle = cssv("--muted"); g.lineWidth = 1; g.beginPath();
  sm.forEach((v, i) => { const x = X((p.times[i] + p.times[i + 1]) / 2), y = 52 - Math.max(-1, Math.min(1, v)) * 16; i ? g.lineTo(x, y) : g.moveTo(x, y); }); g.stroke();
  g.fillStyle = cssv("--muted"); g.font = "10px system-ui, sans-serif"; g.fillText("tempo vs MIDI", 4, 34);
  g.setLineDash([2, 3]); g.beginPath(); g.moveTo(0, 52); g.lineTo(W, 52); g.stroke(); g.setLineDash([]);
  // motif ticks
  const shown = p.motifs.filter(m => st.shown[st.key][m.id] || st.open === m.id);
  shown.forEach((m, i) => { const y = 72 + (i % 4) * 6; g.fillStyle = m.color; m.hits.forEach(h => g.fillRect(X(h.a0), y, Math.max(2, X(h.a1) - X(h.a0)), 5)); });
  const t = audio.currentTime || 0; g.fillStyle = cssv("--nowline"); g.fillRect(X(t) - 1, 0, 2, H);
}
$("#timeline").onclick = e => { const r = e.currentTarget.getBoundingClientRect(); const T1 = P().duration; seek((e.clientX - r.left) / r.width * T1); };

// ---------- transport ----------
$("#play").onclick = () => audio.paused ? audio.play() : audio.pause();
audio.onplay = () => $("#play").textContent = "❚❚"; audio.onpause = () => $("#play").textContent = "▶";
$("#rate").onchange = e => audio.playbackRate = +e.target.value;
$("#gotobar").onchange = e => { const b = +e.target.value; if (!isNaN(b)) go(st.key, b); };
document.addEventListener("keydown", e => {
  if (e.target.tagName === "INPUT" || e.target.tagName === "SELECT") return;
  if (e.code === "Space") { e.preventDefault(); $("#play").click(); }
  if (e.code === "ArrowRight") { e.preventDefault(); go(st.key, Math.min((st.bar ?? P().first) + 1, P().first + P().bars.length - 1)); }
  if (e.code === "ArrowLeft") { e.preventDefault(); go(st.key, Math.max((st.bar ?? P().first) - 1, P().first)); }
  if (e.code === "PageDown") { e.preventDefault(); $("#nextpage").click(); }
  if (e.code === "PageUp") { e.preventDefault(); $("#prevpage").click(); }
});
let lastSec = null;
function update(force) {
  const t = audio.currentTime || 0;
  if (st.stopAt && t >= st.stopAt) { audio.pause(); st.stopAt = null; }
  if (st.sel && $("#loopsel").checked && t >= barEnd(st.sel[1])) { audio.currentTime = barStart(st.sel[0]); }
  $("#now").textContent = fmt(t);
  const b = barAt(t);
  if (b !== st.bar || force) {
    st.bar = b; $("#barlabel").textContent = "bar " + b;
    const s = sectionAt(b); $("#seclabel").textContent = s ? s.title : "";
    if ($("#follow").checked) showPage(pageOfBar(b));
    $$(".bx").forEach(x => x.classList.toggle("now", +x.dataset.bar === b));
    const si = s ? sections(st.key).indexOf(s) : -1;
    if (si !== lastSec || force) { lastSec = si; $$("#sub-form .sec").forEach(x => x.classList.toggle("active", +x.dataset.i === si)); }
    if ($("#sub-bar").classList.contains("on")) showBar(b);
    $$(".hit").forEach(x => x.classList.remove("now"));
  }
  drawTimeline();
}
audio.addEventListener("timeupdate", () => update(false));
setInterval(() => { if (!audio.paused) update(false); }, 120);

// ---------- compare ----------
function drawCompare() {
  $("#cmptext").innerHTML = rich(T.compare, "");
  const keys = ["nw", "mi"], names = { nw: "Night Wind", mi: "Minacciosa" };
  const pal = ["#e9dccb", "#d8e2dc", "#e4d6e8", "#dbe6ef", "#efe0d0", "#e2e8d0"];
  const c = $("#cmpform"), r = devicePixelRatio || 1, W = c.clientWidth, H = 180; c.width = W * r; c.height = H * r;
  const g = c.getContext("2d"); g.scale(r, r); g.clearRect(0, 0, W, H); g.font = "11px system-ui, sans-serif";
  const L = 90;
  keys.forEach((k, row) => {
    const p = D[k], T1 = p.times[p.times.length - 1], X = t => L + t / T1 * (W - L - 10), y = 8 + row * 86;
    g.fillStyle = cssv("--ink"); g.fillText(names[k], 6, y + 22);
    let j = 0, n1 = 0;
    const tt = s => [p.times[s.a - p.first], p.times[Math.min(s.b - p.first + 1, p.times.length - 1)]];
    sections(k).filter(s => s.l === 1).forEach(s => { const [t0] = tt(s); g.fillStyle = cssv("--accent"); g.fillRect(X(t0), y + 4, 2, 40);
      g.fillStyle = cssv("--muted"); g.fillText(`${Math.round(t0 / T1 * 100)}%`, X(t0) + 3, y + 54 + (n1++ % 2) * 11); });
    leaves(k).forEach(s => {
      const [t0, t1] = tt(s);
      { g.fillStyle = pal[j++ % pal.length]; g.fillRect(X(t0), y + 6, X(t1) - X(t0) - 1, 30);
        g.save(); g.beginPath(); g.rect(X(t0), y + 6, X(t1) - X(t0) - 3, 30); g.clip(); g.fillStyle = "#222"; g.fillText(s.title, X(t0) + 3, y + 25); g.restore(); }
    });

  });
  // chroma
  const c2 = $("#cmpchroma"), H2 = 150; c2.width = W * r; c2.height = H2 * r; const g2 = c2.getContext("2d"); g2.scale(r, r); g2.clearRect(0, 0, W, H2);
  const cols = { nw: "#00798c", mi: "#d1495b" }; g2.font = "11px system-ui, sans-serif";
  const lo = .55, hi = 1, Y = v => 10 + (1 - (v - lo) / (hi - lo)) * (H2 - 30);
  keys.forEach((k, ki) => {
    const p = D[k], n = p.chroma.length; g2.strokeStyle = cols[k]; g2.lineWidth = 1.4; g2.beginPath();
    p.chroma.forEach((v, i) => { const x = 30 + i / (n - 1) * (W - 40); i ? g2.lineTo(x, Y(v)) : g2.moveTo(x, Y(v)); }); g2.stroke();
    const mean = p.chroma.reduce((a, b) => a + b, 0) / n; g2.fillStyle = cols[k]; g2.fillText(`${names[k]} (mean ${mean.toFixed(2)})`, 34 + ki * 200, H2 - 6);
  });
  g2.fillStyle = cssv("--muted"); [.6, .8, 1].forEach(v => g2.fillText(v.toFixed(1), 4, Y(v) + 4));
  // motif density
  const c3 = $("#cmpdensity"), H3 = 150; c3.width = W * r; c3.height = H3 * r; const g3 = c3.getContext("2d"); g3.scale(r, r); g3.clearRect(0, 0, W, H3);
  g3.font = "11px system-ui, sans-serif";
  const dens = keys.map(k => { const p = D[k], T1 = p.times[p.times.length - 1], b = Array(10).fill(0);
    p.motifs.forEach(m => m.hits.forEach(h => b[Math.min(9, Math.floor(h.a0 / T1 * 10))]++));
    const n = b.reduce((x, y) => x + y, 0); return b.map(v => v / n * 100); });
  const mx = Math.max(...dens.flat()), bw = (W - 40) / 10;
  dens.forEach((b, ki) => b.forEach((v, i) => { g3.fillStyle = cols[keys[ki]]; const h = v / mx * (H3 - 34); g3.fillRect(30 + i * bw + ki * bw / 2.2 + 2, H3 - 20 - h, bw / 2.4, h); }));
  keys.forEach((k, ki) => { g3.fillStyle = cols[k]; g3.fillRect(W - 210 + ki * 100, 8, 10, 10); g3.fillText(names[k], W - 196 + ki * 100, 17); });
  g3.fillStyle = cssv("--muted"); for (let i = 0; i < 10; i++) g3.fillText(`${i * 10}%`, 30 + i * bw + 2, H3 - 6);
}

setTab("nw");
})();
