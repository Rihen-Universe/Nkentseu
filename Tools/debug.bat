@echo off
REM @File debug.bat
REM @Description Débogage des binaires sous Windows
REM @Usage debug.bat [--project=<NomProjet>]
REM @Example: debug.bat --project=Unitest

setlocal enabledelayedexpansion

set "CONFIG=Debug"
set "PROJECT_NAME=Nova"
set "SYSTEM=windows"
set "ARCH=x86_64"

:loop
if "%~1"=="" goto endloop
if /i "%~1"=="--project" (
    set "PROJECT_NAME=%~2"
    shift
)
shift
goto loop
:endloop

set "BUILD_DIR=Build\bin\%CONFIG%-%SYSTEM%-%ARCH%\%PROJECT_NAME%"
set "EXECUTABLE=%BUILD_DIR%\%PROJECT_NAME%.exe"

if not exist "%EXECUTABLE%" (
    echo Erreur: Fichier debug introuvable - %EXECUTABLE%
    echo Compilez d'abord en mode Debug avec: build.bat debug --project=%PROJECT_NAME%
    exit /b 1
)

echo Lancement du debugger LLDB...
lldb "%EXECUTABLE%"

exit /b %errorlevel%