#!/bin/bash
# @File debug.sh
# @Description Débogage des binaires Linux/macOS
# @Usage ./debug.sh [--project=<NomProjet>]
# @Example: ./debug.sh --project=Unitest

CONFIG="Debug"
PROJECT_NAME="Nova"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --project=*)
            PROJECT_NAME="${1#*=}"
            ;;
        *)
            echo "Paramètre ignoré: $1"
            ;;
    esac
    shift
done

PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
[[ "$PLATFORM" == "darwin" ]] && PLATFORM="macosx"
ARCH="x86_64"

BUILD_DIR="Build/bin/${CONFIG}-${PLATFORM}-${ARCH}/${PROJECT_NAME}"
EXEC_PATH="${BUILD_DIR}/${PROJECT_NAME}"

if [[ ! -x "$EXEC_PATH" ]]; then
    echo "Erreur: Exécutable debug introuvable - $EXEC_PATH"
    echo "Compilez d'abord en mode Debug avec: ./build.sh debug --project=${PROJECT_NAME}"
    exit 1
fi

echo "Lancement du debugger LLDB..."
lldb "${EXEC_PATH}"

exit $?