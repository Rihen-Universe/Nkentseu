# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# INSTRUMENT VERSIONNE. Un script cite dans un rapport et absent de la branche rend le
# chiffre invérifiable : TRANSMISSION.md, « ton instrument se commite, ta trace non ».
# La RACINE et le dossier de SORTIE se deduisent de l'emplacement du script : il marche
# dans n'importe quel arbre de travail. Les traces vont dans Build/ (non versionne).
$R = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$S = Join-Path $R 'Build\sondes-nkuidesign'
if (-not (Test-Path $S)) { New-Item -ItemType Directory -Path $S | Out-Null }
# LE COUT DU DETECTEUR DE GEL, dans le MEME BINAIRE : NK_GEL=0 contre le defaut.
# ⚠️ COURSES ALTERNEES. Un premier essai groupe (3 puis 3) donnait 129-139 contre 140-141 :
#    l'ordre et la charge de la machine expliquaient l'ecart autant que le detecteur. On
#    alterne, et on donne les SIX chiffres, pas une moyenne.
# NK_PHASES=2 imprime le cumul (periode reelle d'une image) tous les 300 images.
$exe = "$R\Build\Bin\Release-Windows\NKUIDesign\NKUIDesign.exe"
Write-Output ("binaire : {0}" -f (Get-Item $exe).LastWriteTime.ToString('MM-dd HH:mm:ss'))
foreach ($i in 1, 2, 3, 4) {
    foreach ($etat in @('defaut', 'eteint')) {
        $log = "$S\gel_cout_$etat`_$i.log"
        Remove-Item $log, "$log.err" -ErrorAction SilentlyContinue
        if ($etat -eq 'eteint') { $env:NK_GEL = '0' } else { Remove-Item Env:\NK_GEL -ErrorAction SilentlyContinue }
        $env:NK_PHASES = '2'
        $p = Start-Process -FilePath $exe -ArgumentList @('--titre-sonde', '--mesure-fps=5000') `
                -WorkingDirectory $R -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru
        Remove-Item Env:\NK_GEL, Env:\NK_PHASES -ErrorAction SilentlyContinue
        if (-not $p.WaitForExit(90000)) { Stop-Process -Id $p.Id -Force }
        $fps = (Select-String -Path $log -Pattern 'images/s ->|-> \d+\.\d images/s' | Select-Object -Last 1).Line
        $per = (Select-String -Path $log -Pattern "periode reelle" | Select-Object -Last 1).Line
        Write-Output ("{0} #{1} : {2}" -f $etat, $i, ($fps -replace '^\s+', ''))
        if ($per) { Write-Output ("              {0}" -f ($per -replace '^\s+', '')) }
    }
}

