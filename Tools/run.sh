#!/bin/bash
# @File run.sh
# @Description Exécution des binaires compilés sur Linux/macOS
# @Usage ./run.sh [--config=<Debug|Release>] [--project=<NomProjet>] [args...]
# @Examples:
#   ./run.sh --config=Release --project=Nova
#   ./run.sh --project=Unitest -- arg1 arg2
# @Author [Votre Nom]
# @Date [2025-01-20]
# @License Rihen

##############################################################################
# Configuration Initiale
##############################################################################
# @Section Default Values
# @Description Paramètres d'exécution par défaut
CONFIG="Debug"
PROJECT_NAME="Nova"
declare -a ARGS=()

##############################################################################
# Traitement des Arguments
##############################################################################
# @Section Argument Handling
# @Description Analyse des paramètres et arguments utilisateur
while [[ $# -gt 0 ]]; do
    case "$1" in
        --config=*)
            CONFIG="${1#*=}"
            ;;
        --project=*)
            PROJECT_NAME="${1#*=}"
            ;;
        --)
            shift
            ARGS+=("$@")
            break
            ;;
        *)
            ARGS+=("$1")
            ;;
    esac
    shift
done

##############################################################################
# Normalisation des Paramètres
##############################################################################
# @Section Configuration Normalization
# @Description Standardisation des valeurs de configuration
CONFIG=$(echo "$CONFIG" | awk '{print toupper(substr($0,1,1)) tolower(substr($0,2))}')
PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
[[ "$PLATFORM" == "darwin" ]] && PLATFORM="macosx"
ARCH="x86_64"

##############################################################################
# Construction du Chemin
##############################################################################
# @Section Path Generation
# @Description Construction dynamique du chemin d'accès
BUILD_DIR="Build/bin/${CONFIG}-${PLATFORM}-${ARCH}"
EXEC_PATH="${BUILD_DIR}/${PROJECT_NAME}/${PROJECT_NAME}"

##############################################################################
# Exécution du Programme
##############################################################################
# @Section Program Execution
# @Description Lancement contrôlé du binaire compilé
echo "Exécution de $EXEC_PATH..."
if [[ -x "$EXEC_PATH" ]]; then
    "$EXEC_PATH" "${ARGS[@]}"
else
    echo "Erreur: Aucune application trouvée pour le projet '$PROJECT_NAME'"
    echo "Projets disponibles dans ${BUILD_DIR}:"
    ls -1 "${BUILD_DIR}"
    exit 1
fi

exit $?


# Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
# Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
# © Rihen 2025 - Tous droits réservés.