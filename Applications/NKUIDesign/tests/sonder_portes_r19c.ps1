# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# INSTRUMENT VERSIONNE. Un script cite dans un rapport et absent de la branche rend le
# chiffre invérifiable : TRANSMISSION.md, « ton instrument se commite, ta trace non ».
# La RACINE et le dossier de SORTIE se deduisent de l'emplacement du script : il marche
# dans n'importe quel arbre de travail. Les traces vont dans Build/ (non versionne).
$R = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$S = Join-Path $R 'Build\sondes-nkuidesign'
if (-not (Test-Path $S)) { New-Item -ItemType Directory -Path $S | Out-Null }
# (R19c) Les menus du clic droit de la toile, dessines en OVERLAY : preuves, negatifs et
# mutations, TOUTES dans le MEME binaire. Aucune injection d'entree sur la machine.
$exe = "$R\Build\Bin\Release-Windows\NKUIDesign\NKUIDesign.exe"
Write-Output ("binaire : {0}" -f (Get-Item $exe).LastWriteTime.ToString('MM-dd HH:mm:ss'))

$courses = @(
    @{ nom = 'r19c-menu-noeud';            sources = 'menu-noeud'; mutation = $null },
    @{ nom = 'r19c-menu-vide';             sources = 'menu-vide';  mutation = $null },
    # (c) le dessin revient DANS le panneau : le rouge doit revenir
    @{ nom = 'r19c-mut-panneau-noeud';     sources = 'menu-noeud'; mutation = 'panneau' },
    @{ nom = 'r19c-mut-panneau-vide';      sources = 'menu-vide';  mutation = 'panneau' },
    # (b) non-regression
    @{ nom = 'r19c-saines';   sources = 'preferences,menu-ctx,menu-fichier,couleur,export,export-croix,export-etat,selecteur-fichier,palette-echap,palette-commande,nouveau-projet,nouveau-projet-croix'; mutation = $null },
    @{ nom = 'r19c-trio';     sources = 'role,format,rapport'; mutation = $null }
)

foreach ($c in $courses) {
    $log = "$S\portes_$($c.nom).log"
    Remove-Item $log, "$log.err" -ErrorAction SilentlyContinue
    $env:NK_PORTES = '1'
    if ($c.mutation) { $env:NK_PORTES_MUTATION = $c.mutation } else { Remove-Item Env:\NK_PORTES_MUTATION -ErrorAction SilentlyContinue }
    Write-Output ("=== course {0} ({1}) : FENETRE OUVERTE a {2}" -f $c.nom, ($c.mutation ?? 'aucune mutation'), (Get-Date -Format 'HH:mm:ss'))
    $p = Start-Process -FilePath $exe -ArgumentList @('--titre-sonde', "--sonde-portes=$($c.sources)") `
            -WorkingDirectory $R -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru
    Remove-Item Env:\NK_PORTES, Env:\NK_PORTES_MUTATION -ErrorAction SilentlyContinue
    if (-not $p.WaitForExit(180000)) {
        Write-Output "   ARRET : au-dela de 180 s, j'arrete MON processus Id $($p.Id)"
        Stop-Process -Id $p.Id -Force
    }
    $bilan = (Select-String -Path $log -Pattern 'BILAN : ' -SimpleMatch | Select-Object -Last 1).Line
    Write-Output ("    FERMEE a {0}, sortie {1} -- {2}" -f (Get-Date -Format 'HH:mm:ss'), $p.ExitCode, $bilan)
}

