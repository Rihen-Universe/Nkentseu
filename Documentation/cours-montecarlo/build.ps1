# =============================================================================
# Compile le cours « Monte-Carlo » en PDF (XeLaTeX, deux passes).
#   ./build.ps1          compilation complete
#   ./build.ps1 -Quick   une seule passe (apercu, table des matieres peut etre
#                        en retard d'une compilation)
#   ./build.ps1 -Clean   efface les fichiers intermediaires
# XeLaTeX et non pdfLaTeX : francais accentue + polices systeme.
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
    # MiKTeX ecrit sur stderr des avertissements benins (« no administrator has
    # checked for updates »). Avec $ErrorActionPreference = 'Stop', PowerShell
    # les transforme en erreur fatale et le script echoue alors que le PDF est
    # correctement produit. On juge sur le CODE DE RETOUR, seul signal fiable.
    $ErrorActionPreference = 'Continue'
    & xelatex -interaction=nonstopmode -halt-on-error `
              -output-directory="$build" 'main.tex' 2>&1 | Out-Null
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
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
    $final = Join-Path $racine 'Cours_MonteCarlo.pdf'
    Copy-Item $pdf $final -Force
    $ko = [Math]::Round((Get-Item $final).Length / 1KB)
    Write-Host "PDF genere : $final ($ko Ko)" -ForegroundColor Green
} else {
    throw "Aucun PDF produit."
}
