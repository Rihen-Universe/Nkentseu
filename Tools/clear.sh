#!/bin/bash
# @File clean.sh
# @Description Script de nettoyage des artefacts de compilation pour Linux/macOS
# @Usage ./clean.sh
# @Examples:
#   ./clean.sh  - Supprime tous les fichiers générés
# @Author [Votre Nom]
# @Date [2025-01-20]
# @License Rihen

##############################################################################
# Initialisation
##############################################################################
# @Section Démarrage
# @Description Affiche le message de début et vérifie les permissions
echo "Nettoyage des builds..."

##############################################################################
# Suppression des répertoires
##############################################################################
# @Section Nettoyage Répertoires
# @Description Supprime les dossiers de build et binaires
# @CriticalOperation - Suppression récursive irréversible
rm -rf Build Bin .obj

##############################################################################
# Nettoyage des Makefiles
##############################################################################
# @Section Fichiers Générés
# @Description Supprime tous les fichiers de configuration Make
# @Patterns:
#   - *.make: Fichiers intermédiaires
#   - Makefile: Fichier principal
find . -name "*.make" -type f -delete
find . -name "Makefile" -type f -delete
rm -f Makefile

##############################################################################
# Finalisation
##############################################################################
# @Section Confirmation
# @Description Affiche le statut final de l'opération
echo "Nettoyage terminé"

# Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
# Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
# © Rihen 2025 - Tous droits réservés.