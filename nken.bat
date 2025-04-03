@echo off

set COMMAND=%1
shift

if "%COMMAND%"=="gen" (
    call tools\gen.bat %*
) else if "%COMMAND%"=="build" (
    call tools\build.bat %*
) else if "%COMMAND%"=="clear" (
    call tools\clear.bat %*
) else if "%COMMAND%"=="run" (
    call tools\run.bat %*
) else (
    echo Usage: nken.bat [gen^|build^|clear^|run] [options]
    echo Exemples:
    echo   nken.bat gen --arch=avx2
    echo   nken.bat build debug
    echo   nken.bat run --test
    exit /b 1
)