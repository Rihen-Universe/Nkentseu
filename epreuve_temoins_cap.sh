#!/usr/bin/env bash
#
# epreuve_temoins_cap.sh — LES TEMOINS D1/D2 SONT-ILS LE DERNIER MAILLON ? (2026-08-27)
# =============================================================================
# POURQUOI CETTE EPREUVE, ET POURQUOI ELLE ARRIVE MAINTENANT
#
#   Le tri du 27/08 a mesure, sur les 14 controles jamais rouges, EXACTEMENT
#   DEUX faux verts : cap/temoin-D1 et cap/temoin-D2. Tous les autres criaient
#   deja, ou etaient rattrapes par un voisin. Ces deux-la ne le sont par
#   personne :
#       detecteur rendu muet + les deux temoins neutralises -> CODE 0, VERT,
#       « 0 candidat(s) detecte(s), 145 ligne(s) de classement ».
#   Le tri servait a isoler ceux qui meritaient une epreuve invasive. Il en a
#   isole deux. Les voici.
#
# CE QUE L EPREUVE DOIT RESTAURER COUTE PLUS CHER A ECRIRE QUE L EPREUVE
# -----------------------------------------------------------------------------
#   Lecon du 22/08, payee 49 lignes : un `trap ... EXIT` arme AVANT son controle
#   transforme le refus de ce controle en `git checkout` qui DETRUIT la
#   modification non commitee qu il refusait justement d ecraser.
#   Cette epreuve-ci est donc dessinee pour n avoir PRESQUE RIEN a restaurer :
#     - elle NE MODIFIE AUCUN FICHIER EXISTANT ;
#     - elle CREE un fichier neuf (le fantome) et le supprime ;
#     - elle travaille sur une COPIE du detecteur, jamais sur l original.
#   Restauration totale : deux `rm`. Aucun `git checkout` n est necessaire, donc
#   aucun travail non commite ne peut etre detruit par elle.
#
# LE FANTOME, ET POURQUOI IL EST UN FICHIER NEUF
# -----------------------------------------------------------------------------
#   Le detecteur lit le SYSTEME DE FICHIERS (grep -r sur Kernel/), pas l index
#   git. Un fichier neuf non suivi est donc lu comme les autres. Cela permet de
#   faire disparaitre les temoins SANS TOUCHER a une seule ligne existante.
#   Il ne porte ni TODO, ni stub, ni WIP : aucune heuristique de nom ne le
#   verrait. Meme choix que NkBancFantome, NkCapFantome et NkCheminFantome.
#
# LES CINQ EPREUVES
#   U   le fantome tue LES DEUX temoins   -> ECHEC D INSTRUMENT 2, les deux NOMMES
#   U1  il ne tue que D2 (une lecture)    -> ECHEC 2, D2 nomme, D1 PAS nomme
#   U2  il ne tue que D1 (un appel)       -> ECHEC 2, D1 nomme, D2 PAS nomme
#   V   temoins RETIRES du script + detecteur muet
#                                          -> CODE 0, VERT : le faux vert, montre
#   W   tout retire                        -> VERT, arbre identique
#
#   U1 et U2 ne sont pas du zele : sans elles, U prouverait qu UN controle
#   quelconque a rougi, pas que LES DEUX temoins fonctionnent chacun de son cote.
#   V est celle qui compte : elle montre ce que le dispositif rend quand ces
#   deux-la ne sont plus la. C est le seul endroit du dispositif ou un detecteur
#   aveugle rend VERT.
#
# CONTROLE NEGATIF — AVANT TOUT, ET AVANT LE FILET
#   Si le detecteur est deja rouge, ou si le fantome existe deja, on REFUSE de
#   demarrer. Faire disparaitre un temoin dans un depot deja rouge rendrait U
#   vraie pour rien.
#
# CODES DE SORTIE
#   0  les cinq epreuves passent
#   1  au moins une exigence non tenue
#   2  refus de demarrer, ou restauration douteuse
# =============================================================================
set -uo pipefail

dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[ep-tem] cd $ROOT impossible"; exit 2; }

FANTOME="Kernel/NkTemoinFantome.cpp"
COPIE="./.ep_temoins_copie.sh"
ECHECS=0

attendu() {
  if [ "$2" = "ok" ]; then dire "   [vu] $1"; else dire "   [RATE] $1"; ECHECS=$((ECHECS + 1)); fi
}
sep() { dire ""; dire "======================================================================"; dire " $*"; dire "======================================================================"; }

# --- CONTROLE NEGATIF, AVANT LE FILET ---------------------------------------
[ -f verif_capacites.sh ] || { dire2 "[ep-tem] REFUS : verif_capacites.sh absent."; exit 2; }
if [ -e "$FANTOME" ]; then
  dire2 "[ep-tem] REFUS : $FANTOME existe deja. Je ne l ecrase pas."
  exit 2
fi
if [ -e "$COPIE" ]; then
  dire2 "[ep-tem] REFUS : $COPIE existe deja (epreuve interrompue ?). Retire-le a la main."
  exit 2
fi
./verif_capacites.sh > /dev/null 2>&1
if [ "$?" -ne 0 ]; then
  dire2 "[ep-tem] REFUS DE DEMARRER : verif_capacites.sh est DEJA rouge."
  dire2 "[ep-tem]   Faire disparaitre un temoin dans un depot deja rouge rendrait"
  dire2 "[ep-tem]   l epreuve U vraie pour rien."
  exit 2
fi
dire "  ok   le detecteur est au vert, aucun fantome ne traine."

# --- LE FILET, MAINTENANT SEULEMENT. Deux rm, rien d autre. ------------------
AVANT="$(git status --porcelain -- "$FANTOME" "$COPIE" 2>/dev/null | sort)"
nettoyer() { rm -f "$FANTOME" "$COPIE" 2>/dev/null; }
trap nettoyer EXIT

# Le fantome : $1 = "lire" | "appeler" | "les-deux"
poser_fantome() {
  local quoi="$1"
  {
    printf '%s\n' '// Fantome de contre-epreuve : cree et retire par epreuve_temoins_cap.sh'
    printf '%s\n' '#include "NKRHI/Core/NkIDevice.h"'
    printf '%s\n' 'namespace nkentseu {'
    printf '%s\n' '	struct NkTemoinFantome {'
    printf '%s\n' '			NkDeviceCaps mCaps;'
    if [ "$quoi" = "lire" ] || [ "$quoi" = "les-deux" ]; then
      printf '%s\n' '			bool Lire() const {'
      printf '%s\n' '				return mCaps.timestampQueries;'
      printf '%s\n' '			}'
    fi
    if [ "$quoi" = "appeler" ] || [ "$quoi" = "les-deux" ]; then
      printf '%s\n' '			float32 Appeler(NkIDevice *d) const {'
      printf '%s\n' '				return d->GetTimestampPeriodNs();'
      printf '%s\n' '			}'
    fi
    printf '%s\n' '	};'
    printf '%s\n' '} // namespace nkentseu'
  } > "$FANTOME"
}

# ---------------------------------------------------------------- U
sep "EPREUVE U — le fantome tue LES DEUX temoins"
poser_fantome les-deux
S="$(./verif_capacites.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 2)"
[ "$C" -eq 2 ] && attendu "code 2 : ECHEC D INSTRUMENT, et surtout PAS un vert" ok \
               || attendu "code 2 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "temoin(s) non retrouve(s)" \
  && attendu "l outil dit que le temoin n est plus retrouve" ok || attendu "rien sur le temoin" ko
printf '%s\n' "$S" | grep -qF "NkIDevice::GetTimestampPeriodNs(D1)" \
  && attendu "le temoin D1 est NOMME" ok || attendu "D1 non nomme" ko
printf '%s\n' "$S" | grep -qF "NkDeviceCaps::timestampQueries(D2)" \
  && attendu "le temoin D2 est NOMME" ok || attendu "D2 non nomme" ko
printf '%s\n' "$S" | grep -qF "produisent la MEME sortie verte" \
  && attendu "la raison est dite : sans temoin, casse et sain se ressemblent" ok \
  || attendu "la raison n est pas dite" ko

# ---------------------------------------------------------------- U1
sep "EPREUVE U1 — seul D2 tombe (une lecture ajoutee)"
poser_fantome lire
S="$(./verif_capacites.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 2)"
[ "$C" -eq 2 ] && attendu "code 2" ok || attendu "code 2 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "NkDeviceCaps::timestampQueries(D2)" \
  && attendu "D2 est nomme" ok || attendu "D2 non nomme" ko
printf '%s\n' "$S" | grep -qF "NkIDevice::GetTimestampPeriodNs(D1)" \
  && attendu "D1 NE doit PAS etre nomme (il tient encore)" ko \
  || attendu "D1 n est PAS nomme : les deux temoins sont INDEPENDANTS" ok

# ---------------------------------------------------------------- U2
sep "EPREUVE U2 — seul D1 tombe (un appel ajoute)"
poser_fantome appeler
S="$(./verif_capacites.sh 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 2)"
[ "$C" -eq 2 ] && attendu "code 2" ok || attendu "code 2 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "NkIDevice::GetTimestampPeriodNs(D1)" \
  && attendu "D1 est nomme" ok || attendu "D1 non nomme" ko
printf '%s\n' "$S" | grep -qF "NkDeviceCaps::timestampQueries(D2)" \
  && attendu "D2 NE doit PAS etre nomme (il tient encore)" ko \
  || attendu "D2 n est PAS nomme : les deux temoins sont INDEPENDANTS" ok
rm -f "$FANTOME"

# ---------------------------------------------------------------- V
sep "EPREUVE V — temoins RETIRES + detecteur muet : LE FAUX VERT, MONTRE"
# Sur une COPIE. L original n est jamais touche, donc rien a restaurer de lui.
python - "$COPIE" <<'PYV'
import io, sys
s = io.open('verif_capacites.sh', encoding='utf-8', newline='').read()
s = s.replace('NB_DET=$(grep -c \'\' < "$TMP/detectes.txt")',
              ': > "$TMP/detectes.txt"\nNB_DET=$(grep -c \'\' < "$TMP/detectes.txt")', 1)
s = s.replace('if [ -n "$manque" ]; then', 'if false; then', 1)
io.open(sys.argv[1], 'w', encoding='utf-8', newline='').write(s)
PYV
chmod +x "$COPIE"
S="$("$COPIE" 2>&1)"; C=$?
dire "   code de sortie : $C   (attendu : 0 — c est le probleme, pas une reussite)"
[ "$C" -eq 0 ] && attendu "code 0 : le detecteur aveugle rend VERT quand les temoins sont partis" ok \
               || attendu "code 0 attendu, obtenu $C" ko
printf '%s\n' "$S" | grep -qF "0 candidat(s) detecte(s)" \
  && attendu "il annonce 0 candidat detecte" ok || attendu "le 0 candidat n est pas dit" ko
printf '%s\n' "$S" | grep -qF "tous classes" \
  && attendu "et il le presente comme « tous classes » — le mensonge exact" ok \
  || attendu "le « tous classes » n apparait pas" ko
dire ""
dire "   C est la seule configuration du dispositif ou un detecteur qui ne detecte"
dire "   plus RIEN rend un verdict VERT. Les temoins D1/D2 sont ce qui separe les"
dire "   deux, et rien d autre ne les rattrape."
rm -f "$COPIE"

# ---------------------------------------------------------------- W
sep "EPREUVE W — retrait complet -> VERT et arbre identique"
nettoyer
./verif_capacites.sh > /dev/null 2>&1; C=$?
[ "$C" -eq 0 ] && attendu "code 0 : le detecteur revient au vert de lui-meme" ok \
               || attendu "code 0 attendu apres retrait, obtenu $C" ko
[ ! -e "$FANTOME" ] && attendu "le fantome est retire du disque" ok || attendu "le fantome traine" ko
[ ! -e "$COPIE" ]   && attendu "la copie du detecteur est retiree" ok || attendu "la copie traine" ko
# ⚠️ CE QUE CETTE VERIFICATION DOIT COMPARER, ET LA PREMIERE VERSION SE TROMPAIT.
# Elle exigeait « git ne voit RIEN » — donc un arbre globalement propre. Au
# premier lancement, l epreuve elle-meme etait non suivie, et le detecteur
# portait une modification non commitee : elle a rendu ROUGE en accusant l arbre
# alors qu elle n avait, elle, rien laisse. Un instrument qui accuse le sujet a
# la place de son propre montage — pour la enieme fois de ce chantier.
# LA BONNE COMPARAISON EST DIFFERENTIELLE : ce que git voyait AVANT, contre ce
# qu il voit APRES. L epreuve repond de ce qu ELLE a change, pas de l etat du
# monde qu elle a trouve en arrivant.
APRES="$(git status --porcelain -- "$FANTOME" "$COPIE" 2>/dev/null | sort)"
if [ "$AVANT" = "$APRES" ]; then
  attendu "arbre identique a ce qu il etait AVANT l epreuve (comparaison differentielle)" ok
else
  attendu "arbre identique a ce qu il etait avant" ko
  dire "        apparu ou disparu :"
  diff <(printf '%s
' "$AVANT") <(printf '%s
' "$APRES") 2>/dev/null | head -6     | while IFS= read -r l; do dire "          $l"; done
fi

dire ""
dire "======================================================================"
if [ "$ECHECS" -eq 0 ]; then
  dire " EPREUVE REUSSIE — les deux temoins sont le dernier maillon, et ils tiennent."
  dire "   U  : les deux tombent    -> ECHEC D INSTRUMENT 2, les deux NOMMES"
  dire "   U1 : seul D2 tombe       -> 2, D2 nomme, D1 intact"
  dire "   U2 : seul D1 tombe       -> 2, D1 nomme, D2 intact"
  dire "   V  : les deux RETIRES    -> CODE 0, VERT sur un detecteur aveugle"
  dire "   W  : apres retrait       -> vert, arbre identique"
  dire ""
  dire " L epreuve n a modifie AUCUN fichier existant : un fichier neuf, une copie,"
  dire " et deux rm. Ce qu elle avait a restaurer ne pouvait ecraser le travail de"
  dire " personne."
else
  dire " EPREUVE RATEE — $ECHECS exigence(s) non tenue(s)."
fi
dire "======================================================================"
exit $([ "$ECHECS" -eq 0 ] && echo 0 || echo 1)
