@echo off
setlocal enabledelayedexpansion

REM convert_shader.bat - Convertit un fichier SPIR-V en tableau C++
REM Usage: convert_shader.bat vert.spv vert_shader.hpp

if "%1"=="" (
    echo Usage: %0 input.spv output.hpp
    echo.
    echo Exemple: %0 vert.spv vert_shader.hpp
    exit /b 1
)

if "%2"=="" (
    echo Erreur: Veuillez specifier le fichier de sortie
    echo Usage: %0 input.spv output.hpp
    exit /b 1
)

set INPUT_FILE=%1
set OUTPUT_FILE=%2

REM Vérifier que le fichier d'entrée existe
if not exist "%INPUT_FILE%" (
    echo Erreur: Le fichier %INPUT_FILE% n'existe pas
    echo.
    echo Fichiers .spv disponibles:
    dir *.spv /b
    exit /b 1
)

REM Obtenir la taille du fichier
for %%A in ("%INPUT_FILE%") do set FILE_SIZE=%%~zA

echo Conversion de %INPUT_FILE% (%FILE_SIZE% bytes) vers %OUTPUT_FILE%...

REM Créer un fichier temporaire pour le format hexadécimal
set TEMP_FILE=%TEMP%\spv_hex.txt
certutil -encodehex "%INPUT_FILE%" "%TEMP_FILE%" >nul 2>&1

if %ERRORLEVEL% neq 0 (
    echo Erreur: Impossible de lire le fichier %INPUT_FILE%
    exit /b 1
)

REM Générer l'en-tête du fichier
(
    echo // Auto-generated from %INPUT_FILE%
    echo // File size: %FILE_SIZE% bytes
    echo // Generated on %DATE% %TIME%
    echo.
    echo #include ^<stdint.h^>
    echo #include ^<array^>
    echo.
    echo namespace Shaders {
    echo     static constexpr size_t %~n1_size = %FILE_SIZE%;
    echo     static constexpr std::array^<uint32_t, %FILE_SIZE% / 4^> %~n1_code = {{
) > "%OUTPUT_FILE%"

REM Lire le fichier hexadécimal et convertir en uint32_t
set LINE_COUNT=0
set HEX_LINE=
set OUTPUT_LINE=       

for /f "skip=1 tokens=*" %%A in (%TEMP_FILE%) do (
    set "LINE=%%A"
    set "LINE=!LINE: =!"
    
    REM Traiter par groupes de 8 caractères hex (4 octets)
    set "HEX=!LINE!"
    for /l %%i in (0,8,80) do (
        if "!HEX:~%%i,8!" neq "" (
            set "WORD=!HEX:~%%i,8!"
            REM Inverser l'endianness (little-endian to big-endian pour l'affichage)
            set "REV=!WORD:~6,2!!WORD:~4,2!!WORD:~2,2!!WORD:~0,2!"
            
            REM Ajouter à la ligne de sortie
            set "OUTPUT_LINE=!OUTPUT_LINE! 0x!REV!,"
            
            set /a LINE_COUNT+=1
            set /a MOD=!LINE_COUNT! %% 8
            if !MOD! equ 0 (
                echo !OUTPUT_LINE!>> "%OUTPUT_FILE%"
                set "OUTPUT_LINE=        "
            )
        )
    )
)

REM Écrire la dernière ligne si elle n'est pas vide
if not "!OUTPUT_LINE!"=="        " (
    echo !OUTPUT_LINE:~0,-1!>> "%OUTPUT_FILE%"
)

REM Fermer le namespace
(
    echo     };
    echo } // namespace Shaders
) >> "%OUTPUT_FILE%"

REM Nettoyer
del "%TEMP_FILE%" 2>nul

echo Conversion terminee avec succes!
echo Fichier genere: %OUTPUT_FILE%
echo Taille: %FILE_SIZE% bytes, %FILE_SIZE% / 4 = %FILE_SIZE% / 4 uint32_t