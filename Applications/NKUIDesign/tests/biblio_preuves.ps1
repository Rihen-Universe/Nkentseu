# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# INSTRUMENT VERSIONNE. Un script cite dans un rapport et absent de la branche rend le
# chiffre invérifiable : TRANSMISSION.md, « ton instrument se commite, ta trace non ».
# La RACINE et le dossier de SORTIE se deduisent de l'emplacement du script : il marche
# dans n'importe quel arbre de travail. Les traces vont dans Build/ (non versionne).
$R = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$S = Join-Path $R 'Build\sondes-nkuidesign'
if (-not (Test-Path $S)) { New-Item -ItemType Directory -Path $S | Out-Null }
# (R20) Les preuves du depot par la Bibliotheque. Quatre courses, MEME binaire :
#   ref             : le tiroir ouvert, AUCUN glisser          -> 48 noeuds, 0 composant
#   apres           : le glisser vers la toile                 -> 49 noeuds, 1 composant + image
#   mutation-tiroir : le tiroir reclame le corps meme en glisser -> doit ROUGIR (retour a ref)
#   negatif-tiroir  : hors glisser, la toile reste protegee     -> porte S(sous surface)
# Aucune entree sur la machine : --glisser est pose dans NOTRE contexte.
$exe = "$R\Build\Bin\Release-Windows\NKUIDesign\NKUIDesign.exe"

& (Join-Path $PSScriptRoot 'biblio_depot.ps1') -Nom 'ref' -Sans
& (Join-Path $PSScriptRoot 'biblio_depot.ps1') -Nom 'apres'
& (Join-Path $PSScriptRoot 'biblio_depot.ps1') -Nom 'mutation-tiroir' -Mutation 'tiroir'

# LE NEGATIF DU CONTRAT DU TIROIR : tiroir ouvert, AUCUN glisser, molette tenue sur la toile.
# Le journal des portes doit nommer S(sous surface) : hors glisser, le tiroir reclame le corps.
$log = "$S\biblio_negatif_tiroir.log"
Remove-Item $log, "$log.err" -ErrorAction SilentlyContinue
$env:NK_PORTES = '1'
Remove-Item Env:\NK_PORTES_MUTATION -ErrorAction SilentlyContinue
Write-Output ("=== negatif tiroir : FENETRE OUVERTE (NKUIDesign SONDE) a {0}" -f (Get-Date -Format 'HH:mm:ss'))
$p = Start-Process -FilePath $exe -ArgumentList @('--titre-sonde', '--tiroir=droit:0', '--molette=500:450:0:80', '--mesure-fps=2500') `
        -WorkingDirectory $R -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru
Remove-Item Env:\NK_PORTES -ErrorAction SilentlyContinue
if (-not $p.WaitForExit(90000)) { Write-Output "ARRET : j'arrete MON processus Id $($p.Id)"; Stop-Process -Id $p.Id -Force }
Write-Output ("    FENETRE FERMEE a {0}, sortie {1}" -f (Get-Date -Format 'HH:mm:ss'), $p.ExitCode)
Get-Content $log | Where-Object { $_ -match 'portes' } | ForEach-Object { Write-Output ("    {0}" -f $_) }

