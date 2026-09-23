#!/usr/bin/env node
// Serves giant-scale-lab.html and proxies AI-tutor calls to DeepSeek.
// Browsers can't call api.deepseek.com directly (CORS), so the page posts to
// /api/chat on this server, which forwards the request with the API key.
//
// Config (environment variables or a .env file next to this script):
//   DEEPSEEK_API_KEY  key used for all requests (optional; otherwise each
//                     learner pastes their own key into the page)
//   ACCESS_CODE       if set, learners must enter this code to use the
//                     server's key (protects your credit on a public host)
//   PORT              default 8080
//   HOST              default 0.0.0.0
//   DEEPSEEK_MODEL    default model if the page doesn't pick one (deepseek-chat)
//   RATE_LIMIT        max AI requests per IP per hour (default 120)
//   DEEPSEEK_BASE_URL default https://api.deepseek.com
"use strict";
const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const envFile = path.join(__dirname, ".env");
if (fs.existsSync(envFile)) {
  for (const line of fs.readFileSync(envFile, "utf8").split(/\r?\n/)) {
    const m = line.match(/^\s*([A-Z0-9_]+)\s*=\s*(.*?)\s*$/);
    if (m && !(m[1] in process.env)) process.env[m[1]] = m[2].replace(/^(['"])(.*)\1$/, "$2");
  }
}

const PORT = +process.env.PORT || 8080;
const HOST = process.env.HOST || "0.0.0.0";
const KEY = (process.env.DEEPSEEK_API_KEY || "").trim();
const CODE = (process.env.ACCESS_CODE || "").trim();
const MODEL = process.env.DEEPSEEK_MODEL || "deepseek-chat";
const RATE = +process.env.RATE_LIMIT || 120;
const UPSTREAM = (process.env.DEEPSEEK_BASE_URL || "https://api.deepseek.com").replace(/\/+$/, "") + "/chat/completions";
const PAGE = path.join(__dirname, "giant-scale-lab.html");
// Static files the page needs: the paper itself and the vendored PDF.js viewer.
const STATIC = {
  "/paper.pdf": ["paper.pdf", "application/pdf"],
  "/vendor/pdfjs/pdf.min.js": ["vendor/pdfjs/pdf.min.js", "text/javascript; charset=utf-8"],
  "/vendor/pdfjs/pdf.worker.min.js": ["vendor/pdfjs/pdf.worker.min.js", "text/javascript; charset=utf-8"]
};
const MAX_BODY = 64 * 1024;
const MAX_TOKENS = 1500;

const hits = new Map(); // ip -> timestamps within the last hour
function allow(ip) {
  const now = Date.now(), list = (hits.get(ip) || []).filter(t => now - t < 3600e3);
  if (list.length >= RATE) { hits.set(ip, list); return false; }
  list.push(now); hits.set(ip, list); return true;
}

function send(res, status, body, type = "application/json; charset=utf-8", cache = "no-store") {
  res.writeHead(status, { "Content-Type": type, "Cache-Control": cache, "X-Content-Type-Options": "nosniff" });
  res.end(typeof body === "string" || Buffer.isBuffer(body) ? body : JSON.stringify(body));
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let size = 0; const chunks = [];
    req.on("data", c => { size += c.length; if (size > MAX_BODY) { reject(new Error("too large")); req.destroy(); } else chunks.push(c); });
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", reject);
  });
}

async function chat(req, res) {
  const ip = (req.headers["x-forwarded-for"] || "").split(",")[0].trim() || req.socket.remoteAddress;
  if (!allow(ip)) return send(res, 429, { error: "Rate limit reached. Try again later." });

  const userKey = (req.headers.authorization || "").replace(/^Bearer\s+/i, "").trim();
  let key = userKey;
  if (!key && KEY) {
    if (CODE && req.headers["x-access-code"] !== CODE) return send(res, 401, { error: "Wrong or missing access code." });
    key = KEY;
  }
  if (!key) return send(res, 401, { error: "No API key: set DEEPSEEK_API_KEY on the server or enter a key in the page." });

  let body;
  try { body = JSON.parse(await readBody(req)); } catch (e) { return send(res, 400, { error: "Bad request body." }); }
  if (!Array.isArray(body.messages)) return send(res, 400, { error: "messages[] required." });
  const payload = {
    model: body.model || MODEL,
    messages: body.messages,
    temperature: typeof body.temperature === "number" ? body.temperature : 0.2,
    max_tokens: Math.min(+body.max_tokens || 900, MAX_TOKENS),
    response_format: body.response_format
  };

  try {
    const up = await fetch(UPSTREAM, {
      method: "POST",
      headers: { "Content-Type": "application/json", "Authorization": "Bearer " + key },
      body: JSON.stringify(payload),
      signal: AbortSignal.timeout(90e3)
    });
    send(res, up.status, await up.text());
  } catch (e) {
    send(res, 502, { error: "Upstream request failed: " + e.message });
  }
}

http.createServer(async (req, res) => {
  const url = new URL(req.url, "http://x");
  try {
    if (req.method === "GET" && (url.pathname === "/" || url.pathname === "/index.html" || url.pathname === "/giant-scale-lab.html"))
      return send(res, 200, fs.readFileSync(PAGE), "text/html; charset=utf-8");
    if (req.method === "GET" && STATIC[url.pathname]) {
      const [file, type] = STATIC[url.pathname];
      return send(res, 200, fs.readFileSync(path.join(__dirname, file)), type, "public, max-age=86400");
    }
    if (req.method === "GET" && url.pathname === "/api/status")
      return send(res, 200, { proxy: true, serverKey: !!KEY, needsCode: !!(KEY && CODE), model: MODEL });
    if (req.method === "GET" && url.pathname === "/healthz") return send(res, 200, "ok", "text/plain");
    if (req.method === "POST" && url.pathname === "/api/chat") return await chat(req, res);
    send(res, 404, { error: "Not found" });
  } catch (e) {
    send(res, 500, { error: "Server error" });
  }
}).listen(PORT, HOST, () => {
  console.log(`Giant-Scale On-Call running at http://${HOST === "0.0.0.0" ? "localhost" : HOST}:${PORT}`);
  console.log(KEY ? `AI tutor: server key configured${CODE ? " (access code required)" : ""}.` : "AI tutor: no server key; learners can paste their own.");
});
