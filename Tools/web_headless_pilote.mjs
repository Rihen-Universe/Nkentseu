// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Pilote DevTools de Tools/web_headless_mesure.sh (2026-09-04). La boucle web de renderdemo est
// emscripten_sleep(0) sous Asyncify -> une image = un setTimeout a delai <= 1 ms (pas requestAnimationFrame) ;
// en controle croise, les appels de dessin WebGL sont comptes (~1 050 par image dans la demo 3D).
// Compte les images (hook setTimeout injecte AVANT le chargement), capture la console
// (stdout/stderr du wasm + avertissements WebGL du navigateur), s'arrete apres N images ou T secondes.
// Usage : node web_headless_pilote.mjs <url> <port_devtools> <images_cible> <secondes_max> <sortie.log>
import { writeFileSync } from "node:fs";

const [url, port, cibleStr, maxSecStr, out] = process.argv.slice(2);
const cible = parseInt(cibleStr || "100", 10);
const maxSec = parseInt(maxSecStr || "120", 10);
const lignes = [];
const log = (l) => { lignes.push(l); };

async function attendreDevTools() {
  for (let i = 0; i < 60; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${port}/json/version`);
      if (r.ok) return await r.json();
    } catch (e) {}
    await new Promise((r) => setTimeout(r, 500));
  }
  throw new Error("DevTools injoignable");
}

const ver = await attendreDevTools();
const ws = new WebSocket(ver.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let id = 0;
const attentes = new Map();
const envoyer = (method, params = {}, sessionId) => new Promise((res, rej) => {
  const mid = ++id;
  attentes.set(mid, { res, rej });
  ws.send(JSON.stringify(sessionId ? { id: mid, method, params, sessionId } : { id: mid, method, params }));
});
let images = 0;
let draws = 0;
let debutT = Date.now();
const evenements = [];
ws.onmessage = (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && attentes.has(m.id)) {
    const a = attentes.get(m.id); attentes.delete(m.id);
    if (m.error) a.rej(new Error(JSON.stringify(m.error))); else a.res(m.result);
    return;
  }
  if (m.method === "Runtime.consoleAPICalled") {
    const txt = (m.params.args || []).map((a) => a.value ?? a.description ?? "").join(" ");
    log(`[console.${m.params.type}] ${txt}`);
  } else if (m.method === "Log.entryAdded") {
    const e = m.params.entry;
    log(`[log.${e.source}.${e.level}] ${e.text}`);
  } else if (m.method === "Runtime.exceptionThrown") {
    log(`[exception] ${JSON.stringify(m.params.exceptionDetails).slice(0, 300)}`);
  }
};

// Cible : la page
const { targetInfos } = await envoyer("Target.getTargets");
const page = targetInfos.find((t) => t.type === "page");
const { sessionId } = await envoyer("Target.attachToTarget", { targetId: page.targetId, flatten: true });
await envoyer("Runtime.enable", {}, sessionId);
await envoyer("Log.enable", {}, sessionId);
await envoyer("Page.enable", {}, sessionId);
await envoyer("Page.addScriptToEvaluateOnNewDocument", {
  source: "(function(){window.__nkImages=0;window.__nkDraws=0;"+
          "var st=window.setTimeout;window.setTimeout=function(cb,ms){if(!(ms>1))window.__nkImages++;return st.apply(window,arguments);};"+
          "var P=WebGL2RenderingContext.prototype;var da=P.drawArrays,de=P.drawElements,dai=P.drawArraysInstanced,dei=P.drawElementsInstanced;"+
          "P.drawArrays=function(){window.__nkDraws++;return da.apply(this,arguments);};P.drawElements=function(){window.__nkDraws++;return de.apply(this,arguments);};"+
          "P.drawArraysInstanced=function(){window.__nkDraws++;return dai.apply(this,arguments);};P.drawElementsInstanced=function(){window.__nkDraws++;return dei.apply(this,arguments);};})();",
}, sessionId);
await envoyer("Page.navigate", { url }, sessionId);

// Boucle d'attente : images cible ou temps max
let t0 = Date.now();
let premiereImageT = null;
for (;;) {
  await new Promise((r) => setTimeout(r, 1000));
  try {
    const r = await envoyer("Runtime.evaluate", { expression: "(window.__nkImages|0)+' '+(window.__nkDraws|0)", returnByValue: true }, sessionId);
    const parts = String(r.result.value).split(" ");
    images = parseInt(parts[0], 10) | 0;
    draws = parseInt(parts[1], 10) | 0;
  } catch (e) {}
  if (images > 0 && premiereImageT === null) premiereImageT = Date.now();
  const s = (Date.now() - t0) / 1000;
  if (images >= cible || s >= maxSec) break;
}
try {
  const rr = await envoyer("Runtime.evaluate", { expression: "(function(){try{var c=document.createElement('canvas');var g=c.getContext('webgl2');var e=g.getExtension('WEBGL_debug_renderer_info');return e?g.getParameter(e.UNMASKED_RENDERER_WEBGL)+' | '+g.getParameter(e.UNMASKED_VENDOR_WEBGL):g.getParameter(g.RENDERER);}catch(x){return '?'+x;}})()", returnByValue: true }, sessionId);
  log(`[pilote] renderer WebGL : ${rr.result.value}`);
} catch (e) { log("[pilote] renderer WebGL : illisible " + e); }
const dureeImages = premiereImageT ? (Date.now() - premiereImageT) / 1000 : 0;
log(`[pilote] images(setTimeout<=1ms)=${images} dessins WebGL=${draws} (~${images ? Math.round(draws / images) : 0} par image) en ${((Date.now() - t0) / 1000).toFixed(1)} s total, ${dureeImages.toFixed(1)} s depuis la premiere image` +
    (premiereImageT && images > 1 ? ` (~${(images / dureeImages).toFixed(1)} img/s)` : ""));
writeFileSync(out, lignes.join("\n") + "\n", "utf8");
try { await envoyer("Browser.close"); } catch (e) {}
ws.close();
process.exit(0);
