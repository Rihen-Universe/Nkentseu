#!/usr/bin/env bash
#
# check_nkui_dependants.sh — GARDE ANTI-NOUVEAU-DEPENDANT NKUI (2026-08-17)
# -----------------------------------------------------------------------------
# POURQUOI : NKUI est deprecie (campagne de retrait, ROADMAP Noge §10sexies).
# Un nouveau dependant est pourtant ne APRES la depreciation, dans une PR
# passee sans que personne ne le voie (NkMatInventaireTest.jenga:50, fantome
# des la naissance). Ce controle rend ce scenario IMPOSSIBLE a rater.
#
# CE QU'IL FAIT : mesure les fichiers `.jenga` citant la chaine "NKUI"
# (guillemets droits) dans l'arbre courant, hors NKUI lui-meme, et les
# compare a la liste decroissante `config/nkui_dependants.list`.
#   - citation HORS liste  -> ECHEC (exit 1) : nouveau dependant interdit ;
#   - entree de liste sans citation ici -> INFORMATION (attrition ou branche
#     en avance), jamais un echec ;
#   - controle positif d'instrument : la meme commande doit trouver
#     Kernel/Runtime/NKUI/NKUI.jenga ; sinon ECHEC (exit 2) — un grep qui ne
#     sait rien trouver ne prouve rien (regle du corpus).
#
# QUAND IL TOURNE :
#   - a la main :          ./check_nkui_dependants.sh
#   - automatiquement :    via ./gitcommit.sh, pour tout commit qui touche un
#                          fichier .jenga (c'est le seul chemin par lequel un
#                          nouveau dependant peut naitre).
# Il mesure L'ARBRE DE TRAVAIL, pas l'index — perimetre declare ci-dessous.
# Aucune CI n'est engagee (non tranchee par Rodolf).
# -----------------------------------------------------------------------------
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { echo "[garde-nkui] cd $ROOT impossible" >&2; exit 2; }

LIST="config/nkui_dependants.list"
SELF="Kernel/Runtime/NKUI/NKUI.jenga"

[ -f "$LIST" ] || { echo "[garde-nkui] ECHEC : liste absente ($LIST)" >&2; exit 2; }

# --- Mesure -------------------------------------------------------------------
# Perimetre : tout l'arbre de travail depuis la racine du depot, fichiers
# *.jenga, hors .git et repertoires de sortie de build. Les sous-modules non
# initialises sont des dossiers vides : ils ne sont PAS couverts (etat connu
# d'un worktree neuf — voir carnet Noge, piege n.1).
mapfile -t FOUND < <(grep -rl '"NKUI"' --include='*.jenga' \
  --exclude-dir=.git --exclude-dir=Build --exclude-dir=build . 2>/dev/null \
  | sed 's|^\./||' | sort)

# --- Controle positif : l'instrument sait-il trouver ? ------------------------
SELF_SEEN=0
for f in "${FOUND[@]:-}"; do [ "$f" = "$SELF" ] && SELF_SEEN=1; done
if [ "$SELF_SEEN" -ne 1 ]; then
  echo "[garde-nkui] ECHEC D'INSTRUMENT : $SELF n'est pas trouve par le grep." >&2
  echo "[garde-nkui] Soit NKUI a ete supprime (mettre ce controle a jour)," >&2
  echo "[garde-nkui] soit la commande ne sait plus trouver — ne rien conclure." >&2
  exit 2
fi

# --- Lecture de la liste (lignes non vides, hors commentaires) ----------------
mapfile -t ALLOWED < <(grep -v '^[[:space:]]*#' "$LIST" | grep -v '^[[:space:]]*$' \
  | sed 's|[[:space:]]*$||' | sort)

in_list() { local x="$1" a; for a in "${ALLOWED[@]}"; do [ "$a" = "$x" ] && return 0; done; return 1; }
is_found() { local x="$1" f; for f in "${FOUND[@]}"; do [ "$f" = "$x" ] && return 0; done; return 1; }

# --- Comparaison --------------------------------------------------------------
NEW=()
COUNT=0
for f in "${FOUND[@]}"; do
  [ "$f" = "$SELF" ] && continue
  COUNT=$((COUNT + 1))
  in_list "$f" || NEW+=("$f")
done

STALE=()
for a in "${ALLOWED[@]}"; do
  is_found "$a" || STALE+=("$a")
done

# --- Verdict ------------------------------------------------------------------
echo "[garde-nkui] dependants citant \"NKUI\" dans CET arbre : $COUNT (liste : ${#ALLOWED[@]})"

if [ "${#STALE[@]}" -gt 0 ]; then
  echo "[garde-nkui] info — ${#STALE[@]} entree(s) de la liste sans citation ici"
  echo "[garde-nkui]        (attrition, ou coupe pas encore fusionnee) :"
  for a in "${STALE[@]}"; do echo "[garde-nkui]        - $a"; done
  echo "[garde-nkui]        A retirer de la liste quand TOUTES les branches ont fusionne."
fi

if [ "${#NEW[@]}" -gt 0 ]; then
  echo "" >&2
  echo "[garde-nkui] ECHEC : ${#NEW[@]} NOUVEAU(X) dependant(s) NKUI hors liste :" >&2
  for f in "${NEW[@]}"; do echo "[garde-nkui]   - $f" >&2; done
  echo "[garde-nkui] NKUI est deprecie (remplace par NKGui/NKCanvas) : aucun" >&2
  echo "[garde-nkui] nouveau .jenga ne doit citer \"NKUI\". Voir ROADMAP Noge" >&2
  echo "[garde-nkui] §10sexies et config/nkui_dependants.list. Si c'est une" >&2
  echo "[garde-nkui] exception voulue, elle se decide chez Rodolf." >&2
  exit 1
fi

echo "[garde-nkui] OK : aucun nouveau dependant."
exit 0
