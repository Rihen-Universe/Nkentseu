param([string]$Nom, [switch]$Sans, [string]$Mutation = '')
# ⚠️ `param()` DOIT RESTER LA PREMIERE INSTRUCTION du script : pose apres une affectation,
#    PowerShell ne lie plus les arguments -- mesure du 17/09, les trois courses deposaient
#    toutes et la mutation ne mutait rien.
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# INSTRUMENT VERSIONNE. Un script cite dans un rapport et absent de la branche rend le
# chiffre invérifiable : TRANSMISSION.md, « ton instrument se commite, ta trace non ».
# La RACINE et le dossier de SORTIE se deduisent de l'emplacement du script : il marche
# dans n'importe quel arbre de travail. Les traces vont dans Build/ (non versionne).
$R = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$S = Join-Path $R 'Build\sondes-nkuidesign'
if (-not (Test-Path $S)) { New-Item -ItemType Directory -Path $S | Out-Null }
# (R20) UNE course = un glisser de la premiere ligne de la Bibliotheque vers la toile
# (--glisser, entree INTERNE posee dans notre contexte), le document SAUVE puis RELU a
# l'image 95, et la photo a l'image 100 -- les deux dans la MEME course, donc du meme etat.
# Aucune entree sur la machine de Rodolf.
# Usage : biblio_depot.ps1 -Nom <course> [-Sans] [-Mutation <m>]
$exe = "$R\Build\Bin\Release-Windows\NKUIDesign\NKUIDesign.exe"
$log = "$S\biblio_$Nom.log"
$doc = "$S\biblio_$Nom.nkuidoc"
$png = "$S\biblio_$Nom.png"
Write-Output ("binaire : {0} ({1})" -f $exe, (Get-Item $exe).LastWriteTime.ToString('MM-dd HH:mm:ss'))
Remove-Item $log, "$log.err", $doc, $png -ErrorAction SilentlyContinue
# Point decimal : PowerShell fr-FR ecrirait « 12,5 » et atof rendrait 12.
$inv = [System.Globalization.CultureInfo]::InvariantCulture
$a = @('--titre-sonde', '--tiroir=droit:0', "--sauver-document=95:$doc", '--capture-frame=100', "--capture=$png")
if (-not $Sans) {
    # (1150, 297) = la premiere ligne « bouton » du tiroir, que la trace publie ; (500, 450) = la toile.
    $a += ("--glisser={0}:{1}:{2}:{3}:60:24" -f (1150).ToString($inv), (297).ToString($inv), (500).ToString($inv), (450).ToString($inv))
}
$env:NK_TRACE_DEPOT = '1'
if ($Mutation) { $env:NK_PORTES_MUTATION = $Mutation } else { Remove-Item Env:\NK_PORTES_MUTATION -ErrorAction SilentlyContinue }
Write-Output ("FENETRE OUVERTE (NKUIDesign SONDE, {0}) a {1} ; {2}" -f $Nom, (Get-Date -Format 'HH:mm:ss'), ($a -join ' '))
$p = Start-Process -FilePath $exe -ArgumentList $a -WorkingDirectory $R `
        -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru
Remove-Item Env:\NK_TRACE_DEPOT, Env:\NK_PORTES_MUTATION -ErrorAction SilentlyContinue
if (-not $p.WaitForExit(90000)) { Write-Output "ARRET : j'arrete MON processus Id $($p.Id)"; Stop-Process -Id $p.Id -Force }
Write-Output ("FENETRE FERMEE a {0}, sortie {1}" -f (Get-Date -Format 'HH:mm:ss'), $p.ExitCode)
Get-Content $log | Where-Object { $_ -match 'depot|capture' } | ForEach-Object { Write-Output ("    {0}" -f $_) }


