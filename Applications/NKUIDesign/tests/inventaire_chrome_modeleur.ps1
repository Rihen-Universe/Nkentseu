# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# INSTRUMENT VERSIONNE — l'inventaire « NKUIDesign peut-il decrire NK3DModeler ? ».
# Trois mesures, dans cet ordre :
#   1. le tableau DIT / MONTE / POSABLE (inventaire_nkgui.py) ;
#   2. la VALIDATION REELLE d'un corpus de sonde : un fichier .nkgui par element de chrome,
#      plus TROIS NEGATIFS (TitleBar, Rail, StatusBar) dont le refus prouve que les
#      acceptations veulent dire quelque chose ;
#   3. le compte de la peinture directe de NK3DModeler -- ce qu'un document devrait remplacer.
#
# ⚠️ Le verdict de la validation N'EST PAS a la console : il part dans
#    `nkuidesign_validation.txt`, a la racine du repertoire de travail.
# ⚠️ Le corpus de sonde est une TRACE, pas un instrument : il se reecrit avec
#    `ecrire_corpus_chrome.ps1`, qui est versionne a cote.

$R = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$S = Join-Path $R 'Build\sondes-nkuidesign'
if (-not (Test-Path $S)) { New-Item -ItemType Directory -Path $S | Out-Null }
$corpus = Join-Path $S 'corpus-chrome'
$exe = Join-Path $R 'Build\Bin\Release-Windows\NKUIDesign\NKUIDesign.exe'

Write-Output '=== 1. LE TABLEAU DES TROIS AUTORITES ==='
python (Join-Path $PSScriptRoot 'inventaire_nkgui.py') $R

Write-Output ''
Write-Output '=== 2. LA VALIDATION REELLE DU CORPUS DE SONDE ==='
if (-not (Test-Path $corpus)) { & (Join-Path $PSScriptRoot 'ecrire_corpus_chrome.ps1') }
if (Test-Path $corpus) {
    Push-Location $R
    $null = & $exe "--valider=$corpus"
    Pop-Location
    $rapport = Join-Path $R 'nkuidesign_validation.txt'
    if (Test-Path $rapport) { Get-Content $rapport } else { Write-Output 'AUCUN RAPPORT ECRIT -- ROUGE' }
} else {
    Write-Output "corpus absent et non reecrit : $corpus -- ROUGE"
}

Write-Output ''
Write-Output '=== 3. LA PEINTURE DIRECTE DE NK3DModeler (ce qu''un document devrait remplacer) ==='
# ⚠️ DEUX SILENCES INVENTES PAYES ICI, et c'est pour cela que le compte se verifie contre
#    une reponse connue (1299 / 48 le 2026-09-17) :
#      - une profondeur ECRITE A LA MAIN (`*\*\*.h`) rendait 1298 : elle manquait un dossier ;
#      - `Get-ChildItem -Path <dossier> -Recurse -Include *.h` SANS `\*` final rend ZERO.
$src = Join-Path $R 'Applications\NK3DModeler\src'
$fichiers = @(Get-ChildItem -Path (Join-Path $src '*') -Recurse -Include *.h, *.cpp -File)
if ($fichiers.Count -eq 0) { Write-Output 'AUCUN FICHIER BALAYE -- ROUGE (le filtre ment)'; exit 2 }
$appels = (Select-String -Path $fichiers.FullName -Pattern '\bp\.[A-Z][A-Za-z]*\(' -AllMatches |
           ForEach-Object { $_.Matches.Count } | Measure-Object -Sum).Sum
$paints = (Select-String -Path $fichiers.FullName -Pattern 'inline .*\bPaint[A-Z][A-Za-z]*\(').Count
Write-Output ("fichiers balayes               : {0}" -f $fichiers.Count)
Write-Output ("appels du peintre p.*          : {0}" -f $appels)
Write-Output ("fonctions Paint* de la chrome  : {0}" -f $paints)
