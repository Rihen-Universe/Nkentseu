#!/usr/bin/env bash
# =========================================================================
# TEMOIN — noms de macros ADDITIFS (ARM32 / WEB)
# =========================================================================
#
# Arbitrage de Rodolf (2026-08-17) :
#     « WEB et EMSCRIPT sont permis. ARM32, ARM64 et ARM aussi. »
#
# Ce script compile temoin_noms_additifs.cpp sur QUATRE cibles et verifie en
# plus que la victime vivante est guerie : la cascade d'architecture de
# NkPlatform.cpp, extraite du VRAI fichier (pas recopiee), doit rendre
# NK_ARM32 sur armeabi-v7a la ou elle rendait NK_UNKNOWN.
#
# PERIMETRE ET EXCLUSIONS — imprimes a chaque execution, pas seulement ici :
#   - verifie le PREPROCESSEUR sur 4 cibles croisees. N'EXECUTE rien.
#   - ne dit RIEN de SSE42 : hors perimetre par decision (R28/R29).
#   - ne touche pas a Build/_jenga-embed/ : artefact de build, pas une source.
#
# Code de sortie : 0 = toutes les attentes tenues ; 1 = au moins une echoue.
# Il ECHOUE, il ne se contente pas d'afficher.
# =========================================================================

set -u

ICI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RACINE="$(cd "$ICI/../../../.." && pwd)"
INC="$RACINE/Kernel/Foundation/NKPlatform/src"
SRC="$ICI/temoin_noms_additifs.cpp"
PLATFORM_CPP="$RACINE/Kernel/Foundation/NKCore/src/NKCore/NkPlatform.cpp"

NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-C:/Android/ndk/27.0.12077973}}"
NDKBIN="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin"
CC_ARMV7="$NDKBIN/armv7a-linux-androideabi21-clang++"
CC_ARM64="$NDKBIN/aarch64-linux-android21-clang++"
CC_HOTE="clang++"
CC_WEB="${EMSDK_EMPP:-C:/emsdk/emsdk/upstream/emscripten/em++.bat}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

ECHECS=0
DIALECTE="-std=c++17"   # le depot est en C++17 (cppdialect("C++17"), 205 projets).
                        # Un temoin compile dans un autre dialecte mesure autre chose.

# Un compilateur est present s'il REPOND, pas s'il a le bit executable : sous
# MSYS un .bat n'est ni -x ni vu par `command -v`, et le declarer absent aurait
# rendu « NON MESURE » un outil parfaitement fonctionnel.
disponible() { "$1" --version >/dev/null 2>&1; }

echo "================================================================"
echo " TEMOIN — les DEUX noms repondent vrai (ARM32/ARM, WEB/EMSCRIPTEN)"
echo "================================================================"
echo " Perimetre : preprocesseur, 4 cibles croisees, dialecte $DIALECTE"
echo " N'EXECUTE rien : aucun materiel ARM 32 bits n'est sollicite."
echo " Exclusions : SSE42 (hors perimetre), Build/_jenga-embed/ (artefact)."
echo "----------------------------------------------------------------"

# -------------------------------------------------------------------------
# Partie A — le temoin lui-meme
# -------------------------------------------------------------------------
essai() {
	local nom="$1" ; shift
	local compilo="$1" ; shift
	if ! disponible "$compilo"; then
		echo "  [ABSENT ] $nom — compilateur introuvable : $compilo"
		echo "            NON MESURE. Ce n'est pas un succes."
		ECHECS=$((ECHECS + 1))
		return
	fi
	local sortie
	sortie="$("$compilo" $DIALECTE -fsyntax-only -I "$INC" "$@" "$SRC" 2>&1)"
	if [ $? -eq 0 ]; then
		echo "  [ OK    ] $nom"
	else
		echo "  [ ECHEC ] $nom"
		echo "$sortie" | grep -i "error" | sed 's/^/            /'
		ECHECS=$((ECHECS + 1))
	fi
}

echo " A. le temoin, sur chaque cible"
essai "armeabi-v7a  (ARM 32 bits — attend ARM ET ARM32)" "$CC_ARMV7"
essai "arm64-v8a    (controle — attend NI ARM NI ARM32)"  "$CC_ARM64"
essai "x86_64 hote  (controle — attend NI ARM NI ARM32)"  "$CC_HOTE"
essai "wasm32 web   (Emscripten — attend EMSCRIPTEN ET WEB)" "$CC_WEB"

# -------------------------------------------------------------------------
# Partie B — la victime vivante, lue dans le VRAI NkPlatform.cpp
# -------------------------------------------------------------------------
# On extrait la cascade reelle plutot que d'en recopier une : une copie
# resterait verte le jour ou l'originale changerait.
echo "----------------------------------------------------------------"
echo " B. la victime : la cascade d'architecture de NkPlatform.cpp"

CASCADE="$TMP/cascade.inc"
awk '/Determination du type d.architecture|termination du type d.architecture/{prise=1}
     prise{print}
     prise && /^#endif/{exit}' "$PLATFORM_CPP" \
  | sed 's/sPlatformInfo\.architecture *= *NkArchitectureType::\(NK_[A-Z0-9_]*\);/const char *kArch = "\1";/' \
  > "$CASCADE"

if ! grep -q "kArch" "$CASCADE"; then
	echo "  [ ECHEC ] extraction de la cascade impossible — NkPlatform.cpp a change de forme."
	echo "            NON MESURE. Corrigez l'extraction plutot que de croire le vert."
	ECHECS=$((ECHECS + 1))
else
	cat > "$TMP/victime.cpp" <<'ENTETE'
#include "NKPlatform/NkArchDetect.h"
ENTETE
	cat "$CASCADE" >> "$TMP/victime.cpp"

	victime() {
		local nom="$1" ; shift
		local compilo="$1" ; shift
		local attendu="$1" ; shift
		if ! disponible "$compilo"; then
			echo "  [ABSENT ] $nom — compilateur introuvable. NON MESURE."
			ECHECS=$((ECHECS + 1))
			return
		fi
		local obtenu
		obtenu="$("$compilo" $DIALECTE -E -I "$INC" "$TMP/victime.cpp" 2>/dev/null \
		          | grep -o 'NK_[A-Z0-9_]*' | tail -1)"
		if [ "$obtenu" = "$attendu" ]; then
			echo "  [ OK    ] $nom -> $obtenu"
		else
			echo "  [ ECHEC ] $nom -> $obtenu   (attendu : $attendu)"
			ECHECS=$((ECHECS + 1))
		fi
	}

	victime "armeabi-v7a" "$CC_ARMV7" "NK_ARM32"
	victime "arm64-v8a  " "$CC_ARM64" "NK_ARM64"
	victime "x86_64 hote" "$CC_HOTE"  "NK_X64"
fi

echo "----------------------------------------------------------------"
if [ "$ECHECS" -eq 0 ]; then
	echo " RESULTAT : les deux noms repondent vrai, sans sur-definition."
	exit 0
fi
echo " RESULTAT : $ECHECS attente(s) non tenue(s)."
exit 1
