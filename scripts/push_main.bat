@echo off
setlocal enabledelayedexpansion

set "TARGET_BRANCH=main"
set "REMOTE_NAME=origin"
set "STATE_FILE=.git\NK_LAST_PUBLISH_COMMIT"
set "COMMIT_MESSAGE=%*"

if "%COMMIT_MESSAGE%"=="" (
  echo Usage: %~nx0 "commit message"
  exit /b 1
)

for /f %%i in ('git rev-parse --abbrev-ref HEAD') do set "START_BRANCH=%%i"

for /f %%i in ('git status --porcelain ^| find /c /v ""') do set "CHANGES=%%i"
if "%CHANGES%"=="0" (
  echo No local changes to commit.
  exit /b 1
)

git add -A || goto :fail
git commit -m "%COMMIT_MESSAGE%" || goto :fail

for /f %%i in ('git rev-parse HEAD') do set "COMMIT_SHA=%%i"
> "%STATE_FILE%" echo %COMMIT_SHA%

echo Created commit: %COMMIT_SHA%

if /I "%START_BRANCH%"=="%TARGET_BRANCH%" (
  git pull --rebase %REMOTE_NAME% %TARGET_BRANCH% || goto :fail
) else (
  git switch %TARGET_BRANCH% || goto :fail
  git pull --rebase %REMOTE_NAME% %TARGET_BRANCH% || goto :fail
  git cherry-pick %COMMIT_SHA% || goto :fail
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
