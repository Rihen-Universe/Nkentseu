// Fabrique de références pixel — export Banani → HTML statique.
// Mécanique : transpile le JSX (Babel preset-react), évalue avec React,
// renderToStaticMarkup, aucun ajout. Les composants @components/* sont
// résolus depuis sharedFiles de l'export. t() = identité (marqueur i18n Banani).
'use strict';
const fs = require('fs');
const path = require('path');
const babel = require('@babel/core');
const React = require('react');
const ReactDOMServer = require('react-dom/server');

const EXPORT = process.argv[2];
const OUTDIR = process.argv[3];
const data = JSON.parse(fs.readFileSync(EXPORT, 'utf8'));

// ---- registre de modules : sharedFiles d'abord, puis designs "components/" ----
const moduleSrc = {};
for (const f of data.sharedFiles) {
  const m = f.path.match(/^\/components\/(\w+)\.jsx$/);
  if (m) moduleSrc[m[1]] = f.content;
}
for (const d of data.designs) {
  const m = d.screenId.match(/\/components\/(\w+)\.jsx$/);
  if (m && !(m[1] in moduleSrc)) moduleSrc[m[1]] = d.source;
}

const t = (s) => s; // marqueur i18n Banani : identité

const cache = {};
function loadModule(name, src) {
  if (cache[name]) return cache[name];
  // imports → require maison ; exports → affectations
  let code = src
    .replace(/^import\s+(\w+)\s+from\s+'@components\/(\w+)';?\s*$/gm,
             "const $1 = __req('$2');")
    .replace(/^export\s+default\s+/gm, 'module.exports.default = ')
    .replace(/^export\s+const\s+/gm, 'const ');
  const out = babel.transformSync(code, {
    presets: [['@babel/preset-react', { runtime: 'classic' }]],
    babelrc: false, configFile: false,
  });
  const mod = { exports: {} };
  const fn = new Function('React', 'module', 'exports', '__req', 't', out.code);
  fn(React, mod, mod.exports, (n) => {
    if (!moduleSrc[n]) throw new Error('module inconnu: ' + n);
    return loadModule(n, moduleSrc[n]);
  }, t);
  cache[name] = mod.exports.default;
  return cache[name];
}

// ---- style.css de l'export (le bloc @theme) ----
const themeCss = data.sharedFiles.find(f => f.path === '/style.css').content;

// ---- rendu de chaque écran ----
fs.mkdirSync(OUTDIR, { recursive: true });
const index = [];
for (const d of data.designs) {
  const m = d.screenId.match(/\/screens\/new_screen(\d+)\.jsx$/);
  if (!m) continue; // les composants ne sont pas des écrans
  const num = parseInt(m[1], 10);
  let src = d.source;
  if (num === 1) {
    // ÉCART NOMMÉ : le wrapper de la toile est un bloc dans le JSX ; DesignCanvasV2
    // (racine flex-1, enfants absolus) s'effondre à 0 px. Banani étire ses previews.
    // Correctif minimal : le wrapper devient un conteneur flex (la toile remplit).
    src = src.replace(
      `<div className="flex-1 relative" style={{ minHeight: 0 }}>`,
      `<div className="flex-1 relative flex" style={{ minHeight: 0 }}>`);
  }
  const Comp = loadModule('screen' + num, src);
  const html = ReactDOMServer.renderToStaticMarkup(React.createElement(Comp));
  const slug = d.screenName.normalize('NFD').replace(/[̀-ͯ]/g, '')
    .replace(/[^A-Za-z0-9]+/g, '_').replace(/^_+|_+$/g, '');
  const base = `ecran${String(num).padStart(2, '0')}_${slug}`;
  const doc = `<!doctype html>
<html><head><meta charset="utf-8">
<link rel="stylesheet" href="fonts.css">
<link rel="stylesheet" href="tailwind.css">
<title>${d.screenName}</title>
<style>html,body{margin:0;background:#0d1117;overflow:hidden}</style>
</head><body>
${html}
</body></html>`;
  fs.writeFileSync(path.join(OUTDIR, base + '.html'), doc);
  index.push({ num, name: d.screenName, base });
}
// input.css pour tailwind v4 : préflight + utilitaires + le @theme de l'export
fs.writeFileSync(path.join(OUTDIR, 'input.css'),
  '@import "tailwindcss";\n@source "./";\n' + themeCss + '\n');
fs.writeFileSync(path.join(OUTDIR, 'index.json'), JSON.stringify(index, null, 2));
console.log('écrans rendus:', index.length);
