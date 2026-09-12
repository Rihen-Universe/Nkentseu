#!/usr/bin/env bash
#
# contre_epreuve_chemins.sh — LA GARDE DES CHEMINS SAIT-ELLE ROUGIR ? (2026-08-24)
# =============================================================================
# POURQUOI ELLE EXISTE
#
#   « Un controle qui n a jamais rougi n est pas un controle, c est une
#     intention. »
#
#   verif_chemins.sh est ne aujourd hui. Il a rougi une premiere fois tout seul,
#   sur deux fichiers reels — mais un rouge de naissance ne dit pas qu il sait
#   rougir SUR DEMANDE, ni qu il refuse un classement complaisant, ni qu il
#   distingue un depot propre d un instrument casse. Ce script le lui demande.
#
# LE FICHIER FANTOME, ET SON NOM EST CHOISI EXPRES
#   NkCheminFantome.sh ne contient ni tmp, ni temp, ni cache dans son NOM. Aucune
#   heuristique de nom ne le verrait. L epreuve ne peut passer que si la
#   detection porte sur le CONTENU. Meme choix que NkBancFantome et NkCapFantome.
#   Il est cree puis « git add -N » : un script NON SUIVI n est deliberement pas
#   examine par la garde, donc l injecter sans l enregistrer ne prouverait rien.
#
# LES SEPT EPREUVES
#   N  un chemin fixe injecte, non classe      -> ECHEC code 6, fichier NOMME
#   O  classe « tolere » SANS note             -> ECHEC code 6 : une tolerance
#                                                 sans raison ecrite est le mot
#                                                 qu on met pour faire taire
#   P  classe « tolere » AVEC note             -> VERT code 0 : on revient au
#                                                 vert en CLASSANT, pas en
#                                                 desactivant l outil
#   Q  classe « interdit »                     -> ECHEC code 6 : reconnaitre un
#                                                 defaut ne le fait pas taire
#   R  la directive CANARI retiree             -> code 2, ECHEC D INSTRUMENT,
#                                                 JAMAIS un vert
#   S  tout est retire                         -> VERT code 0, arbre IDENTIQUE
#
# CONTROLE NEGATIF — le montage doit etre sain AVANT de casser quoi que ce soit
#   Si verif_chemins.sh est DEJA rouge, ou si la liste porte deja des
#   modifications non commitees, ce script REFUSE DE DEMARRER. Injecter dans un
#   depot deja rouge rendrait N vraie pour rien.
#
# ORDRE CRITIQUE — LE CONTROLE NEGATIF D ABORD, LE FILET APRES.
#   Un trap restaurer EXIT arme AVANT le controle « l arbre est-il propre ? »
#   transforme le refus de ce controle en git checkout qui DETRUIT la
#   modification non commitee qu il refusait justement d ecraser. C est arrive
#   dans ce depot le 22/08, et ca a coute 49 lignes.
#
# CODES DE SORTIE
#   0  les sept epreuves passent
#   1  au moins une exigence non tenue
#   2  refus de demarrer, ou restauration douteuse
# =============================================================================
set -uo pipefail

dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[ce-chem] cd $ROOT impossible"; exit 2; }

LISTE="config/chemins_partages.list"
FANTOME="Applications/NkCheminFantome.sh"
ECHECS=0

attendu() {
  if [ "$2" = "ok" ]; then dire "   [vu] $1"; else dire "   [RATE] $1"; ECHECS=$((ECHECS + 1)); fi
}

# --- CONTROLE NEGATIF, ET IL PASSE AVANT LE FILET ---------------------------
[ -f "$LISTE" ]           || { dire2 "[ce-chem] REFUS : $LISTE absent."; exit 2; }
[ -f verif_chemins.sh ]   || { dire2 "[ce-chem] REFUS : verif_chemins.sh absent."; exit 2; }
if [ -e "$FANTOME" ]; then
  dire2 "[ce-chem] REFUS : $FANTOME existe deja. Je ne l ecrase pas : il porte"
  dire2 "[ce-chem]   peut-etre le travail de quelqu un."
  exit 2
fi
if ! git diff --quiet -- "$LISTE" 2>/dev/null; then
  dire2 "[ce-chem] REFUS DE DEMARRER : $LISTE porte des modifications non commitees."
  dire2 "[ce-chem]   Les restaurer serait DETRUIRE un travail que ce controle-ci"
  dire2 "[ce-chem]   refusait justement d ecraser. Commite d abord."
  exit 2
fi
./verif_chemins.sh > /dev/null 2>&1
if [ "$?" -ne 0 ]; then
  dire2 "[ce-chem] REFUS DE DEMARRER : verif_chemins.sh est DEJA rouge."
  dire2 "[ce-chem]   Injecter dans un depot deja rouge rendrait l epreuve N vraie"
  dire2 "[ce-chem]   pour rien."
  exit 2
fi
dire "  ok   verif_chemins.sh est au vert, la liste est propre, aucun fantome."

# --- LE FILET, MAINTENANT SEULEMENT -----------------------------------------
nettoyer() {
  rm -f "$FANTOME" 2>/dev/null
  git rm --cached --quiet "$FANTOME" 2>/dev/null
  git checkout -- "$LISTE" 2>/dev/null
}
trap nettoyer EXIT

EMPREINTE_LISTE_AVANT="$(git hash-object "$LISTE")"

# Le fantome : un chemin fixe, dans du code, pas dans un commentaire.
mkdir -p "$(dirname "$FANTOME")"
{
  printf '%s\n' '#!/usr/bin/env bash'
  printf '%s\n' '# fantome de contre-epreuve : retire par contre_epreuve_chemins.sh'
  printf '%s\n' 'sortie=/tmp/nkchemin_fantome.log'
  printf '%s\n' 'printf "%s\n" "coucou" > "$sortie"'
} > "$FANTOME"
git add -N "$FANTOME" 2>/dev/null

CHEMIN='/tmp/nkchemin_fantome.log'

classer() {  # $1 = classe, $2 = note
  git checkout -- "$LISTE" 2>/dev/null
  printf '%s | %s | %s | %s\n' "$FANTOME" "$CHEMIN" "$1" "$2" >> "$LISTE"
}

sep() { dire ""; dire "======================================================================"; dire " $*"; dire "======================================================================"; }

# ---------------------------------------------------------------- N
sep "EPREUVE N — un chemin fixe injecte, NON CLASSE"
S="$(./verif_chemins.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 6)"
[ "$C" -eq 6 ] && attendu "code 6 : la garde rougit sur une occurrence non classee" ok \
               || attendu "code 6 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "NON CLASSE" \
  && attendu "l outil dit NON CLASSE" ok || attendu "NON CLASSE non dit" ko
printf '%s\n' "$S" | grep -qF "$FANTOME" \
  && attendu "le fichier est NOMME, pas seulement compte" ok || attendu "fichier non nomme" ko
printf '%s\n' "$S" | grep -qF "$CHEMIN" \
  && attendu "le CHEMIN exact est cite" ok || attendu "chemin non cite" ko

# ---------------------------------------------------------------- O
sep "EPREUVE O — classe « tolere » SANS note"
classer "tolere" ""
S="$(./verif_chemins.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 6)"
[ "$C" -eq 6 ] && attendu "code 6 : une tolerance sans raison ecrite ne fait pas taire l outil" ok \
               || attendu "code 6 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "TOLERE SANS NOTE" \
  && attendu "l outil dit TOLERE SANS NOTE" ok || attendu "TOLERE SANS NOTE non dit" ko

# ---------------------------------------------------------------- P
sep "EPREUVE P — classe « tolere » AVEC note -> retour au VERT"
classer "tolere" "fantome de contre-epreuve ; aucun agent ne lance ce script"
S="$(./verif_chemins.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 0)"
[ "$C" -eq 0 ] && attendu "code 0 : on revient au vert en CLASSANT, pas en desactivant" ok \
               || attendu "code 0 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "toleree(s) avec raison ecrite" \
  && attendu "le compte des tolerees est dit, la tolerance ne disparait pas du rapport" ok \
  || attendu "les tolerees ne sont pas comptees" ko

# ---------------------------------------------------------------- Q
sep "EPREUVE Q — classe « interdit » -> reconnaitre un defaut ne le fait pas taire"
classer "interdit" "deux agents peuvent se croiser dessus"
S="$(./verif_chemins.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 6)"
[ "$C" -eq 6 ] && attendu "code 6 : « interdit » reste ROUGE, ce n est pas une case pour classer sans reparer" ok \
               || attendu "code 6 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "[INTERDIT]" \
  && attendu "l outil dit INTERDIT" ok || attendu "INTERDIT non dit" ko

# ---------------------------------------------------------------- R
sep "EPREUVE R — la directive CANARI retiree -> ECHEC D INSTRUMENT, jamais un vert"
git checkout -- "$LISTE" 2>/dev/null
grep -v '^# CANARI' "$LISTE" > "$LISTE.tmp" && mv "$LISTE.tmp" "$LISTE"
S="$(./verif_chemins.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 2)"
[ "$C" -eq 2 ] && attendu "code 2 : ECHEC D INSTRUMENT, et surtout PAS un vert" ok \
               || attendu "code 2 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "un detecteur casse et un depot sain rendent la meme" \
  && attendu "la raison est dite : casse et sain se ressemblent" ok \
  || attendu "la raison n est pas dite" ko
git checkout -- "$LISTE" 2>/dev/null

# ---------------------------------------------------------------- T
sep "EPREUVE T — le PLANCHER DE LECTURE : moins de 20 scripts examines"
# On ne casse pas le depot pour eprouver ce controle : on pousse la DONNEE
# (les arbres exclus) jusqu a l absurde. C est exactement ce que ferait un
# .list mal edite, et c est le seul cas ou l outil lit presque rien tout en
# ayant l air de fonctionner.
git checkout -- "$LISTE" 2>/dev/null
for pfx in s K A c R v g D p e b a T E .; do
  printf '# EXCLU-ARBRE = %s
' "$pfx" >> "$LISTE"
done
S="$(./verif_chemins.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 2)"
[ "$C" -eq 2 ] && attendu "code 2 : un outil qui ne lit presque plus rien ne conclut pas" ok                || attendu "code 2 attendu, obtenu $C" ko
printf '%s
' "$S" | grep -qF "script(s) examine(s)"   && attendu "il DIT combien de scripts il a examines" ok || attendu "le compte n est pas dit" ko
printf '%s
' "$S" | grep -qF "rendent la meme sortie verte"   && attendu "la raison est dite : rien trouve et rien lu se ressemblent" ok   || attendu "la raison n est pas dite" ko
git checkout -- "$LISTE" 2>/dev/null

# ---------------------------------------------------------------- S
sep "EPREUVE S — retrait complet -> VERT et arbre identique"
nettoyer
S="$(./verif_chemins.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 0)"
[ "$C" -eq 0 ] && attendu "code 0 : l outil revient au vert de lui-meme" ok \
               || attendu "code 0 attendu apres retrait, obtenu $C" ko
[ ! -e "$FANTOME" ] && attendu "le fantome est retire du disque" ok \
                    || attendu "le fantome traine encore" ko
if git diff --quiet -- "$LISTE" 2>/dev/null; then
  attendu "arbre identique : git ne voit plus rien sur la liste" ok
else
  attendu "arbre identique sur la liste" ko
fi
EMPREINTE_LISTE_APRES="$(git hash-object "$LISTE")"
[ "$EMPREINTE_LISTE_AVANT" = "$EMPREINTE_LISTE_APRES" ] \
  && attendu "la liste est revenue octet pour octet" ok \
  || attendu "la liste a change ($EMPREINTE_LISTE_AVANT -> $EMPREINTE_LISTE_APRES)" ko

dire ""
dire "======================================================================"
if [ "$ECHECS" -eq 0 ]; then
  dire " CONTRE-EPREUVE REUSSIE — la garde des chemins sait rougir."
  dire "   N : chemin fixe non classe    -> ECHEC 6, fichier et chemin nommes"
  dire "   O : tolere SANS note          -> ECHEC 6"
  dire "   P : tolere AVEC note          -> VERT 0, et la tolerance reste comptee"
  dire "   Q : interdit                  -> ECHEC 6, classer n est pas reparer"
  dire "   R : canari retire             -> ECHEC D INSTRUMENT 2, jamais un vert"
  dire "   T : plancher de lecture       -> ECHEC D INSTRUMENT 2, et le compte est dit"
  dire "   S : apres retrait             -> vert, liste octet pour octet"
else
  dire " CONTRE-EPREUVE RATEE — $ECHECS exigence(s) non tenue(s)."
fi
dire "======================================================================"
exit $([ "$ECHECS" -eq 0 ] && echo 0 || echo 1)
