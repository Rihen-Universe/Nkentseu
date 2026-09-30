#!/usr/bin/env bash
#
# capturer-rendu.sh — UNE IMAGE RENDUE, ECRITE EN PNG, SANS ECRAN (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   « La fenetre s'ouvre » ne dit pas « l'image est juste ». Le runner macOS a
#   un GPU Metal (paravirtualise), mais pas d'autorisation de capture d'ecran.
#   Les tutoriels savent rendre une image dans une cible HORS ECRAN et l'ecrire
#   en PNG (Tutoriels3D/Commun/NkTutoCapture.h, variables NK_CAPTURE*) ; ce
#   script les lance ainsi, pour une API donnee (NK_GFX_BACKEND), et dit si
#   l'image a ete ecrite ET avec quelle API elle a ete rendue.
#
# USAGE
#   capturer-rendu.sh <binaire> <api> <image-n> <png> <dossier-sortie>
#     api : metal | software | ... (NK_GFX_BACKEND)
#   Sortie 0 : PNG ecrit, rendu par l'API demandee. Sortie 1 sinon.
# =============================================================================

set -u

BIN="${1:?binaire manquant}"
API="${2:?api manquante}"
IMAGE="${3:?numero de l image manquant}"
PNG="${4:?png manquant}"
SORTIE="${5:?dossier de sortie manquant}"

NOM=$(basename "$BIN")
mkdir -p "$SORTIE" "$(dirname "$PNG")"
JOURNAL="$SORTIE/$NOM-capture-$API.log"
rm -f "$PNG"

echo "== capture : $NOM, API $API, image $IMAGE -> $PNG"
AVANT=$(wc -l < logs/app.log 2>/dev/null || echo 0)
NK_GFX_BACKEND="$API" NK_CAPTURE="$IMAGE" NK_CAPTURE_PATH="$PNG" "$BIN" > "$JOURNAL" 2>&1 &
PID=$!
ECOULE=0
while [ "$ECOULE" -lt 120 ] && kill -0 "$PID" 2>/dev/null; do
    sleep 1
    ECOULE=$((ECOULE + 1))
done
if kill -0 "$PID" 2>/dev/null; then
    kill -TERM "$PID" 2>/dev/null
    sleep 2
    kill -KILL "$PID" 2>/dev/null || true
    CODE="delai depasse (120 s)"
else
    wait "$PID"
    CODE=$?
fi

Echapper() {
    perl -pe 's/%/%25/g; s/\r/%0D/g; s/\n/%0A/g'
}

# Le journal du moteur va a la sortie ET dans logs/app.log, cumule sur le job :
# on n'en garde que les lignes de CE lancement.
APPLOG="$SORTIE/$NOM-app-$API.log"
tail -n +$((AVANT + 1)) logs/app.log > "$APPLOG" 2>/dev/null || true
CHOISIE=$(grep -a -h -o "API selectionnee = [A-Za-z0-9 ]*" "$JOURNAL" "$APPLOG" 2>/dev/null | tail -1)
LIGNE_CAPTURE=$(grep -a -h "NkTutoCapture" "$JOURNAL" "$APPLOG" 2>/dev/null | tail -1 | cut -c1-300)
echo "code : $CODE ; $CHOISIE"
echo "$LIGNE_CAPTURE"

if [ ! -s "$PNG" ]; then
    echo "--- sortie du programme (fin) ---"
    tail -n 80 "$JOURNAL"
    printf '::error title=%s capture %s::%s\n' "$NOM" "$API" \
        "$( { echo "aucun PNG ecrit (code $CODE ; $CHOISIE)"; grep -a -h -E "ERR|rror|refuse|KO|assert" "$JOURNAL" "$APPLOG" 2>/dev/null | head -n 25 | cut -c1-250; } | Echapper)"
    exit 1
fi

# L'image doit venir de l'API demandee : sans cela, un Metal en echec replie
# sur le rendu logiciel passerait pour un Metal qui marche.
case "$API" in
    metal) ATTENDU="Metal" ;;
    software) ATTENDU="Software" ;;
    *) ATTENDU="" ;;
esac
if [ -n "$ATTENDU" ] && ! printf '%s' "$CHOISIE $LIGNE_CAPTURE" | grep -qi "$ATTENDU"; then
    printf '::error title=%s capture %s::%s\n' "$NOM" "$API" \
        "$( { echo "PNG ecrit, mais PAS par $ATTENDU : $CHOISIE"; echo "$LIGNE_CAPTURE"; } | Echapper)"
    exit 1
fi

printf '::notice title=%s capture %s::%s\n' "$NOM" "$API" \
    "$( { echo "$CHOISIE"; echo "$LIGNE_CAPTURE"; } | Echapper)"
exit 0
