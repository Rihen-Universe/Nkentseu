# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_gestes.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — (b8) LES GESTES MODAUX, COMPORTEMENT PAR COMPORTEMENT.
#
# Premier comportement : LE G LIBRE.
#   Blender : G sans axe deplace dans le PLAN DE LA CAMERA -- la selection suit
#   la souris, en X et en Y de l'ecran, et aucun axe du monde n'intervient.
#   Nous, avant ce lot : le code repliait sur `: 1`, c'est-a-dire l'axe Y DU
#   MONDE, pendant que le commentaire d'a cote annoncait « repere de la vue ».
#   G libre montait donc tout droit, quelle que soit la camera.
#
# ⚠ LE COMMENTAIRE PROTEGEAIT LE DEFAUT : on le lisait, on croyait que c'etait
#   fait, et on allait chercher ailleurs.
#
# ── LES CRITERES, DERIVES ───────────────────────────────────────────────────
#   (b) LE ZERO : sans geste, aucun deplacement. Sans lui, les directions
#       mesurees plus bas pourraient venir de n'importe quoi.
#   (a) MEME GESTE, DEUX CAMERAS -> la direction dans le MONDE doit CHANGER.
#       C'est le critere qui porte le defaut : avant, elle ne changeait pas.
#   (c) L'AMPLITUDE ne doit pas dependre de la camera : meme geste, meme norme.
#   (d) NEGATIF : G sur un axe EXPLICITE (X, Y, Z) ne change pas d'un iota.
#       C'est ce qui distingue « j'ai repare le libre » de « j'ai casse les
#       contraints », et sans lui les deux se ressemblent.
#
# ⚠ AUCUNE INJECTION D'ENTREE : la camera est posee par NK_CAM_YAW/PITCH et le
#   geste par NK_MODAL_DRAG, c'est-a-dire par des crochets d'API. La fenetre
#   porte *** SONDE DE MESURE ***.
#
# ── LA MUTATION ─────────────────────────────────────────────────────────────
#   -Mutation pose NK_GLIBRE_AXEY=1, qui remet le repli sur l'axe Y du monde.
#   (a) doit alors rougir : la direction cesse de suivre la camera.
#
# USAGE : pwsh -File sonde_gestes.ps1 [-Config Debug] [-Mutation]
# SORTIE : 0 tout vert · 1 un critere rouge · 2 la mutation a survecu

param(
	[string]$Config = "Debug",
	[switch]$Mutation,
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

function Geste([string]$nom, [hashtable]$vars) {
	$sortie = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_geste_$nom.txt"
	$env:NK_SONDE = "1"
	$env:NK_ADD_NODE = "2,0,20"
	$env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"
	$env:NK_EDIT_SEL = "n"
	$env:NK_MODAL_OP = "move"
	$env:NK_MODAL_CONFIRM = "1"
	$env:NK_AGENT_EXIT = "200"
	if ($Mutation) { $env:NK_GLIBRE_AXEY = "1" } elseif (Test-Path "Env:\NK_GLIBRE_AXEY") { Remove-Item -Path "Env:\NK_GLIBRE_AXEY" }
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k] }
	$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
		-RedirectStandardOutput $sortie
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
			"NK_MODAL_OP", "NK_MODAL_CONFIRM", "NK_AGENT_EXIT", "NK_GLIBRE_AXEY",
			"NK_CAM_YAW", "NK_CAM_PITCH", "NK_MODAL_DRAG", "NK_MODAL_DRAG_FRAMES", "NK_MODAL_AXIS")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$m = @(Select-String -Path $sortie -Pattern "translation=\(")
	if ($m.Count -eq 0) { return $null }
	$r = [regex]::Match($m[$m.Count - 1].Line, "translation=\((-?[0-9.e-]+), (-?[0-9.e-]+), (-?[0-9.e-]+)\)")
	if (-not $r.Success) { return $null }
	return [pscustomobject]@{
		x = [double]$r.Groups[1].Value; y = [double]$r.Groups[2].Value; z = [double]$r.Groups[3].Value
	}
}
function Norme($v) { if ($null -eq $v) { return -1.0 } return [Math]::Sqrt($v.x * $v.x + $v.y * $v.y + $v.z * $v.z) }
function Txt($v) { if ($null -eq $v) { return "(rien)" } return "($([Math]::Round($v.x,4)), $([Math]::Round($v.y,4)), $([Math]::Round($v.z,4)))" }

$rouges = 0
function Dire([string]$nom, [bool]$vert, [string]$detail) {
	if ($vert) { Write-Host "VERT   $nom  $detail" } else { Write-Host "ROUGE  $nom  $detail"; $script:rouges++ }
}

Write-Host ""
Write-Host "SONDE DE MESURE — (b8) le G libre · mutation NK_GLIBRE_AXEY : $(if ($Mutation) { 'ACTIVE' } else { 'inactive' })"
Write-Host "-----------------------------------------------------------------------"

$z = Geste "zero" @{ "NK_CAM_YAW" = "0"; "NK_CAM_PITCH" = "0" }
$a0 = Geste "libre0" @{ "NK_CAM_YAW" = "0"; "NK_CAM_PITCH" = "0"; "NK_MODAL_DRAG" = "120"; "NK_MODAL_DRAG_FRAMES" = "6" }
$a9 = Geste "libre90" @{ "NK_CAM_YAW" = "90"; "NK_CAM_PITCH" = "0"; "NK_MODAL_DRAG" = "120"; "NK_MODAL_DRAG_FRAMES" = "6" }
$dx = Geste "axeX" @{ "NK_CAM_YAW" = "0"; "NK_CAM_PITCH" = "0"; "NK_MODAL_DRAG" = "120"; "NK_MODAL_DRAG_FRAMES" = "6"; "NK_MODAL_AXIS" = "x" }
$dy = Geste "axeY" @{ "NK_CAM_YAW" = "0"; "NK_CAM_PITCH" = "0"; "NK_MODAL_DRAG" = "120"; "NK_MODAL_DRAG_FRAMES" = "6"; "NK_MODAL_AXIS" = "y" }

Dire "(b) LE ZERO : sans geste, aucun deplacement" ((Norme $z) -lt 1e-6) `
	"aucun drag -> $(Txt $z) (exige le vecteur nul ; sinon les directions d'en bas ne prouvent rien)"

$change = $false
if (($null -ne $a0) -and ($null -ne $a9)) {
	$d = [Math]::Sqrt([Math]::Pow($a0.x - $a9.x, 2) + [Math]::Pow($a0.y - $a9.y, 2) + [Math]::Pow($a0.z - $a9.z, 2))
	$change = $d -gt 0.01
}
Dire "(a) meme geste, deux cameras : la direction CHANGE" $change `
	"yaw 0 -> $(Txt $a0) · yaw 90 -> $(Txt $a9) (exige DIFFERENTS ; avant, c'etait +Y dans les deux cas)"

$n0 = Norme $a0; $n9 = Norme $a9
Dire "(c) ... et l'amplitude ne depend PAS de la camera" ([Math]::Abs($n0 - $n9) -lt 0.01) `
	"normes $([Math]::Round($n0,4)) et $([Math]::Round($n9,4)) (exige EGALES)"

$axOk = ($null -ne $dx) -and ($null -ne $dy) -and ([Math]::Abs($dx.x - $n0) -lt 0.01) -and
		([Math]::Abs($dx.y) -lt 1e-6) -and ([Math]::Abs($dx.z) -lt 1e-6) -and
		([Math]::Abs($dy.y - $n0) -lt 0.01) -and ([Math]::Abs($dy.x) -lt 1e-6)
Dire "(d) NEGATIF : G sur un axe explicite est INCHANGE" $axOk `
	"axe X -> $(Txt $dx) · axe Y -> $(Txt $dy) (exige purement sur l'axe, meme amplitude $([Math]::Round($n0,4)))"

Write-Host "-----------------------------------------------------------------------"
if ($Mutation) {
	if ($rouges -gt 0) { Write-Host "MUTATION TUEE ($rouges rouge(s)) — les criteres mordent."; exit 1 }
	Write-Host "ECHEC DE LA SONDE : la mutation a SURVECU, les criteres ne testent rien."
	exit 2
}
if ($rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "ECHEC ($rouges rouge(s))"
exit 1
