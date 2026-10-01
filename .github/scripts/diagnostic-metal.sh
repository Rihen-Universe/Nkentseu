#!/usr/bin/env bash
#
# diagnostic-metal.sh — CE QUE LA VALIDATION METAL REPROCHE A UN RENDU (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Sans couche de validation, Metal ne dit presque rien : un index de texture
#   faux, un tampon trop court ou un format de cible different se paient d'une
#   image fausse, sans message. La couche de validation (MTL_DEBUG_LAYER) les
#   nomme ; en mode « nslog », elle les ECRIT au lieu d'arreter le processus au
#   premier, ce qui donne la liste entiere en un seul passage de CI.
#
#   Le script lance le binaire ainsi (Metal impose, MSL de chaque nuanceur
#   ecrit dans un dossier : NK_DUMP_MSL), et publie en annotation les messages
#   de validation distincts. C'est un DIAGNOSTIC : il ne decide pas du verdict.
#
# USAGE
#   diagnostic-metal.sh <binaire> <secondes> <dossier-sortie>
# =============================================================================

set -u

BIN="${1:?binaire manquant}"
SECONDES="${2:?duree manquante}"
SORTIE="${3:?dossier de sortie manquant}"
NOM=$(basename "$BIN")
mkdir -p "$SORTIE/msl"
JOURNAL="$SORTIE/$NOM-validation.log"

MTL_DEBUG_LAYER=1 MTL_DEBUG_LAYER_ERROR_MODE=nslog MTL_DEBUG_LAYER_WARNING_MODE=nslog \
    NK_GFX_BACKEND=metal NK_DUMP_MSL="$SORTIE/msl" \
    "$BIN" > "$JOURNAL" 2>&1 &
PID=$!
ECOULE=0
while [ "$ECOULE" -lt "$SECONDES" ] && kill -0 "$PID" 2>/dev/null; do
    sleep 1
    ECOULE=$((ECOULE + 1))
done
kill -TERM "$PID" 2>/dev/null
sleep 2
kill -KILL "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

NB_MSL=$(ls "$SORTIE/msl" 2>/dev/null | wc -l | tr -d ' ')
MESSAGES=$(grep -a -h -E "failed assertion|Validation|validat|MTLDebug|NkRHI_Metal\]\[ERR\]|pas de source MSL" "$JOURNAL" \
    | sed -E 's/^[0-9-]+ [0-9:.]+ [^ ]+\[[0-9:a-f]+\] //' | cut -c1-260 | sort | uniq -c | sort -rn | head -n 40)
echo "MSL ecrits : $NB_MSL"
echo "$MESSAGES"
printf '::warning title=%s validation Metal::%s\n' "$NOM" \
    "$( { echo "MSL ecrits : $NB_MSL (artefact)"; echo "$MESSAGES"; } | perl -pe 's/%/%25/g; s/\r/%0D/g; s/\n/%0A/g')"
exit 0
