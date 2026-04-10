@echo off
REM =============================================================================
REM genspv.bat — Windows batch: compile 2D renderer shaders to SPIR-V
REM
REM Usage:
REM   genspv.bat
REM   genspv.bat [glslangValidator|glslc]
REM
REM Place this file next to nkrenderer2d.vert and nkrenderer2d.frag
REM =============================================================================
setlocal

set COMPILER=%~1
if "%COMPILER%"=="" set COMPILER=glslangValidator

REM Try to find compiler on PATH or in VULKAN_SDK
where %COMPILER% >nul 2>&1
if errorlevel 1 (
    if defined VULKAN_SDK (
        set COMPILER="%VULKAN_SDK%\Bin\%COMPILER%.exe"
    ) else (
        echo ERROR: %COMPILER% not found.
        echo Install Vulkan SDK from https://vulkan.lunarg.com/ and add %%VULKAN_SDK%%\Bin to PATH
        exit /b 1
    )
)

set SCRIPT_DIR=%~dp0
set SHADER_DIR=%SCRIPT_DIR%..\shaders
set OUT_DIR=%SCRIPT_DIR%..\shaders

echo Compiling vertex shader...
%COMPILER% -V "%SHADER_DIR%\nkrenderer2d.vert" -o "%OUT_DIR%\nkrenderer2d_vert.spv"
if errorlevel 1 ( echo FAILED: vertex shader & exit /b 1 )
echo   OK

echo Compiling fragment shader...
%COMPILER% -V "%SHADER_DIR%\nkrenderer2d.frag" -o "%OUT_DIR%\nkrenderer2d_frag.spv"
if errorlevel 1 ( echo FAILED: fragment shader & exit /b 1 )
echo   OK

echo Generating NkRenderer2DVkSpv.inl...
python "%SCRIPT_DIR%genspv.py" --shader-dir "%SHADER_DIR%" --out "%OUT_DIR%"
if errorlevel 1 ( echo FAILED: genspv.py & exit /b 1 )

echo.
echo Done! Copy NkRenderer2DVkSpv.inl to:
echo   src\NKContext\Renderer\Backends\Vulkan\NkRenderer2DVkSpv.inl
echo.
endlocal