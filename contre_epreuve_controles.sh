#!/usr/bin/env bash
#
# contre_epreuve_controles.sh — LE JOURNAL DES ROUGES SAIT-IL ROUGIR ? (2026-08-24)
# =============================================================================
# POURQUOI ELLE EXISTE, ET C'EST LA MEME PHRASE QUE CELLE QU'ELLE SERT
#
#   « Un controle qui n'a jamais rougi n'est pas un controle, c'est une
#     intention. »
#
#   verif_controles.sh est ne aujourd'hui pour repondre a cette phrase. Le
#   laisser sans contre-epreuve serait la commettre dans l'outil meme qui la
#   traque : un tableau de bord des controles jamais rouges, jamais rouge
#   lui-meme. Ce script casse les trois choses qu'il promet et exige le rouge.
#
# LES QUATRE EPREUVES
#   J  un controle NEUF, jamais rouge, ajoute a la liste
#        -> CLIQUET ROMPU, code 5. C'est la promesse centrale : le stock
#           d'intentions ne peut pas grossir en silence.
#   K  un controle dont le MOTIF ne peut pas apparaitre dans la sortie
#        -> rien n'est ecrit au journal, et le journal ne grossit PAS.
#           C'est le defaut du chantier design retourne contre cet outil : un
#           motif introuvable ne doit surtout pas rendre vert ET enregistrer.
#   L  la liste privee de sa directive de plafond
#        -> ECHEC D'INSTRUMENT, code 2, jamais un vert. Un cliquet ABSENT et un
#           cliquet SATISFAIT produisent la meme sortie verte.
#   M  tout est retire
#        -> code 0, et l'arbre est IDENTIQUE (liste ET journal).
#
# CONTROLE NEGATIF — le montage doit etre sain AVANT de casser quoi que ce soit
#   Si verif_controles.sh est deja rouge, ou si la liste ou le journal sont deja
#   modifies, ce script REFUSE DE DEMARRER. Casser un outil deja rouge rendrait
#   J vraie pour rien.
#
# ⚠️ ORDRE CRITIQUE — LE CONTROLE NEGATIF D'ABORD, LE FILET APRES.
#   Un `trap restaurer EXIT` arme AVANT le controle « l'arbre est-il propre ? »
#   transforme le refus de ce controle en `git checkout` qui DETRUIT la
#   modification non commitee qu'il refusait justement d'ecraser. C'est arrive
#   dans ce depot le 22/08. Un filet ne s'arme jamais avant son controle.
#
# CODES DE SORTIE
#   0  les quatre epreuves passent
#   1  au moins une exigence non tenue
#   2  refus de demarrer, ou restauration douteuse
# =============================================================================
set -uo pipefail

dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[ce-ctl] cd $ROOT impossible"; exit 2; }

LISTE="config/controles.list"
JOURNAL="controles_rouges.journal"
ECHECS=0

attendu() { # $1 = libelle, $2 = ok|ko
  if [ "$2" = "ok" ]; then dire "   [vu] $1"; else dire "   [RATE] $1"; ECHECS=$((ECHECS + 1)); fi
}

# --- CONTROLE NEGATIF, ET IL PASSE AVANT LE FILET ---------------------------
for f in "$LISTE" "$JOURNAL" verif_controles.sh; do
  [ -f "$f" ] || { dire2 "[ce-ctl] REFUS : $f absent."; exit 2; }
done
if ! git diff --quiet -- "$LISTE" "$JOURNAL" 2>/dev/null; then
  dire2 "[ce-ctl] REFUS DE DEMARRER : $LISTE ou $JOURNAL portent deja des modifications"
  dire2 "[ce-ctl]   non commitees. Les restaurer serait DETRUIRE un travail que ce"
  dire2 "[ce-ctl]   controle-ci refusait justement d'ecraser. Commite d'abord."
  exit 2
fi
./verif_controles.sh > /dev/null 2>&1
if [ "$?" -ne 0 ]; then
  dire2 "[ce-ctl] REFUS DE DEMARRER : verif_controles.sh est DEJA rouge."
  dire2 "[ce-ctl]   Casser un outil deja rouge rendrait l'epreuve J vraie pour rien."
  exit 2
fi
dire "  ok   verif_controles.sh est au vert, et la liste et le journal sont propres."

# --- LE FILET, MAINTENANT SEULEMENT -----------------------------------------
restaurer() { git checkout -- "$LISTE" "$JOURNAL" 2>/dev/null; }
trap restaurer EXIT

EMPREINTE_JOURNAL_AVANT="$(git hash-object "$JOURNAL")"

# =============================================================================
dire ""
dire "======================================================================"
dire " EPREUVE J — un controle NEUF, jamais rouge -> CLIQUET ROMPU"
dire "======================================================================"
printf 'ce-ctl/fantome-jamais-rouge      | contre_epreuve_controles.sh | un controle neuf que personne n a fait rougir | aucune |\n' >> "$LISTE"
SORTIE_J="$(./verif_controles.sh 2>&1)"; CODE_J=$?
dire "   code de sortie : $CODE_J   (attendu : 5)"
[ "$CODE_J" -eq 5 ] && attendu "code 5 : le cliquet refuse que le stock d'intentions MONTE" ok \
                    || attendu "code 5 attendu, obtenu $CODE_J" ko
printf '%s\n' "$SORTIE_J" | grep -qF "CLIQUET ROMPU" \
  && attendu "l'outil dit CLIQUET ROMPU" ok || attendu "CLIQUET ROMPU non dit" ko
printf '%s\n' "$SORTIE_J" | grep -qF "ce-ctl/fantome-jamais-rouge" \
  && attendu "le controle fantome est NOMME, pas seulement compte" ok || attendu "fantome non nomme" ko
printf '%s\n' "$SORTIE_J" | grep -qF "AUCUNE EPREUVE" \
  && attendu "il dit qu'aucune epreuve ne sait le faire rougir" ok || attendu "l'absence d'epreuve n'est pas dite" ko
git checkout -- "$LISTE" 2>/dev/null

# =============================================================================
dire ""
dire "======================================================================"
dire " EPREUVE K — un MOTIF introuvable -> rien n'est ecrit au journal"
dire "======================================================================"
printf 'ce-ctl/motif-impossible          | contre_epreuve_controles.sh | un motif qui ne peut pas apparaitre | ce-capacites | NKMOTIFQUINEXISTEPASDUTOUT-20260824\n' >> "$LISTE"
SORTIE_K="$(./verif_controles.sh --enregistrer ce-capacites 2>&1)"; CODE_K=$?
dire "   code de sortie : $CODE_K   (attendu : 0)"
printf '%s\n' "$SORTIE_K" | grep -qF "motif ABSENT, rien enregistre : ce-ctl/motif-impossible" \
  && attendu "le motif introuvable est DIT, et rien n'est enregistre pour lui" ok \
  || attendu "le motif introuvable n'est pas signale" ko
grep -qF "ce-ctl/motif-impossible" "$JOURNAL" \
  && attendu "aucune ligne de journal pour un motif introuvable" ko \
  || attendu "aucune ligne de journal pour un motif introuvable" ok
# ⚠️ Le journal a le droit d'avoir grossi des AUTRES controles de ce-capacites si
# la journee est nouvelle ; ce qu'on exige, c'est qu'AUCUNE ligne ne porte l'id
# du fantome. C'est l'exigence exacte, pas une approximation par la taille.
git checkout -- "$LISTE" "$JOURNAL" 2>/dev/null

# =============================================================================
dire ""
dire "======================================================================"
dire " EPREUVE L — plus de directive de plafond -> ECHEC D'INSTRUMENT"
dire "======================================================================"
grep -v '^# PLAFOND-JAMAIS-ROUGE' "$LISTE" > "$LISTE.tmp" && mv "$LISTE.tmp" "$LISTE"
SORTIE_L="$(./verif_controles.sh 2>&1)"; CODE_L=$?
dire "   code de sortie : $CODE_L   (attendu : 2)"
[ "$CODE_L" -eq 2 ] && attendu "code 2 : ECHEC D'INSTRUMENT, jamais un vert" ok \
                    || attendu "code 2 attendu, obtenu $CODE_L" ko
printf '%s\n' "$SORTIE_L" | grep -qF "cliquet ABSENT et un cliquet SATISFAIT" \
  && attendu "la raison est dite : absent et satisfait se ressemblent" ok \
  || attendu "la raison n'est pas dite" ko
git checkout -- "$LISTE" 2>/dev/null

# =============================================================================
dire ""
dire "======================================================================"
dire " EPREUVE M — retrait complet -> VERT et arbre identique"
dire "======================================================================"
./verif_controles.sh > /dev/null 2>&1; CODE_M=$?
[ "$CODE_M" -eq 0 ] && attendu "code 0 : l'outil revient au vert de lui-meme" ok \
                    || attendu "code 0 attendu apres restauration, obtenu $CODE_M" ko
if git diff --quiet -- "$LISTE" "$JOURNAL" 2>/dev/null; then
  attendu "arbre identique : git ne voit plus rien sur la liste ni sur le journal" ok
else
  attendu "arbre identique" ko
fi
EMPREINTE_JOURNAL_APRES="$(git hash-object "$JOURNAL")"
[ "$EMPREINTE_JOURNAL_AVANT" = "$EMPREINTE_JOURNAL_APRES" ] \
  && attendu "le journal est revenu octet pour octet" ok \
  || attendu "le journal a change ($EMPREINTE_JOURNAL_AVANT -> $EMPREINTE_JOURNAL_APRES)" ko

dire ""
dire "======================================================================"
if [ "$ECHECS" -eq 0 ]; then
  dire " CONTRE-EPREUVE REUSSIE — le journal des rouges sait rougir."
  dire "   J : controle neuf jamais rouge -> CLIQUET ROMPU, code 5"
  dire "   K : motif introuvable          -> rien enregistre, et c'est DIT"
  dire "   L : plafond absent             -> ECHEC D'INSTRUMENT, code 2"
  dire "   M : apres retrait              -> vert, liste et journal a l'identique"
else
  dire " CONTRE-EPREUVE RATEE — $ECHECS exigence(s) non tenue(s)."
fi
dire "======================================================================"
exit $([ "$ECHECS" -eq 0 ] && echo 0 || echo 1)
