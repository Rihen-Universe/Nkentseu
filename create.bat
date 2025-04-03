@echo off
setlocal enabledelayedexpansion

:: Définition des chemins racine
set ROOT_DIR=%~dp0
set CORE_DIR="%ROOT_DIR%\Core\src\Core"

set LOGGER_DIR="%CORE_DIR%\Logger"
set PLATFORM_DIR="%CORE_DIR%\Platform"

:: Création de l'arborescence CORE
for %%D in (Containers Text System Utilities Logger Platform) do (
    if not exist "%CORE_DIR%\%%D" mkdir "%CORE_DIR%\%%D"
)

:: Fichiers pour les containers
for %%F in (Vector.h List.h ArrayList.h LinkedList.h) do (
    if not exist "%CORE_DIR%\Containers\%%F" @echo. > "%CORE_DIR%\Containers\%%F"
)

:: Fichiers pour le texte
for %%F in (String.cpp String.h) do (
    if not exist "%CORE_DIR%\Text\%%F" @echo. > "%CORE_DIR%\Text\%%F"
)

:: Fichiers système
for %%F in (Memory.cpp Memory.h) do (
    if not exist "%CORE_DIR%\System\%%F" @echo. > "%CORE_DIR%\System\%%F"
)

:: Utilitaires
for %%F in (Algorithms.h MathUtils.h) do (
    if not exist "%CORE_DIR%\Utilities\%%F" @echo. > "%CORE_DIR%\Utilities\%%F"
)

:: Logger
for %%D in (LogSink LogFormatter) do (
    if not exist "%LOGGER_DIR%\%%D" mkdir "%LOGGER_DIR%\%%D"
)

for %%F in (LogLevel.h LogCore.cpp LogConfig.cpp LogMacros.h AsyncLogQueue.cpp) do (
    if not exist "%LOGGER_DIR%\%%F" @echo. > "%LOGGER_DIR%\%%F"
)

:: LogSink
for %%F in (ConsoleSink.cpp ConsoleSink.h FileSink.cpp FileSink.h) do (
    if not exist "%LOGGER_DIR%\LogSink\%%F" @echo. > "%LOGGER_DIR%\LogSink\%%F"
)

:: LogFormatter
for %%F in (TextFormatter.cpp TextFormatter.h) do (
    if not exist "%LOGGER_DIR%\LogFormatter\%%F" @echo. > "%LOGGER_DIR%\LogFormatter\%%F"
)

:: Création structure Platform
for %%D in (FileSystem Threading DynamicLibrary) do (
    if not exist "%PLATFORM_DIR%\%%D" mkdir "%PLATFORM_DIR%\%%D"
)

for %%F in (FileSystem.cpp FileSystem.h) do (
    if not exist "%PLATFORM_DIR%\FileSystem\%%F" @echo. > "%PLATFORM_DIR%\FileSystem\%%F"
)

for %%F in (Thread.cpp Thread.h) do (
    if not exist "%PLATFORM_DIR%\Threading\%%F" @echo. > "%PLATFORM_DIR%\Threading\%%F"
)

for %%F in (LibraryLoader.cpp LibraryLoader.h) do (
    if not exist "%PLATFORM_DIR%\DynamicLibrary\%%F" @echo. > "%PLATFORM_DIR%\DynamicLibrary\%%F"
)

echo Structure générée avec succès!
pause
