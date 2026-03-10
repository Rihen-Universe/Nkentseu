@echo off
setlocal enabledelayedexpansion

set "TARGET_BRANCH=RihenAcademicSchoolBeginning"
set "REMOTE_NAME=origin"
set "STATE_FILE=.git\NK_LAST_PUBLISH_COMMIT"

if not "%~1"=="" (
  set "COMMIT_SHA=%~1"
) else (
  if not exist "%STATE_FILE%" (
    echo Usage: %~nx0 ^<commit-sha^>
    echo No commit sha found in %STATE_FILE%
    exit /b 1
  )
  set /p COMMIT_SHA=<"%STATE_FILE%"
)

for /f %%i in ('git rev-parse --abbrev-ref HEAD') do set "START_BRANCH=%%i"

git switch %TARGET_BRANCH% || goto :fail
git pull --rebase %REMOTE_NAME% %TARGET_BRANCH% || goto :fail

git merge-base --is-ancestor %COMMIT_SHA% HEAD
if errorlevel 1 (
  git cherry-pick %COMMIT_SHA% || goto :fail
) else (
  echo Commit already present on %TARGET_BRANCH%: %COMMIT_SHA%
)

git push %REMOTE_NAME% %TARGET_BRANCH% || goto :fail

if /I not "%START_BRANCH%"=="%TARGET_BRANCH%" (
  git switch %START_BRANCH% || goto :fail
)

echo Done: pushed %COMMIT_SHA% to %TARGET_BRANCH%
exit /b 0

:fail
echo FAILED. Resolve conflict/error then retry.
exit /b 1
