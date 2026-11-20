#!/usr/bin/env bash
#
# banc_depot.sh — UNE PLANCHE .nkref GLISSEE DANS LA FENETRE S'OUVRE, PAR LA PORTE DE Ctrl+O (20/11/2026)
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# =============================================================================
# Rodolf : « quand je glisse un .nkref dans NkRef, rien ne se charge ; avec Ctrl+O et le meme fichier, ca marche ».
# Cause : le traitement de NkDropFileEvent envoyait CHAQUE fichier au decodeur d'images (addImageFromFile) ; un .nkref
# n'est pas une image. Correctif (src/NkRef/main.cpp) : un .nkref depose passe par openBoardFrom, la porte de Ctrl+O.
#
# Le VRAI NkRef, fenetre cachee (NK_FENETRE_CACHEE=1), lance depuis la racine ; une COPIE de NKORA_idees.nkref.
#   A : NK_AGENT_DEPOT -- un vrai NkDropFileEvent pose dans la file d'evenements de la fenetre (comme Windows) ;
#   B : NK_AGENT_OPEN  -- la porte de Ctrl+O (openBoardFrom), sur le meme fichier.
#   (d1) A : le depot a bien ete pose (« depot simule ») -- sinon l'instrument n'a rien eprouve ;
#   (d2) A : « planche ouverte <- ...NKORA_idees.nkref (N images) », N >= 1 ;
#   (d3) A et B ouvrent le MEME nombre d'images (la meme porte, le meme resultat).
#   ./Applications/NkRef/tools/banc_depot.sh [--contre-essai]   (NK_BANC_EXE=<NkRef.exe>, NK_BANC_PLANCHE=<.nkref>)
#   contre-essai : NK_NKREF_MUTATION=depot-planche-comme-image (l'etat d'avant) -> d2, d3 ROUGES.
# Le fichier des recents (nkref_recents.txt, dossier courant = la racine) est rendu tel qu'il etait.
# =============================================================================
RACINE="$(cd "$(dirname "$0")/../../.." && pwd)"
EXE="${NK_BANC_EXE:-$RACINE/Build/Bin/Release-Windows/NkRef/NkRef.exe}"
SOURCE="${NK_BANC_PLANCHE:-/d/Rihen/Livraisons/Tournage_NKCode_NKORA_2026-11-20/NKORA_idees.nkref}"
[ -x "$EXE" ] || { echo "[banc-depot] ECHEC D'INSTRUMENT : $EXE absent"; exit 2; }
[ -s "$SOURCE" ] || { echo "[banc-depot] ECHEC D'INSTRUMENT : $SOURCE absent"; exit 2; }
win() { cygpath -w "$1"; }
TMP="$(mktemp -d)"
cp "$SOURCE" "$TMP/NKORA_idees.nkref"
REC="$RACINE/nkref_recents.txt"
if [ -f "$REC" ]; then cp "$REC" "$TMP/recents_avant"; fi

course() { # course <mutation> <journal> <VAR=valeur>
	( cd "$RACINE" && env NK_FENETRE_CACHEE=1 NK_NKREF_MUTATION="$1" "$3" NK_AGENT_EXIT=150 \
		timeout -s KILL 90 "$EXE" > "$2" 2>&1 )
}
lancer() { # lancer <mutation> -> 0 vert, 1 rouge
	local m="$1" ja="$TMP/a_$1.log" jb="$TMP/b_$1.log" na nb rouge=0
	course "$m" "$ja" "NK_AGENT_DEPOT=40:640:400:$(win "$TMP/NKORA_idees.nkref")"
	course "$m" "$jb" "NK_AGENT_OPEN=40:$(win "$TMP/NKORA_idees.nkref")"
	grep -a "^\[NkRef\]" "$ja" | cut -c1-200 | sed 's/^/[banc-depot]   A /'
	grep -a "planche ouverte" "$jb" | cut -c1-200 | sed 's/^/[banc-depot]   B /'
	na="$(grep -a "planche ouverte <- .*NKORA_idees.nkref" "$ja" | sed -n 's/.*(\([0-9]*\) images).*/\1/p' | head -1)"
	nb="$(grep -a "planche ouverte <- .*NKORA_idees.nkref" "$jb" | sed -n 's/.*(\([0-9]*\) images).*/\1/p' | head -1)"
	if grep -aq "depot simule : .*NKORA_idees.nkref" "$ja"; then echo "[banc-depot] (d1) le depot est pose dans la file d'evenements OK"
	else echo "[banc-depot] (d1) aucun depot pose : l'instrument n'a rien eprouve ECHEC"; rouge=1; fi
	if [ -n "$na" ] && [ "$na" -ge 1 ]; then echo "[banc-depot] (d2) le .nkref depose ouvre la planche ($na images) OK"
	else echo "[banc-depot] (d2) le .nkref depose n'ouvre rien ECHEC"; rouge=1; fi
	if [ -n "$na" ] && [ "$na" = "$nb" ]; then echo "[banc-depot] (d3) depot et Ctrl+O : la meme planche ($na = $nb images) OK"
	else echo "[banc-depot] (d3) depot ${na:-rien} contre Ctrl+O ${nb:-rien} ECHEC"; rouge=1; fi
	if [ "$rouge" = 1 ]; then echo "[banc-depot] VERDICT : ROUGE"; return 1; fi
	echo "[banc-depot] VERDICT : VERT"; return 0
}
fin() { # le fichier des recents, tel qu'il etait
	if [ -f "$TMP/recents_avant" ]; then cp "$TMP/recents_avant" "$REC"; else rm -f "$REC"; fi
	rm -rf "$TMP"
}

if [ "${1:-}" = "--contre-essai" ]; then
	echo "[banc-depot] contre-essai : depot-planche-comme-image (ROUGE attendu)"
	if lancer depot-planche-comme-image; then echo "[banc-depot] CONTRE-ESSAI RATE : la mutation reste verte"; fin; exit 1; fi
	echo "[banc-depot] CONTRE-ESSAI REUSSI : la mutation rougit le banc"; fin; exit 0
fi
lancer ""; r=$?
fin
exit $r
