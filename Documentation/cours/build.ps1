# =============================================================================
# Compile le cours en PDF (XeLaTeX, deux passes pour la table des matieres).
#   ./build.ps1          compilation complete
#   ./build.ps1 -Quick   une seule passe (apercu)
#   ./build.ps1 -Clean   efface les fichiers intermediaires
# XeLaTeX et non pdfLaTeX : le document utilise les polices systeme et du
# francais accentue -- seul un moteur Unicode compose cela sans substitution.
# =============================================================================
param([switch]$Quick, [switch]$Clean)

$ErrorActionPreference = 'Stop'
$racine = $PSScriptRoot
$tex    = Join-Path $racine 'tex'
$build  = Join-Path $racine 'build'

if ($Clean) {
    if (Test-Path $build) { Remove-Item -Recurse -Force $build }
    Write-Host "Nettoye." -ForegroundColor Green
    return
}

New-Item -ItemType Directory -Force $build | Out-Null

$xelatex = (Get-Command xelatex -ErrorAction SilentlyContinue)
if (-not $xelatex) { throw "xelatex introuvable : MiKTeX est-il installe ?" }

$passes = if ($Quick) { 1 } else { 2 }
for ($i = 1; $i -le $passes; ++$i) {
    Write-Host "Passe $i/$passes..." -ForegroundColor Cyan
    Push-Location $tex
    & xelatex -interaction=nonstopmode -halt-on-error `
              -output-directory="$build" 'main.tex' | Out-Null
    $code = $LASTEXITCODE
    Pop-Location
    if ($code -ne 0) {
        Write-Host "Echec (passe $i). Dernieres erreurs :" -ForegroundColor Red
        $log = Join-Path $build 'main.log'
        if (Test-Path $log) {
            Select-String -Path $log -Pattern '^!' -Context 0,4 |
                Select-Object -First 6 | ForEach-Object { $_.Line; $_.Context.PostContext }
        }
        throw "Compilation interrompue."
    }
}

$pdf = Join-Path $build 'main.pdf'
if (Test-Path $pdf) {
    $final = Join-Path $racine 'Construire_avec_Nkentseu.pdf'
    Copy-Item $pdf $final -Force
    $ko = [Math]::Round((Get-Item $final).Length / 1KB)
    Write-Host "PDF genere : $final ($ko Ko)" -ForegroundColor Green
} else {
    throw "Aucun PDF produit."
}
