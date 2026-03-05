@echo off
REM @File build.bat
REM @Description Script principal de compilation Windows avec gestion dynamique des paramètres
REM @Usage build [debug|release] [llvm] (ordre libre)
REM @Examples:
REM   build llvm debug    - Compilation debug avec LLVM
REM   build release       - Compilation release standard
REM   build debug llvm    - Même effet que la première commande
REM @Author [Votre Nom]
REM @Date [2025-01-20]
REM @License Rihen

setlocal enabledelayedexpansion

REM ##########################################################################
REM # Configuration Initiale
REM ##########################################################################
REM @Section Configuration
REM @Description Définit les chemins par défaut et les paramètres de base
set "CONFIG=debug_x64"
set "USE_LLVM=0"
set "MINGW_MAKE=C:\Install\mingw\ucrt\bin\mingw32-make.exe"
set "DEFAULT_CLANG_BIN=C:\Install\mingw\ucrt\bin\"
set "LLVM_BIN=C:\Program Files\LLVM\bin\"

REM ##########################################################################
REM # Traitement des Arguments
REM ##########################################################################
REM @Section Arguments
REM @Description Analyse dynamique des paramètres d'entrée
REM @Param %* - Liste des arguments fournis
for %%a in (%*) do (
    if /i "%%a"=="debug" set "CONFIG=debug_x64"
    if /i "%%a"=="release" set "CONFIG=release_x64"
    if /i "%%a"=="llvm" (
        set "CLANG_BIN=!LLVM_BIN!"
        set "USE_LLVM=1"
    )
)

REM @Section Fallback Configuration
REM @Description Applique la configuration par défaut si non spécifiée
if not defined CLANG_BIN set "CLANG_BIN=!DEFAULT_CLANG_BIN!"

REM ##########################################################################
REM # Vérification des Outils
REM ##########################################################################
REM @Function Vérification Outils
REM @Description Contrôle la présence des exécutables nécessaires
if not exist "!MINGW_MAKE!" (
    echo [ERREUR] mingw32-make introuvable
    exit /b 1
)

if not exist "!CLANG_BIN!clang++.exe" (
    echo [ERREUR] clang++ introuvable dans !CLANG_BIN!
    exit /b 1
)

if !USE_LLVM! equ 1 if not exist "!CLANG_BIN!llvm-ar.exe" (
    echo [ERREUR] llvm-ar introuvable dans !CLANG_BIN!
    exit /b 1
)

REM ##########################################################################
REM # Construction de la Commande
REM ##########################################################################
REM @Section Toolchain Configuration
REM @Description Sélection dynamique des outils de compilation
set "AR_TOOL=!CLANG_BIN!ar.exe"
if !USE_LLVM! equ 1 set "AR_TOOL=!CLANG_BIN!llvm-ar.exe"

echo #############################################
echo # Compilation en mode !CONFIG! - Toolchain: !CLANG_BIN!
echo #############################################

REM ##########################################################################
REM # Exécution de la Compilation
REM ##########################################################################
REM @Section Compilation
REM @Description Lancement du processus de build avec les paramètres
"!MINGW_MAKE!" -j4 ^
    CC="!CLANG_BIN!clang.exe" ^
    CXX="!CLANG_BIN!clang++.exe" ^
    AR="!AR_TOOL!" ^
    config=!CONFIG!

REM ##########################################################################
REM # Gestion des Erreurs
REM ##########################################################################
REM @Section Error Handling
REM @Description Contrôle du résultat de la compilation
if !errorlevel! neq 0 (
    echo [ERREUR] Echec compilation
    exit /b 1
)

echo [SUCCES] Build reussi: Build\!CONFIG!
exit /b 0

REM Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
REM Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
REM © Rihen 2025 - Tous droits réservés.