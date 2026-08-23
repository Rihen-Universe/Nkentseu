#!/usr/bin/env pwsh
# =============================================================================
# ambiguite.ps1 -- compile et lance tests/NkcAmbiguite.cpp.
#
#   ./ambiguite.ps1
#
# Ni fenetre, ni module a charger : le banc appelle directement la fonction qui
# decide quel coup part au clic. A lancer apres toute retouche de ClickCell,
# CollecterCoups ou MoveTouches dans NkcSession.h.
#
# Meme pile et memes -I que compat.ps1 : si l'un compile, l'autre compile.
# =============================================================================
param([string]$Config = 'Release')

$ErrorActionPreference = 'Stop'

$tests = $PSScriptRoot
$lab   = Split-Path $tests -Parent
$repo  = Split-Path (Split-Path $lab -Parent) -Parent
$out   = Join-Path $repo "Build\ConquerorLab-Compat"

$cxx = $env:NK_CXX
if (-not $cxx -or -not (Test-Path $cxx)) {
    $cxx = @('C:\msys64\ucrt64\bin\clang++.exe', 'C:\msys64\mingw64\bin\clang++.exe') |
           Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $cxx) { throw "Aucun compilateur C++ trouve. Posez NK_CXX." }

New-Item -ItemType Directory -Force $out | Out-Null

$incRoots = @(
    'Kernel\Foundation\NKCore\src',        'Kernel\Foundation\NKPlatform\src',
    'Kernel\Foundation\NKMemory\src',      'Kernel\Foundation\NKContainers\src',
    'Kernel\Foundation\NKMath\src',        'Kernel\System\NKLogger\src',
    'Kernel\System\NKTime\src',            'Kernel\System\NKStream\src',
    'Kernel\System\NKFileSystem\src',      'Kernel\System\NKThreading\src',
    'Kernel\System\NKSerialization\src',   'Kernel\System\NKReflection\src'
)
$incStack = ($incRoots | ForEach-Object { "-I`"$(Join-Path $repo $_)`"" }) -join ' '

$libNames = @('NKSerialization','NKReflection','NKLogger','NKFileSystem','NKStream',
              'NKThreading','NKTime','NKContainers','NKMath','NKMemory','NKPlatform','NKCore')
$libDir = Join-Path $repo "Build\Lib\$Config-Windows"
$libs   = "-L`"$libDir`" -Wl,--start-group " + (($libNames | ForEach-Object { "-l$_" }) -join ' ') + " -Wl,--end-group"

$inc = Join-Path $lab 'include'
$src = Join-Path $lab 'src'
$exe = Join-Path $out 'ambiguite.exe'
$log = Join-Path $out 'amb.log'

Write-Host "compilation du banc d'ambiguite..." -ForegroundColor Cyan
$argline = "-std=c++17 -O1 -static -I`"$inc`" -I`"$src`" $incStack -o `"$exe`" `"$(Join-Path $tests 'NkcAmbiguite.cpp')`" $libs"
$p = Start-Process -FilePath $cxx -ArgumentList $argline -NoNewWindow -Wait -PassThru -RedirectStandardError $log -RedirectStandardOutput "$log.out"
if ($p.ExitCode -ne 0) { Write-Host (Get-Content $log -Raw); throw "compilation echouee." }

Push-Location $out
& $exe
$code = $LASTEXITCODE
Pop-Location

if ($code -ne 0) { Write-Host "ECHEC : le clic ne designe pas le coup attendu." -ForegroundColor Red }
else            { Write-Host "Le clic designe bien un coup, ou demande lequel." -ForegroundColor Green }
exit $code
