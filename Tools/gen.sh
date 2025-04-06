#!/bin/bash
# @File gen.sh
# @Description Génération des fichiers de projet avec premake5 local
# @Usage gen [--arch=<avx2|sse4|none>]
# @Examples:
#   ./gen.sh --arch=avx2  - Génération avec optimisations AVX2
#   ./gen.sh              - Génération sans optimisations spécifiques
# @Author [Votre Nom]
# @Date [2025-01-20]
# @License Rihen

##############################################################################
# Configuration Initiale
##############################################################################
# @Section Path Setup
# @Description Définition des chemins absolus pour la robustesse
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR="$SCRIPT_DIR/.."
PREMAKE_DIR="$ROOT_DIR/External/Bins/Premake5"

##############################################################################
# Paramètres par Défaut
##############################################################################
# @Section Default Values
# @Description Valeurs initiales des paramètres configurables
ARCH="none"

##############################################################################
# Analyse des Arguments
##############################################################################
# @Section Argument Parsing
# @Description Traitement des paramètres de ligne de commande
# @Param --arch - Spécifie l'architecture cible (avx2/sse4/none)
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch=*)
            ARCH="${1#*=}"
            shift
            ;;
        *)
            echo "Argument inconnu: $1"
            exit 1
            ;;
    esac
done

##############################################################################
# Détection de l'OS
##############################################################################
# @Section OS Detection
# @Description Sélection du binaire premake5 adapté au système
case "$(uname -s)" in
    Linux*)    PREMAKE_BIN="$PREMAKE_DIR/premake5.linux";;
    Darwin*)   PREMAKE_BIN="$PREMAKE_DIR/premake5.macos";;
    *)         echo "OS non supporté"; exit 1;;
esac

##############################################################################
# Vérifications Préalables
##############################################################################
# @Section Tool Checks
# @Description Contrôle de la disponibilité des outils requis
if [ ! -f "$PREMAKE_BIN" ]; then
    echo "Erreur : premake5 introuvable à $PREMAKE_BIN"
    exit 1
fi

if [ ! -x "$PREMAKE_BIN" ]; then
    chmod +x "$PREMAKE_BIN" || exit 1
fi

##############################################################################
# Génération des Fichiers
##############################################################################
# @Section Build Generation
# @Description Exécution de premake5 avec les paramètres
echo "Génération pour arch=$ARCH"
"$PREMAKE_BIN" --arch="$ARCH" gmake2
exit $?

# Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
# Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
# © Rihen 2025 - Tous droits réservés.