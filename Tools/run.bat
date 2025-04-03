@echo off
REM @File run.bat
REM @Description Exécution des binaires compilés sous Windows
REM @Usage run.bat [--config=<Debug|Release>] [--project=<NomProjet>]
REM @Examples:
REM   run.bat --config=Release --project=Nova
REM   run.bat --project=Unitest
REM @Author [Votre Nom]
REM @Date [2025-01-20]
REM @License Rihen

setlocal enabledelayedexpansion

@REM ##############################################################################
@REM # Configuration Initiale
@REM ##############################################################################
REM @Section Default Values
REM @Description Paramètres par défaut de l'exécution
set "CONFIG=Debug"
set "PROJECT_NAME=Nova"
set "SYSTEM=windows"
set "ARCH=x86_64"

@REM ##############################################################################
@REM # Traitement des Arguments
@REM ##############################################################################
REM @Section Argument Parsing
REM @Description Analyse des paramètres de ligne de commande
:loop
if "%~1"=="" goto endloop
if "%~1"=="--config" (
    set "CONFIG=%~2"
    shift
)
if "%~1"=="--project" (
    set "PROJECT_NAME=%~2"
    shift
)
shift
goto loop
:endloop

@REM ##############################################################################
@REM # Normalisation des Paramètres
@REM ##############################################################################
REM @Section Configuration Normalization
REM @Description Uniformisation des valeurs de configuration
if /i "!CONFIG!"=="debug" set "CONFIG=Debug"
if /i "!CONFIG!"=="release" set "CONFIG=Release"

@REM ##############################################################################
@REM # Construction du Chemin
@REM ##############################################################################
REM @Section Path Generation
REM @Description Génération dynamique du chemin du binaire
set "EXECUTABLE=Build\bin\!CONFIG!-!SYSTEM!-!ARCH!\!PROJECT_NAME!\!PROJECT_NAME!.exe"

@REM ##############################################################################
@REM # Exécution du Programme
@REM ##############################################################################
REM @Section Program Execution
REM @Description Lancement du binaire avec gestion d'erreur
echo Exécution de !EXECUTABLE!...
if exist "!EXECUTABLE!" (
    "!EXECUTABLE!"
) else (
    echo Erreur: Aucune application trouvée pour le projet "!PROJECT_NAME!"
    echo Projets disponibles dans Build\bin\!CONFIG!-!SYSTEM!-!ARCH! : 
    dir /b /ad "Build\bin\!CONFIG!-!SYSTEM!-!ARCH!"
    exit /b 1
)

exit /b %errorlevel%

REM Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
REM Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
REM © Rihen 2025 - Tous droits réservés.