#!/bin/bash
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# =============================================================================
# web_headless_mesure.sh — lance renderdemo (Release-Web) dans Edge headless,
# pilote par DevTools (Tools/web_headless_pilote.mjs), et ecrit dans <sortie.log>
# la console de la page (stdout/stderr du wasm + avertissements WebGL du
# navigateur) et une ligne [pilote] : images, dessins WebGL, renderer.
#
# Ecrit le 2026-09-04 pour mesurer « WebGL: INVALID_ENUM: disable » (c'etait
# GL_FRAMEBUFFER_SRGB, pose par BeginFrame a chaque image) : une erreur par
# image se compte, elle ne se devine pas.
#
# Usage, depuis la racine de l'arbre :
#   Tools/web_headless_mesure.sh <images_cible> <secondes_max> <sortie.log> [params_url]
#   ex. Tools/web_headless_mesure.sh 100 240 /tmp/web.log "demo=2"
# Lire ensuite : grep -c GLERR <sortie.log> ; grep -c INVALID_ENUM <sortie.log>
#
# Ce que ca mesure et ne mesure pas : la validation WebGL (ANGLE) est la meme
# sur GPU reel et logiciel ; les VITESSES (img/s) ne sont pas celles de Rodolf
# (Asyncify + onglet headless + GPU partage). Le renderer utilise est dit dans
# la ligne [pilote] : lire ANGLE(NVIDIA…) ou SwiftShader avant de conclure.
# =============================================================================
set -u
CIBLE=${1:-100}
MAXS=${2:-240}
OUT=${3:-web_headless_mesure.log}
PARAMS=${4:-demo=2}
ICI="$(cd "$(dirname "$0")" && pwd)"
RACINE="$(cd "$ICI/.." && pwd)"
DIR="$RACINE/Build/Bin/Release-Web/renderdemo"
NODE="${NK_NODE:-/c/emsdk/emsdk/node/22.16.0_64bit/bin/node.exe}"
EDGE="${NK_EDGE:-/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe}"
PORT=${NK_WEB_PORT:-9002}
DEVPORT=${NK_DEVTOOLS_PORT:-9333}
PROFIL="${TMPDIR:-/tmp}/nk_edge_headless_profile"

[ -f "$DIR/renderdemo.html" ] || { echo "renderdemo.html introuvable dans $DIR (construire : jenga build --platform web --config Release --target renderdemo)"; exit 2; }
[ -x "$NODE" ] || { echo "node introuvable : $NODE (NK_NODE=...)"; exit 2; }
[ -f "$EDGE" ] || { echo "msedge introuvable : $EDGE (NK_EDGE=...)"; exit 2; }

cd "$DIR" || exit 2
python -m http.server "$PORT" --bind 127.0.0.1 > /dev/null 2>&1 &
HTTP_PID=$!
sleep 1
"$EDGE" --headless=new --remote-debugging-port="$DEVPORT" --window-size=640,480 \
  --user-data-dir="$PROFIL" --no-first-run --no-default-browser-check \
  --disable-extensions --mute-audio "about:blank" > /dev/null 2>&1 &
EDGE_PID=$!
"$NODE" "$ICI/web_headless_pilote.mjs" "http://127.0.0.1:$PORT/renderdemo.html?$PARAMS" "$DEVPORT" "$CIBLE" "$MAXS" "$OUT"
RC=$?
kill $EDGE_PID 2>/dev/null
kill $HTTP_PID 2>/dev/null
exit $RC
