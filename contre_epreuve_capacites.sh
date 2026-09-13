#!/usr/bin/env bash
#
# contre_epreuve_capacites.sh — LA CONTRE-EPREUVE DU DETECTEUR (2026-08-22)
# =============================================================================
# POURQUOI ELLE EXISTE
#
#   « Un detecteur qui ne trouve rien et un detecteur qui ne lit rien
#     produisent la MEME sortie verte. »
#
#   verif_capacites.sh rend 0. Cette phrase ne vaut rien tant que personne n'a
#   montre qu'il sait rendre autre chose. Cette contre-epreuve INTRODUIT une
#   capacite creuse et EXIGE le rouge.
#
# LE NOM EST CHOISI EXPRES : `NkCapFantome`
#   Il ne contient ni TODO, ni stub, ni unimplemented, ni notimpl, ni WIP. Aucune
#   heuristique de nom ne le verrait. L'epreuve ne peut passer que si la
#   detection est STRUCTURELLE — un corps creux apparie a un drapeau qui promet
#   `true` a du code. C'est le meme choix que `NkBancFantome` dans la
#   contre-epreuve des bancs, et pour la meme raison.
#
# LES QUATRE EPREUVES
#   E  la capacite creuse est introduite         -> ECHEC, code 4, DEUX symboles
#                                                   nommes (D1 et D2)
#   F  elle est classee « vision-assumee »       -> ECHEC, code 4, classement
#                                                   REFUSE (regle R3-a)
#   G  elle est classee « dette-datee » + date   -> VERT, code 0
#   I  une dette « A-DATER » de plus             -> ECHEC, code 4, CLIQUET ROMPU
#      et le fichier sans directive de plafond   -> code 2, jamais un vert
#   H  tout est retire                           -> VERT, code 0, arbre IDENTIQUE
#
#   E prouve que l'outil detecte. F prouve qu'il refuse le classement interdit —
#   sans F, la regle (a) ne serait qu'une phrase dans un commentaire. G prouve
#   qu'on peut revenir au vert autrement qu'en desactivant l'outil. H prouve que
#   la contre-epreuve n'a rien laisse derriere elle.
#
# CONTROLE NEGATIF — le montage doit etre sain AVANT de casser quoi que ce soit
#   Si l'outil est DEJA rouge, ou si les fichiers a modifier ne sont pas propres,
#   cette contre-epreuve REFUSE DE DEMARRER (code 2). Introduire une capacite
#   creuse dans un depot deja rouge rendrait E vraie pour rien, et on lirait
#   quatre [vu] sur un montage faux. C'est arrive le 2026-08-22 dans la
#   contre-epreuve des bancs ; on ne le refait pas.
#
# CODES DE SORTIE
#   0  les quatre epreuves passent
#   1  au moins une epreuve echoue (le detecteur ne fait pas ce qu'il promet)
#   2  refus de demarrer, ou restauration douteuse
# =============================================================================
set -uo pipefail

dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[ce-cap] cd $ROOT impossible"; exit 2; }

ENTETE="Kernel/Runtime/NKRHI/src/NKRHI/Core/NkIDevice.h"
SOURCE="Kernel/Runtime/NKRHI/src/NKRHI/Opengl/NkOpenglDevice.cpp"
LISTE="config/capacites.list"
SYM_D1="NkIDevice::GetNkCapFantomeLevel"
SYM_D2="NkDeviceCaps::nkCapFantome"

NB_VU=0
NB_RATE=0

vu()   { NB_VU=$((NB_VU + 1));   dire "  [vu]   $*"; }
rate() { NB_RATE=$((NB_RATE + 1)); dire2 "  [RATE] $*"; }

# =============================================================================
# RESTAURATION — et la lecon de `cp -p`
# =============================================================================
# La contre-epreuve des bancs a menti une fois parce qu'elle restaurait par
# `cp -p`, qui PRESERVE LA DATE : le cache de Jenga voyait une source plus
# ancienne que les objets, ne recompilait rien, et l'ANCIEN binaire tournait.
# Ici rien n'est compile — mais on ne restaure quand meme pas « a la main » :
# on demande a git de remettre l'etat exact, et on VERIFIE qu'il l'a fait.
restaurer() {
  # `git checkout --` sur les TROIS fichiers, y compris la liste. C'est plus sur
  # que reposer la sauvegarde : si la sauvegarde a deja ete consommee, ou si la
  # contre-epreuve est morte entre deux etapes, git connait quand meme l'etat de
  # depart. Et c'est SANS RISQUE precisement grace au controle negatif : on a
  # refuse de demarrer sur un arbre sale, donc il n'y a aucun travail non commite
  # a jeter ici.
  git checkout -- "$ENTETE" "$SOURCE" "$LISTE" 2>/dev/null
  rm -f "$LISTE.sauvegarde"
  return 0
}
# ⚠️ EXIT NE SUFFIT PAS, ET CA A COUTE UNE FUITE REELLE (2026-08-22).
# Cette contre-epreuve a ete tuee par un delai d'execution. Bash n'execute PAS le
# piege EXIT sur un signal fatal non trappe : elle est morte en laissant
# `config/capacites.list` ampute de sa directive de plafond et porteur de deux
# lignes fantomes. Un outil qui modifie des fichiers versionnes doit rendre l'arbre
# sur TOUTES ses sorties, pas seulement sur celles qu'il a prevues.
# (Ce qui l'a rattrape : son propre controle negatif, qui a refuse de repartir sur
#  un arbre sale. Le filet a tenu — mais un filet n'excuse pas la chute.)
trap 'restaurer' EXIT INT TERM HUP

# =============================================================================
# CONTROLE NEGATIF
# =============================================================================
dire "======================================================================"
dire " CONTRE-EPREUVE DU DETECTEUR DE CAPACITES"
dire " arbre : $ROOT"
dire "======================================================================"
dire ""
dire "-- Controle negatif : le montage est-il sain AVANT ? -----------------"

sale=$(git status --porcelain -- "$ENTETE" "$SOURCE" "$LISTE")
if [ -n "$sale" ]; then
  dire2 "[ce-cap] REFUS DE DEMARRER : ces fichiers ne sont pas propres."
  dire2 "$sale"
  dire2 "[ce-cap]   Cette contre-epreuve les modifie puis les restaure par un"
  dire2 "[ce-cap]   git checkout. Partir d'un arbre sale reviendrait a jeter"
  dire2 "[ce-cap]   ton travail en cours a la restauration."
  exit 2
fi

./verif_capacites.sh > /dev/null 2>&1
rc0=$?
if [ "$rc0" -ne 0 ]; then
  dire2 "[ce-cap] REFUS DE DEMARRER : verif_capacites.sh rend deja $rc0 sur cet arbre."
  dire2 "[ce-cap]   Introduire une capacite creuse dans un depot deja rouge rendrait"
  dire2 "[ce-cap]   l'epreuve E vraie POUR RIEN, et on lirait quatre [vu] sur un"
  dire2 "[ce-cap]   montage faux. Classe d'abord ce qui manque, puis relance."
  exit 2
fi
dire "  ok   arbre propre, et le detecteur rend 0 avant qu'on touche a quoi que ce soit"

# =============================================================================
# INTRODUCTION DE LA CAPACITE CREUSE
# =============================================================================
introduire() {
  # (1) le drapeau, dans NkDeviceCaps
  python - "$ENTETE" <<'PY'
import io, sys
p = sys.argv[1]
s = io.open(p, encoding='utf-8', newline='').read()
a = "\t\t\tbool timestampQueries = false;"
assert a in s, "ancre du drapeau introuvable"
s = s.replace(a, a + "\n\t\t\tbool nkCapFantome = false;", 1)
b = "\t\t\tvirtual float32 GetTimestampPeriodNs() {"
assert b in s, "ancre de la virtuelle introuvable"
s = s.replace(b, "\t\t\tvirtual bool GetNkCapFantomeLevel() {\n\t\t\t\treturn false;\n\t\t\t}\n\n" + b, 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
PY
  [ "$?" -ne 0 ] && return 1
  # (2) la promesse : un drapeau mis a true PAR DU CODE
  python - "$SOURCE" <<'PY'
import io, sys
p = sys.argv[1]
s = io.open(p, encoding='utf-8', newline='').read()
a = "mCaps.timestampQueries = true;"
assert a in s, "ancre de la promesse introuvable"
s = s.replace(a, a + "\n\t\tmCaps.nkCapFantome = true;", 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
PY
  return $?
}

dire ""
dire "-- Introduction de NkCapFantome --------------------------------------"
if ! introduire; then
  dire2 "[ce-cap] REFUS : les ancres d'insertion n'existent plus dans les sources."
  dire2 "[ce-cap]   Ce n'est pas un echec du detecteur : c'est la contre-epreuve qui"
  dire2 "[ce-cap]   ne sait plus ou piquer. Mets les ancres a jour."
  exit 2
fi
dire "  drapeau   NkDeviceCaps::nkCapFantome = false"
dire "  virtuelle NkIDevice::GetNkCapFantomeLevel() { return false; }"
dire "  promesse  mCaps.nkCapFantome = true;   ($SOURCE)"
dire "  ⚠️ aucun des trois ne porte TODO, stub, unimplemented ni WIP."

# =============================================================================
# EPREUVE E — la capacite creuse doit rendre l'outil ROUGE
# =============================================================================
dire ""
dire "-- E : capacite creuse non classee -> ECHEC de classement ------------"
sortie=$(./verif_capacites.sh 2>&1)
rcE=$?
[ "$rcE" -eq 4 ] && vu "code 4 (echec de classement)" || rate "code $rcE, attendu 4"
printf '%s' "$sortie" | grep -q "$SYM_D1" && vu "D1 nomme $SYM_D1" || rate "$SYM_D1 non nomme"
printf '%s' "$sortie" | grep -q "$SYM_D2" && vu "D2 nomme $SYM_D2" || rate "$SYM_D2 non nomme"
printf '%s' "$sortie" | grep -q 'vision-assumee.*INTERDIT' \
  && vu "l'interdit de « vision-assumee » est annonce des le non-classe" \
  || rate "l'interdit de « vision-assumee » n'est pas annonce"

# =============================================================================
# EPREUVE F — le classement INTERDIT doit etre refuse
# =============================================================================
dire ""
dire "-- F : classee « vision-assumee » -> classement REFUSE (regle R3-a) --"
cp "$LISTE" "$LISTE.sauvegarde"
{
  printf '%s | %s | %s | %s | %s\n' "$SYM_D1" "D1" "vision-assumee" "" "contre-epreuve F"
  printf '%s | %s | %s | %s | %s\n' "$SYM_D2" "D2" "vision-assumee" "" "contre-epreuve F"
} >> "$LISTE"

sortie=$(./verif_capacites.sh 2>&1)
rcF=$?
[ "$rcF" -eq 4 ] && vu "code 4 : le classement est refuse" || rate "code $rcF, attendu 4"
printf '%s' "$sortie" | grep -q 'CLASSEMENT INTERDIT' \
  && vu "l'outil dit CLASSEMENT INTERDIT, pas seulement « non classe »" \
  || rate "l'outil ne distingue pas un classement interdit d'un manque de ligne"
printf '%s' "$sortie" | grep -q 'DEUX SORTIES SEULEMENT' \
  && vu "les deux seules sorties sont rappelees (drapeau a false, ou implementer)" \
  || rate "les deux sorties ne sont pas rappelees"

# =============================================================================
# EPREUVE G — le classement LICITE doit rendre le vert
# =============================================================================
dire ""
dire "-- G : classee « dette-datee » + date -> retour au VERT ---------------"
cp "$LISTE.sauvegarde" "$LISTE"
{
  printf '%s | %s | %s | %s | %s\n' "$SYM_D1" "D1" "dette-datee" "2026-09-30" "contre-epreuve G"
  printf '%s | %s | %s | %s | %s\n' "$SYM_D2" "D2" "dette-datee" "2026-09-30" "contre-epreuve G"
} >> "$LISTE"

./verif_capacites.sh > /dev/null 2>&1
rcG=$?
[ "$rcG" -eq 0 ] && vu "code 0 : on revient au vert en CLASSANT, pas en desactivant" \
                 || rate "code $rcG, attendu 0"

# -- et la meme sans date : une dette sans echeance n'est pas datee ----------
cp "$LISTE.sauvegarde" "$LISTE"
{
  printf '%s | %s | %s | %s | %s\n' "$SYM_D1" "D1" "dette-datee" "" "contre-epreuve G bis"
  printf '%s | %s | %s | %s | %s\n' "$SYM_D2" "D2" "dette-datee" "" "contre-epreuve G bis"
} >> "$LISTE"
sortie=$(./verif_capacites.sh 2>&1)
rcG2=$?
[ "$rcG2" -eq 4 ] && vu "« dette-datee » a date VIDE reste rouge (un oubli n'est pas une dette datee)" \
                  || rate "code $rcG2 sur une dette sans date, attendu 4"

# =============================================================================
# EPREUVE I — le CLIQUET de A-DATER : il descend, il ne monte pas
# =============================================================================
# G a montre qu'une dette DATEE rend le vert. Reste a montrer qu'une dette
# NON DATEE de plus fait monter le compte et casse le cliquet — sinon
# « A-DATER » redeviendrait une porte ouverte, et le stock se renouvellerait
# exactement pendant qu'on le vide.
dire ""
dire "-- I : une dette « A-DATER » de plus -> CLIQUET ROMPU -----------------"
cp "$LISTE.sauvegarde" "$LISTE"
{
  printf '%s | %s | %s | %s | %s
' "$SYM_D1" "D1" "dette-datee" "A-DATER" "contre-epreuve I"
  printf '%s | %s | %s | %s | %s
' "$SYM_D2" "D2" "dette-datee" "A-DATER" "contre-epreuve I"
} >> "$LISTE"

sortie=$(./verif_capacites.sh 2>&1)
rcI=$?
[ "$rcI" -eq 4 ] && vu "code 4 : le cliquet refuse que le stock d'echeances non fixees MONTE"                  || rate "code $rcI sur un cliquet depasse, attendu 4"
printf '%s' "$sortie" | grep -q 'CLIQUET ROMPU'   && vu "l'outil dit CLIQUET ROMPU, et nomme le plafond"   || rate "l'outil ne distingue pas un cliquet rompu d'une dette ordinaire"
printf '%s' "$sortie" | grep -q 'se date a la naissance'   && vu "la regle est rappelee : toute capacite nouvelle se date a la naissance"   || rate "la regle du cliquet n'est pas rappelee"

# -- et le cas symetrique : sans directive de plafond, PAS de silence vert ----
cp "$LISTE.sauvegarde" "$LISTE"
grep -v 'PLAFOND-A-DATER' "$LISTE" > "$LISTE.sansplafond" && mv -f "$LISTE.sansplafond" "$LISTE"
sortie=$(./verif_capacites.sh 2>&1)
rcI2=$?
[ "$rcI2" -eq 2 ] && vu "sans directive de plafond : ECHEC D'INSTRUMENT (code 2), pas un vert"                   || rate "code $rcI2 sans directive de plafond, attendu 2"
printf '%s' "$sortie" | grep -q 'meme sortie verte'   && vu "la raison est dite : un cliquet absent et un cliquet satisfait se ressemblent"   || rate "la raison de l'echec d'instrument n'est pas dite"
cp "$LISTE.sauvegarde" "$LISTE"

# =============================================================================
# EPREUVE H — tout est retire, l'arbre doit etre IDENTIQUE
# =============================================================================
dire ""
dire "-- H : retrait complet -> VERT et arbre identique ---------------------"
restaurer
trap - EXIT INT TERM HUP

reste=$(git status --porcelain -- "$ENTETE" "$SOURCE" "$LISTE")
[ -z "$reste" ] && vu "arbre identique : git ne voit plus rien sur les trois fichiers" \
                || rate "l'arbre porte encore des traces : $reste"
[ -f "$LISTE.sauvegarde" ] && rate "$LISTE.sauvegarde n'a pas ete retire" \
                           || vu "aucune sauvegarde laissee derriere"

./verif_capacites.sh > /dev/null 2>&1
rcH=$?
[ "$rcH" -eq 0 ] && vu "code 0 : le detecteur est revenu au vert de lui-meme" \
                 || rate "code $rcH apres restauration, attendu 0"

# =============================================================================
dire ""
dire "======================================================================"
if [ "$NB_RATE" -eq 0 ]; then
  dire " CONTRE-EPREUVE : $NB_VU controle(s) passe(s), 0 rate"
  dire " Le detecteur sait rendre autre chose que vert, et sait refuser un"
  dire " classement interdit. Sa sortie verte veut donc dire quelque chose."
  dire "======================================================================"
  exit 0
fi
dire " CONTRE-EPREUVE : $NB_RATE rate(s) sur $((NB_VU + NB_RATE))"
dire " ⚠️ Tant que ceci est rouge, une sortie verte de verif_capacites.sh ne"
dire "    prouve RIEN : on ne sait pas s'il ne trouve rien ou s'il ne lit rien."
dire "======================================================================"
exit 1
