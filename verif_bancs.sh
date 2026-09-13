#!/usr/bin/env bash
#
# verif_bancs.sh — LE VERIFICATEUR DE BANCS (2026-08-22)
# =============================================================================
# POURQUOI IL EXISTE
#
#   « Un banc s'ignore en ne le lancant pas — et on ne le lance pas le jour ou
#     on est presse, c'est-a-dire exactement le jour ou on casse quelque chose. »
#
#   Une commande unique qui CONSTRUIT tous les bancs declares, les EXECUTE,
#   rend un verdict lisible, et sort NON NUL si quoi que ce soit echoue.
#
# CE QU'IL FAIT, DANS L'ORDRE
#   1. Preconditions — il refuse de juger le depot depuis un montage casse.
#   2. Garde de la liste — tout projet d'Applications/ doit etre CLASSE.
#   3. Construction + execution de chaque banc, avec son critere de verdict.
#   4. Rapport : nom, construit ou non, code de sortie, duree, et en cas
#      d'echec la premiere ligne significative de la sortie.
#
# LES SIX REGLES QU'IL APPLIQUE, ET LE DEFAUT QUI LES A FAIT ECRIRE
#   1. Le code de sortie de la CONSTRUCTION est une donnee du controle. Un banc
#      dont la construction echoue est un ECHEC — jamais un succes silencieux.
#      (Un agent a compte une mutation « survivante » alors que la compilation
#      avait echoue et que L'ANCIEN BINAIRE avait tourne. Ici : construction
#      ratee => on N'EXECUTE PAS, et on ne regarde meme pas l'exe.)
#   2. La liste est GARDEE, pas seulement ecrite : un projet d'Applications/
#      sans ligne de classement fait ECHOUER le verificateur. Et le marquage
#      « ceci est un banc » est une DONNEE (config/bancs.list), jamais une
#      heuristique sur le nom — « les projets finissant par Test » rate le
#      premier banc nomme autrement, en silence.
#   3. Il a sa contre-epreuve : voir contre_epreuve_verif.sh, qui casse un banc
#      volontairement et exige que ce script le dise et sorte non nul.
#   4. Aucun texte a accolades ne passe par un formateur a marqueurs : tout
#      sort par `printf '%s\n'` avec le texte en ARGUMENT. (Le C++ du depot a
#      paye trois jours pour cette regle ; un shell n'y echappe que s'il ne met
#      jamais de donnee dans une chaine de format.)
#   5. Il dit POURQUOI, pas seulement QUE.
#   6. Il doit etre lancable : il chronometre chaque etape et affiche sa duree.
#      Deux modes, `rapide` et `complet` — ce que chacun garantit est ecrit
#      dans le rapport et dans config/bancs.list.
#
# USAGE
#   ./verif_bancs.sh                        # mode rapide, config Debug
#   ./verif_bancs.sh --mode complet         # tous les bancs
#   ./verif_bancs.sh --banc NkSLCheck       # un seul banc, par son nom
#   ./verif_bancs.sh --liste                # inventaire seul, ne construit rien
#   ./verif_bancs.sh --capacites            # garde des capacites SEULE (~10 s)
#   ./verif_bancs.sh --sans-capacites       # passe sans la garde des capacites
#   ./verif_bancs.sh --config Release
#   ./verif_bancs.sh --sans-construire      # execute les exes deja presents
#   ./verif_bancs.sh --reconstruire         # `jenga rebuild` : table rase par banc
#
# ET UN QUATRIEME OUTIL, A COTE DE CELUI-CI (2026-08-24)
#   ./verif_controles.sh   dit lesquels de MES PROPRES CONTROLES ont deja
#   rougi au moins une fois, et nomme ceux qui n ont jamais rougi.
#   « Un controle qui n a jamais rougi n est pas un controle, c est une
#   intention. » Le journal (controles_rouges.journal) n est jamais ecrit de
#   memoire : `--enregistrer <epreuve>` lance l epreuve et n inscrit une ligne
#   que si le motif declare est TROUVE dans la sortie obtenue.
#   Mesure du 24/08 : 45 controles, 31 ont deja rougi, 14 JAMAIS -- et les 14
#   sont les preconditions et les temoins d instrument de CE script-ci et de
#   verif_capacites.sh. Personne ne peut aujourd hui distinguer « il garde »
#   de « il ne garde plus » pour ces quatorze-la.
#
#   ./verif_chemins.sh     refuse qu un script suivi ecrive a un chemin FIXE
#   sous un repertoire temporaire partage. « Un chemin partage entre agents
#   concurrents est une variable globale deguisee en fichier temporaire » :
#   le 24/08, deux chantiers ont echange leurs messages de commit par ce
#   moyen, et RIEN N A ECHOUE. Rouge de naissance sur deux fichiers reels,
#   repares le jour meme.
#
# TROIS NIVEAUX DE GARANTIE SUR LA FRAICHEUR DU BINAIRE — dire lequel garantit
# quoi fait partie du travail :
#   --sans-construire  ne garantit RIEN sur la fraicheur. Utile pour rejouer une
#                      execution sans repayer la construction, jamais pour
#                      conclure « ca passe ».
#   (defaut)           garantit ce que le systeme de build affirme. Suffisant au
#                      quotidien, et c'est le mode a lancer avant un commit.
#   --reconstruire     garantit que le binaire vient des sources ACTUELLES.
#                      ⚠️ Mesure du 2026-08-22 : un cache qui compare les DATES
#                      ne voit pas un fichier restaure depuis une sauvegarde qui
#                      preserve la sienne (`cp -p`, `tar -p`, `rsync -t`). Le
#                      build dit « a jour », et l'ANCIEN binaire tourne. C'est ce
#                      mode qui tranche ce cas — et c'est arrive ICI, dans la
#                      contre-epreuve, avant d'etre corrige.
#
# TROIS ETATS, PAS DEUX — OK, ECHEC, ET IGNORE
#   Un banc qui exige un GPU (ou un pilote, ou un peripherique) sera SAUTE sur une
#   machine qui ne l'a pas. Le compter OK est le pire des deux mensonges possibles :
#   on croit la question mesuree alors que personne ne l'a posee. Le compter ECHEC
#   n'est pas mieux — une machine sans carte n'est le bug de personne, et un banc
#   qui accuse la moitie du parc est desactive en une semaine.
#   Donc IGNORE est un TROISIEME etat : compte a part, NOMME avec sa raison lue
#   dans la sortie du banc, et rappele sous la ligne de verdict. Il ne change pas
#   le code de sortie (rien n'est casse), mais il rend impossible de lire « OK »
#   comme « tout est verifie ».
#   Un banc le declare par le verdict « code+ignore:<n> » dans config/bancs.list ;
#   <n> est une DONNEE de sa ligne, jamais une constante devinee par ce script.
#   ⚠️ Le precedent a ne pas suivre : NkGpuProbe rend 1 quand aucun device n'existe.
#   Il confond « pas de carte » et « la carte est cassee » — un faux ECHEC, le
#   symetrique du faux vert.
#
# CODES DE SORTIE
#   0  tout va bien
#   1  au moins un banc en ECHEC (construction ou execution ou verdict)
#   2  ECHEC D'INSTRUMENT : precondition manquante, montage casse. Rien n'est
#      conclu sur le depot — c'est le verificateur qui n'est pas en etat.
#   3  ECHEC DE CLASSEMENT : un projet d'Applications/ n'est pas classe.
#   (une capacite annoncee et non classee — voir config/capacites.list et
#    verif_capacites.sh — ROUGIT cette passe via le code 1. Elle ne refuse
#    JAMAIS un commit : un controle qui bloque se contourne par --no-verify,
#    en silence ; un banc rouge, lui, se voit.)
#   (les categories se cumulent en prenant le plus grand code rencontre, sauf
#    2 qui l'emporte toujours : un instrument casse ne prouve rien.)
# =============================================================================
set -uo pipefail

# --- Le shell ne met JAMAIS de donnee dans une chaine de format (regle 4) -----
dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[verif] cd $ROOT impossible"; exit 2; }

LISTE="config/bancs.list"
SORTIE="Build/Verif"
CONFIG="Debug"
MODE="rapide"
UN_BANC=""
INVENTAIRE_SEUL=0
CAPACITES_SEULES=0
AVEC_CAPACITES=1
SANS_CONSTRUIRE=0
RECONSTRUIRE=0
DELAI_DEFAUT=600          # secondes ; un banc muet ne bloque pas la journee
JENGA_MIN="2.4.0"         # --target sur build, --force sur test

# --- Arguments ---------------------------------------------------------------
while [ "$#" -gt 0 ]; do
  case "$1" in
    --mode)            MODE="${2:-}"; shift 2 ;;
    --config)          CONFIG="${2:-}"; shift 2 ;;
    --banc)            UN_BANC="${2:-}"; shift 2 ;;
    --liste)           INVENTAIRE_SEUL=1; shift ;;
    --capacites)       CAPACITES_SEULES=1; shift ;;
    --sans-capacites)  AVEC_CAPACITES=0; shift ;;
    --sans-construire) SANS_CONSTRUIRE=1; shift ;;
    --reconstruire)    RECONSTRUIRE=1; shift ;;
    --delai)           DELAI_DEFAUT="${2:-}"; shift 2 ;;
    -h|--help)         awk 'NR<2{next} /^# ={10,}$/{n++; print; if(n==2) exit; next} {print}' "$0"; exit 0 ;;
    *) dire2 "[verif] option inconnue : $1 (voir --help)"; exit 2 ;;
  esac
done

case "$MODE" in rapide|complet) ;; *) dire2 "[verif] --mode doit valoir rapide ou complet"; exit 2 ;; esac

T0=$(date +%s)
horodate() { date '+%Y-%m-%d %H:%M:%S'; }
duree_depuis() { local d=$(( $(date +%s) - $1 )); printf '%dm%02ds' $(( d / 60 )) $(( d % 60 )); }

# =============================================================================
# 1. PRECONDITIONS — refuser de juger le depot depuis un montage casse
# =============================================================================
# Regle du depot : « une mesure ne vaut que ce que vaut son montage ». Sans ce
# bloc, un worktree neuf aux sous-modules vides rend « 9 bancs en echec de
# construction » pour une cause unique qui n'a rien a voir avec les bancs.
verifier_preconditions() {
  local mauvais=0

  # -- jenga present et assez recent ------------------------------------------
  if ! command -v jenga >/dev/null 2>&1; then
    dire2 "[verif] PRECONDITION : la commande 'jenga' est introuvable."
    return 1
  fi
  local ver
  ver=$(jenga --version 2>&1 | sed 's/\x1b\[[0-9;]*m//g' \
        | grep -oE 'v?[0-9]+\.[0-9]+\.[0-9]+' | head -1 | tr -d 'v')
  if [ -z "$ver" ]; then
    dire "[precond] ATTENTION : version de jenga illisible — on continue."
  else
    local plus_petit
    plus_petit=$(printf '%s\n%s\n' "$JENGA_MIN" "$ver" | sort -V | head -1)
    if [ "$plus_petit" != "$JENGA_MIN" ]; then
      dire2 "[verif] PRECONDITION : jenga $ver < $JENGA_MIN."
      dire2 "[verif]   Avant $JENGA_MIN, 'build --project X' etait IGNORE EN SILENCE"
      dire2 "[verif]   (le drapeau est --target) : le verificateur croirait construire"
      dire2 "[verif]   un banc et construirait tout le workspace. Refus."
      return 1
    fi
    dire "[precond] ok   jenga $ver (>= $JENGA_MIN : --target honore)"
  fi
  # La copie embarquee sous Build/_jenga-embed/ peut etre plus ancienne : on ne
  # l'utilise jamais, on le dit si elle est la et qu'elle diverge.
  local emb="Build/_jenga-embed/Jenga/_version.py"
  if [ -f "$emb" ]; then
    local vemb
    vemb=$(grep -oE '__version__ *= *"[0-9.]+"' "$emb" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    if [ -n "$vemb" ] && [ "$vemb" != "$ver" ]; then
      dire "[precond] info Build/_jenga-embed/ porte jenga $vemb (plus ancien)."
      dire "[precond]      Ce script appelle la commande 'jenga' du PATH, pas cette copie."
    fi
  fi

  # -- sous-modules peuples ---------------------------------------------------
  # Mesure : chaque chemin de .gitmodules doit contenir au moins une entree.
  # Piege paye le 2026-08-22 : dans un worktree, `git submodule update --init`
  # ne fait RIEN et sort 0 ; il faut --force.
  if [ -f ".gitmodules" ]; then
    local vides=() chemin
    while IFS= read -r chemin; do
      [ -z "$chemin" ] && continue
      if [ ! -d "$chemin" ] || [ -z "$(ls -A "$chemin" 2>/dev/null)" ]; then
        vides+=("$chemin")
      fi
    done < <(grep -E '^[[:space:]]*path[[:space:]]*=' .gitmodules | sed 's/.*=[[:space:]]*//')
    if [ "${#vides[@]}" -gt 0 ]; then
      # Un sous-module vide n'est fatal que s'il est cite par le workspace.
      local cites=() c
      for c in "${vides[@]}"; do
        if grep -qF "$c" Nkentseu.jenga config/*.jenga 2>/dev/null; then cites+=("$c"); fi
      done
      for c in "${vides[@]}"; do dire "[precond] info sous-module vide : $c"; done
      if [ "${#cites[@]}" -gt 0 ]; then
        dire2 "[verif] PRECONDITION : ${#cites[@]} sous-module(s) VIDE(S) et cite(s) par le workspace :"
        for c in "${cites[@]}"; do dire2 "[verif]   - $c"; done
        dire2 "[verif]   Repare :  git submodule update --init --force"
        dire2 "[verif]   (le --force est OBLIGATOIRE dans un worktree : sans lui, la"
        dire2 "[verif]    commande ne fait RIEN et sort 0.)"
        return 1
      fi
      dire "[precond] ok   aucun sous-module vide n'est cite par le workspace"
    else
      dire "[precond] ok   sous-modules peuples"
    fi
  fi

  # -- la liste existe --------------------------------------------------------
  if [ ! -f "$LISTE" ]; then
    dire2 "[verif] PRECONDITION : $LISTE absent. Le classement des bancs est la"
    dire2 "[verif]   DONNEE de ce controle ; sans elle il n'y a rien a verifier."
    return 1
  fi
  dire "[precond] ok   $LISTE present"

  [ "$mauvais" -eq 0 ]
}

# =============================================================================
# 2. INVENTAIRE — ce qui EXISTE dans Applications/, et ce qui est CLASSE
# =============================================================================
# L'ensemble mesure ne vient d'aucune heuristique de nom : c'est la liste des
# `with project("...")` declares par les fichiers .jenga d'Applications/. Un
# projet est une donnee du systeme de build ; « finit par Test » n'en est pas une.
declare -a PROJETS=()          # noms mesures
declare -A PROJ_FICHIER=()     # projet -> son .jenga
declare -A PROJ_DANS_WS=()     # projet -> 1 si inclus (non commente) dans Nkentseu.jenga

inventorier() {
  local inc paire rel nom
  # includes NON COMMENTES du workspace : `^[^#]*` interdit qu'un « # » precede
  # sur la ligne. C'est ce qui distingue un banc actif d'un banc ARRETE, dont
  # l'include est simplement commente (cas NkMatInventaireTest, ligne ~1220).
  declare -A INCLUS=()
  while IFS= read -r inc; do
    INCLUS["$inc"]=1
  done < <(grep -oP '^[^#]*with include\("\K[^"]+' Nkentseu.jenga 2>/dev/null)

  # UN SEUL grep pour tout l'arbre (voir la note de cout sur `rogner`) :
  #   Applications/X/X.jenga:NomDuProjet
  declare -A vu=()
  while IFS= read -r paire; do
    rel="${paire%%:*}"; nom="${paire#*:}"
    [ -z "$nom" ] && continue
    [ -n "${vu[$nom]:-}" ] && continue
    vu["$nom"]=1
    PROJETS+=("$nom")
    PROJ_FICHIER["$nom"]="${rel#./}"
    if [ -n "${INCLUS[${rel#./}]:-}" ]; then PROJ_DANS_WS["$nom"]=1; else PROJ_DANS_WS["$nom"]=0; fi
  done < <(grep -rHoP '^\s*with project\("\K[^"]+' Applications --include='*.jenga' 2>/dev/null | sort)
}

# -- Controle positif d'instrument -------------------------------------------
# Un inventaire qui ne sait rien trouver ne prouve rien. Deux temoins choisis
# pour couvrir les deux formes : un banc PRESENT dans le workspace, et un banc
# ARRETE dont l'include est commente. Si le second ressort « dans le workspace »,
# c'est que le parseur ignore les commentaires — et tout le reste est faux.
TEMOIN_PRESENT="NkSLCheck"
TEMOIN_ARRETE="NkMatInventaireTest"
controle_instrument() {
  local p ok1=0 ok2=0
  for p in "${PROJETS[@]}"; do
    [ "$p" = "$TEMOIN_PRESENT" ] && ok1=1
    [ "$p" = "$TEMOIN_ARRETE"  ] && ok2=1
  done
  if [ "$ok1" -ne 1 ]; then
    dire2 "[verif] ECHEC D'INSTRUMENT : l'inventaire ne trouve pas $TEMOIN_PRESENT."
    dire2 "[verif]   Soit il a ete supprime (mets ce temoin a jour), soit le parseur"
    dire2 "[verif]   ne sait plus lire les .jenga. Ne rien conclure."
    return 1
  fi
  if [ "$ok2" -ne 1 ]; then
    dire2 "[verif] ECHEC D'INSTRUMENT : l'inventaire ne trouve pas $TEMOIN_ARRETE."
    return 1
  fi
  if [ "${PROJ_DANS_WS[$TEMOIN_ARRETE]}" -ne 0 ]; then
    dire2 "[verif] ECHEC D'INSTRUMENT : $TEMOIN_ARRETE est donne DANS le workspace."
    dire2 "[verif]   Son include est commente dans Nkentseu.jenga (ligne ~1220) : le"
    dire2 "[verif]   parseur ignore donc les commentaires, et l'inventaire est faux."
    return 1
  fi
  if [ "${PROJ_DANS_WS[$TEMOIN_PRESENT]}" -ne 1 ]; then
    dire2 "[verif] ECHEC D'INSTRUMENT : $TEMOIN_PRESENT est donne HORS du workspace."
    return 1
  fi
  dire "[instr]   ok   temoins : $TEMOIN_PRESENT dans le workspace, $TEMOIN_ARRETE hors"
  return 0
}

# =============================================================================
# 3. LECTURE DE config/bancs.list
# =============================================================================
# Format, un projet par ligne, champs separes par « | » :
#   <projet> | <classe> | <mode> | <arguments> | <verdict> | <note>
declare -A CL_CLASSE=() CL_MODE=() CL_ARGS=() CL_VERDICT=() CL_NOTE=()
declare -a CL_ORDRE=()

# Rognage en bash pur, SANS sous-processus. Mesure du 2026-08-22 : six appels a
# `sed` par ligne sur 121 lignes coutaient 36 s a eux seuls sous Windows (le
# demarrage d'un processus y vaut ~50 ms). L'exigence 6 — « il doit etre
# lancable » — se perd dans ce genre de detail, pas dans les grands choix.
# ROGNE ecrit dans la variable globale ROGNE : ni `sed`, ni substitution de
# commande — les deux forkent, et un fork coute ~50 ms sous Windows.
ROGNE=""
rogner() {
  local s="${1:-}"
  s="${s#"${s%%[![:space:]]*}"}"
  s="${s%"${s##*[![:space:]]}"}"
  ROGNE="$s"
}

lire_liste() {
  local ligne nom classe mode args verdict note num=0
  while IFS= read -r ligne; do
    num=$((num + 1))
    case "$ligne" in ''|\#*) continue ;; esac
    IFS='|' read -r nom classe mode args verdict note <<< "$ligne"
    rogner "${nom:-}";     nom="$ROGNE"
    rogner "${classe:-}";  classe="$ROGNE"
    rogner "${mode:-}";    mode="$ROGNE"
    rogner "${args:-}";    args="$ROGNE"
    rogner "${verdict:-}"; verdict="$ROGNE"
    rogner "${note:-}";    note="$ROGNE"
    [ -z "$nom" ] && continue
    if [ -n "${CL_CLASSE[$nom]:-}" ]; then
      dire2 "[verif] $LISTE ligne $num : $nom declare deux fois."
      return 1
    fi
    case "$classe" in
      banc|pas-banc|banc-arrete) ;;
      *) dire2 "[verif] $LISTE ligne $num : classe inconnue « $classe » pour $nom"
         dire2 "[verif]   attendu : banc | pas-banc | banc-arrete"
         return 1 ;;
    esac
    CL_CLASSE["$nom"]="$classe"; CL_MODE["$nom"]="${mode:-complet}"
    CL_ARGS["$nom"]="$args";     CL_VERDICT["$nom"]="${verdict:-code}"
    CL_NOTE["$nom"]="$note";     CL_ORDRE+=("$nom")
  done < "$LISTE"
  return 0
}

# =============================================================================
# 4. LA GARDE (exigence 2) — mesure contre classement
# =============================================================================
garde_liste() {
  local p non_classes=() orphelines=()
  for p in "${PROJETS[@]}"; do
    [ -z "${CL_CLASSE[$p]:-}" ] && non_classes+=("$p")
  done
  local n
  for n in "${CL_ORDRE[@]}"; do
    local trouve=0 q
    for q in "${PROJETS[@]}"; do [ "$q" = "$n" ] && trouve=1 && break; done
    [ "$trouve" -eq 0 ] && orphelines+=("$n")
  done

  dire "[garde]   ${#PROJETS[@]} projet(s) declare(s) sous Applications/, ${#CL_ORDRE[@]} ligne(s) de classement"

  if [ "${#orphelines[@]}" -gt 0 ]; then
    dire "[garde]   info ${#orphelines[@]} ligne(s) sans projet dans CET arbre (branche en avance,"
    dire "[garde]        ou projet supprime) — information, jamais un echec :"
    for n in "${orphelines[@]}"; do dire "[garde]        - $n"; done
  fi

  if [ "${#non_classes[@]}" -gt 0 ]; then
    dire2 ""
    dire2 "[garde]   ECHEC : ${#non_classes[@]} projet(s) d'Applications/ NON CLASSE(S) :"
    for n in "${non_classes[@]}"; do
      dire2 "[garde]     - $n   (${PROJ_FICHIER[$n]})"
    done
    dire2 "[garde]   Ajoute une ligne dans $LISTE pour chacun. Dire « ce n'est pas un"
    dire2 "[garde]   banc » est une reponse valide — ne rien dire n'en est pas une."
    dire2 "[garde]   C'est la raison d'etre de ce controle : un banc neuf qu'on"
    dire2 "[garde]   n'aurait pas pense a declarer ne peut pas passer inapercu."
    return 1
  fi
  dire "[garde]   ok   aucun projet non classe"
  return 0
}

# =============================================================================
# 5. CONSTRUCTION ET EXECUTION D'UN BANC
# =============================================================================
premiere_ligne_utile() {
  # $1 = journal, $2 = « build » ou « run »
  local j="$1" quoi="$2" l=""
  if [ "$quoi" = "build" ]; then
    l=$(sed 's/\x1b\[[0-9;]*m//g' "$j" 2>/dev/null \
        | grep -aiE 'error|erreur|fatal|undefined reference|unresolved external|LNK[0-9]+|C[0-9]{4}' \
        | grep -avE 'Build Successful|0 error' | head -1)
  else
    # ⚠️ DEUX MESURES ONT ECRIT CE BLOC. Il choisit UNE ligne pour dire pourquoi
    # un banc est tombe ; se tromper de ligne, c'est envoyer chercher au mauvais
    # endroit — donc pire que se taire.
    #
    # MESURE 1 (22/08 matin) — la CAUSE precede le SYMPTOME. Sur un banc dont la
    # fixture manque, le journal porte d'abord « fichier introuvable :
    # .../CesiumMan.fbx », PUIS « [FAIL] LoadFBX reussit ». Citer le [FAIL] envoie
    # chercher un bug de chargeur ; citer l'introuvable envoie chercher le fichier.
    #
    # MESURE 2 (22/08 soir) — et le motif large a MENTI, dans les deux sens :
    #   a) il a cite « [OK] NkOBJIO::Import: aucune erreur » comme cause d'un
    #      echec — le mot `erreur` accroche A L'INTERIEUR DE SA PROPRE NEGATION ;
    #   b) une fois (a) corrige, il a cite un WRN de texture situe 180 lignes
    #      avant le vrai [FAIL], sans aucun rapport avec lui.
    # Chercher un MOT au lieu d'un ETAT : la faute meme que cet outil existe pour
    # interdire, venue se loger dedans. Deux fois.
    #
    # LA REGLE QUI TIENT LES DEUX : on s'ancre sur le MARQUEUR D'ECHEC (une
    # donnee, pas un mot), et on ne remonte a une ligne de cause que si elle est
    # ADJACENTE (5 lignes avant, au plus). Adjacente = elle parle du meme echec.
    # Lointaine = c'est du bruit qui passait par la.
    l=$(sed 's/\x1b\[[0-9;]*m//g' "$j" 2>/dev/null \
        | grep -avE '\[[[:space:]]*OK[[:space:]]*\]|aucune erreur|aucun echec|sans erreur|no error|0 error' \
        | awk '
            { buf[NR] = $0 }
            !prem && tolower($0) ~ /\[fail\]|\[err\]|\[echec\]|\<ko\>|assert|abort|exception/ { prem = NR }
            END {
              if (prem) {
                deb = prem - 5; if (deb < 1) deb = 1
                for (k = deb; k < prem; k++)
                  if (tolower(buf[k]) ~ /introuvable|not found|no such file|manquant|missing/) {
                    # On rend LES DEUX : l echec fait foi, la cause adjacente
                    # l accompagne. Choisir entre les deux, c est parier ; les
                    # donner toutes les deux, c est renseigner.
                    print buf[prem] "   || cause adjacente : " buf[k]; exit
                  }
                print buf[prem]; exit
              }
            }')
    # Aucun marqueur d echec : le banc est tombe sans le dire (code non nul, sortie
    # muette). On retombe alors sur le motif large — faute de mieux, et c est ecrit.
    if [ -z "$l" ]; then
      l=$(sed 's/\x1b\[[0-9;]*m//g' "$j" 2>/dev/null \
          | grep -avE '\[[[:space:]]*OK[[:space:]]*\]|aucune erreur|aucun echec|sans erreur|no error|0 error' \
          | grep -aiE 'error|erreur|fatal|introuvable|not found|no such file|manquant|missing' \
          | head -1)
    fi
  fi
  [ -z "$l" ] && l=$(sed 's/\x1b\[[0-9;]*m//g' "$j" 2>/dev/null | grep -av '^[[:space:]]*$' | tail -1)
  [ -z "$l" ] && l="(sortie vide)"
  printf '%s' "$l" | cut -c1-320
}

# =============================================================================
# 5bis. SITUER UN ECHEC — nommer les branches non fusionnees qui le touchent
# =============================================================================
# LA MESURE QUI A FAIT ECRIRE CE BLOC (2026-08-22)
#
#   Ce verificateur ne mesure QU'UNE reference : celle sur laquelle il tourne.
#   Le 22/08 il a rapporte trois bancs rouges sur `main`. Les trois etaient des
#   ATTENTES PERIMEES DU BANC, et les corrections existaient deja — dans des
#   branches non fusionnees. Vingt-quatre heures plus tard, `main` a jour :
#   ZERO rouge. Le verificateur n'avait pas menti sur ce qu'il mesurait ; il
#   avait laisse croire qu'il mesurait « le code », alors qu'il mesurait « le
#   code ICI ».
#
#   ⚠️ Et le rapport lui-meme a paye la meme faute : `feat/verificateur` etait
#   4 commits derriere `main` — la premiere passe mesurait une PHOTO DE MAIN
#   prise le matin, pas `main`.
#
# CE QUE CE BLOC CHANGE
#   Un outil qui dit « NkAssetIODemo est rouge » ACCUSE.
#   Un outil qui dit « NkAssetIODemo est rouge, et cinq branches non fusionnees
#   touchent les fichiers accuses » SITUE. C'est toute la difference entre
#   envoyer quelqu'un chercher un bug et lui dire ou regarder d'abord.
#
#   ⚠️ ET L'ABSENCE COMPTE AUTANT QUE LA PRESENCE. « Aucune branche non fusionnee
#   ne touche ces fichiers » est l'information la PLUS utile des deux : elle dit
#   que le defaut est bien ici, et qu'il ne faut pas perdre une heure a fusionner
#   avant de chercher. Un outil qui ne parlerait que quand il trouve laisserait
#   le silence signifier deux choses a la fois.
#
# CE QU'IL NE FAIT PAS : il ne dit pas que la branche CORRIGE l'echec. Il dit
# qu'elle TOUCHE les fichiers accuses. Le premier serait un jugement, et il
# serait faux une fois sur deux ; le second est un fait, et il se verifie.

BR_CALCULE=0
declare -A BR_FICHIERS=()    # branche -> fichiers qu'elle touche ET qui different encore
declare -A BR_INEDITS=()     # branche -> nb de ses commits sans equivalent sur HEAD
INDEX_SUIVIS=""

calculer_branches() {
  [ "$BR_CALCULE" -eq 1 ] && return 0
  BR_CALCULE=1
  local b mb
  # Cout mesure le 2026-08-22 : ~10 s pour 16 branches. Calcule UNE SEULE FOIS,
  # et UNIQUEMENT si un banc est tombe : une passe verte ne paie rien.
  while IFS= read -r b; do
    [ -z "$b" ] && continue
    mb=$(git merge-base HEAD "$b" 2>/dev/null) || continue
    [ -z "$mb" ] && continue
    # DEUX conditions, et pas une :
    #   - le fichier a bouge SUR la branche depuis la base commune ;
    #   - et il DIFFERE ENCORE de ce qu'on mesure aujourd'hui.
    # Sans la seconde, on nommerait des branches dont la modification est deja
    # dans HEAD : l'outil enverrait chercher une correction qui est sous ses yeux.
    BR_FICHIERS["$b"]="$(comm -12 \
        <(git diff --name-only "$mb" "$b" 2>/dev/null | sort) \
        <(git diff --name-only HEAD "$b" 2>/dev/null | sort))"
    # Commits de la branche dont AUCUN equivalent (patch-id) n'est sur HEAD.
    BR_INEDITS["$b"]=$(git cherry HEAD "$b" 2>/dev/null | grep -ac '^+')
  done < <(git branch --no-merged HEAD --format='%(refname:short)' 2>/dev/null)
  INDEX_SUIVIS="$(git ls-files 2>/dev/null)"
  return 0
}

# Les fichiers ACCUSES par un echec : le dossier du banc, plus tout fichier
# source dont le NOM apparait dans la ligne de cause. `NkFBXImporter::Import`
# donne le jeton `NkFBXImporter`, qui donne `.../NkFBXImporter.cpp` s'il existe.
# On ne devine pas : un jeton ne devient un chemin que si git suit reellement un
# fichier de ce nom.
chemins_accuses() {
  local banc="$1" cause="$2" jeton res nd
  printf '%s
' "Applications/$banc/"
  printf '%s
' "$cause"     | grep -aoE '[A-Za-z_][A-Za-z0-9_]{3,}'     | sort -u     | while IFS= read -r jeton; do
        res=$(printf '%s
' "$INDEX_SUIVIS" | grep -aE "(^|/)${jeton}\.(h|hpp|cpp|inl|c)$")
        [ -z "$res" ] && continue
        # ⚠️ UN NOM QUI DESIGNE CENT FICHIERS NE DESIGNE RIEN.
        # Defaut mesure le 2026-08-22, dans cette fonction meme : la ligne de
        # cause disait « Compilation Error: main.cpp ». Le jeton `main` resout
        # vers ~100 fichiers, et l'outil a nomme SIX branches sur des `main.cpp`
        # totalement etrangers au banc tombe. C'est exactement ce que cette
        # fonction existe pour empecher — un outil qui ACCUSE au lieu de SITUER —
        # venu se loger dans la fonction qui porte l'exigence.
        #
        # La regle qui tient : un jeton ne designe une unite de code que si tous
        # ses fichiers vivent dans LE MEME dossier. `NkGLTFLoader` -> le .h et le
        # .cpp d'un meme module : une unite. `main` -> cent dossiers : rien.
        nd=$(printf '%s
' "$res" | sed 's#/[^/]*$##' | sort -u | grep -c '')
        [ "$nd" -gt 1 ] && continue
        printf '%s
' "$res"
      done
}

situer_echec() {
  local banc="$1" cause="$2"
  calculer_branches
  local accuses b liste inter
  accuses="$(chemins_accuses "$banc" "$cause" | grep -av '^[[:space:]]*$' | sort -u)"
  # ⚠️ COMBIEN DE FICHIERS DE CODE la ligne de cause a-t-elle vraiment livres ?
  # `chemins_accuses` rend TOUJOURS « Applications/<banc>/ ». Si c'est tout ce
  # qu'elle rend, l'outil n'a RIEN a chercher -- et son silence n'est pas une
  # information sur le depot, c'est une information sur lui-meme.
  local nb_code
  nb_code="$(printf '%s
' "$accuses" | grep -acE '\.(h|hpp|cpp|inl|c)$')"
  if [ -z "$accuses" ]; then
    dire "    (aucun chemin accuse identifiable dans la ligne de cause)"
    return 0
  fi
  # « inedits d'abord » : une branche dont tous les commits ont deja un
  # equivalent sur HEAD est la moins susceptible de porter la correction.
  local -a avec=() sans=()
  for b in "${!BR_FICHIERS[@]}"; do
    liste="${BR_FICHIERS[$b]}"
    [ -z "$liste" ] && continue
    inter="$(printf '%s\n' "$liste" | grep -aFf <(printf '%s\n' "$accuses") | head -4)"
    [ -z "$inter" ] && continue
    if [ "${BR_INEDITS[$b]:-0}" -gt 0 ]; then avec+=("$b|$inter"); else sans+=("$b|$inter"); fi
  done

  local n=$(( ${#avec[@]} + ${#sans[@]} ))
  if [ "$n" -eq 0 ]; then
    # ⚠️ DEUX SILENCES DIFFERENTS, ET LES CONFONDRE A COUTE UNE FAUSSE CERTITUDE
    # LE 2026-08-24. NkMsaaVulkanCheck est tombe avec la ligne de cause
    # « [FAIL] 3 echantillons : le contrat REFUSE, la carte ACCEPTE. » — pas un
    # seul nom de fichier dedans. `chemins_accuses` n'a donc rendu que le
    # dossier du banc, aucune branche ne le touche, et l'outil a repondu
    # « AUCUNE ... le defaut est bien sur CETTE reference. Ne perds pas une
    # heure a fusionner. »
    # C'ETAIT FAUX : feat/rendu-temps-reel portait deja, dans 9b774ab8, la
    # requete reelle de framebufferColorSampleCounts sur les trois backends qui
    # ecrivaient `mCaps.msaa2x = msaa4x = msaa8x = true` en dur -- soit la moitie
    # exacte de ce que le banc accusait.
    # « Un outil qui situe peut accuser plus fort qu'un outil qui se tait » :
    # ici il a RASSURE plus fort qu'un outil qui se tait, ce qui est le meme
    # defaut retourne. Il ne peut pas dire « le defaut est bien ici » quand il
    # n'avait aucun fichier a confronter.
    if [ "${nb_code:-0}" -eq 0 ]; then
      dire "    branches non fusionnees : RIEN CHERCHE, et ce n'est pas AUCUNE."
      dire "      La ligne de cause de ce banc ne nomme AUCUN fichier de code ; il n'y"
      dire "      avait donc rien a confronter aux ${#BR_FICHIERS[@]} branche(s)."
      dire "      ⚠️ Ne lis pas ce silence comme « le defaut est sur cette reference » :"
      dire "      c'est un silence sur MON montage, pas sur le depot. Si le banc peut"
      dire "      citer le fichier qu'il met en cause dans sa sortie, il le doit."
      return 0
    fi
    dire "    branches non fusionnees touchant les fichiers accuses : AUCUNE"
    dire "      (${#BR_FICHIERS[@]} branche(s) examinee(s), $nb_code fichier(s) de code"
    dire "      confronte(s)) — le defaut est bien sur CETTE reference."
    dire "      Ne perds pas une heure a fusionner avant de chercher."
    return 0
  fi

  dire "    ⚠️ $n branche(s) non fusionnee(s) touchent les fichiers accuses"
  dire "       ET en different encore aujourd'hui :"
  local t nom fics f
  for t in "${avec[@]}" "${sans[@]}"; do
    [ -z "$t" ] && continue
    nom="${t%%|*}"; fics="${t#*|}"
    if [ "${BR_INEDITS[$nom]:-0}" -gt 0 ]; then
      dire "       $nom   (${BR_INEDITS[$nom]} commit(s) inedit(s))"
    else
      dire "       $nom   (0 inedit : contenu deja sur HEAD, regarde-la en dernier)"
    fi
    printf '%s\n' "$fics" | while IFS= read -r f; do [ -n "$f" ] && dire "         $f"; done
  done
  dire "    Ce verificateur ne mesure QU'UNE reference. Fusionne d'abord, remesure"
  dire "    ensuite : il tient a etre rouge pour une correction qui existe deja."
  dire "    ⚠️ « commit(s) inedit(s) » se compte par patch-id : une fusion ECRASEE"
  dire "    (squash) en fait surcompter, jamais sous-compter. C'est un indice de"
  dire "    lecture, pas un verdict — l'outil situe, il ne tranche pas."
}

chemin_exe() {
  local nom="$1" c
  for c in "Build/Bin/$CONFIG-Windows/$nom/$nom.exe" \
           "Build/Bin/$CONFIG-Windows/$nom/$nom" \
           "Build/Bin/$CONFIG-Linux/$nom/$nom" \
           "Build/Bin/$CONFIG-macOS/$nom/$nom"; do
    [ -f "$c" ] && { printf '%s' "$c"; return 0; }
  done
  return 1
}

# RAISON D'UN IGNORE — on la LIT dans la sortie du banc, on ne l'invente pas.
# Un banc qui rend son code « rien mesure » DOIT dire pourquoi : « pas de carte »
# et « la carte refuse la texture la plus banale » n'appellent pas la meme suite.
# S'il ne l'a pas dit, on ecrit qu'il ne l'a pas dit — jamais une raison plausible
# a sa place. Une supposition consignee comme mesure se propage avec l'autorite
# d'une mesure.
raison_ignore() {
  local jr="$1" code="$2" l=""
  if [ -f "$jr" ]; then
    l=$(grep -aF '[IGNORE]' "$jr" | head -1 | sed 's/^[[:space:]]*//' | cut -c1-200)
  fi
  if [ -n "$l" ]; then
    printf '%s' "$l"
  else
    printf 'code %s (IGNORE declare dans %s) — mais le banc N A PAS DIT pourquoi : aucune ligne [IGNORE] dans sa sortie' "$code" "$LISTE"
  fi
}

# Resultats
declare -a R_NOM=() R_CONSTRUIT=() R_CODE=() R_DUREE=() R_VERDICT=() R_POURQUOI=() R_JOURNAL=()

traiter_banc() {
  local nom="$1"
  local args="${CL_ARGS[$nom]}" crit="${CL_VERDICT[$nom]}"
  local jb="$SORTIE/$nom.build.log" jr="$SORTIE/$nom.run.log"
  local t_debut=$(date +%s) construit="non" code="-" verdict="" pourquoi="" journal="-"

  # ---- CONSTRUCTION (exigence 1) -------------------------------------------
  if [ "$SANS_CONSTRUIRE" -eq 1 ]; then
    construit="saute"
  else
    if [ "$RECONSTRUIRE" -eq 1 ]; then
      jenga rebuild --target "$nom" --config "$CONFIG" > "$jb" 2>&1
    else
      jenga build --target "$nom" --config "$CONFIG" > "$jb" 2>&1
    fi
    local rcb=$?
    if [ "$rcb" -ne 0 ]; then
      # ECHEC DUR. On n'execute PAS : un exe present serait l'ANCIEN, et le
      # faire tourner produirait exactement le faux « survivant » qui a motive
      # ce chantier.
      R_NOM+=("$nom"); R_CONSTRUIT+=("NON"); R_CODE+=("-")
      R_DUREE+=("$(( $(date +%s) - t_debut ))s"); R_VERDICT+=("ECHEC")
      R_POURQUOI+=("construction echouee (jenga code $rcb) : $(premiere_ligne_utile "$jb" build)")
      R_JOURNAL+=("$jb")
      return 1
    fi
    construit="oui"
  fi

  # ---- L'EXE DOIT EXISTER ---------------------------------------------------
  local exe
  if ! exe=$(chemin_exe "$nom"); then
    R_NOM+=("$nom"); R_CONSTRUIT+=("$construit"); R_CODE+=("-")
    R_DUREE+=("$(( $(date +%s) - t_debut ))s"); R_VERDICT+=("ECHEC")
    R_POURQUOI+=("construction annoncee reussie mais aucun executable sous Build/Bin/$CONFIG-*/$nom/")
    R_JOURNAL+=("$jb")
    return 1
  fi

  # ---- EXECUTION ------------------------------------------------------------
  local t_run=$(date +%s)
  if [ -n "$args" ]; then
    # shellcheck disable=SC2086
    timeout "$DELAI_DEFAUT" "./$exe" $args > "$jr" 2>&1
  else
    timeout "$DELAI_DEFAUT" "./$exe" > "$jr" 2>&1
  fi
  code=$?
  local dt=$(( $(date +%s) - t_run ))
  journal="$jr"

  if [ "$code" -eq 124 ]; then
    verdict="ECHEC"; pourquoi="delai depasse (> ${DELAI_DEFAUT}s) — banc tue"
  else
    # ---- CRITERE DE VERDICT (exigence 5, et la lecon « un repli qui preserve
    #      success n'est pas un repli ») ------------------------------------
    case "$crit" in
      sans-verdict)
        verdict="INDETERMINE"
        pourquoi="aucun critere declare : ce banc ne sait pas dire s'il va bien (code $code)"
        ;;
      code+absence:*)
        local motif="${crit#code+absence:}"
        if [ "$code" -ne 0 ]; then
          verdict="ECHEC"; pourquoi="code de sortie $code : $(premiere_ligne_utile "$jr" run)"
        elif grep -aqE "$motif" "$jr"; then
          verdict="ECHEC"
          pourquoi="motif interdit trouve (/$motif/) : $(grep -aE "$motif" "$jr" | head -1 | cut -c1-200)"
        else
          verdict="OK"
        fi
        ;;
      code+presence:*)
        local motif="${crit#code+presence:}"
        if [ "$code" -ne 0 ]; then
          verdict="ECHEC"; pourquoi="code de sortie $code : $(premiere_ligne_utile "$jr" run)"
        elif ! grep -aqE "$motif" "$jr"; then
          verdict="ECHEC"
          pourquoi="motif de succes attendu ABSENT (/$motif/) — le banc n'a pas dit qu'il avait reussi"
        else
          verdict="OK"
        fi
        ;;
      code+ignore:*)
        # TROIS ETATS. Le banc exige quelque chose que la machine peut ne pas
        # avoir (un GPU, un pilote). Son code « rien mesure » est une DONNEE de
        # sa ligne dans la liste, pas une constante devinee ici : sans cela, le
        # banc et le verificateur auraient un accord tacite que rien ne garde.
        local nign="${crit#code+ignore:}"
        case "$nign" in
          ''|*[!0-9]*)
            verdict="ECHEC"
            pourquoi="critere « $crit » malforme dans $LISTE : « $nign » n'est pas un entier"
            ;;
          0)
            # ⚠️ Refus DELIBERE. « code+ignore:0 » ferait compter IGNORE tout
            # banc qui reussit — chaque vert deviendrait un « rien mesure », ou
            # l'inverse selon l'ordre des tests. Une donnee qui detruit la
            # distinction que le critere existe pour porter n'est pas acceptable.
            verdict="ECHEC"
            pourquoi="critere « $crit » interdit : 0 est deja le code du succes, il ne peut pas etre celui d'IGNORE"
            ;;
          *)
            if [ "$code" -eq "$nign" ]; then
              # NI OK, NI ECHEC. Le banc n'a rien mesure, et le dire est tout
              # l'interet : un banc ignore compte vert est le mensonge que ce
              # chantier traque — on croit la question reglee.
              verdict="IGNORE"; pourquoi="$(raison_ignore "$jr" "$code")"
            elif [ "$code" -ne 0 ]; then
              verdict="ECHEC"; pourquoi="code de sortie $code : $(premiere_ligne_utile "$jr" run)"
            else
              verdict="OK"
            fi
            ;;
        esac
        ;;
      code|"")
        if [ "$code" -ne 0 ]; then
          verdict="ECHEC"; pourquoi="code de sortie $code : $(premiere_ligne_utile "$jr" run)"
        else
          verdict="OK"
        fi
        ;;
      *)
        verdict="ECHEC"; pourquoi="critere de verdict inconnu dans $LISTE : « $crit »"
        ;;
    esac
  fi

  R_NOM+=("$nom"); R_CONSTRUIT+=("$construit"); R_CODE+=("$code")
  R_DUREE+=("${dt}s"); R_VERDICT+=("$verdict"); R_POURQUOI+=("$pourquoi"); R_JOURNAL+=("$journal")
  [ "$verdict" = "ECHEC" ] && return 1
  return 0
}

# =============================================================================
# DEROULE
# =============================================================================
dire "======================================================================"
dire " VERIFICATEUR DE BANCS — Nkentseu"
dire " arbre   : $ROOT"
dire " branche : $(git rev-parse --abbrev-ref HEAD 2>/dev/null)   HEAD $(git rev-parse --short HEAD 2>/dev/null)"
# ⚠️ EN HAUT, PAS EN NOTE. Le 22/08 au soir, `feat/verificateur` etait 4 commits
# derriere `main` : la passe mesurait une PHOTO de main prise le matin, en
# croyant mesurer main. Le lendemain, sur main a jour, DEUX des rouges rapportes
# avaient disparu — ils etaient corriges depuis la veille. Un verificateur qui
# ne dit pas de quand date sa reference laisse lire ses rouges comme des faits
# sur le code.
RETARD=$(git rev-list --count HEAD..main 2>/dev/null || printf '?')
if [ "$RETARD" != "0" ] && [ "$RETARD" != "?" ] && [ -n "$RETARD" ]; then
  dire " ⚠️ RETARD  : $RETARD commit(s) derriere main. Ce que tu vas lire mesure CETTE"
  dire "             photo, pas main. Fusionne avant de conclure quoi que ce soit."
elif [ "$RETARD" = "0" ]; then
  dire " a jour   : 0 commit derriere main"
fi
dire " mode    : $MODE        config : $CONFIG"
dire " debut   : $(horodate)"
dire "======================================================================"
dire ""
dire "-- Preconditions -----------------------------------------------------"
verifier_preconditions || { dire2 ""; dire2 "[verif] ECHEC D'INSTRUMENT — rien n'est conclu sur le depot."; exit 2; }

dire ""
dire "-- Inventaire et garde de la liste -----------------------------------"
inventorier
controle_instrument || { dire2 ""; dire2 "[verif] ECHEC D'INSTRUMENT — rien n'est conclu sur le depot."; exit 2; }
lire_liste || exit 2

RC=0
garde_liste || RC=3

# =============================================================================
# 4bis. LA GARDE DES CAPACITES ANNONCEES
# =============================================================================
# MEME MECANISME QUE config/bancs.list, PAS UN SECOND (arbitrage de Rodolf,
# R3-b). Une capacite creuse detectee et non classee ROUGIT LE VERIFICATEUR.
#
# ELLE NE REFUSE PAS LE COMMIT, ET C'EST DELIBERE. Un controle qui bloque le
# commit se contourne par --no-verify, en silence, par quelqu'un de presse — et
# personne ne sait jamais qu'il a ete contourne. Un banc rouge, lui, reste rouge
# et SE VOIT. On echange un blocage qu'on peut faire taire contre un signal
# qu'on ne peut pas.
#
# ELLE N'EST PAS DANS --liste NON PLUS, et pour la meme raison, mesuree : elle
# coute ~10 s. --liste est ce que gitcommit.sh appelle a chaque commit (1 a 4 s
# aujourd'hui). Y ajouter 10 s, c'est fabriquer la raison de contourner le hook
# — exactement le probleme que R3-b demande de ne pas reconstruire.
# Elle vit donc dans la PASSE, et se lance a la demande :
#     ./verif_bancs.sh --capacites     (la garde des capacites SEULE, ~10 s)
CAP_RC=0
lancer_garde_capacites() {
  if [ ! -f ./verif_capacites.sh ]; then
    dire "[cap]     info verif_capacites.sh absent — garde des capacites non lancee."
    return 0
  fi
  dire ""
  dire "-- Garde des capacites annoncees -------------------------------------"
  bash ./verif_capacites.sh
  CAP_RC=$?
  case "$CAP_RC" in
    0) return 0 ;;
    2) dire2 "[cap]     ECHEC D'INSTRUMENT du detecteur de capacites — rien n'est"
       dire2 "[cap]     conclu sur les capacites. Le reste de cette passe tient."
       return 0 ;;
    *) return 1 ;;
  esac
}

if [ "$CAPACITES_SEULES" -eq 1 ]; then
  lancer_garde_capacites || RC=1
  dire ""
  dire " duree : $(duree_depuis "$T0")"
  exit "$RC"
fi

# --- Selection des bancs ------------------------------------------------------
declare -a A_LANCER=() ARRETES=() HORS_WS=()
for n in "${CL_ORDRE[@]}"; do
  [ -n "$UN_BANC" ] && [ "$n" != "$UN_BANC" ] && continue
  case "${CL_CLASSE[$n]}" in
    pas-banc)    continue ;;
    banc-arrete) ARRETES+=("$n"); continue ;;
  esac
  # un banc declare doit exister dans cet arbre
  local_trouve=0
  for q in "${PROJETS[@]}"; do [ "$q" = "$n" ] && local_trouve=1 && break; done
  [ "$local_trouve" -eq 0 ] && continue
  if [ "${PROJ_DANS_WS[$n]}" -ne 1 ]; then HORS_WS+=("$n"); continue; fi
  if [ "$MODE" = "rapide" ] && [ "${CL_MODE[$n]}" != "rapide" ]; then continue; fi
  A_LANCER+=("$n")
done

if [ -n "$UN_BANC" ] && [ "${#A_LANCER[@]}" -eq 0 ] && [ "${#ARRETES[@]}" -eq 0 ] && [ "${#HORS_WS[@]}" -eq 0 ]; then
  dire2 "[verif] --banc $UN_BANC : aucun banc de ce nom dans $LISTE"
  exit 2
fi

# --- Bancs arretes : NOMMES a chaque passage, jamais tus ----------------------
if [ "${#ARRETES[@]}" -gt 0 ]; then
  dire ""
  dire "-- Bancs arretes (declares, NON construits) --------------------------"
  for n in "${ARRETES[@]}"; do
    dire "  $n — ${CL_NOTE[$n]}"
  done
  dire "  Un banc arrete qu'on ne mentionne plus est un banc oublie."
fi
if [ "${#HORS_WS[@]}" -gt 0 ]; then
  dire ""
  dire "-- ECHEC : declares « banc » mais HORS du workspace -------------------"
  for n in "${HORS_WS[@]}"; do
    dire "  $n — aucun include actif dans Nkentseu.jenga (${PROJ_FICHIER[$n]})"
  done
  dire "  Soit tu le reintegres, soit tu le passes en classe « banc-arrete »"
  dire "  avec sa raison. Un banc qu'on croit lancer et qui n'existe pas dans"
  dire "  le workspace est le pire des deux mondes."
  RC=1
fi

if [ "$INVENTAIRE_SEUL" -eq 1 ]; then
  dire ""
  dire "-- Inventaire seul (--liste) : rien n'a ete construit -----------------"
  dire "  bancs a lancer en mode $MODE : ${#A_LANCER[@]}"
  for n in "${A_LANCER[@]}"; do
    printf '    %-24s %-9s verdict=%-13s args=%s\n' \
      "$n" "[${CL_MODE[$n]}]" "${CL_VERDICT[$n]}" "${CL_ARGS[$n]:-(aucun)}"
  done
  dire ""
  dire " duree : $(duree_depuis "$T0")"
  exit "$RC"
fi

mkdir -p "$SORTIE"

dire ""
dire "-- Bancs -------------------------------------------------------------"
for n in "${A_LANCER[@]}"; do
  printf '  %-26s ... ' "$n"
  if traiter_banc "$n"; then
    dire "${R_VERDICT[$(( ${#R_VERDICT[@]} - 1 ))]} (${R_DUREE[$(( ${#R_DUREE[@]} - 1 ))]})"
  else
    dire "${R_VERDICT[$(( ${#R_VERDICT[@]} - 1 ))]} (${R_DUREE[$(( ${#R_DUREE[@]} - 1 ))]})"
  fi
done

if [ "$AVEC_CAPACITES" -eq 1 ]; then
  lancer_garde_capacites || { [ "$RC" -lt 1 ] && RC=1; }
fi

# =============================================================================
# RAPPORT
# =============================================================================
dire ""
dire "-- Tableau -----------------------------------------------------------"
printf '  %-26s %-10s %-6s %-8s %s\n' "banc" "construit" "code" "duree" "verdict"
printf '  %-26s %-10s %-6s %-8s %s\n' "--------------------------" "---------" "-----" "-------" "-------"
NB_OK=0; NB_ECHEC=0; NB_IND=0; NB_IGNORE=0
for i in "${!R_NOM[@]}"; do
  printf '  %-26s %-10s %-6s %-8s %s\n' \
    "${R_NOM[$i]}" "${R_CONSTRUIT[$i]}" "${R_CODE[$i]}" "${R_DUREE[$i]}" "${R_VERDICT[$i]}"
  case "${R_VERDICT[$i]}" in
    OK)          NB_OK=$((NB_OK + 1)) ;;
    ECHEC)       NB_ECHEC=$((NB_ECHEC + 1)) ;;
    INDETERMINE) NB_IND=$((NB_IND + 1)) ;;
    IGNORE)      NB_IGNORE=$((NB_IGNORE + 1)) ;;
  esac
done

if [ "$NB_ECHEC" -gt 0 ]; then
  dire ""
  dire "-- POURQUOI (exigence 5) ---------------------------------------------"
  for i in "${!R_NOM[@]}"; do
    [ "${R_VERDICT[$i]}" != "ECHEC" ] && continue
    dire "  ${R_NOM[$i]} :"
    dire "    ${R_POURQUOI[$i]}"
    dire "    journal complet : ${R_JOURNAL[$i]}"
    situer_echec "${R_NOM[$i]}" "${R_POURQUOI[$i]}"
  done
fi

if [ "$NB_IGNORE" -gt 0 ]; then
  dire ""
  dire "-- IGNORES : ces bancs N ONT RIEN MESURE -----------------------------"
  for i in "${!R_NOM[@]}"; do
    [ "${R_VERDICT[$i]}" != "IGNORE" ] && continue
    dire "  ${R_NOM[$i]} : ${R_POURQUOI[$i]}"
    dire "    journal complet : ${R_JOURNAL[$i]}"
  done
  dire "  Ni succes, ni echec. Ce qu ils verifient n est NI confirme NI infirme"
  dire "  sur cette machine. Les compter verts ferait croire la question reglee :"
  dire "  c est exactement le mensonge que ces bancs existent pour traquer."
fi

if [ "$NB_IND" -gt 0 ]; then
  dire ""
  dire "-- Indetermines (a doter d'un critere de verdict) --------------------"
  for i in "${!R_NOM[@]}"; do
    [ "${R_VERDICT[$i]}" != "INDETERMINE" ] && continue
    dire "  ${R_NOM[$i]} : ${R_POURQUOI[$i]}"
  done
  dire "  Ces bancs tournent, mais leur code de sortie ne juge rien. Ils ne"
  dire "  sont PAS comptes verts. Donne-leur un critere dans $LISTE."
fi

[ "$NB_ECHEC" -gt 0 ] && [ "$RC" -lt 1 ] && RC=1

dire ""
dire "======================================================================"
if [ "$RC" -eq 0 ] && [ "$NB_ECHEC" -eq 0 ]; then
  dire " VERDICT : OK — $NB_OK banc(s) au vert, $NB_IGNORE ignore(s), $NB_IND indetermine(s)"
else
  dire " VERDICT : ECHEC — $NB_ECHEC banc(s) en echec, $NB_OK au vert, $NB_IGNORE ignore(s), $NB_IND indetermine(s)"
fi
dire " mode $MODE : ${#A_LANCER[@]} banc(s) lance(s) sur ${#CL_ORDRE[@]} projet(s) classe(s)"
if [ "$NB_IGNORE" -gt 0 ]; then
  # ⚠️ La phrase la plus importante de ce rapport quand elle s affiche : sans
  # elle, « VERDICT : OK » se lit comme « tout est verifie », alors qu une part
  # des bancs n a pas pose sa question. Un banc ignore qui rapporte vert est le
  # mensonge que ce chantier traque.
  dire " ⚠️ $NB_IGNORE banc(s) IGNORE(S) : leur question n a pas ete posee sur cette"
  dire "    machine. Le verdict ci-dessus ne porte QUE sur les $NB_OK banc(s) qui ont"
  dire "    reellement tourne. Voir la section IGNORES pour la raison de chacun."
fi
if [ "$AVEC_CAPACITES" -eq 1 ]; then
  case "$CAP_RC" in
    0) dire " capacites : toutes classees" ;;
    2) dire " capacites : ECHEC D'INSTRUMENT du detecteur — rien conclu" ;;
    *) dire " capacites : ECHEC DE CLASSEMENT (code $CAP_RC) — voir plus haut" ;;
  esac
fi
dire " duree totale : $(duree_depuis "$T0")     fin : $(horodate)"
dire " journaux : $SORTIE/"
dire "======================================================================"
exit "$RC"
