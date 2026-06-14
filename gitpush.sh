#!/usr/bin/env bash
#
# gitpush.sh — add + commit + push du depot Nkentseu (RihenUniverse/Nkentseu).
# -----------------------------------------------------------------------------
# Commit PROPRE (aucune mention Claude) puis push de la branche indiquee. Option
# --release pour creer et pousser un tag de version.
#
# NOTE : Nkentseu n'a pas (encore) de workflow GitHub Actions. Le push ne
# declenche donc aucune automatisation (wiki/release) — le tag sert a marquer
# une version. Quand une CI sera ajoutee, ce script restera valable tel quel.
#
# USAGE
#   ./gitpush.sh <branche> "<message>"
#       Commit + push de la branche.
#
#   ./gitpush.sh <branche> "<message>" --release vX.Y.Z
#       Idem, PUIS cree le tag vX.Y.Z et le pousse.
#       (Nkentseu n'ayant pas de fichier de version unique, la version doit etre
#        fournie explicitement.)
#
#   Options :
#     -d, --dry-run   Affiche les commandes sans rien executer (test a blanc).
#     -h, --help      Affiche cette aide.
#
# EXEMPLES
#   ./gitpush.sh dev "feat(canvas): scissor 5 backends"
#   ./gitpush.sh main "release 2.1.0" --release v2.1.0
#   ./gitpush.sh dev "wip" --dry-run
# -----------------------------------------------------------------------------
set -uo pipefail

# ── Couleurs (desactivees hors terminal) ─────────────────────────────────────
if [ -t 1 ]; then
  C_OK=$'\033[1;32m'; C_ERR=$'\033[1;31m'; C_INFO=$'\033[1;36m'
  C_WARN=$'\033[1;33m'; C_DIM=$'\033[2m'; C_RST=$'\033[0m'
else
  C_OK=""; C_ERR=""; C_INFO=""; C_WARN=""; C_DIM=""; C_RST=""
fi
info() { echo "${C_INFO}==>${C_RST} $*"; }
ok()   { echo "${C_OK}[OK]${C_RST} $*"; }
warn() { echo "${C_WARN}[!]${C_RST} $*"; }
die()  { echo "${C_ERR}[ERREUR]${C_RST} $*" >&2; exit 1; }
usage() { sed -n '2,38p' "$0" | sed 's/^# \{0,1\}//'; }

# ── Parsing des arguments ────────────────────────────────────────────────────
DRYRUN=0; RELEASE=0; TAGVER=""
POS=()
while [ $# -gt 0 ]; do
  case "$1" in
    -d|--dry-run) DRYRUN=1 ;;
    -h|--help)    usage; exit 0 ;;
    -r|--release)
      RELEASE=1
      if [ $# -gt 1 ] && [ "${2:0:1}" != "-" ]; then TAGVER="$2"; shift; fi
      ;;
    *) POS+=("$1") ;;
  esac
  shift
done
set -- "${POS[@]:-}"

[ "${1:-}" != "" ] && [ "${2:-}" != "" ] || { usage; echo; die "Il faut : <branche> et un <message>."; }
BRANCH="$1"
MSG="$2"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || die "impossible d'aller dans $ROOT"

run() {
  echo "   ${C_DIM}\$ $*${C_RST}"
  if [ "$DRYRUN" -eq 0 ]; then "$@"; else return 0; fi
}

echo "${C_INFO}════════════════════════════════════════════════════════════${C_RST}"
echo " gitpush (Nkentseu)  |  branche : ${C_OK}$BRANCH${C_RST}$([ "$DRYRUN" -eq 1 ] && echo "   ${C_WARN}(DRY-RUN)${C_RST}")"
echo "   message : \"$MSG\""
echo "   release : $([ "$RELEASE" -eq 1 ] && echo "OUI ($TAGVER)" || echo "non")"
echo "${C_INFO}════════════════════════════════════════════════════════════${C_RST}"

# ── 1) Se placer sur la bonne branche ────────────────────────────────────────
CUR="$(git symbolic-ref --short -q HEAD || echo "")"
if [ "$CUR" != "$BRANCH" ]; then
  echo "   branche courante : ${CUR:-<HEAD detache>} -> bascule sur '$BRANCH'"
  run git checkout "$BRANCH" || die "checkout '$BRANCH' impossible (conflits ?)."
fi

# ── 2) Indexer tout le depot ─────────────────────────────────────────────────
run git add -A || die "git add a echoue."

# ── 3) Committer s'il y a quelque chose d'indexe ─────────────────────────────
if git diff --cached --quiet 2>/dev/null; then
  echo "   ${C_DIM}rien de nouveau a committer${C_RST}"
else
  run git commit -m "$MSG" || die "git commit a echoue."
  ok "commit cree : \"$MSG\""
fi

# ── 4) Pousser la branche ────────────────────────────────────────────────────
run git push origin "$BRANCH" || die "push de la branche '$BRANCH' echoue."
ok "branche '$BRANCH' poussee vers origin."

# ── 5) Optionnel : tag de version ────────────────────────────────────────────
if [ "$RELEASE" -eq 1 ]; then
  [ -n "$TAGVER" ] || die "--release requiert une version explicite (ex: --release v2.1.0)."
  TAG="v${TAGVER#v}"   # garantit un seul 'v' en prefixe
  info "Release : tag $TAG"
  if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null 2>&1; then
    warn "le tag $TAG existe deja en local — non recree."
  else
    run git tag -a "$TAG" -m "Release $TAG" || die "creation du tag $TAG echouee."
    ok "tag $TAG cree."
  fi
  run git push origin "$TAG" || die "push du tag $TAG echoue (deja pousse ? remote en avance ?)."
  ok "tag $TAG pousse."
fi

echo ""
ok "Termine."
[ "$DRYRUN" -eq 1 ] && warn "DRY-RUN : rien n'a ete reellement modifie."
exit 0
