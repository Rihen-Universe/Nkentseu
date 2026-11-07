#!/usr/bin/env bash
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# banc_webaudio.sh — L'AUDIO WEB SE DEBLOQUE AU PREMIER GESTE (08/11)
# =============================================================================
# Rodolf (08/11) : « il se pourrait que le web ait des problemes avec l'audio ». Cause trouvee par la lecture : l'AudioContext
# etait cree au demarrage, hors de tout geste, donc « suspended » (regle de lecture automatique des navigateurs), et
# `resume()` n'etait rappele qu'a Init / Start. Ce banc COMPILE le vrai JavaScript du dorsal (src/NKAudio/NkWebAudioJs.h) dans une
# page Emscripten, la sert par `python -m http.server` (comme le panneau Web de NKCode), l'ouvre dans Chrome SANS fenetre et SANS
# SON (--headless=new --mute-audio), et lit l'etat REEL par le protocole de debogage :
#   (w0) AVANT tout geste le contexte n'est PAS « running » (sinon le banc ne prouve rien : ECHEC D'INSTRUMENT) ;
#   (w1) APRES un clic de confiance (Input.dispatchMouseEvent) il est « running », le navigateur demande des trames audio,
#        et la console n'a aucune erreur.
#
#   ./Kernel/Runtime/NKAudio/tools/banc_webaudio.sh                     (VERT attendu)
#   ./Kernel/Runtime/NKAudio/tools/banc_webaudio.sh --contre-essai      sans-deblocage (l'ancien code) -> ROUGE
# La page est compilee avec les DRAPEAUX REELS des kits Web (Pong.jenga : ASYNCIFY, SAFE_HEAP, ASSERTIONS, memoire extensible).
# Aucun son : --mute-audio. Aucune fenetre : --headless=new. Reseau : 127.0.0.1 seulement.
# =============================================================================
set -u
ICI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHROME="${NK_BANC_CHROME:-/c/Program Files/Google/Chrome/Application/chrome.exe}"
NODE="${NK_BANC_NODE:-$(command -v node || echo /c/emsdk/node/24.19.0_64bit/node)}"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
win() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

lancer() { # lancer <mutation>
	local m="$1" D="$TMP/$1" defs=() PS PC chromeProfil
	mkdir -p "$D/site"
	[ "$m" = "sans-deblocage" ] && defs=(-DNK_WEBAUDIO_MUTATION_SANS_DEBLOCAGE)
	if ! emcc "$ICI/webaudio/test_webaudio.cpp" "${defs[@]}" -O1 -o "$D/site/index.html" --shell-file "$ICI/webaudio/shell_min.html" \
		-s ASYNCIFY -s DYNAMIC_EXECUTION "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPU8']" -s INITIAL_MEMORY=67108864 \
		-s ALLOW_MEMORY_GROWTH=1 -s ASSERTIONS=1 -s NO_EXIT_RUNTIME=1 -s SAFE_HEAP=1 >"$D/emcc.log" 2>&1; then
		echo "[banc-webaudio] ECHEC D'INSTRUMENT : emcc a echoue : $(tail -3 "$D/emcc.log" | tr '\n' ' ' | cut -c1-200)"; return
	fi
	local portS=$((20000 + RANDOM % 8000)) portC=$((29000 + RANDOM % 8000))
	( cd "$D/site" && exec python -m http.server "$portS" --bind 127.0.0.1 ) > "$D/serveur.log" 2>&1 &
	PS=$!
	chromeProfil="$D/profil"
	"$CHROME" --headless=new --mute-audio --no-first-run --no-default-browser-check --disable-gpu \
		--remote-debugging-port=$portC --user-data-dir="$(win "$chromeProfil")" about:blank > "$D/chrome.log" 2>&1 &
	PC=$!
	local sortie
	sortie="$("$NODE" "$(win "$ICI/webaudio/cdp.js")" "$portC" "http://127.0.0.1:$portS/index.html" 2>&1)"
	# on ne ferme que NOS processus : le serveur, et le Chrome dont le profil est celui du banc
	kill $PS 2> /dev/null
	powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='chrome.exe'\" | Where-Object { \$_.CommandLine -like '*$(basename "$TMP")*' } | ForEach-Object { Stop-Process -Id \$_.ProcessId -Force }" > /dev/null 2>&1
	wait $PC 2> /dev/null
	local l; l="$(echo "$sortie" | grep -a "^\[webaudio\] avant=" | head -1)"
	if [ -z "$l" ]; then echo "[banc-webaudio] ECHEC D'INSTRUMENT : $(echo "$sortie" | tr '\n' ' ' | cut -c1-300)"; return; fi
	local avant apres ta tp er
	avant="$(echo "$l" | sed -n 's/^\[webaudio\] avant=\([a-z]*\).*/\1/p')"; ta="$(echo "$l" | sed -n 's/.*trames_avant=\([0-9]*\).*/\1/p')"
	apres="$(echo "$l" | sed -n 's/.* apres=\([a-z]*\).*/\1/p')"; tp="$(echo "$l" | sed -n 's/.*trames_apres=\([0-9]*\).*/\1/p')"
	er="$(echo "$l" | sed -n 's/.*erreurs=\([0-9]*\).*/\1/p')"
	if [ "$avant" = running ]; then
		echo "[banc-webaudio] ECHEC D'INSTRUMENT : le contexte tourne deja avant tout geste ($l) : ce navigateur n'applique pas la regle"; return
	fi
	echo "[banc-webaudio] (w0) avant tout geste : contexte « $avant », $ta trame(s) demandee(s) (la regle de lecture automatique joue) OK"
	if [ "$apres" = running ] && [ "${tp:-0}" -gt "${ta:-0}" ] && [ "${er:-1}" = 0 ]; then
		echo "[banc-webaudio] (w1) apres un clic : contexte « $apres », $tp trame(s), $er erreur(s) OK"
		echo "[banc-webaudio] 2 temoin(s) -- VERDICT : VERT"
	else
		echo "[banc-webaudio] (w1) apres un clic : contexte « $apres », $tp trame(s), $er erreur(s) -- le son ne part jamais ECHEC"
		echo "$sortie" | grep -a "^\[webaudio\]   " | head -3
		echo "[banc-webaudio] 2 temoin(s) -- VERDICT : ROUGE"
	fi
}

mode="${1:-}"
if [ "$mode" = "--contre-essai" ]; then
	s="$(lancer sans-deblocage)"; echo "$s" | grep -a "ECHEC\|VERDICT"
	if echo "$s" | grep -q "VERDICT : ROUGE"; then echo "[banc] contre-essai « sans-deblocage » : a rougi -- REUSSI"; exit 0; fi
	echo "[banc] contre-essai « sans-deblocage » : N'A PAS ROUGI -- ECHOUE"; exit 1
fi
s="$(lancer aucune)"; echo "$s"
echo "$s" | grep -q "VERDICT : VERT" && exit 0
echo "$s" | grep -q "VERDICT" || exit 2
exit 1
