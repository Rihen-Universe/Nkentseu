#!/usr/bin/env bash
#
# verif_ressources.sh — LES RESSOURCES SE TROUVENT QUEL QUE SOIT LE DOSSIER DE
# LANCEMENT (2026-10-01)
# =============================================================================
# POURQUOI IL EXISTE
#
#   « Je ne sais pas pourquoi tu n'executes pas ces applications en tenant
#     compte du dossier ressource ou vivent les shaders, textures, icones. »
#     (Rihen, 01/10)
#
#   Chaque application cherchait ses ressources a partir du DOSSIER COURANT.
#   Lancee par double-clic depuis Build/Bin/<cfg>/<App>/, elle ne trouvait
#   rien (NKCode : icones vides, carre bleu a la place du logo). Le remede est
#   NkPath::LocateResource (NKFileSystem) ; ce banc dit s'il tient.
#
# CE QU'IL FAIT
#   Il lance chaque TEMOIN SANS FENETRE (ou hors ecran) depuis TROIS dossiers :
#     (a) la racine du depot, (b) le dossier de l'exe, (c) un dossier
#     temporaire sans rapport (mktemp -d) ;
#   et rougit (code 1) si une ressource manque dans l'un d'eux.
#     NKFileSystem_Tests      le cas LocateResource (depuis la racine)
#     NKCode --ressources     data/ : polices, logo, icones, icons.cfg, langues
#                             + (d) depuis le dossier d'un PROJET DE JEU, comme
#                             quand UnkenyEditor lance NKCode sur un script
#     NKUIDesign --sonde-coquille   Resources/Interface/NKUIDesign
#     NogeDemo --capture=...  Resources/NKRenderer/Shaders (fenetre HORS ECRAN)
#   Il ne construit rien : construire d'abord (jenga build --target X).
#
# LE CONTRE-ESSAI (un banc qui n'a jamais rougi n'est qu'une intention)
#   ./verif_ressources.sh --contre-essai
#   renomme TEMPORAIREMENT Applications/NKCode/data, Resources/Interface/NKUIDesign
#   et Resources/NKRenderer/Shaders (suffixe .contre-essai), relance le banc,
#   EXIGE qu'il rougisse, puis remet les dossiers (meme en cas d'interruption).
#   Code 0 = le banc a su rougir ; 1 = il est reste vert sans ses ressources.
#
# CODES : 0 vert, 1 rouge, 2 instrument casse (exe absent, mktemp refuse).
# =============================================================================
set -u
RACINE="$(cd "$(dirname "$0")" && pwd)"
CFG="Debug-Windows"
BIN="$RACINE/Build/Bin/$CFG"
dire() { printf '%s\n' "$*"; }

if [ "${1:-}" = "--contre-essai" ]; then
  CIBLES=("Applications/NKCode/data" "Resources/Interface/NKUIDesign" "Resources/NKRenderer/Shaders")
  remettre() {
    for c in "${CIBLES[@]}"; do
      [ -e "$RACINE/$c.contre-essai" ] && [ ! -e "$RACINE/$c" ] && mv "$RACINE/$c.contre-essai" "$RACINE/$c"
    done
  }
  trap remettre EXIT INT TERM
  for c in "${CIBLES[@]}"; do
    [ -e "$RACINE/$c" ] || { dire "[ress] ECHEC D INSTRUMENT : $c absent avant le contre-essai."; exit 2; }
    mv "$RACINE/$c" "$RACINE/$c.contre-essai" || { dire "[ress] ECHEC D INSTRUMENT : renommage de $c refuse."; exit 2; }
  done
  dire "[ress] CONTRE-ESSAI : ressources retirees, le banc DOIT rougir."
  "$0"; rc=$?
  remettre; trap - EXIT INT TERM
  for c in "${CIBLES[@]}"; do
    [ -e "$RACINE/$c" ] || { dire "[ress] ECHEC : $c n'a pas ete remis."; exit 2; }
  done
  if [ "$rc" -eq 1 ]; then dire "[ress] CONTRE-ESSAI REUSSI : le banc a rougi sans ses ressources (code 1), dossiers remis."; exit 0; fi
  dire "[ress] CONTRE-ESSAI ECHOUE : le banc a rendu $rc sans ses ressources."; exit 1
fi

TMP="$(mktemp -d)" || { dire "[ress] ECHEC D INSTRUMENT : mktemp -d a echoue."; exit 2; }
trap 'rm -rf "$TMP"' EXIT
AILLEURS="$TMP/ailleurs"; mkdir -p "$AILLEURS"
TMPW="$(cygpath -w "$TMP" 2>/dev/null || echo "$TMP")"

ko=0; vus=0
# temoin NOM EXE DOSSIER MOTIF ARGS... : vert si code 0 ET le motif est lu.
temoin() {
  local nom="$1" exe="$2" ou="$3" motif="$4"; shift 4
  local journal="$TMP/$nom.log"
  [ -x "$exe" ] || { dire "[ress] ECHEC D INSTRUMENT : $exe absent (construire d'abord)."; exit 2; }
  ( cd "$ou" && timeout 240 "$exe" "$@" ) > "$journal" 2>&1
  local rc=$?
  vus=$((vus + 1))
  if [ "$rc" -eq 0 ] && grep -q -- "$motif" "$journal" && ! grep -q "ressource INTROUVABLE" "$journal"; then
    dire "  [ OK ] $nom"
  else
    ko=$((ko + 1))
    dire "  [ KO ] $nom (code $rc) -- $(grep -m1 -E 'INTROUVABLE|NON LU|KO|ECHEC|MANQUANTES' "$journal" | cut -c1-160)"
  fi
}

dire "=== verif_ressources : (a) racine, (b) dossier de l'exe, (c) $(cygpath -w "$AILLEURS" 2>/dev/null || echo "$AILLEURS") ==="
temoin "NKFileSystem_Tests" "$RACINE/Build/Tests/$CFG/NKFileSystem_Tests.exe" "$RACINE" "Tous les tests sont r"
for app in NKCode NKUIDesign NogeDemo; do
  exe="$BIN/$app/$app.exe"
  for k in a b c; do
    case $k in a) ou="$RACINE";; b) ou="$BIN/$app";; c) ou="$AILLEURS";; esac
    case $app in
      NKCode)     temoin "NKCode($k)" "$exe" "$ou" "TOUTES TROUVEES" --ressources ;;
      NKUIDesign) temoin "NKUIDesign($k)" "$exe" "$ou" "interface.nkgui : lu" --sonde-coquille ;;
      NogeDemo)   temoin "NogeDemo($k)" "$exe" "$ou" "BANC CAPTURE REUSSI" "--capture=$TMPW\\nogedemo_$k.png" ;;
    esac
  done
done
# (d) NKCode LANCE PAR UnkenyEditor (Script/NkEditeurScripts.cpp,
#     NkEditeurOuvrirTexteExterne) : le dossier courant est alors celui de
#     l'editeur ou du PROJET DU JEU, jamais celui de NKCode -- son logo et ses
#     icones etaient des carres vides sur l'ecran de chargement.
PROJET="$TMP/projet_jeu"; mkdir -p "$PROJET/Contenu/Scripts"
printf '// script de jeu\n' > "$PROJET/Contenu/Scripts/Porte.cpp"
temoin "NKCode(d: projet du jeu)" "$BIN/NKCode/NKCode.exe" "$PROJET" "TOUTES TROUVEES" --ressources
if [ "$ko" -eq 0 ]; then dire "=== VERT : $vus temoins, 0 KO ==="; exit 0; fi
dire "=== ROUGE : $ko KO sur $vus temoins ==="; exit 1
