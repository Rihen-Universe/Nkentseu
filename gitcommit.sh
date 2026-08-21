#!/usr/bin/env bash
#
# gitcommit.sh — Commit PROPRE pour le depot Nkentseu.
# -----------------------------------------------------------------------------
# Le message de commit est EXACTEMENT celui passe. L'identite
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

# GARDE ANTI-NOUVEAU-DEPENDANT NKUI (2026-08-17, campagne de retrait NKUI).
# Tout commit qui touche un fichier .jenga passe le controle : NKUI est
# deprecie, un nouveau .jenga citant "NKUI" hors de la liste decroissante
# (config/nkui_dependants.list) est refuse AVANT le commit. Lancable aussi a
# la main : ./check_nkui_dependants.sh. Ce n'est pas une CI — controle local.
if git diff --cached --name-only | grep -q '\.jenga$'; then
  if [ -f "$ROOT/check_nkui_dependants.sh" ]; then
    bash "$ROOT/check_nkui_dependants.sh" || {
      echo "[gitcommit] GARDE NKUI : commit refuse (voir ci-dessus)." >&2
      echo "[gitcommit] L'index reste stage ; corrige puis relance." >&2
      exit 1
    }
  fi
fi

# Construit les options d'identite seulement si on veut la forcer.
ID_OPTS=()
[ -n "$GIT_NAME" ]  && ID_OPTS+=(-c "user.name=$GIT_NAME")
[ -n "$GIT_EMAIL" ] && ID_OPTS+=(-c "user.email=$GIT_EMAIL")

# AVEC des chemins : committer PAR PATHSPEC (git commit -- <chemins>), qui
# n'emporte QUE ces chemins — un `commit -m` nu commite TOUT l'index, y
# compris ce qu'un AUTRE agent a stage en preparant son propre commit
# (incident du 9 aout : 4 fichiers du chantier NKAI embarques dans b9a82e62,
# repares par 61edb893). Sans chemins : comportement historique (add -u).
if [ "$#" -gt 0 ]; then
  git "${ID_OPTS[@]}" commit -m "$MSG" -- "$@" \
    || { echo "[gitcommit] git commit a echoue" >&2; exit 1; }
else
  git "${ID_OPTS[@]}" commit -m "$MSG" \
    || { echo "[gitcommit] git commit a echoue" >&2; exit 1; }
fi

echo "[gitcommit] OK — commit cree."
git --no-pager log -1 --format='   %h  A:%an <%ae>  C:%cn <%ce>'
