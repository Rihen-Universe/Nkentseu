#!/usr/bin/env bash
#
# verifier-dorsal.sh — LE DORSAL CHOISI PAR DEFAUT EST-IL LE BON ? (2026-10-01)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Sur macOS, l'ordre est : Vulkan (si disponible) -> Metal -> OpenGL ->
#   logiciel (decision de Rihen). Le runner n'a pas de SDK Vulkan : Vulkan
#   doit etre ECARTE, avec sa raison, et Metal RETENU. Ce script lance le
#   programme SANS NK_GFX_BACKEND ni --backend, et verifie que le journal dit
#   bien chacune des phrases attendues.
#
# USAGE
#   verifier-dorsal.sh <binaire> <dossier-sortie> <phrase>... -- [arguments...]
#   (variables d'environnement du programme : celles du job, NK_GFX_BACKEND
#   retiree). Le programme doit se terminer seul (capture) ; borne a 120 s.
#   Sortie 0 : toutes les phrases sont dans le journal. Sortie 1 sinon.
# =============================================================================

set -u

BIN="${1:?binaire manquant}"
SORTIE="${2:?dossier de sortie manquant}"
shift 2
PHRASES=()
while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    PHRASES+=("$1")
    shift
done
[ "${1:-}" = "--" ] && shift

NOM=$(basename "$BIN")
mkdir -p "$SORTIE"
JOURNAL="$SORTIE/$NOM-dorsal-defaut.log"

env -u NK_GFX_BACKEND perl -e 'alarm 120; exec @ARGV' "$BIN" "$@" > "$JOURNAL" 2>&1
CODE=$?
echo "code : $CODE"

Echapper() {
    perl -pe 's/%/%25/g; s/\r/%0D/g; s/\n/%0A/g'
}

LIGNES=$(grep -a -h -E "ordre macOS|ecarte|retenu|API selectionnee" "$JOURNAL" | sed -E 's/^\[[^]]*\] \[[A-Z]+\] \[[^]]*\] \[[^]]*\] -> //' | cut -c1-220 | head -n 12)
echo "$LIGNES"

MANQUE=""
for P in "${PHRASES[@]}"; do
    grep -a -q -- "$P" "$JOURNAL" || MANQUE="$MANQUE [$P]"
done
if [ -n "$MANQUE" ]; then
    printf '::error title=%s dorsal par defaut::%s\n' "$NOM" \
        "$( { echo "phrases absentes du journal :$MANQUE"; echo "$LIGNES"; } | Echapper)"
    exit 1
fi
printf '::notice title=%s dorsal par defaut::%s\n' "$NOM" "$(echo "$LIGNES" | Echapper)"
exit 0
