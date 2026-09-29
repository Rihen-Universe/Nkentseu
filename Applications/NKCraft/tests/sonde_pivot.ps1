# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_pivot.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — (b8) LE MODE DE PIVOT ARRIVE-T-IL AU GIZMO ?
#
# La LECTURE dit que les cinq modes de Blender sont la (median, boite
# englobante, curseur 3D, origines individuelles, element actif) et que le
# defaut est bien MEDIAN. Mais « le reglage existe » et « le reglage AGIT » sont
# deux choses differentes, et une seule des deux se lit.
#
# ⚠ POURQUOI CETTE SONDE EXISTE : la veille, le pas d'aimantation etait REGLABLE
#   et REECRIT a chaque image par la boucle du shell. Le reglage existait, il
#   n'agissait pas, et seule la mesure pouvait le dire. On ne relit donc plus un
#   chemin : on lui demande ce qu'il a produit.
#
# ── DEUX MONTAGES, ET C'EST NECESSAIRE ──────────────────────────────────────
#   AUCUN montage unique ne separe les cinq modes, et il faut le dire plutot que
#   de choisir celui qui arrange :
#     A. selection SYMETRIQUE (deux faces) + curseur 3D DECALE
#        -> CURSOR se separe des autres ; MEDIAN et BBOX coincident, parce que
#           le barycentre d'une selection symetrique EST le centre de sa boite.
#     B. selection ASYMETRIQUE (une face + un sommet)
#        -> MEDIAN se separe de BBOX ; mais l'objet naissant AU curseur, CURSOR
#           et BBOX coincident cette fois.
#   Ensemble, les cinq modes sont distingues. Separement, aucun ne suffit -- et
#   un critere qui compare deux grandeurs egales par construction ne prouve rien.
#
# USAGE : pwsh -File sonde_pivot.ps1 [-Config Debug]
# SORTIE : 0 tout vert · 1 un critere rouge

param(
	[string]$Config = "Debug",
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

function Pivot([string]$nom, [int]$mode, [hashtable]$vars) {
	$sortie = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_pivot_$nom$mode.txt"
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SEL = "n"
	$env:NK_PIVOT = "$mode"; $env:NK_PIVOT_REPORT = "120"; $env:NK_AGENT_EXIT = "160"
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k] }
	$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
		-RedirectStandardOutput $sortie
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
			"NK_PIVOT", "NK_PIVOT_REPORT", "NK_AGENT_EXIT", "NK_EDIT_SELMASK",
			"NK_EDIT_PICK_FACE", "NK_EDIT_SELIDX", "NK_CURSOR3D")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$m = @(Select-String -Path $sortie -Pattern "PIVOT frame=")
	if ($m.Count -eq 0) { return $null }
	$r = [regex]::Match($m[$m.Count - 1].Line, "mode=(-?\d+) -> \((-?[0-9.]+), (-?[0-9.]+), (-?[0-9.]+)\) \(lu=(\d)\)")
	if (-not $r.Success) { return $null }
	return [pscustomobject]@{
		mode = [int]$r.Groups[1].Value; x = [double]$r.Groups[2].Value
		y = [double]$r.Groups[3].Value; z = [double]$r.Groups[4].Value
		lu = [int]$r.Groups[5].Value
	}
}
function Txt($v) { if ($null -eq $v) { return "(rien)" } return "($($v.x), $($v.y), $($v.z))" }
function Ecart($a, $b) {
	if (($null -eq $a) -or ($null -eq $b)) { return $false }
	return ([Math]::Abs($a.x - $b.x) + [Math]::Abs($a.y - $b.y) + [Math]::Abs($a.z - $b.z)) -gt 0.01
}
$rouges = 0
function Dire([string]$nom, [bool]$vert, [string]$detail) {
	if ($vert) { Write-Host "VERT   $nom  $detail" } else { Write-Host "ROUGE  $nom  $detail"; $script:rouges++ }
}

Write-Host ""
Write-Host "SONDE DE MESURE — (b8) le mode de pivot arrive-t-il au gizmo ?"
Write-Host "-----------------------------------------------------------------------"

# ── MONTAGE A : selection SYMETRIQUE, curseur 3D DECALE ───────────────────
$A = @{ "NK_EDIT_SELMASK" = "4"; "NK_EDIT_PICK_FACE" = "0,1,1,70"; "NK_CURSOR3D" = "2.5,1.25,-0.75" }
$a0 = Pivot "A" 0 $A; $a2 = Pivot "A" 2 $A; $a4 = Pivot "A" 4 $A
Write-Host "   A · median $(Txt $a0) · curseur $(Txt $a2) · actif $(Txt $a4)"

# ── MONTAGE B : selection ASYMETRIQUE (une face + un sommet) ──────────────
$B = @{ "NK_EDIT_SELMASK" = "1"; "NK_EDIT_SELIDX" = "0,1,2,3,12"; "NK_CURSOR3D" = "7,-3,5" }
$b0 = Pivot "B" 0 $B; $b1 = Pivot "B" 1 $B; $b3 = Pivot "B" 3 $B; $b4 = Pivot "B" 4 $B
Write-Host "   B · median $(Txt $b0) · bbox $(Txt $b1) · individuel $(Txt $b3) · actif $(Txt $b4)"

# (z) LE ZERO : le relevé LIT quelque chose. Sans lui, « les valeurs different »
#     pourrait n'etre que du bruit de lecture.
Dire "(z) ZERO : le pivot est LU dans les deux montages" (($null -ne $a0) -and ($a0.lu -eq 1) -and ($null -ne $b0) -and ($b0.lu -eq 1)) `
	"lu=$(if ($a0) { $a0.lu } else { '?' }) et lu=$(if ($b0) { $b0.lu } else { '?' }) (exige 1 : sinon rien n'est mesure)"

# (a) LE MODE DEMANDE EST CELUI QUI REPOND. Un chemin qui transmettrait le
#     defaut et perdrait les autres passerait tous les criteres de valeur.
$modesOk = ($a0.mode -eq 0) -and ($a2.mode -eq 2) -and ($a4.mode -eq 4) -and
		   ($b1.mode -eq 1) -and ($b3.mode -eq 3)
Dire "(a) les CINQ modes demandes sont ceux que le gizmo porte" $modesOk `
	"modes rendus : $($a0.mode) $($b1.mode) $($a2.mode) $($b3.mode) $($a4.mode) (exige 0 1 2 3 4)"

# (b) MONTAGE A : le CURSEUR se separe, et il vaut EXACTEMENT le curseur pose.
$curOk = ($null -ne $a2) -and ([Math]::Abs($a2.x - 2.5) -lt 0.01) -and
		 ([Math]::Abs($a2.y - 1.25) -lt 0.01) -and ([Math]::Abs($a2.z + 0.75) -lt 0.01)
Dire "(b) CURSEUR : le pivot vaut EXACTEMENT le curseur 3D pose" $curOk `
	"$(Txt $a2) contre NK_CURSOR3D=(2.5, 1.25, -0.75) -- attendu EXACT, pas une ressemblance"
Dire "(c) ... et il DIFFERE du median sur le meme montage" (Ecart $a2 $a0) `
	"curseur $(Txt $a2) contre median $(Txt $a0)"

# (d) MONTAGE B : MEDIAN se separe de BBOX. C'est le montage qui le peut.
Dire "(d) MEDIAN et BBOX different sur une selection ASYMETRIQUE" (Ecart $b0 $b1) `
	"median $(Txt $b0) contre bbox $(Txt $b1) (sur une selection symetrique ils coincident : ce montage-la ne prouverait rien)"

# (e) ACTIF se separe des deux.
Dire "(e) ELEMENT ACTIF differe du median ET de la boite" ((Ecart $b4 $b0) -and (Ecart $b4 $b1)) `
	"actif $(Txt $b4)"

# (f) ORIGINES INDIVIDUELLES : le gizmo est AU BARYCENTRE, comme Blender.
#     Ici l'egalite est l'ATTENDU, et elle n'est pas une coincidence : le code
#     l'annonce, et c'est ce qui la rend verifiable.
Dire "(f) ORIGINES INDIVIDUELLES : gizmo au barycentre (attendu EGAL au median)" (-not (Ecart $b3 $b0)) `
	"individuel $(Txt $b3) contre median $(Txt $b0)"

Write-Host "-----------------------------------------------------------------------"
if ($rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "ECHEC ($rouges rouge(s))"
exit 1
