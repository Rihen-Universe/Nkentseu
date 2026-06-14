#!/usr/bin/env bash
#
# gitcommit.sh — Commit PROPRE pour le depot Nkentseu, AUCUNE mention Claude.
# -----------------------------------------------------------------------------
# Le message de commit est EXACTEMENT celui passe (jamais de trailer
# Co-Authored-By Claude, jamais de "Generated with Claude Code"). L'identite
# auteur/committer est celle configuree pour CE depot (par defaut LeTeguis, via
# la config git locale/globale) ; on peut la forcer en remplissant GIT_NAME /
# GIT_EMAIL ci-dessous, ou via les variables d'environnement du meme nom.
#
# USAGE
#   ./gitcommit.sh "<message>" [chemin1 chemin2 ...]
#     - chemins fournis  -> stage UNIQUEMENT ceux-la (commit cible et propre).
#     - aucun chemin     -> stage les changements des fichiers SUIVIS (git add -u).
#
# EXEMPLES
#   ./gitcommit.sh "feat(canvas): clip scissor 5 backends" Kernel/Runtime/NKCanvas
#   ./gitcommit.sh "docs: maj wiki Foundation"
# -----------------------------------------------------------------------------
set -uo pipefail

# Identite : vide => on utilise la config git du depot (recommande pour Nkentseu).
# Pour forcer une identite independante de la config, renseigne ces deux valeurs
# (ou exporte GIT_NAME / GIT_EMAIL avant l'appel).
GIT_NAME="${GIT_NAME:-}"
GIT_EMAIL="${GIT_EMAIL:-}"

[ "${1:-}" != "" ] || { echo "Usage: $0 \"<message>\" [chemins...]" >&2; exit 1; }
MSG="$1"; shift

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { echo "[gitcommit] cd $ROOT impossible" >&2; exit 1; }

if [ "$#" -gt 0 ]; then
  git add -- "$@" || { echo "[gitcommit] git add a echoue" >&2; exit 1; }
else
  git add -u || { echo "[gitcommit] git add -u a echoue" >&2; exit 1; }
fi

if git diff --cached --quiet; then
  echo "[gitcommit] rien a committer (index vide)."
  exit 0
fi

# Construit les options d'identite seulement si on veut la forcer.
ID_OPTS=()
[ -n "$GIT_NAME" ]  && ID_OPTS+=(-c "user.name=$GIT_NAME")
[ -n "$GIT_EMAIL" ] && ID_OPTS+=(-c "user.email=$GIT_EMAIL")

git "${ID_OPTS[@]}" commit -m "$MSG" \
  || { echo "[gitcommit] git commit a echoue" >&2; exit 1; }

echo "[gitcommit] OK — commit cree (zero mention Claude)."
git --no-pager log -1 --format='   %h  A:%an <%ae>  C:%cn <%ce>'
