#!/usr/bin/env bash
#
# verif_controles.sh — QUELS DE MES CONTROLES ONT DEJA ROUGI ? (2026-08-24)
# =============================================================================
# POURQUOI IL EXISTE
#
#   « Un controle qui n'a jamais rougi n'est pas un controle, c'est une
#     intention. »
#
#   Le chantier design a ecrit un controle avec « \b » dans une chaine Python
#   non brute — ou « \b » est le caractere RETOUR ARRIERE, pas une frontiere de
#   mot. Le controle tournait, ne trouvait rien, et RENDAIT VERT. L'« ok » etait
#   bien APRES l'ecriture, et il ne prouvait rien : il portait sur ZERO CAS
#   EXAMINE.
#
#   La question « ce controle a-t-il deja rougi ? » ne doit donc pas se poser a
#   la memoire de celui qui l'a ecrit. Elle se lit ici.
#
# CE QU'IL FAIT
#   defaut          confronte config/controles.list a controles_rouges.journal,
#                   NOMME tout controle sans entree, et garde le cliquet.
#   --enregistrer <epreuve>
#                   lance l'epreuve, et pour CHAQUE controle qui la declare,
#                   cherche son MOTIF (grep -F, jamais une regex) dans la sortie
#                   obtenue. Trouve -> une ligne de journal est ajoutee. Absent
#                   -> RIEN n'est ecrit, et c'est dit.
#   --epreuves      la liste des epreuves connues.
#
# ⚠️ IL NE ROUGIT PAS SUR UN CONTROLE JAMAIS ROUGE, ET C'EST DELIBERE
#   Un controle jamais rouge n'est pas casse : il est SUSPECT. Le rougir ferait
#   desactiver cet outil-ci dans la semaine, et on aurait reconstruit le probleme
#   qu'il devait resoudre — exactement l'arbitrage deja pris pour les dettes
#   « A-DATER » et pour bancs.list. Ce qui garde, c'est le CLIQUET : leur nombre
#   peut descendre, il ne peut pas monter.
#
# ⚠️ CE QU'UNE ENTREE DE JOURNAL PROUVE, ET RIEN DE PLUS
#   Que l'EPREUVE a affirme avoir vu ce controle rougir. Sa force est celle de
#   l'epreuve. C'est pour cela que les quatre epreuves ont un CONTROLE NEGATIF
#   (elles refusent de demarrer sur un arbre deja rouge ou deja sale) : sans ce
#   refus, un « vu » vaudrait ce que valait l'« ok » du chantier design.
#
# CODES DE SORTIE
#   0  la liste et le journal se repondent, et le cliquet tient
#   2  ECHEC D'INSTRUMENT : liste absente, journal illisible, cliquet absent,
#      epreuve inconnue. Rien n'est conclu.
#   5  CLIQUET ROMPU : plus de controles jamais rouges que le plafond declare.
# =============================================================================
set -uo pipefail

dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[ctl] cd $ROOT impossible"; exit 2; }

LISTE="config/controles.list"
JOURNAL="controles_rouges.journal"

# Les epreuves connues. C'est une DONNEE, pas une heuristique sur les noms de
# fichiers : un script d'epreuve renomme doit se voir, pas disparaitre.
epreuve_script() {
  case "$1" in
    ce-bancs)     printf '%s\n' "./contre_epreuve_verif.sh" ;;
    ce-capacites) printf '%s\n' "./contre_epreuve_capacites.sh" ;;
    ignore-gpu)   printf '%s\n' "./epreuve_ignore_gpu.sh" ;;
    msaa-contrat) printf '%s\n' "./epreuve_msaa_contract.sh" ;;
    ce-controles) dire "./contre_epreuve_controles.sh" ;;
    ce-chemins)   dire "./contre_epreuve_chemins.sh" ;;
    temoins-cap)  dire "./epreuve_temoins_cap.sh" ;;
    disparition)  dire "./epreuve_disparition.sh" ;;
    *)            printf '%s\n' "" ;;
  esac
}

ENREGISTRER=""
while [ "$#" -gt 0 ]; do
  case "$1" in
    --enregistrer) ENREGISTRER="${2:-}"; shift 2 ;;
    --epreuves)    dire "ce-bancs  ce-capacites  ignore-gpu  msaa-contrat  ce-controles  ce-chemins  temoins-cap  disparition"; exit 0 ;;
    -h|--help)     sed -n '2,48p' "$0"; exit 0 ;;
    *) dire2 "[ctl] option inconnue : $1"; exit 2 ;;
  esac
done

# --- Preconditions : ne rien conclure depuis un montage casse ----------------
[ -f "$LISTE" ] || { dire2 "[ctl] ECHEC D'INSTRUMENT : $LISTE absent. Rien n'est conclu."; exit 2; }
[ -f "$JOURNAL" ] || : > "$JOURNAL"

PLAFOND="$(awk -F'=' '/^# PLAFOND-JAMAIS-ROUGE[[:space:]]*=/ { gsub(/[^0-9]/, "", $2); print $2; exit }' "$LISTE")"
if [ -z "$PLAFOND" ]; then
  dire2 "[ctl] ECHEC D'INSTRUMENT : aucune directive « # PLAFOND-JAMAIS-ROUGE = <n> » dans $LISTE."
  dire2 "[ctl]   Un cliquet ABSENT et un cliquet SATISFAIT produisent la meme sortie verte."
  exit 2
fi

# --- Lecture de la liste (le \r final est ampute : un .list reecrit par un -----
#     outil Windows ne doit pas casser le parseur, la parade vit ICI) ----------
lire_liste() {
  awk -F'|' '
    { l = $0; sub(/\r$/, "", l) }
    l ~ /^[[:space:]]*(#|$)/ { next }
    {
      n = split(l, c, "|")
      if (n < 4) next
      for (i = 1; i <= n; i++) { gsub(/^[[:space:]]+|[[:space:]]+$/, "", c[i]) }
      printf "%s\t%s\t%s\t%s\t%s\n", c[1], c[2], c[3], c[4], (n >= 5 ? c[5] : "")
    }' "$LISTE"
}

# =============================================================================
# MODE --enregistrer : lancer une epreuve, et n'ecrire QUE ce qu'on a VU
# =============================================================================
if [ -n "$ENREGISTRER" ]; then
  SCRIPT="$(epreuve_script "$ENREGISTRER")"
  if [ -z "$SCRIPT" ]; then
    dire2 "[ctl] ECHEC D'INSTRUMENT : epreuve inconnue « $ENREGISTRER » (voir --epreuves)."
    exit 2
  fi
  if [ ! -x "$SCRIPT" ] && [ ! -f "$SCRIPT" ]; then
    dire2 "[ctl] ECHEC D'INSTRUMENT : $SCRIPT introuvable. Rien n'est conclu, rien n'est ecrit."
    exit 2
  fi

  ATTENDUS="$(lire_liste | awk -F'\t' -v e="$ENREGISTRER" '$4 == e && $5 != ""')"
  if [ -z "$ATTENDUS" ]; then
    dire2 "[ctl] ECHEC D'INSTRUMENT : aucun controle de $LISTE ne declare l'epreuve « $ENREGISTRER »"
    dire2 "[ctl]   avec un motif. Lancer l'epreuve pour n'enregistrer personne serait un vert vide."
    exit 2
  fi

  mkdir -p Build/Verif/epreuves
  HORO="$(date +%Y%m%d-%H%M%S)"
  SORTIE="Build/Verif/epreuves/${ENREGISTRER}-${HORO}.log"
  SHA="$(git rev-parse --short HEAD 2>/dev/null || printf '%s' 'sans-git')"

  dire "-- $ENREGISTRER : $SCRIPT --"
  "$SCRIPT" > "$SORTIE" 2>&1
  CODE_EPREUVE=$?
  dire "   code de sortie de l'epreuve : $CODE_EPREUVE"
  dire "   sortie complete : $SORTIE"
  dire ""

  # ⚠️ Une epreuve qui ECHOUE ne prouve rien sur les controles qu'elle exerce.
  # On ne s'en sert pas pour ecrire : « une supposition consignee comme mesure
  # se propage avec l'autorite d'une mesure ».
  if [ "$CODE_EPREUVE" -ne 0 ]; then
    dire2 "[ctl] L'EPREUVE ELLE-MEME A ECHOUE (code $CODE_EPREUVE). RIEN n'est ecrit au journal."
    dire2 "[ctl]   Une epreuve ratee ne dit pas que les controles n'ont pas rougi : elle dit"
    dire2 "[ctl]   qu'on ne sait pas. Repare l'epreuve avant d'enregistrer quoi que ce soit."
    exit 2
  fi

  AJOUTS=0; MANQUANTS=0
  DATE="$(date +%Y-%m-%d)"
  printf '%s\n' "$ATTENDUS" | while IFS=$'\t' read -r id fic refus ep motif; do
    [ -n "$id" ] || continue
    if grep -qF -- "$motif" "$SORTIE"; then
      LIGNE="$(grep -m1 -F -- "$motif" "$SORTIE" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
      if grep -qF -- "$DATE | $id | $ep @ $SHA" "$JOURNAL" 2>/dev/null; then
        dire "   deja au journal aujourd'hui : $id"
      else
        printf '%s | %s | %s @ %s | %s\n' "$DATE" "$id" "$ep" "$SHA" "$LIGNE" >> "$JOURNAL"
        dire "   ROUGE VU et enregistre : $id"
      fi
    else
      dire "   motif ABSENT, rien enregistre : $id"
      dire "      motif cherche : $motif"
    fi
  done

  dire ""
  dire "   Aucun controle n'est enregistre sur la foi d'un lancement : seul un"
  dire "   MOTIF TROUVE dans la sortie ecrit une ligne. Un motif introuvable"
  dire "   n'ecrit rien - et c'est ce qui distingue ce journal d'une liste de"
  dire "   bonnes intentions."
  exit 0
fi

# =============================================================================
# MODE PAR DEFAUT : la garde
# =============================================================================
TMP="$(mktemp -d 2>/dev/null || echo "${TMPDIR:-/tmp}/nkctl.$$")"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

lire_liste > "$TMP/liste.txt"
NB_TOTAL=$(grep -c '' < "$TMP/liste.txt")
if [ "$NB_TOTAL" -lt 5 ]; then
  dire2 "[ctl] ECHEC D'INSTRUMENT : $NB_TOTAL controle(s) lu(s) dans $LISTE."
  dire2 "[ctl]   Le parseur ne lit plus le fichier (forme changee ?). Ne rien conclure :"
  dire2 "[ctl]   « je n'ai rien trouve » et « je ne lis plus le fichier » rendent la"
  dire2 "[ctl]   meme sortie verte."
  exit 2
fi

awk -F'|' '{ l = $0; sub(/\r$/, "", l); n = split(l, c, "|"); if (n >= 2) { gsub(/^[[:space:]]+|[[:space:]]+$/, "", c[2]); print c[2] } }' \
    "$JOURNAL" | sort -u > "$TMP/rouges.txt"

dire "======================================================================"
dire " CONTROLES DU DISPOSITIF DE VERIFICATION — qui a deja rougi ?"
dire "======================================================================"

JAMAIS=0; DEJA=0; SANS_EPREUVE=0
: > "$TMP/jamais.txt"
while IFS=$'\t' read -r id fic refus ep motif; do
  [ -n "$id" ] || continue
  if grep -qxF -- "$id" "$TMP/rouges.txt"; then
    DEJA=$((DEJA + 1))
  else
    JAMAIS=$((JAMAIS + 1))
    [ "$ep" = "aucune" ] && SANS_EPREUVE=$((SANS_EPREUVE + 1))
    printf '%s\t%s\t%s\t%s\n' "$id" "$fic" "$ep" "$refus" >> "$TMP/jamais.txt"
  fi
done < "$TMP/liste.txt"

dire ""
dire "  inventories : $NB_TOTAL     deja rouges : $DEJA     jamais rouges : $JAMAIS"
dire ""

if [ "$JAMAIS" -gt 0 ]; then
  dire "-- JAMAIS ROUGES — suspects, pas casses ------------------------------"
  while IFS=$'\t' read -r id fic ep refus; do
    if [ "$ep" = "aucune" ]; then
      dire "  [ jamais rouge, AUCUNE EPREUVE ]  $id"
    else
      dire "  [ jamais rouge, epreuve $ep ]  $id"
    fi
    dire "        $fic — refuse : $refus"
  done < "$TMP/jamais.txt"
  dire ""
  dire "  $SANS_EPREUVE de ces controles n'ont AUCUNE epreuve capable de les faire"
  dire "  rougir a la demande. Ce n'est pas une case a remplir plus tard : c'est"
  dire "  le resultat le plus utile de ce fichier. Tant qu'aucune epreuve ne les"
  dire "  exerce, personne ne peut distinguer « il garde » de « il ne lit plus »."
  dire ""
fi

CODE=0
dire "-- CLIQUET $PLAFOND ---------------------------------------------------"
if [ "$JAMAIS" -gt "$PLAFOND" ]; then
  dire "  CLIQUET ROMPU : $JAMAIS controle(s) jamais rouge(s) pour un plafond de $PLAFOND."
  dire "  Un controle NOUVEAU se fait rougir A LA NAISSANCE. Sans quoi on accumule"
  dire "  des intentions au rythme ou l'on croit accumuler des garanties."
  dire "  Deux sorties : faire rougir le controle neuf sous une epreuve, ou relever"
  dire "  le plafond DELIBEREMENT dans $LISTE — un geste qui apparait dans un diff."
  CODE=5
else
  dire "  ok   $JAMAIS <= $PLAFOND. Le stock d'intentions ne grossit pas."
  if [ "$JAMAIS" -lt "$PLAFOND" ]; then
    dire "  ⚠️ Et il a DESCENDU : pense a abaisser le plafond a $JAMAIS dans $LISTE,"
    dire "     sinon le cliquet redonne de la place a la derive qu'il vient d'eviter."
  fi
fi
dire "======================================================================"
exit $CODE
