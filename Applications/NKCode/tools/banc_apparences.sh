#!/usr/bin/env bash
#
# banc_apparences.sh — LA REVERSIBILITE DES APPARENCES ET DES JEUX D'ICONES (01/10)
# =============================================================================
# « Revenir = rechoisir » (Rihen). Le banc lance le VRAI NKCode SANS FENETRE
# (NK_FENETRE_CACHEE=1), avec un dossier personnel jetable (USERPROFILE/APPDATA
# rediriges : les reglages de Rihen ne sont pas touches), sur un workspace Jenga
# jetable, et NK_BANC_APPARENCES=1 (Shell/NkBancApparences.h) :
#   photo A -> autre apparence + jeu Trait -> photo B (doit differer) -> retour
#   -> photo C (doit etre IDENTIQUE a A, octet pour octet) ; une icone absente du
#   jeu retombe sur les PNG de base ; installer/desinstaller rend la liste d'avant.
#
#   ./Applications/NKCode/tools/banc_apparences.sh                 le banc (VERT attendu)
#   ./Applications/NKCode/tools/banc_apparences.sh --contre-essai  les deux mutations
#        (residu, sansrepli) : le banc DOIT rougir pour chacune.
#
# Lancer depuis la RACINE du depot, NKCode construit (jenga build --target NKCode).
# CODES : 0 vert (ou contre-essai reussi), 1 rouge, 2 instrument casse.
# =============================================================================
set -u
RACINE="$(cd "$(dirname "$0")/../../.." && pwd)"
EXE="$RACINE/Build/Bin/Debug-Windows/NKCode/NKCode.exe"
[ -x "$EXE" ] || { echo "[banc] ECHEC D'INSTRUMENT : $EXE absent (construire NKCode)."; exit 2; }
TMP="$(mktemp -d)" || { echo "[banc] ECHEC D'INSTRUMENT : mktemp"; exit 2; }
trap 'rm -rf "$TMP"' EXIT
win() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

lancer() { # lancer <mutation> -> sortie du banc
	local mut="$1" H="$TMP/home_$1" WS="$TMP/ws_$1"
	mkdir -p "$H/.nkcode" "$H/AppData/Roaming" "$H/AppData/Local" "$WS"
	printf "win=0|0|1600|900\nmaximized=0\n" > "$H/.nkcode/window.cfg"
	printf 'from Jenga import *\nwith workspace("Banc"):\n    configurations(["Debug"])\n    with project("Banc"):\n        consoleapp()\n        language("C++")\n' > "$WS/Banc.jenga"
	( cd "$RACINE" && USERPROFILE="$(win "$H")" HOME="$(win "$H")" APPDATA="$(win "$H/AppData/Roaming")" \
		LOCALAPPDATA="$(win "$H/AppData/Local")" NK_FENETRE_CACHEE=1 NK_BANC_APPARENCES=1 \
		NK_BANC_MUTATION="$mut" NK_AGENT_EXIT=135 timeout 240 "$EXE" "$(win "$WS")" ) 2>&1 | grep -a "\[banc-apparences\]"
}

if [ "${1:-}" = "--contre-essai" ]; then
	rc=0
	for m in residu sansrepli; do
		echo "=== contre-essai : mutation « $m » (le banc DOIT rougir) ==="
		sortie="$(lancer "$m")"
		echo "$sortie"
		if echo "$sortie" | grep -q "VERDICT : ROUGE"; then echo "[banc] contre-essai « $m » : a rougi -- REUSSI"
		else echo "[banc] contre-essai « $m » : N'A PAS ROUGI -- ECHOUE"; rc=1; fi
	done
	exit $rc
fi

sortie="$(lancer aucune)"
echo "$sortie"
echo "$sortie" | grep -q "VERDICT : VERT" && exit 0
echo "$sortie" | grep -q "VERDICT" || { echo "[banc] ECHEC D'INSTRUMENT : pas de verdict"; exit 2; }
exit 1
