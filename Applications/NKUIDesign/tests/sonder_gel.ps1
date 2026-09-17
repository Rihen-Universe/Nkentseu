# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# INSTRUMENT VERSIONNE. Un script cite dans un rapport et absent de la branche rend le
# chiffre invérifiable : TRANSMISSION.md, « ton instrument se commite, ta trace non ».
# La RACINE et le dossier de SORTIE se deduisent de l'emplacement du script : il marche
# dans n'importe quel arbre de travail. Les traces vont dans Build/ (non versionne).
$R = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$S = Join-Path $R 'Build\sondes-nkuidesign'
if (-not (Test-Path $S)) { New-Item -ItemType Directory -Path $S | Out-Null }
# LE DETECTEUR DE GEL : ses deux positifs, son negatif, et le cout.
# Le verdict n'est PAS a la console : il part au journal (logs\app_*.log). On lit le fichier.
$exe = "$R\Build\Bin\Release-Windows\NKUIDesign\NKUIDesign.exe"
Write-Output ("binaire : {0}" -f (Get-Item $exe).LastWriteTime.ToString('MM-dd HH:mm:ss'))

function Course([string]$nom, [string[]]$args2, [string]$gel) {
    $log = "$S\gel_$nom.log"
    Remove-Item $log, "$log.err" -ErrorAction SilentlyContinue
    if ($gel) { $env:NK_GEL = $gel } else { Remove-Item Env:\NK_GEL -ErrorAction SilentlyContinue }
    $avant = Get-Date
    Write-Output ("=== {0} : FENETRE OUVERTE a {1}" -f $nom, $avant.ToString('HH:mm:ss'))
    $p = Start-Process -FilePath $exe -ArgumentList (@('--titre-sonde') + $args2) -WorkingDirectory $R `
            -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru
    Remove-Item Env:\NK_GEL -ErrorAction SilentlyContinue
    if (-not $p.WaitForExit(120000)) { Write-Output "   ARRET de MON processus $($p.Id)"; Stop-Process -Id $p.Id -Force }
    Write-Output ("    FERMEE a {0}, sortie {1}" -f (Get-Date -Format 'HH:mm:ss'), $p.ExitCode)
    # LE JOURNAL DE CETTE COURSE : le fichier logs\app_*.log ecrit apres le debut de la course.
    $j = Get-ChildItem "$R\logs\app_*.log" | Where-Object { $_.LastWriteTime -ge $avant.AddSeconds(-2) } |
         Sort-Object LastWriteTime | Select-Object -Last 1
    if ($j) {
        Copy-Item $j.FullName "$S\gel_$nom.journal.log" -Force
        $lignes = Select-String -Path $j.FullName -Pattern '\[gel\]'
        Write-Output ("    journal : {0} -- {1} ligne(s) [gel]" -f $j.Name, $lignes.Count)
        foreach ($l in $lignes) { Write-Output ("      {0}" -f $l.Line.Trim()) }
    } else { Write-Output "    AUCUN journal trouve pour cette course -- ROUGE (l'instrument n'a rien lu)" }
}

Course 'porte'   @('--sonde-gel=porte')   $null
Course 'inconnu' @('--sonde-gel=inconnu') $null
Course 'sain'    @('--sonde-gel=sain')    $null
Course 'porte-eteint' @('--sonde-gel=porte') '0'

