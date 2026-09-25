# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/essayer_images.ps1
# -----------------------------------------------------------------------------
# ESSAYER UN DOSSIER D'IMAGES D'AFFILEE, SANS RIEN RETAPER.
#
# Rodolf a des centaines d'images d'essai. Juger la pertinence du systeme sur UN
# objet ne dit rien ; la juger sur cinquante demande de ne pas retaper cinquante
# fois. Ce script enchaine : une image, une demande, un projet par objet, et une
# planche de contact a la fin.
#
# ⚠️ IL N'INVENTE AUCUN CHEMIN : il lance le MEME binaire avec les MEMES portes
#    que le panneau (`NK_CREA_DEMANDE`, `NK_CREA_IMAGE`), celles-la memes qui
#    servent aux mesures. Ce qu'il mesure est donc ce que Rodolf obtiendra en
#    tapant sa demande a la main.
#
# ⚠️ ET IL NE TUE RIEN : chaque course attend la fin de la precedente. Si une
#    course reste bloquee, elle bloque le lot -- et c'est preferable a un lot qui
#    tue une fenetre que Rodolf serait en train de regarder.
#
# USAGE :
#   .\Tools\Genia\essayer_images.ps1 -Images "D:\mes_images" -Demande "modelise moi cette table"
#   (ajouter -Max 10 pour n'en essayer que dix)
# -----------------------------------------------------------------------------
param(
    [Parameter(Mandatory = $true)][string]$Images,
    [Parameter(Mandatory = $true)][string]$Demande,
    [string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-genia3d",
    [string]$Sortie = "logs_genia3d/essais",
    [int]$Max = 0
)

$exe = Join-Path $Arbre "Build\Bin\Release-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Error "binaire introuvable : $exe"; exit 2 }
$base = Join-Path $Arbre $Sortie
New-Item -ItemType Directory -Force $base | Out-Null

$fichiers = Get-ChildItem -Path $Images -File | Where-Object { $_.Extension -match '^\.(png|jpg|jpeg|bmp|webp)$' }
if ($Max -gt 0) { $fichiers = $fichiers | Select-Object -First $Max }
if (-not $fichiers) { Write-Error "aucune image dans $Images"; exit 2 }

Write-Output ("{0} image(s) a essayer, demande : {1}" -f $fichiers.Count, $Demande)
$i = 0
foreach ($f in $fichiers) {
    $i++
    $cle = [System.IO.Path]::GetFileNameWithoutExtension($f.Name) -replace '[^A-Za-z0-9_]', '_'
    $proj = Join-Path $base "proj_$cle"
    Remove-Item -Recurse -Force $proj -ErrorAction SilentlyContinue
    $env:NK_SONDE = "1"; $env:NK_AGENT_EXIT = "60000"
    $env:NK_PROJECT = (Join-Path $proj "$cle.nk3dm")
    $env:NK_CREA_DEMANDE = $Demande
    $env:NK_CREA_IMAGE = ($f.FullName -replace '\\', '/')
    $env:NK_CREA_VUES = "$Sortie/vue_$cle"
    if (-not $env:NK_IA_PYTHON) {
        $env:NK_IA_PYTHON = "D:/Projets/2026/Nkentseu/genia-tools/venv/Scripts/python.exe"
    }
    $jrn = Join-Path $base "$cle.txt"
    Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $jrn
    foreach ($v in @("NK_SONDE", "NK_AGENT_EXIT", "NK_PROJECT", "NK_CREA_DEMANDE", "NK_CREA_IMAGE", "NK_CREA_VUES")) {
        Remove-Item "env:$v" -ErrorAction SilentlyContinue
    }
    # ce qui compte, en une ligne par image
    $r = (Select-String -Path $jrn -Pattern "FAMILLE RECONNUE|AJUSTEE|ajust\] L'ajustement|POSE lot").Line
    Write-Output ("[{0}/{1}] {2}" -f $i, $fichiers.Count, $f.Name)
    foreach ($l in $r) { Write-Output ("      " + $l) }
}
Write-Output ""
Write-Output ("Resultats : {0}  (les vues 3/4 sont les fichiers vue_*_34.png)" -f $base)
