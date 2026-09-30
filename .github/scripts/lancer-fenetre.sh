#!/usr/bin/env bash
#
# lancer-fenetre.sh — UNE APPLICATION FENETREE DEMARRE-T-ELLE SUR macOS ? (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Un binaire qui se construit n'est pas un binaire qui tourne. Sur macOS,
#   l'essentiel se joue APRES l'edition de liens : NSApplication, la premiere
#   NSWindow, le calque Metal. Aucune machine de Rihen n'est un Mac ; ce runner
#   l'est, et il a une session graphique.
#
#   Le script lance l'application, la laisse vivre quelques secondes, puis
#   repond a trois questions, dans cet ordre :
#     1. est-elle encore en vie ?          (sinon : code de sortie + rapport
#                                           de plantage macOS en annotation)
#     2. a-t-elle une fenetre a l'ecran ?  (CoreGraphics, par PID)
#     3. a quoi ressemble l'ecran ?        (capture, si l'autorisation existe)
#     4. sait-elle QUITTER ?               (« quit » comme Cmd+Q : elle doit
#                                           sortir d'elle-meme et rendre 0)
#
# USAGE
#   lancer-fenetre.sh <binaire> <secondes> <dossier-sortie> [arguments...]
#
#   Sortie 0 : l'application tenait encore debout au bout du delai.
#   Sortie 1 : elle est morte avant (plantage, ou sortie en erreur).
#   L'absence de fenetre est un AVERTISSEMENT, pas un echec : le runner peut
#   refuser la lecture de la liste des fenetres sans que l'application y soit
#   pour rien.
# =============================================================================

set -u

BIN="${1:?binaire manquant}"
SECONDES="${2:?delai manquant}"
SORTIE="${3:?dossier de sortie manquant}"
shift 3

NOM=$(basename "$BIN")
ICI=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$SORTIE"
JOURNAL="$SORTIE/$NOM-execution.log"

Echapper() {
    perl -pe 's/%/%25/g; s/\r/%0D/g; s/\n/%0A/g'
}

# ── Le rapport de plantage macOS, en clair (voir rapport-plantage.py) ──
RapportDePlantage() {
    python3 "$ICI/rapport-plantage.py" "$NOM"
}

echo "== lancement : $BIN $* (delai ${SECONDES} s)"
"$BIN" "$@" > "$JOURNAL" 2>&1 &
PID=$!

# Attente par tranches d'une seconde : une mort precoce se voit tout de suite,
# et son code de sortie reste recuperable par `wait`.
ECOULE=0
while [ "$ECOULE" -lt "$SECONDES" ]; do
    if ! kill -0 "$PID" 2>/dev/null; then
        break
    fi
    sleep 1
    ECOULE=$((ECOULE + 1))
done

if ! kill -0 "$PID" 2>/dev/null; then
    wait "$PID"
    CODE=$?
    # Le serveur de rapports ecrit le .ips une ou deux secondes apres la mort.
    sleep 3
    RAPPORT=$(RapportDePlantage)
    echo "$RAPPORT" > "$SORTIE/$NOM-plantage.txt"
    echo "== $NOM est MORT apres ${ECOULE} s, code $CODE"
    echo "--- sortie du programme (fin) ---"
    tail -n 60 "$JOURNAL"
    echo "--- rapport de plantage ---"
    echo "$RAPPORT"
    printf '::error title=%s mort apres %s s code %s::%s\n' "$NOM" "$ECOULE" "$CODE" \
        "$( { tail -n 40 "$JOURNAL" | cut -c1-300; echo '--- plantage ---'; echo "$RAPPORT"; } | Echapper)"
    cp -R logs "$SORTIE/" 2>/dev/null || true
    exit 1
fi

echo "== $NOM tient debout apres ${SECONDES} s (pid $PID)"

# ── La fenetre existe-t-elle vraiment ? ──
FENETRES=$(swift "$ICI/fenetres-du-processus.swift" "$PID" 2>&1)
FEN_CODE=$?
echo "$FENETRES" | tee "$SORTIE/$NOM-fenetres.txt"

# ── La capture : meilleur effort ──
if screencapture -x "$SORTIE/$NOM-ecran.png" 2>"$SORTIE/$NOM-capture.err"; then
    echo "capture : $SORTIE/$NOM-ecran.png"
else
    echo "capture refusee : $(cat "$SORTIE/$NOM-capture.err")"
fi

# ── La fermeture PROPRE : « Quitter », comme Cmd+Q (quitter-proprement.swift) ──
# L'application doit fermer sa fenetre, sortir de sa boucle et RENDRE SON CODE.
# SIGTERM ne prouve rien de tout ca ; il ne sert plus que de dernier recours.
FERMETURE="non essayee"
if swift "$ICI/quitter-proprement.swift" "$PID"; then
    ATTENTE=0
    while [ "$ATTENTE" -lt 15 ] && kill -0 "$PID" 2>/dev/null; do
        sleep 1
        ATTENTE=$((ATTENTE + 1))
    done
    if kill -0 "$PID" 2>/dev/null; then
        FERMETURE="REFUSEE : toujours vivant ${ATTENTE} s apres la demande"
    else
        wait "$PID"
        FERMETURE="propre en ${ATTENTE} s, code $?"
    fi
fi
if kill -0 "$PID" 2>/dev/null; then
    kill -TERM "$PID" 2>/dev/null
    sleep 2
    kill -KILL "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
fi
echo "fermeture : $FERMETURE"
JOURNAL_APP=$(tail -n 8 logs/app.log 2>/dev/null | cut -c1-300)

echo "--- sortie du programme (fin) ---"
tail -n 40 "$JOURNAL"
cp -R logs "$SORTIE/" 2>/dev/null || true

if [ "$FEN_CODE" -eq 0 ]; then
    printf '::notice title=%s demarre::%s\n' "$NOM" \
        "$( { echo "vivant apres ${SECONDES} s ; fermeture $FERMETURE"; echo "$FENETRES"; tail -n 15 "$JOURNAL" | cut -c1-300; echo '--- logs/app.log ---'; echo "$JOURNAL_APP"; } | Echapper)"
else
    printf '::warning title=%s sans fenetre visible::%s\n' "$NOM" \
        "$( { echo "vivant apres ${SECONDES} s mais aucune fenetre d'application listee ; fermeture $FERMETURE"; echo "$FENETRES"; tail -n 15 "$JOURNAL" | cut -c1-300; } | Echapper)"
fi
case "$FERMETURE" in
    propre*) ;;
    *) printf '::warning title=%s fermeture::%s\n' "$NOM" "$FERMETURE" ;;
esac
exit 0
