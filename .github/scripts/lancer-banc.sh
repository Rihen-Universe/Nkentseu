#!/usr/bin/env bash
#
# lancer-banc.sh — UN BANC SANS FENETRE, ET SA PILE S'IL PLANTE (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Le premier `UnkenyEditor --selftest` sur macOS est mort en 0,5 s d'un
#   « Segmentation fault: 11 », SANS UNE LIGNE de sortie : dans un tube, la
#   sortie standard est tamponnee par blocs, et un plantage emporte le tampon.
#   Le journal ne disait donc ni ou, ni quand. Ce script :
#     1. lance le banc, sortie dans un fichier ;
#     2. s'il meurt d'un SIGNAL (code > 128), le relance sous lldb, qui
#        s'arrete sur la faute et imprime la pile — c'est elle qui nomme la
#        ligne ; le rapport .ips de macOS est joint aussi ;
#     3. publie le verdict en annotation (::notice vert, ::error rouge).
#
# USAGE
#   lancer-banc.sh <binaire> <titre> <dossier-sortie> [arguments...]
#   Sortie : le code du banc.
# =============================================================================

set -u

BIN="${1:?binaire manquant}"
TITRE="${2:?titre manquant}"
SORTIE="${3:?dossier de sortie manquant}"
shift 3

NOM=$(basename "$BIN")
ICI=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$SORTIE"
FICHIER=$(printf '%s' "$TITRE" | tr -c 'A-Za-z0-9_-' '_')
JOURNAL="$SORTIE/$FICHIER.log"

Echapper() {
    perl -pe 's/%/%25/g; s/\r/%0D/g; s/\n/%0A/g'
}

echo "== banc : $BIN $*"
"$BIN" "$@" > "$JOURNAL" 2>&1
CODE=$?
cat "$JOURNAL"

if [ "$CODE" -eq 0 ]; then
    printf '::notice title=%s vert::%s\n' "$TITRE" "$(tail -n 12 "$JOURNAL" | cut -c1-300 | Echapper)"
    exit 0
fi

PILE=""
if [ "$CODE" -gt 128 ]; then
    # Le serveur de rapports ecrit le .ips une ou deux secondes apres la mort.
    sleep 3
    RAPPORT=$(python3 "$ICI/rapport-plantage.py" "$NOM")
    echo "--- rapport de plantage ---"
    echo "$RAPPORT"
    echo "--- relance sous lldb ---"
    # -k : commandes jouees SEULEMENT si le processus s'arrete sur une faute.
    lldb --batch -o "run" -k "bt 40" -k "frame variable" -k "quit 1" -- "$BIN" "$@" \
        > "$SORTIE/$FICHIER-lldb.log" 2>&1
    PILE=$(grep -A60 -E "stop reason|thread #1" "$SORTIE/$FICHIER-lldb.log" | head -n 70)
    cat "$SORTIE/$FICHIER-lldb.log" | tail -n 120
    printf '::error title=%s plante code %s::%s\n' "$TITRE" "$CODE" \
        "$( { echo '--- fin de la sortie ---'; tail -n 15 "$JOURNAL" | cut -c1-300; echo '--- pile lldb ---'; printf '%s\n' "$PILE" | cut -c1-300; echo '--- rapport macOS ---'; printf '%s\n' "$RAPPORT" | head -n 40; } | Echapper)"
else
    printf '::error title=%s rouge code %s::%s\n' "$TITRE" "$CODE" "$(tail -n 25 "$JOURNAL" | cut -c1-300 | Echapper)"
fi
exit "$CODE"
