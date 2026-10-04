#!/usr/bin/env bash
#
# ref-jenga.sh — QUELLE REF DE JENGA LA CI RECUPERE (2026-09-30, TEMPORAIRE)
# =============================================================================
# POURQUOI IL EXISTE
#
#   La CI recupere Jenga au TAG v<JENGA_VERSION> (config/
#   JENGA_VERSION) et c'est la bonne regle : une branche mouvante a deja fait
#   construire la CI avec Jenga 2.4.0 pendant que NKCode exigeait 2.8.0.
#
#   Mais une correction de Jenga qui ne se voit QUE sur un Mac (frameworks des
#   statiques, objcarc(), toolchain « clang-native ») doit etre eprouvee par
#   cette CI AVANT sa release. D'ou une derogation, explicite et bruyante :
#
#     1. l'entree `jenga_ref` d'un lancement manuel (workflow_dispatch), sinon
#     2. la premiere ligne utile de .github/JENGA_REF, si le fichier existe,
#     3. sinon le tag v<JENGA_VERSION>, comme avant.
#
#   Toute derogation publie un AVERTISSEMENT (annotation) sur chaque job : un
#   run vert construit avec une branche ne doit pas passer pour un run vert
#   construit avec une release.
#
#   ⚠️ A RETIRER apres la release : supprimer .github/JENGA_REF des que
#      Jenga 2.8.8 est publie et que JENGA_VERSION porte 2.8.8. Le script et
#      l'entree manuelle peuvent rester, ils ne changent rien sans fichier.
#
# USAGE
#   ref-jenga.sh <version> [ref-forcee]
#   Ecrit `ref=<ref>` dans $GITHUB_OUTPUT (ou sur la sortie hors CI).
# =============================================================================

set -u

V="${1:?version manquante}"
FORCEE="$(printf '%s' "${2:-}" | tr -d '[:space:]')"
ICI=$(cd "$(dirname "$0")" && pwd)
FICHIER="$ICI/../JENGA_REF"

REF=""
SOURCE=""
if [ -n "$FORCEE" ]; then
    REF="$FORCEE"
    SOURCE="entree jenga_ref du lancement manuel"
elif [ -f "$FICHIER" ]; then
    REF=$(grep -v '^[[:space:]]*#' "$FICHIER" | tr -d '[:space:]' | head -c 200)
    SOURCE=".github/JENGA_REF"
fi

if [ -n "$REF" ]; then
    echo "::warning title=Jenga hors release::la CI construit avec Jenga '$REF' ($SOURCE) et NON avec le tag v$V. Derogation temporaire : supprimer .github/JENGA_REF apres la release."
else
    REF="v$V"
fi

echo "Jenga recupere : $REF"
if [ -n "${GITHUB_OUTPUT:-}" ]; then
    echo "ref=$REF" >> "$GITHUB_OUTPUT"
else
    echo "ref=$REF"
fi
