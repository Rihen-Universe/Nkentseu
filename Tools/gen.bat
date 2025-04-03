@echo off
REM @File gen.bat
REM @Description Génération des fichiers de projet Windows avec premake5 local
REM @Usage gen [--arch=avx2|sse4|none]
REM @Examples:
REM   gen --arch=avx2  - Génération avec optimisations AVX2
REM   gen             - Génération standard sans optimisations
REM @Author [Votre Nom]
REM @Date [2025-01-20]
REM @License Rihen

setlocal enabledelayedexpansion
set ROOT_DIR=%~dp0..
set "PREMAKE_BIN=%ROOT_DIR%\External\Bin\Premake5\premake5.exe"

REM #########################################################################
REM # Initialisation des Paramètres
REM #########################################################################
REM @Section Default Values
REM @Description Valeur par défaut pour l'architecture
set "ARCH=none"

REM #########################################################################
REM # Traitement des Arguments
REM #########################################################################
REM @Section Argument Handling
REM @Description Analyse des paramètres de ligne de commande
for %%A in (%*) do (
    if "%%A"=="--arch=avx2" set "ARCH=avx2"
    if "%%A"=="--arch=sse4" set "ARCH=sse4"
)

REM #########################################################################
REM # Vérification des Outils
REM #########################################################################
REM @Section Tool Verification
REM @Description Contrôle de la présence des exécutables requis
if not exist "%PREMAKE_BIN%" (
    echo Erreur : premake5 introuvable à %PREMAKE_BIN%
    exit /b 1
)

REM #########################################################################
REM # Génération des Fichiers
REM #########################################################################
REM @Section Build Generation
REM @Description Exécution du processus de génération
echo Génération pour arch=%ARCH%...
"%PREMAKE_BIN%" --arch=%ARCH% --os=windows gmake2
exit /b %errorlevel%

REM Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
REM Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
REM © Rihen 2025 - Tous droits réservés.