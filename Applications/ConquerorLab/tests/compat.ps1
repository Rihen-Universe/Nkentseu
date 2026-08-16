#!/usr/bin/env pwsh
# =============================================================================
# compat.ps1 — verifie qu'un module d'EPOQUE tourne encore.
#
#   ./compat.ps1
#
# Trois etapes, et c'est la deuxieme qui compte :
#   1. compiler RegleContratNu + IAMinimale contre les en-tetes FIGES 3.0
#   2. compiler le banc d'essai contre les en-tetes D'AUJOURD'HUI
#   3. les faire jouer ensemble
#
# A lancer apres toute modification de ConquerorRulesABI.h ou ConquerorAIABI.h.
# Voir tests/NkcAbiCompat.cpp pour ce qui est verifie, et pourquoi.
# =============================================================================
param([string]$Config = 'Release')

$ErrorActionPreference = 'Stop'

$tests = $PSScriptRoot
$lab   = Split-Path $tests -Parent
$repo  = Split-Path (Split-Path $lab -Parent) -Parent
$out   = Join-Path $repo "Build\ConquerorLab-Compat"

# ── compilateur ──────────────────────────────────────────────────────────────
$cxx = $env:NK_CXX
if (-not $cxx -or -not (Test-Path $cxx)) {
    $cxx = @('C:\msys64\ucrt64\bin\clang++.exe', 'C:\msys64\mingw64\bin\clang++.exe') |
           Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $cxx) { throw "Aucun compilateur C++ trouve. Posez NK_CXX." }

if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

# ── les -I et -L de la pile, comme l'atelier les passe ───────────────────────
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

function Invoke-Cxx([string]$argline, [string]$what) {
    $log = Join-Path $out 'cc.log'
    $p = Start-Process -FilePath $cxx -ArgumentList $argline -NoNewWindow -Wait -PassThru `
                       -RedirectStandardError $log -RedirectStandardOutput "$log.out"
    if ($p.ExitCode -ne 0) {
        Write-Host (Get-Content $log -Raw)
        throw "$what : compilation echouee."
    }
}

# ── 1. les modules D'EPOQUE, contre les en-tetes figes ───────────────────────
# Le -I pointe sur tests/abi_fige_3_0 : ces modules ne voient PAS les entrees
# ajoutees depuis. C'est exactement la situation d'un stagiaire qui n'a pas
# recompile.
$fige = Join-Path $tests 'abi_fige_3_0'
Write-Host "1/3  modules compiles contre l'ABI figee 3.0..." -ForegroundColor Cyan

foreach ($m in @(@{src='exemples\rules\RegleContratNu.cpp'; out='vieux_regles.dll'},
                 @{src='exemples\ai\IAMinimale.cpp';       out='vieille_ia.dll'})) {
    $src = Join-Path $lab $m.src
    $dst = Join-Path $out $m.out
    # NKC_NO_LOG : le canal de journal n'existait pas en 3.0.
    Invoke-Cxx "-shared -std=c++17 -O2 -fPIC -static -I`"$fige`" $incStack -o `"$dst`" `"$src`" $libs" $m.out
}

# ── 2. le banc d'essai, contre les en-tetes D'AUJOURD'HUI ────────────────────
Write-Host "2/3  banc d'essai compile contre l'ABI courante..." -ForegroundColor Cyan
$inc = Join-Path $lab 'include'
$exe = Join-Path $out 'compat.exe'
Invoke-Cxx "-std=c++17 -O1 -static -I`"$inc`" $incStack -o `"$exe`" `"$(Join-Path $tests 'NkcAbiCompat.cpp')`" $libs" 'compat.exe'

# ── 3. jouer ─────────────────────────────────────────────────────────────────
Write-Host "3/3  execution...`n" -ForegroundColor Cyan
Push-Location $out
& $exe 'vieux_regles.dll' 'vieille_ia.dll'
$code = $LASTEXITCODE
Pop-Location

if ($code -ne 0) { Write-Host "`nECHEC : la compatibilite ascendante est rompue." -ForegroundColor Red }
else            { Write-Host "`nLa compatibilite ascendante tient." -ForegroundColor Green }
exit $code
