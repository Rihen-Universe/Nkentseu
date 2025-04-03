@echo off
REM @File clean.bat
REM @Description Script de nettoyage des artefacts de compilation pour Windows
REM @Usage clean.bat
REM @Examples:
REM   clean.bat  - Supprime tous les fichiers générés
REM @Author [Votre Nom]
REM @Date [2025-01-20]
REM @License Rihen

REM #########################################################################
REM # Initialisation
REM #########################################################################
REM @Section Démarrage
REM @Description Affiche le message de début de processus
echo Nettoyage des builds...

REM #########################################################################
REM # Suppression des Répertoires
REM #########################################################################
REM @Section Nettoyage Répertoires
REM @Description Suppression sécurisée des dossiers de build
REM @CriticalOperation - Utilisation de rmdir avec suppression forcée
if exist Build rmdir /s /q Build >nul 2>&1
if exist Bin rmdir /s /q Bin >nul 2>&1
if exist .obj rmdir /s /q .obj >nul 2>&1

REM #########################################################################
REM # Nettoyage des Fichiers Générés
REM #########################################################################
REM @Section Fichiers Make
REM @Description Suppression des fichiers de configuration
REM @Patterns:
REM   - *.make: Fichiers intermédiaires de compilation
REM   - Makefile: Fichier principal de build
del /q /s *.make >nul 2>&1
del /q /s Makefile >nul 2>&1
if exist Makefile del Makefile >nul 2>&1

REM #########################################################################
REM # Finalisation
REM #########################################################################
REM @Section Confirmation
REM @Description Affiche le résultat final de l'opération
echo Nettoyage terminer

exit /b 0

REM Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
REM Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
REM © Rihen 2025 - Tous droits réservés.