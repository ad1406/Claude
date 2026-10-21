(() => {
const D = window.NW, T = window.NW_TEXT;
const $ = s => document.querySelector(s), $$ = s => [...document.querySelectorAll(s)];
const audio = $("#audio");
const NS = "http://www.w3.org/2000/svg";
const FAM = Object.fromEntries(D.families.map((f, i) => [f.id, Object.assign(f, { i })]));
const MOT = D.motifs.map((m, i) => Object.assign(m, { i, color: FAM[m.family].color }));
const N = D.notes;                         // [page, x, y, t, dur, pitch, hand, bar, tied]
const H = D.hits;
const NAMES = ["C", "C♯", "D", "E♭", "E", "F", "F♯", "G", "A♭", "A", "B♭", "B"];
const pname = p => NAMES[p % 12] + (Math.floor(p / 12) - 1);
const fmt = t => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, "0")}`;
const st = { bar: 1, sel: null, focus: null, famOn: Object.fromEntries(D.families.map(f => [f.id, true])), solo: false,
             station: 0, stopAt: null, loopRange: null, pick: null };

// ---------------- geometry: bar -> system, statement -> per-system segments
const barSys = [];
D.systems.forEach((s, k) => { for (let b = s[5]; b <= s[6]; b++) barSys[b] = k; });
H.forEach((h, k) => {
  h.k = k; const segs = {};
  for (const ni of h.n) { const n = N[ni]; if (!n[0]) continue; const sk = barSys[n[7]]; (segs[sk] = segs[sk] || []).push(ni); }
  h.segs = Object.entries(segs).map(([sk, ns]) => {
    const xs = ns.map(i => N[i][1]), ys = ns.map(i => N[i][2]);
    return { sys: +sk, page: N[ns[0]][0], x0: Math.min(...xs) - 70, x1: Math.max(...xs) + 360, y0: Math.min(...ys) - 165, y1: Math.max(...ys) + 165, notes: ns };
  }).sort((a, b) => a.sys - b.sys);
  h.partners = [];
});
D.wedges.forEach(([a, b]) => { H[a].partners.push(b); H[b].partners.push(a); });
const byPage = {}; H.forEach(h => h.segs.forEach(s => (byPage[s.page] = byPage[s.page] || []).push([h, s])));
const noteHits = {}; H.forEach(h => h.n.forEach(ni => (noteHits[ni] = noteHits[ni] || []).push(h.k)));
const visible = h => st.famOn[MOT[h.m].family] && (!st.solo || st.sel === null || h.m === st.sel);
const stations = T.stations.map(([a, b, title, mark, text], i) => ({ a, b, title, mark, text, i }));
const stationAt = bar => { let s = 0; stations.forEach((x, i) => { if (x.a <= bar) s = i; }); return s; };
const barT = b => D.bar_t[Math.max(0, Math.min(D.bar_t.length - 1, b - 1))];
function barAt(t) { const a = D.bar_t; let lo = 0, hi = a.length - 2; if (t < a[0]) return 1;
  while (lo < hi) { const m = (lo + hi + 1) >> 1; if (a[m] <= t + 1e-3) lo = m; else hi = m - 1; } return lo + 1; }

// ---------------- rich text
function rich(html) {
  return html
    .replace(/\{m:(\w+)\}/g, (_, id) => { const m = MOT.find(x => x.id === id); return m ? `<span class="mc" data-m="${m.i}" style="background:${m.color}">${m.name}</span>` : id; })
    .replace(/\{f:(\w+)\}/g, (_, id) => { const f = FAM[id]; return f ? `<span class="fc" data-f="${id}" style="color:${f.color};border-color:${f.color}">${f.name}</span>` : id; })
    .replace(/\{(\d+)(?:-(\d+))?\}/g, (_, a, b) => `<span class="bl" data-a="${a}" data-b="${b || a}">${b ? `bb. ${a}–${b}` : `b. ${a}`}</span>`);
}
document.addEventListener("click", e => {
  const bl = e.target.closest(".bl"); if (bl) { goBar(+bl.dataset.a, true, +bl.dataset.b); return; }
  const mc = e.target.closest(".mc"); if (mc) { selectMotif(+mc.dataset.m, true); return; }
  const fc = e.target.closest(".fc"); if (fc) { soloFamily(fc.dataset.f); return; }
});

// ---------------- score pages
const score = $("#score"); const pageEls = [];
D.pages.forEach((p, i) => {
  const div = document.createElement("div"); div.className = "page"; div.dataset.p = i + 1;
  div.style.aspectRatio = `${D.page_w} / ${p.h}`;
  div.innerHTML = `<img alt="page ${i + 1}" loading="lazy" src="build/${p.src}"><span class="pno">${i + 1}</span>`;
  const ov = document.createElementNS(NS, "svg"); ov.setAttribute("class", "ov"); ov.setAttribute("viewBox", `0 0 ${D.page_w} ${p.h}`);
  div.appendChild(ov); score.appendChild(div); pageEls.push({ div, ov, drawn: false });
});
const io = new IntersectionObserver(es => es.forEach(e => { if (e.isIntersecting) drawPage(+e.target.dataset.p); }), { root: score, rootMargin: "1200px 0px" });
pageEls.forEach(p => io.observe(p.div));
function el(tag, attrs, parent) { const e = document.createElementNS(NS, tag); for (const k in attrs) e.setAttribute(k, attrs[k]); if (parent) parent.appendChild(e); return e; }
function drawPage(p) {
  const P = pageEls[p - 1]; if (P.drawn) return; P.drawn = true; const ov = P.ov;
  const gBars = el("g", {}, ov), gSeg = el("g", {}, ov), gRing = el("g", {}, ov), gLab = el("g", {}, ov); P.gDots = el("g", {}, ov); P.gPick = el("g", {}, ov);
  D.bars.forEach((b, i) => { if (b[0] !== p) return;
    const r = el("rect", { x: b[1], y: b[2] - 700, width: b[3] - b[1], height: b[4] - b[2] + 1400, class: "barhit", "data-bar": i + 1 }, gBars);
    r.addEventListener("click", ev => clickScore(ev, p, i + 1)); });
  for (const [h, s] of (byPage[p] || [])) {
    const c = MOT[h.m].color;
    const r = el("rect", { x: s.x0, y: s.y0, width: s.x1 - s.x0, height: s.y1 - s.y0, rx: 60, fill: c, stroke: c, class: "seg", "data-h": h.k }, gSeg);
    r.addEventListener("click", ev => { ev.stopPropagation(); clickScore(ev, p, N[s.notes[0]][7], h.k); });
    const tt = el("title", {}, r); tt.textContent = describe(h, true);
    for (const ni of s.notes) el("ellipse", { cx: N[ni][1] + 145, cy: N[ni][2], rx: 175, ry: 128, stroke: c, class: "ring", "data-h": h.k }, gRing);
    if (s === h.segs[0]) { const t = el("text", { x: s.x0 + 10, y: s.y0 - 40, fill: c, class: "seglabel", "data-h": h.k }, gLab); t.textContent = MOT[h.m].name + formTag(h); }
  }
  refreshClasses(p);
}
function formTag(h) {
  const f = { prime: "", inversion: " · inv.", retrograde: " · retr.", "retrograde inversion": " · RI" }[h.form] || "";
  return f + (h.aug ? (h.aug === "augmented" ? " · aug." : " · dim.") : "") + (h.level !== "surface" ? " · structural" : "");
}
function describe(h, plain) {
  const m = MOT[h.m];
  const lvl = h.level === "surface" ? "on the surface" : `in the structural line (${h.level === "bar" ? "one tone per bar" : h.level === "0.5" ? "one tone per half beat" : `one tone per ${h.level === "1" ? "" : h.level + " "}beat${h.level === "1" ? "" : "s"}`})`;
  const mode = { exact: "exact intervals", tonal: "tonal variant (±1 semitone)", contour: "contour (step / skip / leap)", octave: "octave-equivalent" }[h.mode];
  const hand = h.hand.length === 2 ? "both hands" : h.hand[0] === 1 ? "right hand" : "left hand";
  const s = `${m.name}: ${h.form}, ${mode}${h.aug ? ", " + h.aug : ""}, ${lvl}, ${hand}; bb. ${h.bar}${h.bar_end !== h.bar ? "–" + h.bar_end : ""}; ${h.p.map(pname).join(" ")}`;
  return s;
}
function refreshClasses(p) {
  const P = pageEls[p - 1]; if (!P || !P.drawn) return;
  const t = audio.currentTime || 0;
  P.ov.querySelectorAll("[data-h]").forEach(e => {
    const h = H[+e.dataset.h]; const v = visible(h);
    e.classList.toggle("dim", !v);
    e.classList.toggle("sel", v && st.sel === h.m);
    e.classList.toggle("on", v && t >= h.t0 && t <= h.t1);
    e.classList.toggle("focus", st.focus === h.k);
    e.classList.toggle("partner", st.focus !== null && H[st.focus].partners.includes(h.k));
    if (e.tagName === "text") e.style.display = v && (st.sel === h.m || st.focus === h.k || (t >= h.t0 && t <= h.t1)) ? "" : "none";
  });
  P.ov.querySelectorAll(".barhit").forEach(e => e.classList.toggle("cur", +e.dataset.bar === st.bar));
}
const refreshAll = () => pageEls.forEach((_, i) => refreshClasses(i + 1));

// live note dots (notes sounding now)
const byTime = N.map((n, i) => i).filter(i => N[i][0] && !N[i][8]).sort((a, b) => N[a][3] - N[b][3]);
const tArr = byTime.map(i => N[i][3]);
let dotPages = new Set();
function drawDots(t) {
  dotPages.forEach(p => { const P = pageEls[p - 1]; if (P.gDots) P.gDots.innerHTML = ""; }); dotPages = new Set();
  let lo = 0, hi = tArr.length; while (lo < hi) { const m = (lo + hi) >> 1; if (tArr[m] < t - 8) lo = m + 1; else hi = m; }
  for (let k = lo; k < tArr.length && tArr[k] <= t; k++) {
    const n = N[byTime[k]]; if (t > n[3] + Math.max(0.12, n[4])) continue;
    const P = pageEls[n[0] - 1]; if (!P.drawn) continue;
    el("circle", { cx: n[1] + 145, cy: n[2], r: 115, class: "dot" }, P.gDots); dotPages.add(n[0]);
  }
}

// ---------------- clicking
function clickScore(ev, page, bar, hitk) {
  const P = pageEls[page - 1], r = P.ov.getBoundingClientRect();
  const x = (ev.clientX - r.left) / r.width * D.page_w, y = (ev.clientY - r.top) / r.height * D.pages[page - 1].h;
  // nearest notehead
  let best = null, bd = 1e12;
  for (let i = 0; i < N.length; i++) { const n = N[i]; if (n[0] !== page) continue; const d = (n[1] + 145 - x) ** 2 + (n[2] - y) ** 2; if (d < bd) { bd = d; best = i; } }
  if (best !== null && bd < 260 ** 2 && noteHits[best]) { pickNote(best, page); return; }
  if (hitk !== undefined) { focusHit(hitk, true); return; }
  goBar(bar, true);
}
function pickNote(ni, page) {
  const P = pageEls[page - 1]; P.gPick.innerHTML = ""; el("circle", { cx: N[ni][1] + 145, cy: N[ni][2], r: 230, class: "notepick" }, P.gPick);
  const ks = noteHits[ni] || [];
  st.pick = { ni, ks };
  renderNow(true);
  if (ks.length) focusHit(ks[0], false);
}

// ---------------- panel
$("#idea .body").innerHTML = rich(T.thesis);
$$(".fold").forEach(b => b.onclick = () => { $("#idea").classList.toggle("folded"); b.textContent = $("#idea").classList.contains("folded") ? "show" : "hide"; });
function renderStation(i) {
  const s = stations[i]; st.station = i;
  $("#sthead").innerHTML = `${s.title} <span class="hint">bb. ${s.a}–${s.b} · ${i + 1}/${stations.length}</span>`;
  $("#stmeta").textContent = s.mark; $("#sttext").innerHTML = rich(s.text); $("#stationname").textContent = s.title;
}
$("#stprev").onclick = () => station(-1); $("#stnext").onclick = () => station(1);
function station(d) { const i = Math.max(0, Math.min(stations.length - 1, st.station + d)); renderStation(i); goBar(stations[i].a, true); }
function renderLegend() {
  $("#legend").innerHTML = D.families.map((f, fi) => `<div class="fam" style="color:${f.color}">
    <div class="fh"><input type="checkbox" data-fam="${f.id}" ${st.famOn[f.id] ? "checked" : ""}><span>${f.name}</span><span class="key">${fi + 1}</span></div>
    <div class="fd">${f.desc}</div>
    ${MOT.filter(m => m.family === f.id).map(m => `<div class="mot ${st.sel === m.i ? "sel" : ""}" data-m="${m.i}"><span style="color:${f.color}">■</span><span style="color:var(--ink)">${m.name}</span> <span class="lb">${m.label}</span><span class="ct">${m.count}</span></div>
      ${st.sel === m.i ? `<div class="motdesc" style="color:var(--ink)">${rich(m.desc)}<div class="nav"><button data-nav="-1">◀ P</button><span id="navpos"></span><button data-nav="1">N ▶</button><button data-solo>${st.solo ? "show all" : "solo (S)"}</button></div></div>` : ""}`).join("")}
  </div>`).join("");
  $$("#legend input[data-fam]").forEach(c => c.onchange = () => { st.famOn[c.dataset.fam] = c.checked; afterFilter(); });
  $$("#legend .mot").forEach(d => d.onclick = () => selectMotif(st.sel === +d.dataset.m ? null : +d.dataset.m, true));
  $$("#legend [data-nav]").forEach(b => b.onclick = e => { e.stopPropagation(); jumpStatement(+b.dataset.nav); });
  const sb = $("#legend [data-solo]"); if (sb) sb.onclick = e => { e.stopPropagation(); st.solo = !st.solo; afterFilter(); };
  updateNavPos();
}
function updateNavPos() {
  const e = $("#navpos"); if (!e || st.sel === null) return;
  const hs = H.filter(h => h.m === st.sel); const k = hs.findIndex(h => h.k === st.focus);
  e.textContent = k >= 0 ? `${k + 1} of ${hs.length}` : `${hs.length} statements`;
}
function afterFilter() { renderLegend(); refreshAll(); drawTimeline(); renderNow(); }
function selectMotif(m, jump) {
  st.sel = m; st.focus = null; renderLegend(); refreshAll(); drawTimeline();
  if (m !== null && jump) { if (!st.famOn[MOT[m].family]) { st.famOn[MOT[m].family] = true; renderLegend(); refreshAll(); } jumpStatement(1, true); }
  const sel = $("#legend .mot.sel"); if (sel) sel.scrollIntoView({ block: "nearest" });
}
function soloFamily(f) {
  const only = Object.keys(st.famOn).every(k => st.famOn[k] === (k === f));
  for (const k in st.famOn) st.famOn[k] = only ? true : k === f; afterFilter(); toast(only ? "all families" : FAM[f].name + " only");
}
function renderNow(fromPick) {
  const t = audio.currentTime || 0;
  let ks;
  if (fromPick && st.pick) ks = st.pick.ks;
  else ks = H.filter(h => visible(h) && t >= h.t0 && t <= h.t1).map(h => h.k);
  const head = fromPick && st.pick ? `<div class="sub">This note (${pname(N[st.pick.ni][5])}, b. ${N[st.pick.ni][7]}) belongs to ${ks.length} statement${ks.length === 1 ? "" : "s"}:</div>` : "";
  $("#nowlist").innerHTML = head + (ks.length ? ks.map(k => { const h = H[k], m = MOT[h.m];
    const wedge = h.partners.length ? ` · <b>wedge</b> with ${h.partners.slice(0, 2).map(j => MOT[H[j].m].name).join(", ")}` : "";
    return `<div class="it" data-k="${k}"><span class="sw" style="background:${m.color}"></span><div>${m.name} <span class="sub">${describe(h).split(": ")[1]}${wedge}</span></div></div>`; }).join("")
    : `<div class="empty">No shown motif is sounding.</div>`);
  $$("#nowlist .it").forEach(d => d.onclick = () => focusHit(+d.dataset.k, true));
}

// ---------------- navigation
function seek(t, play) {
  st.stopAt = null;
  const go = () => { audio.currentTime = Math.max(0, t); if (play) audio.play().catch(() => {}); tick(true); };
  if (audio.readyState >= 1) go(); else { audio.addEventListener("loadedmetadata", go, { once: true }); audio.load(); }
}
function goBar(b, play, b2) {
  b = Math.max(1, Math.min(D.bars.length, b)); st.loopRange = b2 && b2 > b ? [barT(b), barT(b2 + 1)] : null;
  seek(barT(b) + 0.01, play); scrollToBar(b, true);
}
function focusHit(k, play) {
  const h = H[k]; st.focus = k; if (st.sel !== h.m) { st.sel = h.m; renderLegend(); }
  updateNavPos(); refreshAll(); drawTimeline();
  scrollToBar(h.bar, true);
  if (play) { seek(h.t0 - 0.12, true); st.stopAt = $("#loop").checked ? null : null; st.loopRange = [h.t0 - 0.12, h.t1 + 0.35]; }
}
function jumpStatement(d, fromStart) {
  const t = audio.currentTime || 0;
  const pool = H.filter(h => visible(h) && (st.sel === null || h.m === st.sel));
  if (!pool.length) return;
  let k;
  if (st.focus !== null && pool.some(h => h.k === st.focus) && !fromStart) { const i = pool.findIndex(h => h.k === st.focus); k = pool[(i + d + pool.length) % pool.length]; }
  else k = d > 0 ? (pool.find(h => h.t0 > t + 0.05) || pool[0]) : ([...pool].reverse().find(h => h.t0 < t - 0.05) || pool[pool.length - 1]);
  focusHit(k.k, true); playOnly(k.k);
}
function playOnly(k) { const h = H[k]; st.stopAt = h.t1 + 0.4; seek(h.t0 - 0.12, true); st.stopAt = h.t1 + 0.4; }
function scrollToBar(b, force) {
  const bx = D.bars[b - 1]; if (!bx) return; const P = pageEls[bx[0] - 1]; drawPage(bx[0]);
  const r = P.div.getBoundingClientRect(), sr = score.getBoundingClientRect();
  const ph = D.pages[bx[0] - 1].h;
  const y = r.top - sr.top + score.scrollTop + (bx[2] - 900) / ph * r.height;
  const yb = r.top - sr.top + score.scrollTop + (bx[4] + 900) / ph * r.height;
  if (force || y < score.scrollTop + 20 || yb > score.scrollTop + score.clientHeight - 40) score.scrollTo({ top: y - score.clientHeight * 0.22, behavior: "smooth" });
}

// ---------------- timeline
const tl = $("#timeline");
function cssv(n) { return getComputedStyle(document.documentElement).getPropertyValue(n).trim(); }
function drawTimeline() {
  const r = devicePixelRatio || 1, W = tl.clientWidth, Hh = 112; tl.width = W * r; tl.height = Hh * r;
  const g = tl.getContext("2d"); g.scale(r, r); g.clearRect(0, 0, W, Hh);
  const G = 92, T1 = D.duration, X = t => G + t / T1 * (W - G);
  g.font = "11px system-ui, sans-serif";
  stations.forEach((s, i) => { const x0 = X(barT(s.a)), x1 = X(barT(s.b + 1));
    g.fillStyle = i % 2 ? "rgba(128,128,128,.10)" : "rgba(128,128,128,.03)"; g.fillRect(x0, 0, x1 - x0, 22);
    if (i === st.station) { g.fillStyle = "rgba(243,181,17,.28)"; g.fillRect(x0, 0, x1 - x0, 22); }
    g.save(); g.beginPath(); g.rect(x0, 0, x1 - x0 - 2, 22); g.clip(); g.fillStyle = cssv("--ink"); g.fillText(s.title, x0 + 3, 15); g.restore(); });
  const part2 = X(barT(285)); g.fillStyle = cssv("--accent"); g.fillRect(part2 - 1, 0, 2, Hh);
  D.families.forEach((f, fi) => {
    const y = 28 + fi * 20; g.fillStyle = "rgba(128,128,128,.06)"; g.fillRect(0, y, W, 17);
    g.globalAlpha = st.famOn[f.id] ? 1 : 0.15;
    for (const h of H) { if (MOT[h.m].family !== f.id) continue;
      const sel = st.sel === h.m; g.fillStyle = f.color; g.globalAlpha = st.famOn[f.id] ? (st.sel === null || sel ? 0.85 : 0.18) : 0.1;
      g.fillRect(X(h.t0), y + (sel ? 0 : 3), Math.max(1.5, X(h.t1) - X(h.t0)), sel ? 17 : 11); }
    g.globalAlpha = 1; g.fillStyle = cssv("--panel"); g.fillRect(0, y, G - 2, 17); g.fillStyle = f.color; g.fillText(f.name, 4, y + 12);
  });
  const t = audio.currentTime || 0; g.fillStyle = cssv("--now"); g.fillRect(X(t) - 1, 0, 3, Hh);
  if (st.focus !== null) { const h = H[st.focus]; g.strokeStyle = cssv("--ink"); g.lineWidth = 1.5; g.strokeRect(X(h.t0) - 2, 26, X(h.t1) - X(h.t0) + 4, 84); }
}
tl.addEventListener("click", e => { const r = tl.getBoundingClientRect(), G = 92; const x = e.clientX - r.left; if (x < G) return; seek((x - G) / (r.width - G) * D.duration, true); });
window.addEventListener("resize", drawTimeline);

// ---------------- transport
audio.src = D.audio.split("/").map(encodeURIComponent).join("/").replace(/^\.\./, "..");
$("#play").onclick = () => audio.paused ? audio.play() : audio.pause();
audio.onplay = () => $("#play").textContent = "❚❚"; audio.onpause = () => $("#play").textContent = "▶";
$("#rate").onchange = e => audio.playbackRate = +e.target.value;
$("#follow").onchange = () => toast($("#follow").checked ? "following the music" : "free scrolling");
let lastBar = null, lastStation = null, lastNowKey = "";
function tick(force) {
  const t = audio.currentTime || 0;
  if (st.stopAt !== null && t >= st.stopAt) { if ($("#loop").checked && st.loopRange) { audio.currentTime = st.loopRange[0]; } else { audio.pause(); st.stopAt = null; } }
  else if ($("#loop").checked && !audio.paused) {
    const range = st.loopRange || [barT(stations[st.station].a), barT(stations[st.station].b + 1)];
    if (t >= range[1]) audio.currentTime = range[0];
  }
  $("#clock").textContent = `${fmt(t)} / ${fmt(D.duration)}`;
  const b = barAt(t);
  if (b !== lastBar || force) {
    st.bar = b; lastBar = b; $("#barno").textContent = `bar ${b}`;
    if ($("#follow").checked && !audio.paused) scrollToBar(b, false);
    const si = stationAt(b); if (si !== lastStation) { lastStation = si; renderStation(si); }
  }
  const nowKey = H.filter(h => visible(h) && t >= h.t0 && t <= h.t1).map(h => h.k).join(",");
  if (nowKey !== lastNowKey || force) { lastNowKey = nowKey; if (!st.pick || !audio.paused) { st.pick = null; renderNow(); } refreshAll(); }
  drawDots(t); drawTimeline();
}
audio.addEventListener("timeupdate", () => tick(false));
(function loop() { if (!audio.paused) tick(false); requestAnimationFrame(loop); })();

// ---------------- keyboard
function toast(s) { const d = document.createElement("div"); d.className = "toast"; d.textContent = s; document.body.appendChild(d); setTimeout(() => d.remove(), 1300); }
document.addEventListener("keydown", e => {
  if (e.target.tagName === "INPUT" && e.target.type !== "checkbox" || e.target.tagName === "SELECT") return;
  const k = e.key;
  if (k === " ") { e.preventDefault(); $("#play").click(); }
  else if (k === "ArrowRight") { e.preventDefault(); goBar(st.bar + (e.shiftKey ? 4 : 1), !audio.paused); }
  else if (k === "ArrowLeft") { e.preventDefault(); goBar(st.bar - (e.shiftKey ? 4 : 1), !audio.paused); }
  else if (k === "ArrowDown") { e.preventDefault(); station(1); }
  else if (k === "ArrowUp") { e.preventDefault(); station(-1); }
  else if (k === "n" || k === "N") jumpStatement(1);
  else if (k === "p" || k === "P") jumpStatement(-1);
  else if (k === "m") selectMotif(st.sel === null ? 0 : (st.sel + 1) % MOT.length, true);
  else if (k === "M") selectMotif(st.sel === null ? MOT.length - 1 : (st.sel - 1 + MOT.length) % MOT.length, true);
  else if (k === "Enter") { if (st.focus !== null) playOnly(st.focus); }
  else if ("1234".includes(k)) { const f = D.families[+k - 1]; st.famOn[f.id] = !st.famOn[f.id]; afterFilter(); toast(`${f.name} ${st.famOn[f.id] ? "shown" : "hidden"}`); }
  else if (k === "a" || k === "A") { for (const f in st.famOn) st.famOn[f] = true; st.solo = false; afterFilter(); toast("all families"); }
  else if (k === "s" || k === "S") { st.solo = !st.solo; afterFilter(); toast(st.solo ? "solo: " + (st.sel !== null ? MOT[st.sel].name : "—") : "all motifs"); }
  else if (k === "w" || k === "W") { const h = st.focus !== null ? H[st.focus] : null;
    if (h && h.partners.length) { const j = h.partners[0]; focusHit(j, false); toast(`wedge: ${MOT[h.m].name} ↔ ${MOT[H[j].m].name}`); } else toast("no wedge partner"); }
  else if (k === "Escape") { st.sel = null; st.focus = null; st.solo = false; st.pick = null; st.loopRange = null; pageEls.forEach(p => p.gPick && (p.gPick.innerHTML = "")); afterFilter(); }
  else if (k === "g" || k === "G") { const b = parseInt(prompt("Go to bar (1–723)"), 10); if (b) goBar(b, !audio.paused); }
  else if (k === "f" || k === "F") { $("#follow").checked = !$("#follow").checked; $("#follow").dispatchEvent(new Event("change")); }
  else if (k === "l" || k === "L") { $("#loop").checked = !$("#loop").checked; toast($("#loop").checked ? "loop on" : "loop off"); }
  else if (k === "t" || k === "T") $(".fold").click();
  else if (k === "[" || k === "]") { const r = $("#rate"), i = r.selectedIndex + (k === "]" ? 1 : -1); if (i >= 0 && i < r.options.length) { r.selectedIndex = i; r.dispatchEvent(new Event("change")); toast("speed " + r.value); } }
  else if (k === "?") $("#help").hidden = !$("#help").hidden;
});
$("#helpbtn").onclick = () => $("#help").hidden = false; $("#helpclose").onclick = () => $("#help").hidden = true;
$("#help").onclick = e => { if (e.target.id === "help") $("#help").hidden = true; };

renderLegend(); renderStation(0); drawTimeline(); tick(true);
})();
