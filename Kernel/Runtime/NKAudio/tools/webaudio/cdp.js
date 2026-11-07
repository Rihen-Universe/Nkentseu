// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Pilote Chrome (--headless=new --mute-audio) par le protocole de debogage : lit l'etat REEL de l'AudioContext avant et
// apres un clic (evenement d'entree de confiance), les trames demandees par le navigateur, la console et les erreurs.
// Usage : node cdp.js <port-chrome> <url-de-la-page>
// Sortie : « [webaudio] avant=<etat> trames_avant=<n> apres=<etat> trames_apres=<n> erreurs=<n> » + les erreurs.
const [port, url] = [process.argv[2], process.argv[3]];
const attendre = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  let cible = null;
  for (let i = 0; i < 60 && !cible; ++i) {
    try {
      const l = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
      cible = l.find((t) => t.type === 'page');
    } catch (e) {}
    if (!cible) await attendre(250);
  }
  if (!cible) { console.log('[webaudio] ECHEC D\'INSTRUMENT : Chrome ne repond pas'); process.exit(2); }
  const ws = new WebSocket(cible.webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  let id = 0; const attente = {}; const erreurs = [];
  ws.onmessage = (m) => {
    const d = JSON.parse(m.data);
    if (d.id && attente[d.id]) { attente[d.id](d); delete attente[d.id]; }
    if (d.method === 'Runtime.exceptionThrown') erreurs.push('exception: ' + (d.params.exceptionDetails.exception?.description || d.params.exceptionDetails.text));
    if (d.method === 'Runtime.consoleAPICalled' && (d.params.type === 'error' || d.params.type === 'warning'))
      erreurs.push(d.params.type + ': ' + d.params.args.map((a) => a.value ?? a.description).join(' '));
    if (d.method === 'Log.entryAdded' && d.params.entry.level === 'error') erreurs.push('journal: ' + d.params.entry.text + ' ' + (d.params.entry.url || ''));
  };
  const envoyer = (method, params = {}) => new Promise((r) => { const i = ++id; attente[i] = r; ws.send(JSON.stringify({ id: i, method, params })); });
  const evaluer = async (expr) => (await envoyer('Runtime.evaluate', { expression: expr, returnByValue: true })).result?.result?.value;
  await envoyer('Runtime.enable'); await envoyer('Log.enable'); await envoyer('Page.enable');
  await envoyer('Page.navigate', { url });
  let pret = false;
  for (let i = 0; i < 80 && !pret; ++i) { await attendre(250); pret = await evaluer('!!window.NkAudioCtx'); }
  if (!pret) { console.log('[webaudio] ECHEC : aucun AudioContext cree par la page'); erreurs.forEach((e) => console.log('[webaudio]   ' + e)); process.exit(1); }
  await attendre(1500);
  const etat = () => evaluer('window.NkAudioCtx.state');
  const trames = () => evaluer('window.NkAudioTrames|0');
  const avant = await etat(); const tAvant = await trames();
  // un VRAI evenement d'entree (de confiance) : un clic dans la page, comme le fait l'utilisateur
  await envoyer('Input.dispatchMouseEvent', { type: 'mouseMoved', x: 60, y: 60 });
  await envoyer('Input.dispatchMouseEvent', { type: 'mousePressed', x: 60, y: 60, button: 'left', clickCount: 1 });
  await envoyer('Input.dispatchMouseEvent', { type: 'mouseReleased', x: 60, y: 60, button: 'left', clickCount: 1 });
  await attendre(2000);
  const apres = await etat(); const tApres = await trames();
  console.log(`[webaudio] avant=${avant} trames_avant=${tAvant} apres=${apres} trames_apres=${tApres} erreurs=${erreurs.length}`);
  erreurs.slice(0, 5).forEach((e) => console.log('[webaudio]   ' + e.slice(0, +(process.env.NK_CDP_LONG || 200))));
  ws.close(); process.exit(0);
})();
