#!/usr/bin/env bash
#
# construire.sh — CONSTRUIRE UNE CIBLE JENGA, ET DIRE POURQUOI ELLE ECHOUE (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Chaque etape de construction de la CI faisait la meme chose : lancer
#   Jenga, garder le journal, et — c'est ce qui manquait — rendre l'erreur
#   LISIBLE sans compte GitHub (voir journal-en-annotations.sh). Ecrit une
#   fois ici, pour que la correction faite sur une etape serve a toutes.
#
# USAGE
#   construire.sh build <cible> <config> <dossier-journaux>
#   construire.sh test  <cible> <config> <dossier-journaux>
#
#   Le mode `test` passe par `jenga test --project`, qui construit puis LANCE
#   la suite : son verdict fait partie du resultat.
#
#   Sortie : celle de Jenga. En cas de succes du mode build, le chemin du
#   binaire est ecrit dans <dossier-journaux>/<cible>.chemin.
# =============================================================================

set -u
set -o pipefail

MODE="${1:?mode manquant (build ou test)}"
CIBLE="${2:?cible manquante}"
CONFIG="${3:?configuration manquante}"
JOURNAUX="${4:?dossier des journaux manquant}"
ICI=$(cd "$(dirname "$0")" && pwd)

mkdir -p "$JOURNAUX"
JOURNAL="$JOURNAUX/$MODE-$CIBLE.log"

DEBUT=$(date +%s)
if [ "$MODE" = "test" ]; then
    python3 -m Jenga test --project "$CIBLE" --config "$CONFIG" 2>&1 | tee "$JOURNAL"
else
    python3 -m Jenga build --target "$CIBLE" --config "$CONFIG" 2>&1 | tee "$JOURNAL"
fi
CODE=$?
DUREE=$(( $(date +%s) - DEBUT ))

# ⚠️ JENGA PEUT RENDRE 0 SUR UN ECHEC. Le code de sortie seul ne suffit pas : on
#    relit aussi le resume (« Build failed », « FAILED ») — une construction
#    ratee annoncee verte est pire qu'une construction rouge.
#    Les libelles sont ceux de Jenga 2.8.6 (Reporter.py, Build.py, Test.py).
if [ "$CODE" -eq 0 ] && grep -q -E 'BUILD FAILED|\] Build failed\.|Tests failed for' "$JOURNAL"; then
    echo "::warning title=$CIBLE::Jenga a rendu 0 mais son journal annonce un echec"
    CODE=1
fi

if [ "$CODE" -ne 0 ]; then
    echo "== $MODE $CIBLE ($CONFIG) : ECHEC, code $CODE, ${DUREE} s"
    "$ICI/journal-en-annotations.sh" "$JOURNAL" "$CIBLE"
    exit "$CODE"
fi

echo "== $MODE $CIBLE ($CONFIG) : OK en ${DUREE} s"
if [ "$MODE" = "build" ]; then
    # On CHERCHE le binaire au lieu de deviner son chemin (le dossier porte la
    # configuration et le systeme, et le .app eventuel en contient une copie).
    BIN=$(find Build/Bin -type f -name "$CIBLE" -perm -u+x -not -path "*.app/*" 2>/dev/null | head -1)
    if [ -z "$BIN" ]; then
        echo "::warning title=$CIBLE::construit, mais aucun binaire executable nomme $CIBLE sous Build/Bin"
    else
        echo "binaire : $BIN"
        printf '%s\n' "$BIN" > "$JOURNAUX/$CIBLE.chemin"
    fi
    echo "::notice title=$CIBLE construit::$CONFIG en ${DUREE} s -> ${BIN:-?}"
else
    RESUME=$(grep -E 'passed|reussi|All tests' "$JOURNAL" | tail -n 5 | perl -pe 's/\e\[[0-9;?]*[A-Za-z]//g; s/%/%25/g; s/\n/%0A/g')
    echo "::notice title=$CIBLE vert::$CONFIG en ${DUREE} s%0A$RESUME"
fi
exit 0
