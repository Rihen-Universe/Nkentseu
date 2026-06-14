@echo off
REM ============================================================================
REM gitpush.bat - add + commit + push du depot Nkentseu (equivalent gitpush.sh).
REM   Commit PROPRE (aucune mention Claude) puis push de la branche.
REM   Option --release vX.Y.Z pour creer et pousser un tag.
REM
REM USAGE
REM   gitpush.bat ^<branche^> "^<message^>"
REM   gitpush.bat ^<branche^> "^<message^>" --release vX.Y.Z
REM   gitpush.bat ^<branche^> "^<message^>" --dry-run
REM ============================================================================
setlocal EnableDelayedExpansion

set "BRANCH=%~1"
set "MSG=%~2"
set "RELEASE=0"
set "TAGVER="
set "DRYRUN=0"

if "%BRANCH%"=="" goto :usage
if "%MSG%"=="" goto :usage

REM --- Parsing des options restantes (a partir du 3e argument) ---------------
shift & shift
:parse
if "%~1"=="" goto :endparse
if /I "%~1"=="--release" (
  set "RELEASE=1"
  set "TAGVER=%~2"
  shift
) else if /I "%~1"=="-r" (
  set "RELEASE=1"
  set "TAGVER=%~2"
  shift
) else if /I "%~1"=="--dry-run" (
  set "DRYRUN=1"
) else if /I "%~1"=="-d" (
  set "DRYRUN=1"
)
shift
goto :parse
:endparse

cd /d "%~dp0" || (echo [ERREUR] cd %~dp0 impossible & exit /b 1)

echo ============================================================
echo  gitpush (Nkentseu)  ^|  branche : %BRANCH%
echo    message : "%MSG%"
if "%RELEASE%"=="1" (echo    release : OUI ^(%TAGVER%^)) else (echo    release : non)
if "%DRYRUN%"=="1" echo    *** DRY-RUN ***
echo ============================================================

REM --- 1) Bascule de branche -------------------------------------------------
if "%DRYRUN%"=="1" (echo $ git checkout %BRANCH%) else (
  git checkout %BRANCH% || (echo [ERREUR] checkout %BRANCH% impossible & exit /b 1)
)

REM --- 2) Indexer ------------------------------------------------------------
if "%DRYRUN%"=="1" (echo $ git add -A) else (
  git add -A || (echo [ERREUR] git add a echoue & exit /b 1)
)

REM --- 3) Committer si necessaire --------------------------------------------
if "%DRYRUN%"=="1" (
  echo $ git commit -m "%MSG%"
) else (
  git diff --cached --quiet
  if errorlevel 1 (
    git commit -m "%MSG%" || (echo [ERREUR] git commit a echoue & exit /b 1)
    echo [OK] commit cree : "%MSG%"
  ) else (
    echo    rien de nouveau a committer
  )
)

REM --- 4) Push de la branche -------------------------------------------------
if "%DRYRUN%"=="1" (echo $ git push origin %BRANCH%) else (
  git push origin %BRANCH% || (echo [ERREUR] push de %BRANCH% echoue & exit /b 1)
  echo [OK] branche %BRANCH% poussee.
)

REM --- 5) Tag optionnel ------------------------------------------------------
if "%RELEASE%"=="1" (
  if "%TAGVER%"=="" (echo [ERREUR] --release requiert une version ^(ex: --release v2.1.0^) & exit /b 1)
  set "TAG=%TAGVER%"
  if "%DRYRUN%"=="1" (
    echo $ git tag -a !TAG! -m "Release !TAG!"
    echo $ git push origin !TAG!
  ) else (
    git tag -a !TAG! -m "Release !TAG!" || (echo [!] tag !TAG! deja existant ?)
    git push origin !TAG! || (echo [ERREUR] push du tag !TAG! echoue & exit /b 1)
    echo [OK] tag !TAG! pousse.
  )
)

echo.
echo [OK] Termine.
endlocal
exit /b 0

:usage
echo Usage: gitpush.bat ^<branche^> "^<message^>" [--release vX.Y.Z] [--dry-run]
exit /b 1
