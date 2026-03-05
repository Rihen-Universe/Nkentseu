#!/bin/bash
# @File build.sh
# @Description Script principal de compilation Linux/macOS avec gestion de paramètres avancée
# @Usage ./build.sh [debug|release] [llvm] (ordre libre)
# @Examples:
#   ./build.sh llvm debug    - Debug avec LLVM
#   ./build.sh release       - Release standard
#   ./build.sh llvm          - Debug LLVM par défaut
# @Author [Votre Nom]
# @Date [2025-01-20]
# @License Rihen

###############################################################################
# Configuration Initiale
###############################################################################
# @Section Base Configuration
# @Description Définit les paramètres par défaut et variables globales
CONFIG="debug_x64"
USE_LLVM=0
LLVM_SUFFIX="-14"  # Suffixe versionné pour les systèmes Linux

###############################################################################
# Traitement des Arguments
###############################################################################
# @Section Argument Parsing
# @Description Analyse intelligente des paramètres d'entrée
# @Param $@ - Liste des arguments fournis
for arg in "$@"; do
    case "${arg,,}" in
        debug)    CONFIG="debug_x64" ;;
        release)  CONFIG="release_x64" ;;
        llvm)     USE_LLVM=1 ;;
        *)        echo "[WARNING] Parametre ignore: $arg" ;;
    esac
done

###############################################################################
# Configuration de la Toolchain
###############################################################################
# @Section Toolchain Setup
# @Description Sélection dynamique des outils de compilation
if [ $USE_LLVM -eq 1 ]; then
    # @Subsection Configuration LLVM
    # @Description Utilise la toolchain LLVM versionnée
    export CC="clang${LLVM_SUFFIX}"
    export CXX="clang++${LLVM_SUFFIX}"
    export AR="llvm-ar${LLVM_SUFFIX}"
    EXTRA_FLAGS="-stdlib=libc++"
else
    # @Subsection Configuration Standard
    # @Description Utilise Clang système par défaut
    export CC="clang"
    export CXX="clang++"
    export AR="ar"
fi

###############################################################################
# Vérification des Outils
###############################################################################
# @Function check_tool
# @Description Vérifie la présence d'un outil dans le PATH
# @Param $1 - Nom de l'outil à vérifier
check_tool() {
    command -v "$1" >/dev/null 2>&1 || { 
        echo >&2 "[ERREUR] $1 introuvable"
        exit 1
    }
}

# @Section Tool Verification
# @Description Contrôle la disponibilité des outils requis
check_tool "$CC"
check_tool "$CXX"
check_tool "$AR"

###############################################################################
# Exécution de la Compilation
###############################################################################
# @Section Build Execution
# @Description Lance le processus de compilation avec les paramètres
echo "#############################################"
echo "# Compilation en mode $CONFIG - Toolchain: $CXX"
echo "#############################################"

make -j4 \
    CC="$CC" \
    CXX="$CXX" \
    AR="$AR" \
    CXXFLAGS="$EXTRA_FLAGS" \
    config="$CONFIG"

###############################################################################
# Gestion des Erreurs
###############################################################################
# @Section Error Handling
# @Description Gestion des erreurs de compilation
if [ $? -ne 0 ]; then
    echo "[ERREUR] Echec compilation"
    exit 1
fi

echo "[SUCCES] Build reussi: Build/$CONFIG"
exit 0

# Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
# Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
# © Rihen 2025 - Tous droits réservés.