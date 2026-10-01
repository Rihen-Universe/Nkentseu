#!/usr/bin/env bash
#
# frameworks-au-lien.sh — LES FRAMEWORKS DES BIBLIOTHEQUES STATIQUES ARRIVENT-ILS
#                         AU LIEN DE L'EXECUTABLE ? (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Une bibliotheque statique (.a) ne porte pas ses frameworks : c'est le lien
#   de l'executable qui doit les nommer. Jusqu'a Jenga 2.8.7, un frameworks()
#   pose sur NKRHI ou NKCanvas n'atteignait JAMAIS ce lien ; chaque application
#   recopiait donc la liste a la main (Tutoriels3D.jenga, NKCraft.jenga...).
#   Jenga 2.8.8 les ajoute (transitivement). Ce script le PROUVE sur le binaire :
#   on choisit un framework que SEULE une bibliotheque declare (MetalKit : NKRHI),
#   et on le cherche dans les commandes de chargement (otool -L).
#
#   Tant que les applications recopient leurs frameworks, les retirer n'est sur
#   qu'une fois JENGA_VERSION a 2.8.8 : un Jenga 2.8.7 ne lierait plus.
#
# USAGE
#   frameworks-au-lien.sh <binaire> <framework>...
#   Sortie 0 : tous presents. Sortie 1 : au moins un absent (annotation d'erreur).
# =============================================================================

set -u

BIN="${1:?binaire manquant}"
shift
NOM=$(basename "$BIN")

LISTE=$(otool -L "$BIN" 2>&1)
echo "$LISTE"

MANQUANTS=""
for FW in "$@"; do
    if printf '%s\n' "$LISTE" | grep -q "/$FW.framework/"; then
        echo "present : $FW"
    else
        echo "ABSENT  : $FW"
        MANQUANTS="$MANQUANTS $FW"
    fi
done

if [ -n "$MANQUANTS" ]; then
    echo "::error title=$NOM frameworks::absents du lien :$MANQUANTS (declares par une bibliotheque statique, Jenga >= 2.8.8 attendu)"
    exit 1
fi
echo "::notice title=$NOM frameworks::presents au lien : $* (declares par une bibliotheque statique seulement)"
exit 0
