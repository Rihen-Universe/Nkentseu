@echo off
REM Run oglfont from project root so relative paths work
cd /d "e:\Projets\2026\Nkentseu\Nkentseu"
echo Working directory: %cd%
echo.
echo Running oglfont.exe...
Build\Bin\Debug-Windows\oglfont\oglfont.exe
echo.
echo Program finished.
pause
