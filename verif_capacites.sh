#!/usr/bin/env bash
#
# verif_capacites.sh — LA GARDE DES CAPACITES ANNONCEES (2026-08-22)
# =============================================================================
# POURQUOI IL EXISTE
#
#   « Un drapeau a `true` n'est pas une intention : c'est une reponse que du
#     code va CROIRE, et sur laquelle il va choisir un chemin. »
#
#   Neuf capacites de NKRHI/NKRenderer etaient annoncees et creuses : un champ
#   qu'on regle et qui ne fait rien, une virtuelle qui rend un chiffre credible
#   au lieu d'une erreur, une texture chargee que personne n'echantillonne.
#   Aucune ne casse une compilation. Aucune ne fait rougir un banc. Elles ne se
#   voient que si quelqu'un les cherche — donc jamais.
#
# LE DESSIN, ET IL EST LE POINT ENTIER
#
#   ⚠️ CE DETECTEUR N'A PAS LE DROIT DE JUGER. Il n'a le droit que d'EXIGER UN
#   CLASSEMENT. C'est exactement le dessin de config/bancs.list, et pour la
#   meme raison mesuree : la premiere passe sort des dizaines de candidats. Un
#   outil qui les CRIE est desactive avant vendredi — et on aurait reconstruit
#   le probleme qu'il devait resoudre.
#
#   Jour 1  : on classe les candidats, et l'outil est VERT.
#   Jour 2+ : il devient ROUGE sur le candidat suivant — c'est-a-dire exactement
#             le jour ou il sert.
#
#   Un detecteur qui pretend savoir si une capacite est implementee a tort une
#   fois sur trois, et on l'eteint. Un detecteur qui dit « ce symbole n'est pas
#   classe » a toujours raison : c'est un fait sur le fichier de classement,
#   pas un jugement sur le code.
#
# CE QU'IL DIT, ET CE QU'IL NE DIRA JAMAIS
#   il dit        « ce champ est ecrit et jamais lu »    — exact, verifiable
#   il ne dit PAS « cette capacite n'est pas implementee » — ce n'est pas le
#                 meme enonce, et le second n'est pas mesurable par du texte.
#
# IL NE REFUSE PAS LE COMMIT (arbitrage de Rodolf, R3-b)
#   Un controle qui bloque se contourne par `--no-verify`, en silence, par
#   quelqu'un de presse — et personne ne sait jamais qu'il a ete contourne. Un
#   banc rouge, lui, reste rouge et SE VOIT. On echange un blocage qu'on peut
#   faire taire contre un signal qu'on ne peut pas.
#   => code de sortie 4 ; gitcommit.sh CRIE le 4 et laisse passer le commit.
#
# LA REGLE (a) — LA LIGNE DE PARTAGE DE `vision-assumee`
#   capacite documentee, sans drapeau ............... `vision-assumee` autorise
#   capacite dont un drapeau annonce `true` a du code  `vision-assumee` INTERDIT
#   Un `true` est une promesse faite au CODE, pas au lecteur. Deux sorties
#   seulement : le drapeau rend `false`, ou la capacite s'implemente. La classer
#   et passer n'est pas un troisieme choix — c'est la classe de mensonge qu'on
#   traque, rendue acceptable par un fichier de configuration.
#   ⚠️ Cette regle porte sur le CLASSEMENT, pas sur le code : le detecteur
#   refuse un classement interdit, il ne dit toujours rien sur l'implementation.
#
# LES DETECTEURS
#   D1  virtuelle CREUSE (corps vide, ou un unique `return <litteral>;`), zero
#       surcharge et zero appel dans le depot, APPARIEE a un drapeau
#       `mCaps.<x> = true` dont le nom partage un jeton avec le sien.
#       ⚠️ C'est l'APPARIEMENT qui fait le filtre, pas le corps vide. Sans lui,
#       `SetDebugName` (trois surcharges, corps vides, zero surcharge) serait
#       rapporte a tort : un corps vide qui ne promet rien a personne n'est pas
#       un mensonge, c'est un point d'extension.
#   D2  champ scalaire a valeur par defaut dont AUCUNE occurrence du depot n'est
#       une lecture — que des ecritures, la declaration, des commentaires.
#       Enonce exact : « quelqu'un peut regler ce champ et il ne se passe rien ».
#   D3  NON LIVRE, et la raison est mesuree : `./verif_capacites.sh --pourquoi-pas-d3`
#
# USAGE
#   ./verif_capacites.sh                 # detecte, garde, rapporte
#   ./verif_capacites.sh --liste         # les candidats detectes, sans garde
#   ./verif_capacites.sh --nouveaux      # seulement les non classes, un par ligne
#   ./verif_capacites.sh --pourquoi-pas-d3
#
# CODES DE SORTIE
#   0  tout candidat detecte est classe, et aucun classement n'est interdit
#   2  ECHEC D'INSTRUMENT : temoin non retrouve, corpus absent, liste illisible.
#      Rien n'est conclu sur le depot.
#   4  ECHEC DE CLASSEMENT. NE BLOQUE PAS LE COMMIT — rougit le verificateur.
# =============================================================================
set -uo pipefail

dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[cap] cd $ROOT impossible"; exit 2; }

LISTE="config/capacites.list"
TMP="$(mktemp -d 2>/dev/null || echo "${TMPDIR:-/tmp}/nkcap.$$")"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

LISTE_SEULE=0
NOUVEAUX_SEULS=0

# Les en-tetes examines sont une DONNEE, pas une heuristique : on ne devine pas
# « les fichiers qui ressemblent a de la configuration ».
ENTETES_D2=(
  "Kernel/Runtime/NKRHI/src/NKRHI/Core/NkDescs.h"
  "Kernel/Runtime/NKRHI/src/NKRHI/Core/NkIDevice.h"
  "Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkRendererConfig.h"
  # ELARGISSEMENT DU 2026-08-27, decide par Rodolf sur chiffrage.
  # Ces deux en-tetes portent gpuTimeMs -- declare a 0.f, affecte NULLE PART, et
  # LU POUR ETRE AFFICHE (NkOverlayRenderer.cpp:50, « GPU:%.2fms »). Le detecteur
  # ne l a jamais dit parce qu il ne les LISAIT PAS : limite de PERIMETRE, pas de
  # regle. Cout mesure : +141 champs (349 -> 490, +40 %). Rendement mesure :
  # 17 candidats AU PLANCHER, dont 10 freres de gpuTimeMs (cullTimeMs, postTimeMs,
  # shaderSwitches...). Plancher, parce que l index de mesure ne distinguait pas
  # lecture et ecriture : un champ ECRIT ET JAMAIS LU n y figurait pas.
  "Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkRendererTypes.h"
  "Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkRenderGraph.h"
)
ENTETES_D1=( "Kernel/Runtime/NKRHI/src/NKRHI/Core/NkIDevice.h" )

# -- Controle positif d'instrument -------------------------------------------
# Deux temoins MESURES le 2026-08-22, un par detecteur. Un detecteur qui ne
# trouve plus rien ressemble trait pour trait a un depot sain : sans temoin, on
# ne peut pas distinguer « rien a signaler » de « je ne lis plus le fichier ».
TEMOIN_D1="NkIDevice::GetTimestampPeriodNs"
TEMOIN_D2="NkDeviceCaps::timestampQueries"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --liste)           LISTE_SEULE=1; shift ;;
    --nouveaux)        NOUVEAUX_SEULS=1; shift ;;
    --pourquoi-pas-d3) sed -n '/^# ---- D3 : POURQUOI/,/^# ---- FIN D3/p' "$0" | sed 's/^#\{1,2\} \{0,1\}//'; exit 0 ;;
    -h|--help)         sed -n '2,80p' "$0"; exit 0 ;;
    *) dire2 "[cap] option inconnue : $1 (voir --help)"; exit 2 ;;
  esac
done

T0=$(date +%s)

# =============================================================================
# 0. AUTOCONTROLE — la parade d'un defaut paye TROIS FOIS ce soir
# =============================================================================
# Entre le heredoc qui ecrit ce fichier, le shell qui le lit et awk qui compile
# ses chaines, il y a trois couches d'echappement — et deux d'entre elles
# mangent un saut de ligne de format en silence. Resultat trois fois de suite :
# un printf awk coupe par un VRAI saut de ligne, l awk qui refuse de compiler,
# et une section du rapport RESTEE VIDE. Une section vide se lit
# « rien a signaler ».
#
# ⚠️ C'est la forme exacte que ce chantier traque : un outil casse et un depot
# sain rendent la meme sortie. On ne s'en remet donc pas a la relecture.
if grep -nE 'printf\("?"[^"]*$|printf "[^"]*$' "$0" > /dev/null 2>&1; then
  dire2 "[cap] ECHEC D'INSTRUMENT : une chaine de format awk n'est pas fermee dans $0 :"
  grep -nE 'printf\("?"[^"]*$|printf "[^"]*$' "$0" | while IFS= read -r l; do dire2 "[cap]   $l"; done
  dire2 "[cap]   Un « \n » a ete avale par une couche d'echappement. L'awk concerne ne"
  dire2 "[cap]   compilera pas, et SA SECTION DU RAPPORT SERA VIDE — ce qui se lit"
  dire2 "[cap]   « rien a signaler ». Repare avant de conclure quoi que ce soit."
  exit 2
fi


# =============================================================================
# 1. PRECONDITIONS — ne rien conclure depuis un montage casse
# =============================================================================
for f in "${ENTETES_D2[@]}" "${ENTETES_D1[@]}"; do
  if [ ! -f "$f" ]; then
    dire2 "[cap] ECHEC D'INSTRUMENT : en-tete examine introuvable : $f"
    dire2 "[cap]   Il a ete deplace ou renomme. Mets ENTETES_D1/ENTETES_D2 a jour."
    dire2 "[cap]   Ne rien conclure : « je n'ai rien trouve » et « je n'ai rien lu »"
    dire2 "[cap]   produisent la meme sortie verte."
    exit 2
  fi
done
if [ ! -f "$LISTE" ]; then
  dire2 "[cap] ECHEC D'INSTRUMENT : $LISTE absent."
  exit 2
fi

# =============================================================================
# 2. EXTRACTION D2 — les champs candidats, avec leur struct englobante
# =============================================================================
cat > "$TMP/champs.awk" <<'AWKCHAMPS'
BEGIN { prof = 0; attente = "" }
{
  ligne = $0; sub(/\r$/, "", ligne)
  if (match(ligne, /^[[:space:]]*(struct|class)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*/)) {
    t = substr(ligne, RSTART, RLENGTH); sub(/^[[:space:]]*(struct|class)[[:space:]]+/, "", t)
    attente = t
  }
  tmp = ligne
  n = gsub(/\{/, "{", tmp); m = gsub(/\}/, "}", tmp)
  if (n > 0 && attente != "") { prof++; pile[prof] = attente; attente = "" }
  else if (n > 0) { prof++; pile[prof] = (prof > 1 ? pile[prof-1] : "") }
  if (m > 0 && prof > 0) { prof -= m; if (prof < 0) prof = 0 }
  if (prof < 1) next
  st = pile[prof]; if (st == "") next
  if (ligne ~ /^[[:space:]]*(static|virtual|return|\/\/|\/\*|\*)/) next
  # QUATRIEME DEFAUT DU 2026-08-24, ET IL EST UNE COUCHE PLUS TOT QUE LES TROIS
  # AUTRES : ici on ne classe pas mal, ON N EXTRAIT PAS DU TOUT.
  # Le test « une parenthese => c est une fonction, pas un champ » portait sur la
  # ligne BRUTE. Or la ligne
  #     bool validation = false; // active validation layer (Vulkan)
  # porte sa parenthese DANS SON COMMENTAIRE. Le champ etait rejete a
  # l extraction : jamais candidat, pas « mal classe ». NkRendererConfig::validation
  # a 0 lecture, 0 ecriture, 0 mention dans tout le depot, et le detecteur ne l a
  # jamais nomme.
  # MESURE : 12 declarations perdues ainsi dans les trois en-tetes examines.
  # Meme faute que les trois autres du jour -- DU TEXTE PRIS POUR DU CODE --
  # appliquee cette fois a un test de STRUCTURE. Meme parade : amputer le
  # commentaire AVANT de decider quoi que ce soit.
  sanscom = ligne; sub(/\/\/.*$/, "", sanscom)
  if (sanscom ~ /\(/) next
  if (match(sanscom, /^[[:space:]]*[A-Za-z_][A-Za-z0-9_:<>, \t*&]*[ \t*&]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=[^=]/)) {
    d = sanscom; sub(/[[:space:]]*=.*$/, "", d)
    nom = d; sub(/^.*[ \t*&]/, "", nom); if (nom == "") next
    val = sanscom; sub(/^[^=]*=[[:space:]]*/, "", val); sub(/;.*$/, "", val)
    gsub(/[[:space:]]+$/, "", val)
    printf "%s|%s|%s|%d|%s\n", st, nom, FILENAME, FNR, val
  }
}
AWKCHAMPS
awk -f "$TMP/champs.awk" "${ENTETES_D2[@]}" > "$TMP/champs.txt"

# (les voies d'acces sont extraites en section 4 : elles vivent dans TOUT le
#  depot, pas dans les trois en-tetes examines. Defaut trouve et corrige le
#  2026-08-22 -- voir le bloc VOIES D'ACCES plus bas.)

# =============================================================================
# 3. EXTRACTION D1 — virtuelles creuses, et drapeaux reellement mis a `true`
# =============================================================================
cat > "$TMP/virt.awk" <<'AWKVIRT'
BEGIN { prof = 0; attente = ""; att = 0 }
{
  ligne = $0; sub(/\r$/, "", ligne)
  # -- suivi de la classe englobante (meme mecanique que champs.awk) ----------
  if (att == 0 && match(ligne, /^[[:space:]]*(struct|class)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*/)) {
    t = substr(ligne, RSTART, RLENGTH); sub(/^[[:space:]]*(struct|class)[[:space:]]+/, "", t)
    attente = t
  }
  if (att == 0) {
    tmp = ligne
    n = gsub(/\{/, "{", tmp); m = gsub(/\}/, "}", tmp)
    if (n > 0 && attente != "") { prof++; pile[prof] = attente; attente = "" }
    else if (n > 0) { prof++; pile[prof] = (prof > 1 ? pile[prof-1] : "") }
    if (m > 0 && prof > 0) { prof -= m; if (prof < 0) prof = 0 }
  }
  # -- detection de la virtuelle ---------------------------------------------
  if (att == 0) {
    if (ligne ~ /^[[:space:]]*virtual[[:space:]]/ && ligne ~ /\(/ && ligne ~ /\{[[:space:]]*$/) {
      d = ligne; sub(/\(.*$/, "", d)
      nom = d; sub(/^.*[ \t*&]/, "", nom)
      if (nom != "") { att = 1; enom = nom; eligne = FNR; ecl = (prof >= 1 ? pile[prof] : ""); corps = 0; ret = "" }
    }
    next
  }
  if (ligne ~ /^[[:space:]]*\}/) {
    if (corps == 0)                   printf "%s|%s|%d|vide|\n", ecl, enom, eligne
    else if (corps == 1 && ret != "") printf "%s|%s|%d|constant|%s\n", ecl, enom, eligne, ret
    att = 0; next
  }
  if (ligne ~ /^[[:space:]]*$/) next
  if (ligne ~ /^[[:space:]]*(\/\/|\/\*|\*)/) next
  corps++
  ret = ""
  if (match(ligne, /^[[:space:]]*return[[:space:]]+[^;]*;[[:space:]]*$/)) {
    r = ligne; sub(/^[[:space:]]*return[[:space:]]+/, "", r); sub(/;[[:space:]]*$/, "", r)
    # LITTERAL seulement. `return mThing;` n'est pas creux : il rend un etat.
    if (r ~ /^(-?[0-9]+(\.[0-9]*)?[fFuUlL]*|true|false|nullptr|\{\})$/) ret = r
  }
}
AWKVIRT
awk -f "$TMP/virt.awk" "${ENTETES_D1[@]}" > "$TMP/virtuelles.txt"

# Les drapeaux de capacite REELLEMENT ecrits par du code (pas la declaration).
grep -rhoP 'mCaps\.\K[A-Za-z_][A-Za-z0-9_]*(?=[ \t]*=[^=])' Kernel Engine --include='*.cpp' 2>/dev/null \
  | sort -u > "$TMP/drapeaux.txt"

# =============================================================================
# 3bis. EXTRACTION D3 — ce que le C++ LIE contre ce que les shaders ECHANTILLONNENT
# =============================================================================
# 🔴 AVANT DE TOUCHER A D3, LIRE CECI (mesure du 27/08).
#
#   D3 EST AUSSI UNE SECURITE, ET PERSONNE NE L A VOULUE.
#
#   Il ne lit PAS le corpus C++ (Kernel/Engine/Applications). Il lit Resources/
#   et NkResources.h. Consequence mesuree en neutralisant trois controles a la
#   fois : quand le corpus disparait — grep muet, plancher NB_HITS neutralise,
#   temoins D1/D2 neutralises — D1 et D2 ne detectent plus RIEN, et c est D3,
#   seul, qui detecte encore 7 candidats et fait rougir la passe (code 4).
#
#   ⚠️ DONC : « D3 fait doublon avec D1/D2 » est FAUX, et le nettoyer en croyant
#   retirer un doublon retirerait le SECOND des deux seuls points de rupture du
#   dispositif. Les six controles qui se rattrapent l un l autre ne reposent pas
#   sur six securites : ils reposent sur DEUX — les temoins D1/D2, et D3. Un seul
#   des deux a ete ecrit pour ca.
#
#   « Une securite qu on n a pas voulue est une securite qu on peut retirer sans
#     le savoir. » C est pour ca que cette note vit ICI et pas seulement dans
#   VERIFICATEUR.md : celui qui supprimera D3 lira ce fichier, pas le document.
#
# =============================================================================
# D3 etait refuse le 22/08 au soir, et la raison etait mesuree : cote C++,
# l'index de set n'existait DANS AUCUNE DECLARATION — il vivait dans un
# commentaire de NkResources.h et dans un nom de membre. Comparer les deux cotes
# aurait demande de DEVINER l'appariement enum -> set, et un detecteur qui devine
# fabrique des faux positifs.
#
# Ce n'etait pas une fatalite, c'etait une ligne de code manquante. Elle est
# ecrite : `kStandardBindings` porte (set, binding, type, etage) et
# CreateStandardLayouts() la parcourt. L'appariement est devenu une DONNEE DU
# CODE, donc D3 se calcule — exactement, sans deviner.
#
# ⚠️ CE QUE D3 AFFIRME, ET RIEN DE PLUS :
#   il dit      « le C++ lie ce (set, binding) et AUCUN shader du corpus ne le
#                 declare » — exact, verifiable, et c'est un defaut en soi :
#                 une ressource poussee vers un slot que personne ne lit.
#   il ne dit PAS « le shader lit la mauvaise chose » : deux slots peuvent porter
#                 le meme numero pour des raisons legitimes, et trancher demande
#                 de savoir ce que la carte lit. Ce n'est pas mesurable ici.
#
# ⚠️ ET IL NE REGARDE PAS LE SENS INVERSE COMME UN DEFAUT. Le corpus compte 47
# couples (set, binding) distincts pour 18 bindings standard : la majorite
# appartient a des familles de shaders (compute, 2D, post) qui n'utilisent PAS
# les layouts standard. Crier sur chacun serait fabriquer 29 faux positifs le
# premier jour.
NKRESOURCES_H="Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkResources.h"
if [ -f "$NKRESOURCES_H" ]; then
  # (a) la table : set|binding|nom — les valeurs numeriques viennent des enums
  awk '
    /kStandardBindings\[\] = \{/ { dans = 1 }
    dans && /^[[:space:]]*\};/     { dans = 0 }
    /^[[:space:]]*NK_SET_[A-Z]+[[:space:]]*=[[:space:]]*[0-9]+/ {
      l = $0; sub(/^[[:space:]]*/, "", l)
      nom = l; sub(/[[:space:]]*=.*$/, "", nom)
      v = l; sub(/^[^=]*=[[:space:]]*/, "", v); sub(/[^0-9].*$/, "", v)
      valset[nom] = v
    }
    /^[[:space:]]*NK_BIND_[A-Z_0-9]+[[:space:]]*=[[:space:]]*[0-9]+/ {
      l = $0; sub(/^[[:space:]]*/, "", l)
      nom = l; sub(/[[:space:]]*=.*$/, "", nom)
      v = l; sub(/^[^=]*=[[:space:]]*/, "", v); sub(/[^0-9].*$/, "", v)
      valbind[nom] = v
    }
    dans && /^[[:space:]]*\{NK_SET_/ {
      l = $0; gsub(/[{} 	]/, "", l)
      n = split(l, c, ",")
      if (n >= 2 && (c[1] in valset) && (c[2] in valbind))
        printf "%s|%s|%s\n", valset[c[1]], valbind[c[2]], c[2]
    }
  ' "$NKRESOURCES_H" | sort -u > "$TMP/d3_cpp.txt"

  # (b) ce que les shaders declarent, avec le TYPE et l'IDENTIFIANT
  grep -rhoP 'layout\s*\(\s*set\s*=\s*\d+\s*,\s*binding\s*=\s*\d+\s*\)[^;{]*'        Resources Kernel 2>/dev/null     | sed 's/[[:space:]]\+/ /g'     | awk '{
        s = $0
        if (match(s, /set *= *[0-9]+/))     { st = substr(s, RSTART, RLENGTH); gsub(/[^0-9]/, "", st) }
        if (match(s, /binding *= *[0-9]+/)) { bd = substr(s, RSTART, RLENGTH); gsub(/[^0-9]/, "", bd) }
        r = s; sub(/^[^)]*\)[ ]*/, "", r)
        gsub(/^(readonly|writeonly|coherent|restrict|volatile) +/, "", r)
        n = split(r, w, " ")
        id = (n >= 1 ? w[n] : "?")
        ty = (n >= 2 ? w[n-1] : "?")
        # un IDENTIFIANT, pas un morceau de phrase : la sortie ramassait
        # « ) liraient » et « space0)` » dans du texte de commentaire.
        if (id !~ /^[A-Za-z_][A-Za-z0-9_]*$/) next
        if (ty !~ /^[A-Za-z_][A-Za-z0-9_]*$/) ty = "?"
        if (st != "" && bd != "") printf "%s|%s|%s|%s\n", st, bd, ty, id
      }' | sort -u > "$TMP/d3_shaders.txt"

  # (c) LES DECLARATIONS SANS SET — et c'est la moitie du corpus.
  # ⚠️ Defaut de D3 trouve en contre-verifiant ses deux premiers candidats :
  # OpenGL n'a pas de descriptor sets. Ses shaders ecrivent `layout(binding=2)`
  # tout court. Un D3 qui ne compte que les declarations QUALIFIEES par un set
  # annonce « aucun shader ne declare ce slot » alors qu'une famille entiere le
  # declare — il ACCUSE. On les extrait donc a part, et on les NOMME dans le
  # message : le lecteur sait exactement ce qui est su et ce qui ne l'est pas.
  grep -rhoP 'layout\s*\(\s*binding\s*=\s*\d+\s*\)[^;{]*' Resources Kernel 2>/dev/null     | sed 's/[[:space:]]\+/ /g'     | awk '{
        s = $0
        if (match(s, /binding *= *[0-9]+/)) { bd = substr(s, RSTART, RLENGTH); gsub(/[^0-9]/, "", bd) }
        r = s; sub(/^[^)]*\)[ ]*/, "", r)
        n = split(r, w, " ")
        id = (n >= 1 ? w[n] : "?")
        if (bd != "" && id != "") printf "%s|%s\n", bd, id
      }' | sort -u > "$TMP/d3_sanset.txt"
fi

# =============================================================================
# 4. LES PASSES SUR LE CORPUS — deux greps, et pas un par symbole
# =============================================================================
# Mesure du 2026-08-22 : les prototypes relisaient tout le depot UNE FOIS PAR
# SYMBOLE — 30 s pour D2 seul. Ici : deux greps a motifs fixes, et l'awk trie.
# L'exigence « il doit etre lancable » se perd dans ce genre de detail, jamais
# dans les grands choix.
#
# POURQUOI DEUX ET NON UN. Le premier grep sort les NOMS DE TYPE avec `-o` : il
# repond exactement a « quel fichier nomme quel type », en quelques milliers de
# lignes minuscules. Le second sort les LIGNES ENTIERES des champs, parce qu'il
# faut le texte pour distinguer une ecriture d'une lecture. Les fondre en un
# seul obligerait a re-tester chaque nom de type contre chaque ligne : 340 x
# 40 000 comparaisons pour une information que `-o` donne directement.

INCL=(--include='*.h' --include='*.hpp' --include='*.cpp' --include='*.inl')

cut -d'|' -f1 "$TMP/champs.txt" | sort -u | grep -v '^[[:space:]]*$' > "$TMP/types.txt"
cut -d'|' -f2 "$TMP/champs.txt" | sort -u | grep -v '^[[:space:]]*$' > "$TMP/noms.txt"
cut -d'|' -f2 "$TMP/virtuelles.txt" | sort -u | grep -v '^[[:space:]]*$' > "$TMP/methodes.txt"

# -- VOIES D'ACCES ------------------------------------------------------------
# DEFAUT DE MON PROPRE OUTIL, TROUVE PAR LA MESURE (2026-08-22).
# Premiere version : les voies d'acces etaient cherchees dans les TROIS EN-TETES
# EXAMINES. Or `NkDeviceCaps mCaps{}` est declare dans les SIX en-tetes de
# backend (NkVulkanDevice.h, NkOpenglDevice.h...), jamais dans NkIDevice.h. La
# voie `mCaps` n'existait donc pas, `mCaps.maxDescriptorSets = ...` n'etait
# attribue a personne, et le champ ressortait « jamais mentionne ».
# L'outil transformait « ecrit ailleurs » en « jamais touche » : un instrument
# qui accuse le sujet a la place de son propre montage, pour la troisieme fois
# de ce chantier.
# Correction : les voies se cherchent dans TOUT LE CORPUS, et la reconnaissance
# n'exige plus un `.` ou un `->` devant le membre — `mCaps.x` s'ecrit nu.
#
# Deux formes, et pas une de plus :
#   membre     `NkDeviceCaps mCaps;`            -> mCaps.<champ> / mCaps-><champ>
#   accesseur  `const NkDeviceCaps &GetCaps()`  -> GetCaps().<champ>
# Un accesseur est une voie de LECTURE au meme titre qu'un membre. Ignorer les
# deux, c'est appeler « jamais lu » un champ que tout le moteur lit.
TYPES_ALT=$(paste -sd'|' "$TMP/types.txt")
grep -rhoP "(?<![A-Za-z0-9_])(?:${TYPES_ALT})[ \t]*[&*]?[ \t]+[a-zA-Z_][A-Za-z0-9_]*(?=[ \t]*[;={(])" \
     "${INCL[@]}" Kernel Engine Applications 2>/dev/null \
  | sed 's/[&*]/ /g' \
  | awk 'NF >= 2 { printf "%s|%s\n", $1, $NF }' \
  | sort -u > "$TMP/voies.txt"

NB_MOTIFS=$(cat "$TMP/types.txt" "$TMP/noms.txt" "$TMP/methodes.txt" | grep -c '')
if [ "$NB_MOTIFS" -lt 20 ]; then
  dire2 "[cap] ECHEC D'INSTRUMENT : $NB_MOTIFS symbole(s) extrait(s) des en-tetes."
  dire2 "[cap]   L'extraction ne lit plus les en-tetes (forme changee ?). Ne rien conclure."
  exit 2
fi

# (a) quel fichier NOMME quel type — attribution des homonymes
grep -rnwoF -f "$TMP/types.txt" "${INCL[@]}" Kernel Engine Applications 2>/dev/null \
  | awk -F: '{ t=$0; sub(/^[^:]*:[^:]*:/, "", t); printf "%s|%s\n", $1, t }' \
  | sort -u > "$TMP/nomme.txt"

# (b) les lignes entieres qui portent un champ ou une methode suivie
cat "$TMP/noms.txt" "$TMP/methodes.txt" | sort -u > "$TMP/motifs.txt"
grep -rnwF -f "$TMP/motifs.txt" "${INCL[@]}" Kernel Engine Applications 2>/dev/null > "$TMP/hits.txt"

NB_HITS=$(grep -c '' < "$TMP/hits.txt")
if [ "$NB_HITS" -lt 100 ]; then
  dire2 "[cap] ECHEC D'INSTRUMENT : $NB_HITS occurrence(s) sur tout le corpus."
  dire2 "[cap]   Le depot n'est pas la, ou grep n'a rien lu. Ne rien conclure."
  exit 2
fi

# =============================================================================
# 5. CLASSIFICATION — le coeur, et la seule chose qui ait le droit d'etre exacte
# =============================================================================
awk -v fchamps="$TMP/champs.txt" -v fvoies="$TMP/voies.txt" \
    -v fvirt="$TMP/virtuelles.txt" -v fdrap="$TMP/drapeaux.txt" \
    -v fnomme="$TMP/nomme.txt" -v fentete1="${ENTETES_D1[0]}" '
  # ---- decoupage camelCase / snake_case en jetons minuscules ---------------
  # Decoupage camelCase / snake_case en jetons minuscules.
  # ⚠️ DEFAUT MESURE LE 2026-08-22 : la premiere version coupait a CHAQUE
  # majuscule. Sur un nom en CAPITALES — NK_BIND_IBL_IRRADIANCE — elle rendait
  # vingt-deux jetons d un seul caractere, tous rejetes par le seuil de cinq.
  # Resultat : D3 ne rapprochait RIEN, et son silence ressemblait trait pour
  # trait a « il n y a rien a signaler ».
  # La regle juste est celle de camelCase : une majuscule ouvre un jeton
  # seulement si le caractere PRECEDENT etait minuscule ou chiffre.
  function jetons(s,   i, c, p, mot, out) {
    out = " "; mot = ""; p = ""
    for (i = 1; i <= length(s); i++) {
      c = substr(s, i, 1)
      if (c == "_") { if (mot != "") out = out tolower(mot) " "; mot = ""; p = c; continue }
      if (c ~ /[A-Z]/ && p ~ /[a-z0-9]/) { if (mot != "") out = out tolower(mot) " "; mot = c }
      else mot = mot c
      p = c
    }
    if (mot != "") out = out tolower(mot) " "
    return out
  }
  # Un jeton partage doit etre PORTEUR. « get », « set », « is », « begin »
  # n existent que pour faire du bruit : cinq caracteres est le seuil mesure qui
  # garde « timestamp » et jette « begin ».
  function partage(a, b,   n, i, t, ja, jb) {
    ja = jetons(a); jb = jetons(b)
    n = split(ja, t, " ")
    for (i = 1; i <= n; i++) {
      if (t[i] == "" || length(t[i]) < 5) continue
      if (index(jb, " " t[i] " ") > 0) return t[i]
      # pluriel simple : « query » contre « queries » n est pas un jeton nouveau
      if (index(jb, " " t[i] "s ") > 0) return t[i]
      if (index(jb, " " substr(t[i], 1, length(t[i]) - 1) "ies ") > 0) return t[i]
    }
    return ""
  }
  BEGIN {
    while ((getline l < fchamps) > 0) {
      split(l, a, "|")
      cle = a[1] "::" a[2]
      ch_struct[cle] = a[1]; ch_nom[cle] = a[2]
      ch_fic[cle] = a[3]; ch_lig[cle] = a[4] + 0; ch_def[cle] = a[5]
      champs[cle] = 1
      parnom[a[2]] = parnom[a[2]] " " cle
    }
    close(fchamps)
    while ((getline l < fvoies) > 0) { split(l, a, "|"); voie[a[1]] = voie[a[1]] " " a[2] }
    close(fvoies)
    while ((getline l < fvirt) > 0) {
      split(l, a, "|")
      cle = a[1] "::" a[2]
      vi_cl[cle] = a[1]; vi_nom[cle] = a[2]; vi_lig[cle] = a[3] + 0
      vi_forme[cle] = a[4]; vi_ret[cle] = a[5]
      virt[cle] = 1
      parmeth[a[2]] = parmeth[a[2]] " " cle
    }
    close(fvirt)
    nbdrap = 0
    while ((getline l < fdrap) > 0) { gsub(/[ \t\r]/, "", l); if (l != "") { nbdrap++; drap[nbdrap] = l } }
    close(fdrap)
    while ((getline l < fnomme) > 0) { gsub(/\r/, "", l); nomme[l] = 1 }
    close(fnomme)
  }
  # ---- une passe sur les occurrences, un seul parcours par ligne -----------
  {
    ligne = $0; sub(/\r$/, "", ligne)
    p1 = index(ligne, ":"); if (p1 == 0) next
    fic = substr(ligne, 1, p1 - 1)
    reste = substr(ligne, p1 + 1)
    p2 = index(reste, ":"); if (p2 == 0) next
    num = substr(reste, 1, p2 - 1) + 0
    txt = substr(reste, p2 + 1)

    # le commentaire de fin de ligne est ampute AVANT toute decision : une
    # mention dans un commentaire n est pas une lecture, et prendre l une pour
    # l autre est exactement « chercher un mot au lieu d un etat ».
    #
    # ⚠️ DEFAUT DE CE DETECTEUR, MESURE LE 2026-08-24, ET IL LUI A FAIT MANQUER
    # UN CAS ENTIER. Le commentaire etait ampute ; LA CHAINE DE CARACTERES, non.
    # Resultat : la ligne
    #     logger.Info("[NkRender3D] Shadow pipeline create: ...")
    # portait le jeton « pipeline », et ce jeton etait compte comme une LECTURE
    # de NkRendererConfig::pipeline. Le champ sortait donc lectures>0 et
    # disparaissait de la liste des candidats -- alors qu il est ecrit par six
    # presets et lu par personne.
    # C EST EXACTEMENT LA MEME FAUTE QUE CELLE QU AMPUTE LA LIGNE AU-DESSUS,
    # dans un second vetement : du TEXTE pris pour du CODE. Un nom dans une
    # chaine n est pas plus une lecture qu un nom dans un commentaire.
    # Les chaines partent donc EN PREMIER -- avant le « // », sinon un
    # "http://..." ampute la moitie de sa propre ligne.
    # ⚠️ LA REGLE EST SIMPLE ET SON IMPRECISION EST ASSUMEE : elle apparie les
    # guillemets sans comprendre « \" ». Sur une ligne qui en contient, elle
    # peut retirer un peu trop -- donc effacer une VRAIE lecture, donc AJOUTER
    # un candidat a classer. Jamais en retirer un. C est la seule direction
    # d erreur acceptable ici : un candidat de trop se classe une fois, un
    # candidat manque ne se voit jamais.
    code = txt
    gsub(/"[^"]*"/, "", code)
    sub(/\/\/.*$/, "", code)
    # ligne de commentaire de bloc : « /* ... » ou une continuation « * ... ».
    # champs.awk les saute deja a l extraction ; ne pas les sauter ICI faisait
    # compter un nom de champ cite dans une doc comme une lecture.
    if (txt ~ /^[[:space:]]*(\/\*|\*[^\/])/) code = ""
    nu = (code ~ /^[[:space:]]*$/)

    # les identifiants de la ligne, extraits UNE fois. Ce qui suit est en
    # O(jetons de la ligne), jamais en O(symboles suivis).
    ntok = split(code, tk, /[^A-Za-z0-9_]+/)
    if (nu) ntok = split(txt, tk, /[^A-Za-z0-9_]+/)
    # DEUX BOUCLES, pas une. La premiere connait TOUS les jetons de la ligne
    # avant que la seconde ne decide quoi que ce soit. C est ce qui permet de
    # ne lancer une regex de voie d acces que si le nom de la voie est
    # REELLEMENT sur la ligne : sans ce filtre, l outil lancait trois regex par
    # voie declaree et par candidat, sur 80 000 lignes — mesure : > 120 s.
    delete vus
    for (j = 1; j <= ntok; j++) if (tk[j] != "") vus[tk[j]] = 1
    delete traite
    for (j = 1; j <= ntok; j++) {
      nom = tk[j]
      if (nom == "" || traite[nom]) continue
      traite[nom] = 1

      # ---- D2 : un champ -----------------------------------------------
      if (parnom[nom] != "") {
        nc = split(parnom[nom], cles, " ")
        # ATTRIBUTION, EN DEUX TEMPS ET DANS CET ORDRE.
        #
        # 1) LA VOIE D ACCES PRIME. Si la ligne ecrit `mCfg.shadow.enabled`, ce
        #    `enabled` appartient a NkShadowConfig et a lui seul.
        # 2) A DEFAUT, le fichier qui nomme le type.
        #
        # ⚠️ DEFAUT MESURE LE 2026-08-22, corrige ici. Avec le seul critere (2),
        #    `NkRendererImpl.cpp` nomme NkIBLConfig dans un COMMENTAIRE, et sa
        #    lecture de `mCfg.shadow.enabled` etait comptee comme une lecture de
        #    NkIBLConfig::enabled — qui n en a aucune. Un homonyme lu FAIT
        #    MANQUER un defaut : l outil se taisait sur un champ creux en
        #    croyant le voir lu. C est la direction d erreur la plus dangereuse
        #    des deux, parce qu elle est silencieuse.
        nvoie = 0
        for (k = 1; k <= nc; k++) {
          cle = cles[k]; if (cle == "") continue
          # UNE LECTURE SUFFIT A CLORE LE DOSSIER. Des qu un candidat a une
          # lecture ou une prise d adresse, il n est plus candidat : continuer a
          # classer ses 400 occurrences suivantes ne change plus rien au verdict.
          # Mesure : 17 s -> 5 s, et pas un chiffre du rapport ne bouge (les
          # candidats RAPPORTES ont lectures=0 par definition).
          if (lec[cle] > 0 || adr[cle] > 0) continue
          s2 = ch_struct[cle]
          parvoie[cle] = 0
          if (voie[s2] == "") continue
          nv = split(voie[s2], vv, " ")
          for (v = 1; v <= nv; v++) {
            if (vv[v] == "" || !vus[vv[v]]) continue
            # membre nu, membre pointeur, ou accesseur : les trois formes.
            # La premiere version exigeait un « . » ou un « > » DEVANT le
            # membre. Or `mCaps.timestampQueries` s ecrit nu : la voie n etait
            # jamais reconnue, et le champ ressortait « jamais mentionne ».
            if (code ~ ("(^|[^A-Za-z0-9_])" vv[v] "[[:space:]]*[.][[:space:]]*" nom "([^A-Za-z0-9_]|$)") || \
                code ~ ("(^|[^A-Za-z0-9_])" vv[v] "[[:space:]]*->[[:space:]]*" nom "([^A-Za-z0-9_]|$)") || \
                code ~ ("(^|[^A-Za-z0-9_])" vv[v] "[[:space:]]*[(][^)]*[)][[:space:]]*[.][[:space:]]*" nom "([^A-Za-z0-9_]|$)")) {
              parvoie[cle] = 1; nvoie++; break
            }
          }
        }
        for (k = 1; k <= nc; k++) {
          cle = cles[k]; if (cle == "") continue
          if (lec[cle] > 0 || adr[cle] > 0) continue
          s2 = ch_struct[cle]
          if (nvoie > 0) att = parvoie[cle]
          else           att = nomme[fic "|" s2]
          if (!att) { horstype[cle]++; continue }
          if (fic == ch_fic[cle] && num == ch_lig[cle]) { decl[cle]++; continue }
          if (nu) { comm[cle]++; continue }
          # ---- LA VOIE D ACCES EST EXIGEE, PAS SEULEMENT PREFEREE -----------
          # ⚠️ SECOND DEFAUT MESURE LE 2026-08-24, et c est celui qui comptait.
          # Le repli `nomme[fic|s2]` dit : « ce fichier nomme le type, donc un
          # jeton nu qui porte le nom du champ lui appartient ». C est FAUX des
          # que le nom est banal. NkRender3D.cpp nomme NkRendererConfig (une
          # fois) et declare partout une VARIABLE LOCALE `pipeline` :
          #     NkPipelineHandle pipeline = mPBRPipeline;
          #     cmd->BindGraphicsPipeline(pipeline);
          # Chacune de ces lignes etait comptee comme une lecture -- et une
          # seule suffit a clore le dossier. Un champ creux devenait invisible
          # a cause d une variable locale homonyme dans un autre fichier.
          #
          # LA REGLE JUSTE EST CELLE DU LANGAGE : un membre ne se lit QUE par
          # `x.nom`, `x->nom`, ou nu DANS UNE METHODE DE SA PROPRE CLASSE. Hors
          # du fichier qui le declare, un jeton nu n est pas un acces au membre.
          # Le fichier declarant garde l exemption : ses methodes en ligne
          # ecrivent leurs membres nus, et les compter est juste.
          #
          # ⚠️ DIRECTION DE L ERREUR, ET c est pour ca qu on ose : cette regle ne
          # peut qu AJOUTER des candidats, jamais en retirer. Un candidat de
          # trop se classe une fois ; un candidat manque ne se voit jamais.
          if (fic != ch_fic[cle] &&               code !~ ("[.][[:space:]]*" nom "([^A-Za-z0-9_]|$)") &&               code !~ ("->[[:space:]]*" nom "([^A-Za-z0-9_]|$)")) { horstype[cle]++; continue }
          # prise d adresse : ce n est pas une lecture, mais ce n est pas rien
          if (code ~ ("&[-A-Za-z0-9_.>()]*" nom "([^A-Za-z0-9_]|$)")) { adr[cle]++; continue }
          # ---- L ECRITURE AUSSI PASSE PAR LA VOIE D ACCES --------------------
          # ⚠️ TROISIEME DEFAUT DU 2026-08-24, et il est ne de la correction des
          # deux premiers. La ligne
          #     srcStage |= toStage(bb[i].srcStage);
          # contient DEUX choses : une variable LOCALE `srcStage` a gauche d un
          # `|=`, et une VRAIE LECTURE du champ a droite. Le test d ecriture,
          # cherchant `nom ... =` n importe ou sur la ligne, attrapait la locale
          # et classait la ligne « ecriture » -- donc jamais « lecture ». Quatre
          # champs REELLEMENT LUS (NkBufferBarrier / NkTextureBarrier ::srcStage
          # et ::dstStage, lus en NkVulkanCommandBuffer.cpp:400-401 et 456-457)
          # sortaient « ecrit-jamais-lu ». Des FAUX POSITIFS, trouves en
          # contre-verifiant a la main les quinze candidats que la correction
          # venait de faire apparaitre.
          # La parade est la meme regle que pour la lecture : hors du fichier
          # declarant, une ecriture de MEMBRE porte un « . » ou un « -> » juste
          # devant le nom. `c.pipeline = X`, `mCaps.x = true` et l initialisation
          # designee `{.pipeline = X}` la portent tous les trois.
          anc = (fic == ch_fic[cle] ? "" : "[.>][[:space:]]*")
          if (code ~ (anc nom "[[:space:]]*([+][+]|--)")) { ecr[cle]++; continue }
          if (code ~ (anc nom "[[:space:]]*([-+*/%|&^]|<<|>>)?=[^=]")) {
            ecr[cle]++
            if (code ~ (anc nom "[[:space:]]*=[[:space:]]*true")) prom[cle] = 1
            continue
          }
          lec[cle]++
        }
      }

      # ---- D1 : une methode virtuelle suivie ----------------------------
      if (parmeth[nom] != "") {
        nc = split(parmeth[nom], cles, " ")
        for (k = 1; k <= nc; k++) {
          cle = cles[k]; if (cle == "") continue
          # une DECLARATION (ici ou ailleurs) : virtual ... nom(   ou  ... nom(...) override
          if (code ~ ("virtual[[:space:]].*[^A-Za-z0-9_]" nom "[[:space:]]*\\(") || \
              code ~ (nom "[[:space:]]*\\(.*\\)[^;]*override")) { m_decl[cle]++; continue }
          # un APPEL : nom( precede d un non-identifiant
          if (code ~ ("(^|[^A-Za-z0-9_])" nom "[[:space:]]*\\(")) m_appel[cle]++
        }
      }
    }
  }
  END {
    # -- D1 : creuse, une seule declaration, zero appel, APPARIEE ------------
    for (cle in virt) {
      nom = vi_nom[cle]
      if (m_decl[cle] > 1) continue     # surchargee ailleurs : ce n est pas creux
      if (m_appel[cle] > 0) continue    # appelee quelque part
      jet = ""; dr = ""
      for (d = 1; d <= nbdrap; d++) { j = partage(nom, drap[d]); if (j != "") { jet = j; dr = drap[d]; break } }
      # ⚠️ C EST L APPARIEMENT QUI FAIT LE FILTRE. Sans lui, SetDebugName —
      # corps vides, zero surcharge, zero appel — serait rapporte a tort. Un
      # corps vide qui ne promet rien a personne est un point d extension, pas
      # un mensonge.
      if (jet == "") continue
      printf "D1|%s|%s:%d|%s|%s|corps %s, retour %s ; drapeau mCaps.%s mis a true par du code (jeton partage: %s)\n",
             cle, fentete1, vi_lig[cle], "promesse-true",
             (vi_ret[cle] == "" ? "-" : vi_ret[cle]),
             vi_forme[cle], (vi_ret[cle] == "" ? "(aucun)" : vi_ret[cle]), dr, jet
    }
    # -- D2 : ecrit, jamais lu -----------------------------------------------
    for (cle in champs) {
      if (lec[cle] > 0) continue
      if (adr[cle] > 0) continue
      if (ecr[cle] + decl[cle] == 0) continue   # symbole jamais vu : extraction douteuse
      # DEUX ENONCES DIFFERENTS, et les confondre serait mentir sur ce qu on sait :
      #   ecrit-jamais-lu   quelqu un REGLE ce champ, et il ne se passe rien.
      #   jamais-mentionne  personne ne le nomme nulle part, ni en ecriture ni
      #                     en lecture. Enonce plus FAIBLE : il peut aussi vouloir
      #                     dire que l attribution n a pas trouve la voie d acces.
      # Il est dit tel quel, jamais deguise en le premier.
      forme = (ecr[cle] > 0 ? "ecrit-jamais-lu" : "jamais-mentionne")
      printf "D2|%s|%s:%d|%s|%s|%s ; ecritures=%d lectures=0 adresse=0 commentaires=%d hors-type=%d\n",
             cle, ch_fic[cle], ch_lig[cle],
             (prom[cle] ? "promesse-true" : "sans-promesse"),
             (ch_def[cle] == "" ? "-" : ch_def[cle]),
             forme, ecr[cle], comm[cle], horstype[cle]
    }
  }
' "$TMP/hits.txt" | sort > "$TMP/detectes.txt"

# =============================================================================
# 5bis. D3 — le C++ LIE, les shaders ECHANTILLONNENT : confrontation
# =============================================================================
D3_INFOS="$TMP/d3_infos.txt"
: > "$D3_INFOS"
if [ -s "$TMP/d3_cpp.txt" ] && [ -s "$TMP/d3_shaders.txt" ]; then
  awk -v fsh="$TMP/d3_shaders.txt" -v fsanset="$TMP/d3_sanset.txt" -v finfos="$D3_INFOS" '
    # Meme correction que pour D1/D2 : une majuscule n ouvre un jeton que si le
    # caractere precedent etait minuscule ou chiffre. Sans ca,
    # NK_BIND_IBL_IRRADIANCE se decoupe en 22 jetons d un caractere et D3 reste
    # muet — un silence qui ressemble a « rien a signaler ».
    function jetons(s,   i, c, p, mot, out) {
      out = " "; mot = ""; p = ""
      for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (c == "_") { if (mot != "") out = out tolower(mot) " "; mot = ""; p = c; continue }
        if (c ~ /[A-Z]/ && p ~ /[a-z0-9]/) { if (mot != "") out = out tolower(mot) " "; mot = c }
        else mot = mot c
        p = c
      }
      if (mot != "") out = out tolower(mot) " "
      return out
    }
    function partage(a, b,   n, i, t, ja, jb) {
      ja = jetons(a); jb = jetons(b)
      n = split(ja, t, " ")
      for (i = 1; i <= n; i++) {
        if (t[i] == "" || length(t[i]) < 5) continue
        if (index(jb, " " t[i] " ") > 0) return t[i]
      }
      return ""
    }
    BEGIN {
      FS = "|"
      while ((getline l < fsanset) > 0) {
        split(l, a, "|")
        sanset[a[1]] = sanset[a[1]] (sanset[a[1]] == "" ? "" : ", ") a[2]
      }
      close(fsanset)
      while ((getline l < fsh) > 0) {
        split(l, a, "|")
        cle = a[1] "|" a[2]
        decl[cle] = decl[cle] (decl[cle] == "" ? "" : ", ") a[3] " " a[4]
        nb[cle]++
        nid++; id_set[nid] = a[1]; id_bind[nid] = a[2]; id_nom[nid] = a[4]
      }
      close(fsh)
    }
    {
      st = $1; bd = $2; nom = $3
      cle = st "|" bd
      if (nb[cle] == 0) {
        # LE SEUL VERDICT DE D3, et il est exact : le C++ pousse une ressource
        # vers un slot qu AUCUN shader du corpus ne declare.
        sup = (sanset[bd] != ""                ? " — MAIS des shaders SANS set declarent binding=" bd " : " sanset[bd]                  " (OpenGL n a pas de sets ; l outil ne peut pas dire si c est le meme slot)"                : " — et aucune declaration SANS set ne porte ce numero non plus")
        printf "D3|NkResources::%s|%s:set=%s,binding=%s|sans-promesse|-|le C++ LIE ce slot ; aucun shader QUALIFIE par un set ne le declare%s\n",
               nom, "Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkResources.h", st, bd, sup
        next
      }
      # INFORMATION, PAS VERDICT : un identifiant de shader qui partage un jeton
      # porteur avec CE binding, mais vit a un AUTRE slot. Ca ne prouve rien —
      # ca montre ou regarder.
      for (i = 1; i <= nid; i++) {
        if (id_set[i] == st && id_bind[i] == bd) continue
        j = partage(nom, id_nom[i])
        if (j == "") continue
        # ON NE PARLE QUE SI LE SLOT DU C++ PORTE AUTRE CHOSE. Si le meme jeton
        # se retrouve deja dans ce que ce slot declare, il n y a rien a montrer :
        # le nom vit ailleurs AUSSI, c est tout. Sans ce filtre, l information
        # sortait 32 lignes dont la plupart ne montraient rien — et une
        # information qu on ne lit plus vaut un silence.
        occupe = 0
        m = split(decl[cle], dd, ", ")
        for (q = 1; q <= m; q++) {
          nn = dd[q]; sub(/^[^ ]+ /, "", nn)
          if (partage(nom, nn) != "") { occupe = 1; break }
        }
        if (occupe) continue
        if (j != "")
          # PARENTHESES OBLIGATOIRES : printf fmt, a, b >> f se lit
          # « printf fmt, a, (b >> f) » — le dernier argument est pris pour une
          # comparaison et le fichier reste VIDE. Un rapport vide se lit comme
          # « rien a signaler » : la forme de mensonge la plus courante de ce
          # chantier, obtenue ici par une paire de parentheses absentes.
          printf("%s (set=%s,binding=%s)  <->  %s (set=%s,binding=%s)  jeton commun: %s ; ici le slot du C++ porte: %s\n",
                 nom, st, bd, id_nom[i], id_set[i], id_bind[i], j, decl[cle]) >> finfos
      }
    }
  ' "$TMP/d3_cpp.txt" | sort >> "$TMP/detectes.txt"
  sort -u "$D3_INFOS" -o "$D3_INFOS" 2>/dev/null || true
  sort "$TMP/detectes.txt" -o "$TMP/detectes.txt"
fi

NB_DET=$(grep -c '' < "$TMP/detectes.txt")

# -- Controle positif d'instrument -------------------------------------------
manque=""
grep -q "^D1|$TEMOIN_D1|" "$TMP/detectes.txt" || manque="$manque $TEMOIN_D1(D1)"
grep -q "^D2|$TEMOIN_D2|" "$TMP/detectes.txt" || manque="$manque $TEMOIN_D2(D2)"
if [ -n "$manque" ]; then
  dire2 "[cap] ECHEC D'INSTRUMENT : temoin(s) non retrouve(s) :$manque"
  dire2 "[cap]   Ces deux symboles ont ete MESURES creux le 2026-08-22. Leur"
  dire2 "[cap]   disparition veut dire l'une de deux choses :"
  dire2 "[cap]     - ils ont ete implementes  -> mets le temoin a jour, c'est une bonne nouvelle ;"
  dire2 "[cap]     - le detecteur ne detecte plus -> tout le reste de cette sortie est faux."
  dire2 "[cap]   Sans temoin, ces deux cas produisent la MEME sortie verte."
  exit 2
fi

# =============================================================================
# 6. LECTURE DE config/capacites.list
# =============================================================================
# Format : <symbole> | <detecteur> | <classe> | <date> | <note>
declare -A CL_CLASSE=() CL_DATE=() CL_NOTE=()
declare -a CL_ORDRE=()
ROGNE=""
rogner() { local s="${1:-}"; s="${s#"${s%%[![:space:]]*}"}"; s="${s%"${s##*[![:space:]]}"}"; ROGNE="$s"; }

# -- LE CLIQUET DE `A-DATER` (arbitrage de Rodolf, R4-1) ---------------------
# `A-DATER` a le droit d'exister, mais SON COMPTE NE PEUT QUE DESCENDRE.
#
# Le raisonnement, et il n'est pas de moi : dater a la place de Rodolf est
# impossible, et passer 24 drapeaux a `false` sans lui serait une decision
# PRODUIT deguisee en hygiene. Le cliquet resout les deux : le stock est un
# HERITAGE qui ne se renouvelle pas, et toute capacite NOUVELLE est datee a la
# naissance, sans exception. La dette ne grossit pas pendant qu'on la date.
#
# Le plafond est une DONNEE, declaree dans le fichier de classement :
#     # PLAFOND-A-DATER = 24
# Le modifier est un geste delibere qui apparait dans un diff. C'est le point.
#
# ⚠️ SON ABSENCE EST UN ECHEC D'INSTRUMENT, pas un defaut : un cliquet absent et
# un cliquet satisfait produisent la meme sortie verte. C'est la meme phrase que
# pour les temoins, et elle vaut ici aussi.
PLAFOND_A_DATER=""
while IFS= read -r ligne; do
  case "$ligne" in
    \#*PLAFOND-A-DATER*)
      PLAFOND_A_DATER="${ligne##*=}"
      PLAFOND_A_DATER="${PLAFOND_A_DATER//[![:digit:]]/}"
      ;;
  esac
done < "$LISTE"
if [ -z "$PLAFOND_A_DATER" ]; then
  dire2 "[cap] ECHEC D'INSTRUMENT : aucune directive « # PLAFOND-A-DATER = N » dans $LISTE."
  dire2 "[cap]   Le cliquet de A-DATER n'existe donc pas, et un cliquet absent produit"
  dire2 "[cap]   exactement la meme sortie verte qu'un cliquet satisfait."
  dire2 "[cap]   Ajoute la ligne avec le compte actuel, puis fais-la DESCENDRE."
  exit 2
fi

num=0
while IFS= read -r ligne; do
  num=$((num + 1))
  case "$ligne" in ''|\#*) continue ;; esac
  IFS='|' read -r sym det classe date note <<< "$ligne"
  rogner "${sym:-}";    sym="$ROGNE"
  rogner "${det:-}";    det="$ROGNE"
  rogner "${classe:-}"; classe="$ROGNE"
  rogner "${date:-}";   date="$ROGNE"
  rogner "${note:-}";   note="$ROGNE"
  [ -z "$sym" ] && continue
  cle="$det|$sym"
  if [ -n "${CL_CLASSE[$cle]:-}" ]; then
    dire2 "[cap] $LISTE ligne $num : $cle declare deux fois."
    exit 2
  fi
  case "$classe" in
    implementee|vision-assumee|dette-datee|morte-a-retirer) ;;
    *) dire2 "[cap] $LISTE ligne $num : classe inconnue « $classe » pour $sym"
       dire2 "[cap]   attendu : implementee | vision-assumee | dette-datee | morte-a-retirer"
       exit 2 ;;
  esac
  CL_CLASSE["$cle"]="$classe"; CL_DATE["$cle"]="$date"; CL_NOTE["$cle"]="$note"
  CL_ORDRE+=("$cle")
done < "$LISTE"

# =============================================================================
# 7. LA GARDE
# =============================================================================
declare -a NON_CLASSES=() INTERDITS=() SANS_DATE=() ORPHELINES=() A_DATER=()
declare -A VU=()
declare -A NB_PAR_CLASSE=()

while IFS='|' read -r det sym ou promesse val note; do
  cle="$det|$sym"
  VU["$cle"]=1
  cl="${CL_CLASSE[$cle]:-}"
  if [ -z "$cl" ]; then
    NON_CLASSES+=("$det|$sym|$ou|$promesse|$val|$note")
    continue
  fi
  NB_PAR_CLASSE["$cl"]=$(( ${NB_PAR_CLASSE[$cl]:-0} + 1 ))
  # -- REGLE (a) : `vision-assumee` interdit des qu'un drapeau annonce `true`
  if [ "$cl" = "vision-assumee" ]; then
    case "$det:$promesse" in
      D1:*|*:promesse-true) INTERDITS+=("$det|$sym|$promesse|$note") ;;
    esac
  fi
  # -- Une dette sans echeance n'est pas une dette datee.
  #    VIDE = un oubli -> ECHEC.
  #    A-DATER = une echeance qui appartient a Rodolf et que je n'invente pas.
  #    Les deux se ressemblent et ne veulent PAS dire la meme chose : inventer
  #    une date pour faire taire l'outil serait produire un chiffre credible et
  #    faux, precisement ce qu'on traque. Mais une dette qu'on ne mentionne plus
  #    est une dette oubliee : A-DATER est NOMMEE a chaque passage.
  if [ "$cl" = "dette-datee" ]; then
    case "${CL_DATE[$cle]}" in
      "")        SANS_DATE+=("$det|$sym") ;;
      A-DATER)   A_DATER+=("$det|$sym") ;;
    esac
  fi
done < "$TMP/detectes.txt"

# -- UNE DISPARITION NE DIT PAS POURQUOI ELLE A EU LIEU (27/08) ---------------
# Le message d origine disait « il a ete implemente ou retire » : DEUX causes
# opposees dans une seule phrase, et aucune n etait distinguee.
#   IMPLEMENTE  la capacite est honoree -> excellente nouvelle (B4 : 145 -> 144)
#   RETIRE      le champ n existe plus  -> la ligne est perimee, a supprimer
#   ⚠️ ET UNE TROISIEME, QUE PERSONNE NE VOYAIT : une LECTURE CREUSE ajoutee.
#      Le critere de D2 est « ce champ est-il lu ? », et JOURNALISER est une
#      lecture. Un logger.Warn suffit a faire sortir n importe quelle dette de la
#      liste des candidats sans rien honorer du tout.
# On ne peut pas juger la qualite d une lecture -- mesure du 27/08 : sur 6433
# lignes de journal, 17,6 % portent AUSSI un if/return/affectation, donc la
# distinction se trompe une fois sur six. MAIS ON PEUT REFUSER QU UNE DISPARITION
# PASSE INAPERCUE.
# La seule chose mecanisable sans deviner : le symbole est-il ENCORE DECLARE ?
awk -F'|' '{ print $1 "::" $2 }' "$TMP/champs.txt" "$TMP/virtuelles.txt" 2>/dev/null | sort -u > "$TMP/declares.txt"

declare -a ORPH_A_EXAMINER=() ORPH_TRANCHEE=() ORPH_PERIMEE=()
for cle in "${CL_ORDRE[@]}"; do
  [ -n "${VU[$cle]:-}" ] && continue
  ORPHELINES+=("$cle")
  det="${cle%%|*}"; sym="${cle#*|}"
  # D3 et HUM ne tirent pas leurs symboles des en-tetes : hors mecanisme.
  case "$det" in D3|HUM) continue ;; esac
  if grep -qxF -- "$sym" "$TMP/declares.txt"; then
    # le symbole EXISTE toujours et n est plus candidat : quelqu un doit dire pourquoi
    if [ "${CL_CLASSE[$cle]}" = "dette-datee" ]; then
      ORPH_A_EXAMINER+=("$cle")
    else
      ORPH_TRANCHEE+=("$cle")
    fi
  else
    # le symbole a disparu du code : la ligne est perimee, ce n est pas un defaut
    ORPH_PERIMEE+=("$cle")
  fi
done

# -- LE CLIQUET SE SERRE, IL NE SE DESSERRE PAS ------------------------------
CLIQUET_ROMPU=0
CLIQUET_LACHE=0
if [ "${#A_DATER[@]}" -gt "$PLAFOND_A_DATER" ]; then
  CLIQUET_ROMPU=1
elif [ "${#A_DATER[@]}" -lt "$PLAFOND_A_DATER" ]; then
  CLIQUET_LACHE=1
fi

# =============================================================================
# 8. RAPPORT
# =============================================================================
if [ "$NOUVEAUX_SEULS" -eq 1 ]; then
  for l in "${NON_CLASSES[@]:-}"; do [ -n "$l" ] && dire "$l"; done
  [ "${#NON_CLASSES[@]}" -gt 0 ] && exit 4
  exit 0
fi

dire "-- Capacites : detection ---------------------------------------------"
dire "[cap]     corpus : $(grep -c '' < "$TMP/motifs.txt") symbole(s) suivis, $NB_HITS occurrence(s) lues en une passe"
dire "[cap]     temoins : $TEMOIN_D1 (D1) et $TEMOIN_D2 (D2) toujours detectes"
dire "[cap]     $NB_DET candidat(s) detecte(s), ${#CL_ORDRE[@]} ligne(s) de classement"

if [ "$LISTE_SEULE" -eq 1 ]; then
  dire ""
  dire "-- Candidats detectes (--liste) --------------------------------------"
  while IFS='|' read -r det sym ou promesse val note; do
    printf '  %-3s %-46s %-14s %s\n' "$det" "$sym" "${CL_CLASSE[$det|$sym]:-NON-CLASSE}" "$note"
  done < "$TMP/detectes.txt"
  dire ""
  dire " duree : $(( $(date +%s) - T0 ))s"
  exit 0
fi

RC=0

if [ "${#ORPH_TRANCHEE[@]}" -gt 0 ]; then
  dire "[cap]     info ${#ORPH_TRANCHEE[@]} ligne(s) dont le symbole n'est plus detecte, et qui"
  dire "[cap]          PORTENT DEJA une reponse (implementee, vision-assumee, morte) :"
  for c in "${ORPH_TRANCHEE[@]}"; do dire "[cap]          - ${c#*|}   (${c%%|*}, ${CL_CLASSE[$c]})"; done
fi
if [ "${#ORPH_PERIMEE[@]}" -gt 0 ]; then
  dire "[cap]     info ${#ORPH_PERIMEE[@]} ligne(s) PERIMEE(S) : le symbole n'est plus declare du tout."
  dire "[cap]          Le champ a ete supprime du code. La ligne peut partir — ce n'est"
  dire "[cap]          PAS un defaut, et c'est pourquoi ceci reste une information."
  for c in "${ORPH_PERIMEE[@]}"; do dire "[cap]          - ${c#*|}   (${c%%|*})"; done
fi
if [ "${#ORPH_A_EXAMINER[@]}" -gt 0 ]; then
  dire2 ""
  dire2 "[cap]     ECHEC : ${#ORPH_A_EXAMINER[@]} DETTE(S) ONT DISPARU DE LA DETECTION SANS REPONSE."
  for c in "${ORPH_A_EXAMINER[@]}"; do dire2 "[cap]       - ${c#*|}   (${c%%|*})"; done
  dire2 "[cap]     Le symbole est TOUJOURS DECLARE, la ligne dit toujours « dette-datee »,"
  dire2 "[cap]     et le detecteur ne le voit plus. Trois causes possibles, et elles"
  dire2 "[cap]     n'ont pas du tout le meme sens :"
  dire2 "[cap]       - la capacite a ete HONOREE           -> reclasse « implementee »,"
  dire2 "[cap]         et la note doit dire OU est la lecture. C'est une bonne nouvelle."
  dire2 "[cap]       - une LECTURE CREUSE a ete ajoutee    -> un logger.Warn suffit a faire"
  dire2 "[cap]         sortir une dette de la liste sans rien honorer. Le drapeau ne fait"
  dire2 "[cap]         toujours rien, et le cliquet aurait descendu pour rien."
  dire2 "[cap]       - le detecteur s'est casse            -> voir les temoins D1/D2."
  dire2 "[cap]     ⚠️ ON NE PEUT PAS JUGER LA QUALITE D'UNE LECTURE — mesure du 27/08 :"
  dire2 "[cap]     17,6 % des lignes de journal portent AUSSI un if/return/affectation."
  dire2 "[cap]     MAIS ON PEUT REFUSER QU'UNE DISPARITION PASSE INAPERCUE. Ce controle"
  dire2 "[cap]     ne tranche rien : il exige qu'un humain regarde le moment ou la"
  dire2 "[cap]     capacite quitte la liste. Un faux vert n'est dangereux que tant que"
  dire2 "[cap]     personne ne voit l'instant ou il nait."
  RC=4
fi

if [ "${#NON_CLASSES[@]}" -gt 0 ]; then
  dire2 ""
  dire2 "[cap]     ECHEC DE CLASSEMENT : ${#NON_CLASSES[@]} capacite(s) detectee(s) SANS LIGNE :"
  for l in "${NON_CLASSES[@]}"; do
    IFS='|' read -r det sym ou promesse val note <<< "$l"
    dire2 "[cap]       - $sym"
    dire2 "[cap]           detecteur $det, $ou"
    dire2 "[cap]           $note"
    if [ "$det" = "D1" ] || [ "$promesse" = "promesse-true" ]; then
      dire2 "[cap]           ⚠️ un drapeau annonce « true » a du code : « vision-assumee » est INTERDIT ici."
    fi
  done
  dire2 "[cap]     Ajoute une ligne dans $LISTE pour chacune :"
  dire2 "[cap]       <symbole> | <detecteur> | <classe> | <date> | <note>"
  dire2 "[cap]     classe : implementee | vision-assumee | dette-datee | morte-a-retirer"
  dire2 "[cap]     Dire « le detecteur se trompe, la lecture est ici » est une reponse"
  dire2 "[cap]     valide — ne rien dire n'en est pas une. Le detecteur ne juge pas."
  RC=4
fi

if [ "${#INTERDITS[@]}" -gt 0 ]; then
  dire2 ""
  dire2 "[cap]     CLASSEMENT INTERDIT : ${#INTERDITS[@]} symbole(s) classe(s) « vision-assumee »"
  dire2 "[cap]     alors qu'un drapeau annonce « true » a du code :"
  for l in "${INTERDITS[@]}"; do
    IFS='|' read -r det sym promesse note <<< "$l"
    dire2 "[cap]       - $sym   ($det — $note)"
  done
  dire2 "[cap]     Un « true » n'est pas une declaration d'intention : c'est une reponse"
  dire2 "[cap]     a une question que le code pose a l'execution, et dont il va se"
  dire2 "[cap]     servir pour choisir un chemin. DEUX SORTIES SEULEMENT :"
  dire2 "[cap]       - le drapeau rend « false » ;"
  dire2 "[cap]       - ou la capacite s'implemente."
  dire2 "[cap]     La classer « vision » et passer rendrait acceptable exactement la"
  dire2 "[cap]     classe de mensonge que ce chantier existe pour traquer."
  RC=4
fi

if [ "${#SANS_DATE[@]}" -gt 0 ]; then
  dire2 ""
  dire2 "[cap]     DETTE SANS DATE : ${#SANS_DATE[@]} symbole(s) classe(s) « dette-datee »"
  dire2 "[cap]     avec un champ date vide. Une dette sans echeance n'est pas datee ;"
  dire2 "[cap]     c'est une vision qui n'ose pas dire son nom."
  for l in "${SANS_DATE[@]}"; do dire2 "[cap]       - ${l#*|}"; done
  RC=4
fi

if [ "${#A_DATER[@]}" -gt 0 ] || [ "$CLIQUET_ROMPU" -eq 1 ]; then
  dire ""
  dire "-- Dettes en attente d'echeance (A-DATER) — CLIQUET $PLAFOND_A_DATER ------------------"
  dire "  ${#A_DATER[@]} capacite(s) reconnue(s) comme dette, dont la date appartient a"
  dire "  Rodolf. Nommees a chaque passage : une dette qu'on ne mentionne plus est"
  dire "  une dette oubliee. Ce n'est PAS un echec — c'est une question ouverte."
  for l in "${A_DATER[@]}"; do dire "    ${l#*|}   (${l%%|*})"; done
fi

if [ "$CLIQUET_ROMPU" -eq 1 ]; then
  dire2 ""
  dire2 "[cap]     CLIQUET ROMPU : ${#A_DATER[@]} dette(s) « A-DATER » pour un plafond de $PLAFOND_A_DATER."
  dire2 "[cap]     Le stock d'echeances non fixees est un HERITAGE : il peut descendre,"
  dire2 "[cap]     il ne peut pas MONTER. Toute capacite nouvelle se date a la naissance,"
  dire2 "[cap]     sans exception — sinon la dette grossit exactement pendant qu'on la date."
  dire2 "[cap]     Deux issues, et pas une troisieme :"
  dire2 "[cap]       - donne une date reelle a la (aux) nouvelle(s) ;"
  dire2 "[cap]       - ou classe-la autrement (implementee / morte-a-retirer)."
  dire2 "[cap]     Relever le plafond dans $LISTE est possible — et c'est un geste"
  dire2 "[cap]     DELIBERE qui apparait dans un diff. C'est tout l'interet du cliquet :"
  dire2 "[cap]     il ne s'oppose pas a la decision, il s'oppose a la derive silencieuse."
  RC=4
fi

# -- D3 : ce qui se ressemble et ne vit pas au meme endroit ------------------
# ⚠️ INFORMATION, JAMAIS VERDICT. Un commentaire qui ment sur un numero de
# binding peut etre le commentaire qui a tort OU le code ; trancher demande de
# savoir ce que la CARTE lit, et rien ici ne le mesure. Ce bloc montre ou
# regarder, il ne dit pas qui a raison.
if [ -s "${D3_INFOS:-/dev/null}" ]; then
  dire ""
  dire "-- D3 : meme nom, slot different (information, pas verdict) -----------"
  while IFS= read -r l; do [ -n "$l" ] && dire "    $l"; done < "$D3_INFOS"
  dire "    Ces couples partagent un jeton porteur et ne vivent pas au meme"
  dire "    (set, binding). Ca ne prouve rien — ca dit ou regarder. Trancher"
  dire "    demande de mesurer ce que la carte lit ; ce n'est pas fait ici."
fi

if [ "$CLIQUET_LACHE" -eq 1 ]; then
  dire ""
  dire "[cap]     info le cliquet peut DESCENDRE : ${#A_DATER[@]} dette(s) « A-DATER » pour un"
  dire "[cap]          plafond de $PLAFOND_A_DATER. Mets « # PLAFOND-A-DATER = ${#A_DATER[@]} » dans $LISTE."
  dire "[cap]          Un cliquet qu'on ne resserre jamais n'est plus un cliquet — il"
  dire "[cap]          redevient une marge, et une marge se remplit. Je ne le resserre"
  dire "[cap]          PAS tout seul : un controle qui modifie sa propre donnee sans que"
  dire "[cap]          personne ne le voie a cesse d'etre un controle."
fi

# On compte les LIGNES DE DONNEE, pas les mentions : l'en-tete du fichier parle
# lui aussi de « non-examine », et le compter serait se tromper de trois.
NB_NON_EXAMINE=$(grep -v '^[[:space:]]*#' "$LISTE" | grep -c 'non-examine')

dire ""
resume=""
for c in implementee vision-assumee dette-datee morte-a-retirer; do
  resume="$resume $c=${NB_PAR_CLASSE[$c]:-0}"
done
if [ "$RC" -eq 0 ]; then
  dire "[cap]     ok   $NB_DET candidat(s), tous classes :$resume"
  dire "[cap]          dont $NB_NON_EXAMINE porte(nt) la note « non-examine » : classes pour que la"
  dire "[cap]          garde soit totale, PAS examines un par un. Ne pas lire ce"
  dire "[cap]          remplissage comme un jugement."
else
  dire "[cap]     ECHEC —$resume ; voir ci-dessus"
fi
dire "[cap]     duree : $(( $(date +%s) - T0 ))s"
exit "$RC"

# ---- D3 : POURQUOI IL N'EST PAS LIVRE (mesure du 2026-08-22) ----------------
#
# D3 devait comparer les numeros de binding LIES cote C++ aux `layout(binding=N)`
# ECHANTILLONNES cote shader. C'est le seul detecteur inter-langage des trois, et
# celui qui vaut le plus cher a l'usage : aucun compilateur ne regarde les deux
# cotes ensemble.
#
# CE QUE J'AI MESURE, ET QUI L'ARRETE
#
#   Cote shader, tout est exact et qualifie par son set :
#     51 x layout(set=0, binding=0), 31 x layout(set=1, binding=1), etc.
#     Un binding sans son set ne veut rien dire : binding=5 existe dans les
#     quatre sets a la fois.
#
#   Cote C++, l'index de set N'EXISTE NULLE PART DANS LE CODE.
#     NkResources.h declare quatre enums — NkFrameBinding, NkObjectBinding,
#     NkMaterialBinding — et NkResources.cpp construit quatre layouts. Le lien
#     « NkFrameBinding appartient au set 0 » n'est ecrit que dans un COMMENTAIRE
#     (`// Bindings dans le set Frame (set=0)`) et suggere par un nom de membre
#     (mFrameLayout). Aucune declaration ne le porte.
#
# DONC : un detecteur qui comparerait les deux cotes devrait DEVINER
# l'appariement enum -> set. Un detecteur qui devine est un generateur de faux
# positifs, et un generateur de faux positifs se desactive le vendredi. Je
# prefere ne pas le livrer que le livrer devin.
#
# ⚠️ ET C'EST DEJA UN RESULTAT. Le fait mesure est celui-ci :
#
#     L'INDEX DE SET DES LAYOUTS STANDARD N'EST PORTE PAR AUCUNE DECLARATION.
#     Il vit dans un commentaire. Un commentaire ne peut pas se tromper — il
#     peut seulement etre faux sans que rien ne le dise.
#
# Et un indice va exactement dans ce sens, que je NE presente PAS comme un
# resultat parce que je ne l'ai pas mesure de bout en bout :
#   NkResources.cpp:205 porte « binding 5 : IBL irradiance » ; NK_BIND_IBL_IRRADIANCE
#   vaut bien 5 ; mais `tEnvIrradiance` est declare `binding=8` dans les six
#   familles de shaders. C'est un commentaire contre un litteral. Il faut le
#   mesurer, pas l'affirmer.
#
# CE QUI REND D3 LIVRABLE, ET C'EST UNE LIGNE DE CODE :
#   que chaque layout standard porte son set dans une DECLARATION —
#   par exemple `mFrameLayout` enregistre avec `NK_SET_FRAME`, ou les enums
#   regroupes dans une table `{ NK_SET_FRAME, NkFrameBinding }`. Le jour ou
#   l'appariement est une donnee du code, D3 se calcule exactement et je l'ecris.
#
# ---- FIN D3 ------------------------------------------------------------------
