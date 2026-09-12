#!/usr/bin/env bash
#
# verif_chemins.sh — AUCUN SCRIPT N ECRIT A UN CHEMIN PARTAGE (2026-08-24)
# =============================================================================
# POURQUOI IL EXISTE
#
#   « Un chemin partage entre agents concurrents est une variable globale
#     deguisee en fichier temporaire. »
#
#   Le 24/08, le commit 9b774ab8 du chantier rendu s est retrouve avec le
#   MESSAGE d un autre agent et SES douze fichiers a lui. Les messages etaient
#   composes dans un fichier au chemin fixe sous /tmp — partage par tous les
#   agents de la machine. Deux qui ecrivent au meme instant echangent leurs
#   contenus, ET RIEN N ECHOUE. Aucun code de retour, aucun journal.
#   Le defaut, garde ici comme temoin : /tmp/msg.txt        <-- NKCANARI-TMP
#
#   Le meme defaut vivait chez moi, et a un pire endroit : epreuve_ignore_gpu.sh
#   ecrivait sa sortie a un chemin fixe sous le meme repertoire, et c est ce
#   fichier que l epreuve GREPPE pour decider si un controle a rougi — donc ce
#   que verif_controles.sh inscrit ensuite au JOURNAL DES PREUVES.
#
# POURQUOI CETTE REGLE PLUTOT QUE CELLE DU LIBELLE DE COMMIT
#   « Le message decrit-il son contenu ? » a ete mesuree et marche : elle a
#   retrouve le cas seule sur 2195 commits. Mais elle voit la COPIE, pas le
#   DEPLACEMENT, et elle CONSTATE apres le degat. Celle-ci EMPECHE : le chemin
#   partage EST le degat, et il se voit avant d avoir servi.
#
# CE QU IL FAIT, ET CE QU IL NE FAIT PAS
#   Il lit les scripts SUIVIS PAR GIT et rougit sur toute ecriture a un chemin
#   FIXE sous un repertoire temporaire partage, sauf classement.
#   Il ne lit pas les fichiers non suivis : un brouillon local n est pas une
#   promesse faite a l equipe, et le rougir ferait desactiver l outil.
#   Il ne bloque pas le commit — un controle qui bloque se contourne par
#   --no-verify, en silence. Un rouge, lui, se voit.
#
# LES DEUX PARADES CONTRE LE VERT MENSONGER
#   1. LE CANARI. Un detecteur qui ne trouve rien ressemble trait pour trait a
#      un depot sain. La ligne marquee NKCANARI-TMP ci-dessus porte un vrai
#      chemin fixe et DOIT etre retrouvee a chaque passage ; sinon code 2.
#      Seules les lignes PORTANT LE JETON sont exclues du verdict, pas ce
#      fichier : un vrai chemin fixe ecrit ici doit rougir comme ailleurs.
#   2. LE PLANCHER DE LECTURE. Moins de 20 scripts examines : l outil ne lit
#      plus le depot, et il le DIT au lieu de rendre « rien a signaler ».
#
# LA REGLE, EXACTEMENT
#   Une ligne compte si, APRES avoir ecarte les lignes ENTIEREMENT commentees,
#   elle contient « /tmp/<nom> » NON precede d un caractere de chemin.
#   On n ecarte QUE les lignes dont le premier caractere non blanc ouvre un
#   commentaire. Un commentaire de FIN de ligne n est PAS ecarte : sur cette
#   regle-ci, trop retirer ferait manquer une vraie ecriture — la seule
#   direction d erreur qu on ne peut pas se permettre. C est l inverse du choix
#   fait dans verif_capacites.sh, ou trop retirer ne fait qu ajouter des
#   candidats a classer.
#   L ancrage evite /data/local/tmp/ — un chemin de PERIPHERIQUE Android, pas de
#   cette machine. Sans lui, Tools/nkdeploy.py sortait a tort.
#
# USAGE
#   ./verif_chemins.sh            # garde
#   ./verif_chemins.sh --liste    # les occurrences trouvees, sans garde
#
# CODES DE SORTIE
#   0  toute occurrence est classee, et aucune n est « interdit »
#   2  ECHEC D INSTRUMENT : liste absente, canari perdu, plancher de lecture
#   6  ECHEC : occurrence non classee, « interdit », ou « tolere » sans note
# =============================================================================
set -uo pipefail

dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[chem] cd $ROOT impossible"; exit 2; }

LISTE="config/chemins_partages.list"
LISTE_SEULE=0
case "${1:-}" in
  --liste)   LISTE_SEULE=1 ;;
  -h|--help) sed -n '2,64p' "$0"; exit 0 ;;
  "")        : ;;
  *)         dire2 "[chem] option inconnue : $1"; exit 2 ;;
esac

[ -f "$LISTE" ] || { dire2 "[chem] ECHEC D INSTRUMENT : $LISTE absent. Rien n est conclu."; exit 2; }

CANARI="$(awk -F'=' '/^# CANARI[[:space:]]*=/ { gsub(/[[:space:]]/,"",$2); print $2; exit }' "$LISTE")"
if [ -z "$CANARI" ]; then
  dire2 "[chem] ECHEC D INSTRUMENT : aucune directive CANARI dans $LISTE."
  dire2 "[chem]   Sans canari, un detecteur casse et un depot sain rendent la meme"
  dire2 "[chem]   sortie verte. On ne conclut pas."
  exit 2
fi

TMP="$(mktemp -d)" || { dire2 "[chem] ECHEC D INSTRUMENT : mktemp -d a echoue."; exit 2; }
trap 'rm -rf "$TMP"' EXIT

awk -F'=' '/^# EXCLU-ARBRE[[:space:]]*=/ { gsub(/^[[:space:]]+|[[:space:]]+$/,"",$2); print $2 }' "$LISTE" > "$TMP/exclus.txt"

git ls-files '*.sh' '*.py' '*.bat' '*.ps1' '*.cmd' > "$TMP/tous.txt" 2>/dev/null
: > "$TMP/scripts.txt"
while IFS= read -r f; do
  garde=1
  while IFS= read -r ex; do
    [ -z "$ex" ] && continue
    case "$f" in "$ex"*) garde=0; break;; esac
  done < "$TMP/exclus.txt"
  [ "$garde" -eq 1 ] && printf '%s\n' "$f" >> "$TMP/scripts.txt"
done < "$TMP/tous.txt"

NB=$(grep -c '' < "$TMP/scripts.txt")
if [ "$NB" -lt 20 ]; then
  dire2 "[chem] ECHEC D INSTRUMENT : $NB script(s) examine(s)."
  dire2 "[chem]   L outil ne lit plus le depot. Je n ai rien trouve et je ne lis"
  dire2 "[chem]   plus le fichier rendent la meme sortie verte : on ne conclut pas."
  exit 2
fi

: > "$TMP/hits.txt"; : > "$TMP/canari.txt"
while IFS= read -r f; do
  awk -v F="$f" -v CAN="$CANARI" '
    { l = $0; sub(/\r$/, "", l) }
    index(l, CAN) > 0 { print F ":" FNR > "/dev/stderr"; next }
    l ~ /^[[:space:]]*(#|::|REM |rem )/ { next }
    {
      if (match(l, /(^|[^A-Za-z0-9_.\/-])\/tmp\/[A-Za-z0-9_.-]+/)) {
        ch = substr(l, RSTART, RLENGTH)
        sub(/^[^\/]*/, "", ch)
        print F "|" FNR "|" ch
      }
    }' "$f" 2>> "$TMP/canari.txt" >> "$TMP/hits.txt"
done < "$TMP/scripts.txt"

NB_CANARI=$(grep -c '' < "$TMP/canari.txt")
if [ "$NB_CANARI" -lt 1 ]; then
  dire2 "[chem] ECHEC D INSTRUMENT : le canari $CANARI n a ete retrouve NULLE PART."
  dire2 "[chem]   Il vit dans l en-tete de ce script meme. Ne plus le voir veut dire que"
  dire2 "[chem]   l outil ne lit plus les fichiers — pas que le depot est propre."
  exit 2
fi

TOTAL=$(grep -c '' < "$TMP/hits.txt")

if [ "$LISTE_SEULE" -eq 1 ]; then
  dire "-- occurrences (sans garde) -----------------------------------------"
  dire "   canari retrouve $NB_CANARI fois, $NB script(s) examine(s)"
  if [ "$TOTAL" -eq 0 ]; then dire "   aucune"; else sed 's/^/   /' "$TMP/hits.txt"; fi
  exit 0
fi

dire "-- Chemins partages : la garde ---------------------------------------"
dire "[chem]   $NB script(s) suivi(s) examine(s), canari retrouve $NB_CANARI fois"

CODE=0; NON_CLASSEES=0; INTERDITS=0; SANS_NOTE=0; TOLERES=0
while IFS='|' read -r f n ch; do
  [ -n "$f" ] || continue
  reponse="$(awk -F'|' -v F="$f" -v C="$ch" '
      { r=$0; sub(/\r$/,"",r) }
      r ~ /^[[:space:]]*(#|$)/ { next }
      { split(r, c, "|")
        for (i=1;i<=4;i++) gsub(/^[[:space:]]+|[[:space:]]+$/,"",c[i])
        if (c[1] == F && c[2] == C) { print c[3] "|" c[4]; exit } }' "$LISTE")"
  if [ -z "$reponse" ]; then
    NON_CLASSEES=$((NON_CLASSEES+1)); CODE=6
    dire "  [NON CLASSE] $f:$n"
    dire "        $ch   ->  ajoute une ligne dans $LISTE"
    continue
  fi
  classe="${reponse%%|*}"; note="${reponse#*|}"
  case "$classe" in
    interdit)
      INTERDITS=$((INTERDITS+1)); CODE=6
      dire "  [INTERDIT] $f:$n   $ch"
      [ -n "$note" ] && dire "        $note" ;;
    tolere)
      if [ -z "$note" ]; then
        SANS_NOTE=$((SANS_NOTE+1)); CODE=6
        dire "  [TOLERE SANS NOTE] $f:$n   $ch"
        dire "        tolere sans raison ecrite est le mot qu on met pour faire taire"
        dire "        l outil. La note doit dire POURQUOI ce chemin n est pas"
        dire "        partageable en pratique."
      else
        TOLERES=$((TOLERES+1))
      fi ;;
    *)
      NON_CLASSEES=$((NON_CLASSEES+1)); CODE=6
      dire "  [CLASSE INCONNUE] $f:$n   $ch   ($classe)" ;;
  esac
done < "$TMP/hits.txt"

dire ""
if [ "$CODE" -eq 0 ]; then
  dire "[chem]   ok   $TOTAL occurrence(s), toutes classees ; $TOLERES toleree(s) avec raison ecrite"
  dire "[chem]        aucun chemin fixe sous un repertoire partage n est en vigueur."
else
  dire "[chem]   ECHEC — non classees=$NON_CLASSEES interdits=$INTERDITS tolere-sans-note=$SANS_NOTE"
  dire "[chem]        Un chemin partage entre agents concurrents est une variable"
  dire "[chem]        globale deguisee en fichier temporaire. Corrige (mktemp, ou un"
  dire "[chem]        chemin sous Build/ propre a cet arbre), ou classe-le en disant"
  dire "[chem]        pourquoi il ne peut pas etre partage."
fi
exit $CODE
