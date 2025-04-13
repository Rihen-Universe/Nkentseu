@echo off

set COMMAND=%1
shift

if "%COMMAND%"=="gen" (
    call Tools\gen.bat %*
) else if "%COMMAND%"=="build" (
    call Tools\build.bat %*
) else if "%COMMAND%"=="clear" (
    call Tools\clear.bat %*
)  else if "%COMMAND%"=="debug" (
    call Tools\debug.bat %*
) else if "%COMMAND%"=="run" (
    call Tools\run.bat %*
) else (
    echo Usage: nken.bat [gen^|build^|clear^|run] [options]
    echo Exemples:
    echo   nken.bat gen --arch=avx2
    echo   nken.bat build debug
    echo   nken.bat run --test
    exit /b 1
)