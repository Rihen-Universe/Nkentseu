# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# INSTRUMENT VERSIONNE — la ZONE HOTE du format .nkgui, prouvee de bout en bout.
#
# Trois courses, et la troisieme est celle qui donne du sens aux deux autres :
#   1. le banc de montage complet : la section (m5) monte `valides/12_zone_hote.nkgui`,
#      une fois avec un hote qui remplit « viseur3d », une fois sans aucun hote ;
#   2. la VALIDATION du meme document, plus le NEGATIF conserve : `Callback` employe
#      comme WIDGET doit rester refuse (le mot appartient aux sections et aux appels
#      de comportement, pas au vocabulaire des widgets) ;
#   3. LA MUTATION `NK_HOTE_MUTATION=sansmarqueur` : la zone non remplie cesse de
#      tracer son marqueur. Le banc DOIT alors echouer. Une mutation qui survit dirait
#      que le critere du marqueur ne teste rien.
#
# ⚠️ Le verdict de la validation n'est pas a la console : il part dans
#    `nkuidesign_validation.txt`, a la racine du repertoire de travail.

$R = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$S = Join-Path $R 'Build\sondes-nkuidesign'
if (-not (Test-Path $S)) { New-Item -ItemType Directory -Path $S | Out-Null }
$png = Join-Path $S 'hote'
if (-not (Test-Path $png)) { New-Item -ItemType Directory -Path $png -Force | Out-Null }
$banc = Join-Path $R 'Build\Bin\Release-Windows\NKGuiMonteTest\NKGuiMonteTest.exe'
$design = Join-Path $R 'Build\Bin\Release-Windows\NKUIDesign\NKUIDesign.exe'

# ⚠️ PIEGE PAYE ICI : dans `$x = Course ...`, PowerShell capture TOUT ce que la fonction
#    ecrit, pas seulement son `return`. Avec `Write-Output`, les lignes disparaissaient de
#    la console ET `$sain` devenait un tableau -- mon instrument annoncait « le banc echoue
#    deja sans mutation » alors que le banc n'avait rien dit. On ecrit donc a l'HOTE
#    (`Write-Host`) et on ne rend que le code de sortie.
function Course([string]$nom, [string]$mutation) {
    # ⚠️ UN DOSSIER D'IMAGES PAR COURSE. Les deux courses ecrivaient dans le MEME dossier :
    #    la course mutee ecrasait les images de la course saine, et j'ai regarde l'image de
    #    la mutation en croyant regarder celle du produit -- une trace ecrasee par une autre
    #    course est un instrument qui ment sans rien dire.
    $pngN = Join-Path $png $nom
    if (-not (Test-Path $pngN)) { New-Item -ItemType Directory -Path $pngN -Force | Out-Null }
    $log = Join-Path $S "hote_$nom.log"
    Remove-Item $log, "$log.err" -ErrorAction SilentlyContinue
    if ($mutation) { $env:NK_HOTE_MUTATION = $mutation } else { Remove-Item Env:\NK_HOTE_MUTATION -ErrorAction SilentlyContinue }
    $p = Start-Process -FilePath $banc -ArgumentList "--png=$pngN" -WorkingDirectory $R `
            -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru -NoNewWindow
    Remove-Item Env:\NK_HOTE_MUTATION -ErrorAction SilentlyContinue
    $p.WaitForExit(180000) | Out-Null
    $bilan = (Select-String -Path $log -Pattern '^=== \d+ / \d+ ===' | Select-Object -Last 1).Line
    Write-Host ("--- {0,-14} sortie {1}   {2}" -f $nom, $p.ExitCode, $bilan)
    Get-Content $log | Select-String -Pattern '\(m5\)|zone|marqueur|hote|signature|ECHEC' |
        ForEach-Object { Write-Host ("      " + $_.Line.Trim()) }
    return $p.ExitCode
}

Write-Output '=== 1 et 3. LE BANC DE MONTAGE, SAIN PUIS MUTE ==='
$sain = Course 'sain' $null
$mute = Course 'mutation' 'sansmarqueur'
Write-Output ''
if ($sain -eq 0 -and $mute -ne 0) {
    Write-Output 'VERDICT : VERT -- le banc passe sain, et la mutation du marqueur le fait ROUGIR.'
} elseif ($mute -eq 0) {
    Write-Output 'VERDICT : ROUGE -- la mutation a SURVECU : le critere du marqueur ne teste rien.'
} else {
    Write-Output 'VERDICT : ROUGE -- le banc echoue deja sans mutation.'
}

Write-Output ''
Write-Output '=== 2. LA VALIDATION, ET LE NEGATIF CONSERVE ==='
$corpus = Join-Path $S 'corpus-hote'
if (Test-Path $corpus) { Remove-Item $corpus -Recurse -Force }
New-Item -ItemType Directory -Path $corpus -Force | Out-Null
Copy-Item (Join-Path $R 'Applications\NKUIDesign\exemples\valides\12_zone_hote.nkgui') $corpus
# LE NEGATIF : `Callback` en WIDGET doit rester refuse.
$neg = "nkgui 0.3`n// NEGATIF : `Callback` est une SECTION et un appel de comportement, pas un widget.`nwidgets {`n  Callback `"viseur3d`" {`n  }`n}`n"
[System.IO.File]::WriteAllText((Join-Path $corpus 'n_callback_widget.nkgui'), $neg, (New-Object System.Text.UTF8Encoding($false)))
Push-Location $R
$null = & $design "--valider=$corpus"
Pop-Location
$rapport = Join-Path $R 'nkuidesign_validation.txt'
if (Test-Path $rapport) { Get-Content $rapport } else { Write-Output 'AUCUN RAPPORT -- ROUGE' }
