#!/usr/bin/env bash
#
# epreuve_disparition.sh — UNE DETTE QUI DISPARAIT DOIT SE VOIR (2026-08-27)
# =============================================================================
# CE QUE CE CONTROLE GARDE, ET POURQUOI IL A FALLU RENONCER A MIEUX
#
#   Le critere de D2 est « ce champ est-il LU ? ». Or JOURNALISER un champ est
#   une lecture. Un seul logger.Warn suffit donc a faire sortir n importe quelle
#   dette de la liste des candidats SANS RIEN HONORER, et le cliquet descendrait.
#
#   La parade evidente -- « une lecture dont le seul consommateur est un journal
#   n est pas un honorement » -- a ete MESUREE le 27/08 et REFUSEE :
#     sur 6433 lignes portant un appel de journalisation, 1133 (17,6 %) portent
#     AUSSI un if/while/return/affectation. La distinction se trompe une fois sur
#     six, et ZERO champ du depot a aujourd hui toutes ses lectures en journal --
#     elle ne pourrait meme pas rougir.
#
#   ⚠️ ON NE PEUT PAS JUGER LA QUALITE D UNE LECTURE. ON PEUT REFUSER QU UNE
#   DISPARITION PASSE INAPERCUE. Ce controle ne classe rien : il exige qu un
#   humain REGARDE le moment ou une capacite quitte la liste. Un faux vert n est
#   dangereux que tant que personne ne voit l instant ou il nait.
#
# ET IL DOIT SE TAIRE SUR LES DISPARITIONS LEGITIMES
#   Supprimer un champ du code fait AUSSI disparaitre son candidat. Un controle
#   qui crie a chaque suppression de code est desactive dans la semaine. La seule
#   distinction mecanisable sans deviner : LE SYMBOLE EST-IL ENCORE DECLARE ?
#     encore declare + plus detecte + « dette-datee »  -> ECHEC, il faut repondre
#     encore declare + deja tranche (implementee, ...)  -> information
#     plus declare du tout                              -> ligne PERIMEE, info
#
# LES QUATRE EPREUVES
#   X  une dette disparait sans reponse   -> ECHEC 4, symbole NOMME, causes dites
#   Y  le symbole n existe plus du tout   -> info PERIMEE, code 0 : IL SE TAIT
#   Z  la ligne porte deja une reponse    -> info, code 0
#   W  retrait                            -> vert, arbre identique (differentiel)
#
# CONTROLE NEGATIF AVANT LE FILET : si le detecteur est deja rouge, ou si la
# liste porte des modifications non commitees, on REFUSE de demarrer. Restaurer
# une liste modifiee detruirait le travail que ce refus protege.
#
# CODES : 0 les quatre passent | 1 exigence non tenue | 2 refus de demarrer
# =============================================================================
set -uo pipefail
dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[ep-dis] cd impossible"; exit 2; }

LISTE="config/capacites.list"
CIBLE="NkIBLConfig::enabled"
ECHECS=0
attendu() { if [ "$2" = "ok" ]; then dire "   [vu] $1"; else dire "   [RATE] $1"; ECHECS=$((ECHECS+1)); fi; }
sep() { dire ""; dire "======================================================================"; dire " $*"; dire "======================================================================"; }

[ -f "$LISTE" ] || { dire2 "[ep-dis] REFUS : $LISTE absent."; exit 2; }
if ! git diff --quiet -- "$LISTE" 2>/dev/null; then
  dire2 "[ep-dis] REFUS DE DEMARRER : $LISTE porte des modifications non commitees."
  dire2 "[ep-dis]   Les restaurer detruirait le travail que ce refus protege. Commite."
  exit 2
fi
./verif_capacites.sh > /dev/null 2>&1
if [ "$?" -ne 0 ]; then
  dire2 "[ep-dis] REFUS DE DEMARRER : verif_capacites.sh est DEJA rouge."
  exit 2
fi
grep -q "^${CIBLE} " "$LISTE" || { dire2 "[ep-dis] REFUS : $CIBLE absent de $LISTE."; exit 2; }
dire "  ok   le detecteur est au vert, la liste est propre, la cible est la."

# ⚠️ TROISIEME REGLAGE DE CE COMPARATEUR, ET LES DEUX PREMIERS ETAIENT TROP LARGES.
# v1 : « git ne voit RIEN » -> rougissait sur un arbre deja sale.
# v2 : differentiel sur TOUT l arbre -> a rougi le 27/08 parce qu un AUTRE
#      travail avait cree un fichier pendant la course de l epreuve.
# v3 : differentiel sur LES SEULS FICHIERS QUE L EPREUVE DECLARE TOUCHER.
# Une epreuve repond de ce qu ELLE change. Dans un depot a plusieurs agents,
# comparer l arbre entier revient a s attribuer le travail des autres -- en
# rouge.
AVANT="$(git status --porcelain -- "$LISTE" 2>/dev/null | sort)"
nettoyer() { git checkout -- "$LISTE" 2>/dev/null; }
trap nettoyer EXIT

sep "EPREUVE X — une dette disparait de la detection SANS REPONSE"
python -c "
import io
p='$LISTE'; s=io.open(p,encoding='utf-8',newline='').read()
l=[x for x in s.split('\n') if x.startswith('$CIBLE ')][0]
c=l.split('|'); c[2]=' dette-datee     '; c[3]=' A-DATER '
io.open(p,'w',encoding='utf-8',newline='').write(s.replace(l,'|'.join(c),1))
"
S="$(./verif_capacites.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 4)"
[ "$C" -eq 4 ] && attendu "code 4 : une dette disparue sans reponse fait ROUGIR" ok || attendu "code 4 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "ONT DISPARU DE LA DETECTION SANS REPONSE" && attendu "l outil dit la disparition" ok || attendu "disparition non dite" ko
printf '%s\n' "$S" | grep -qF "$CIBLE" && attendu "le symbole est NOMME" ok || attendu "symbole non nomme" ko
printf '%s\n' "$S" | grep -qF "LECTURE CREUSE" && attendu "la cause dangereuse est nommee (lecture creuse)" ok || attendu "lecture creuse non nommee" ko
printf '%s\n' "$S" | grep -qF "HONOREE" && attendu "la bonne nouvelle est nommee aussi (honoree)" ok || attendu "honoree non nommee" ko
git checkout -- "$LISTE" 2>/dev/null

sep "EPREUVE Y — le symbole n existe plus du tout : IL SE TAIT"
printf '%-44s | %-3s | %-15s | %-7s | %s\n' "NkStructFantome::champDisparu" "D2" "dette-datee" "A-DATER" "fantome d epreuve" >> "$LISTE"
S="$(./verif_capacites.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 0)"
[ "$C" -eq 0 ] && attendu "code 0 : une suppression de code ne fait PAS rougir" ok || attendu "code 0 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "PERIMEE" && attendu "la ligne est dite PERIMEE, et c est une information" ok || attendu "PERIMEE non dit" ko
printf '%s\n' "$S" | grep -qF "NkStructFantome::champDisparu" && attendu "le fantome est nomme" ok || attendu "fantome non nomme" ko
git checkout -- "$LISTE" 2>/dev/null

sep "EPREUVE Z — la ligne porte deja une reponse : information seule"
S="$(./verif_capacites.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 0)"
[ "$C" -eq 0 ] && attendu "code 0 : une disparition deja tranchee ne rougit pas" ok || attendu "code 0 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "PORTENT DEJA une reponse" && attendu "l outil dit qu elle porte deja une reponse" ok || attendu "non dit" ko

sep "EPREUVE W — retrait -> vert et arbre identique"
nettoyer
./verif_capacites.sh > /dev/null 2>&1; C=$?
[ "$C" -eq 0 ] && attendu "code 0 apres retrait" ok || attendu "code 0 attendu, obtenu $C" ko
APRES="$(git status --porcelain -- "$LISTE" 2>/dev/null | sort)"
[ "$AVANT" = "$APRES" ] && attendu "les fichiers touches sont identiques a AVANT (differentiel cible)" ok || attendu "fichiers touches identiques" ko

dire ""
dire "======================================================================"
if [ "$ECHECS" -eq 0 ]; then
  dire " EPREUVE REUSSIE — une disparition ne passe plus inapercue."
  dire "   X : dette disparue sans reponse -> ECHEC 4, symbole nomme, causes dites"
  dire "   Y : symbole supprime du code    -> info PERIMEE, code 0 : IL SE TAIT"
  dire "   Z : disparition deja tranchee   -> info, code 0"
  dire "   W : retrait                     -> vert, arbre identique"
else
  dire " EPREUVE RATEE — $ECHECS exigence(s) non tenue(s)."
fi
dire "======================================================================"
exit $([ "$ECHECS" -eq 0 ] && echo 0 || echo 1)
