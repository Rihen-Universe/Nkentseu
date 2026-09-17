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

# -- (e)(f)(g) TEMPS 2 : L'OBJET RESTE SOUS LE CURSEUR ---------------------
# Blender : le deplacement suit le pointeur, a toute distance. Chez nous
# l'echelle etait indexee sur la TAILLE de l'objet (diagonale / 400 px), donc le
# meme geste donnait le meme deplacement MONDE quelle que soit la distance -- et
# un deplacement ECRAN d'autant plus petit qu'on s'eloignait. L'objet fuyait le
# pointeur.
# ⚠ DEUX DISTANCES FRANCHEMENT DIFFERENTES (simple et TRIPLE) : avec deux
#   distances voisines, l'ecart se noierait dans la tolerance et le critere
#   passerait au vert sans rien avoir distingue.
# ⚠ ET L'INSTRUMENT SAIT ROUGIR, c'est mesure : sous NK_DIST_FIXE (l'echelle
#   d'avant), 120 px de souris donnaient 59,8 px a la distance 4 et 20,1 px a la
#   distance 12 -- un rapport de 2,98 pour une distance triple.
function GestePx([string]$nom, [string]$dist, [string]$mutDist) {
	$sortie = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_geste_$nom.txt"
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SEL = "n"
	$env:NK_CAM_YAW = "0"; $env:NK_CAM_PITCH = "0"; $env:NK_CAM_DIST = $dist
	$env:NK_MODAL_OP = "move"; $env:NK_MODAL_DRAG = "120"; $env:NK_MODAL_DRAG_FRAMES = "6"
	$env:NK_MODAL_CONFIRM = "1"; $env:NK_AGENT_EXIT = "220"
	if ($mutDist) { $env:NK_DIST_FIXE = "1" }
	if ($Mutation) { $env:NK_GLIBRE_AXEY = "1" }
	$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
		-RedirectStandardOutput $sortie
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
			"NK_CAM_YAW", "NK_CAM_PITCH", "NK_CAM_DIST", "NK_MODAL_OP", "NK_MODAL_DRAG",
			"NK_MODAL_DRAG_FRAMES", "NK_MODAL_CONFIRM", "NK_AGENT_EXIT", "NK_DIST_FIXE",
			"NK_GLIBRE_AXEY")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$m = @(Select-String -Path $sortie -Pattern "\(b8\) GESTE")
	if ($m.Count -eq 0) { return @(-1.0, -1.0) }
	$r = [regex]::Match($m[$m.Count - 1].Line, "souris ([0-9.]+) px -> objet ([0-9.]+) px")
	if (-not $r.Success) { return @(-1.0, -1.0) }
	return @([double]$r.Groups[1].Value, [double]$r.Groups[2].Value)
}
$p4 = GestePx "px_d4" "4" ""
$p12 = GestePx "px_d12" "12" ""
$q4 = GestePx "px_d4_fixe" "4" "x"
$q12 = GestePx "px_d12_fixe" "12" "x"

Write-Host "       distance 4  : souris $($p4[0]) px -> objet $($p4[1]) px"
Write-Host "       distance 12 : souris $($p12[0]) px -> objet $($p12[1]) px"
Write-Host "       (echelle fixe, l'ancienne) : $($q4[1]) px et $($q12[1]) px"

Dire "(e) L'INSTRUMENT SAIT ROUGIR : avec l'echelle fixe, l'objet FUIT" (([Math]::Abs($q4[1] - $q4[0]) -gt 20) -and ([Math]::Abs($q12[1] - $q12[0]) -gt 20)) `
	"NK_DIST_FIXE : $($q4[1]) px et $($q12[1]) px pour 120 px de souris (exige un ECART franc ; un critere qui nait vert ne prouve rien)"
Dire "(f) distance SIMPLE : l'objet reste sous le curseur" ([Math]::Abs($p4[1] - $p4[0]) -le 3) `
	"souris $($p4[0]) px -> objet $($p4[1]) px (exige l'egalite a 3 px pres)"
Dire "(g) distance TRIPLE : l'objet reste sous le curseur" ([Math]::Abs($p12[1] - $p12[0]) -le 3) `
	"souris $($p12[0]) px -> objet $($p12[1]) px (exige la MEME chose qu'a la distance simple)"

# -- (h)(i)(j) L'AIMANTATION PENDANT UNE MODALE (Rodolf, 14/09) -------------
# « Est-ce que l'aimant/snap fonctionne aussi en edition mode ? » La reponse
# mesuree etait NON -- et pas seulement en edition. L'aimantation etait reglee
# sur les deux GIZMOS, donc elle agissait au DRAG dans les deux modes, mais la
# modale G/R/S ne passe pas par le drag : le pas n'etait jamais consulte.
# ⚠ ET LE PAS N'ETAIT PAS REGLABLE : la boucle du shell le reecrivait a CHAQUE
#   image avec 0,5 / 15 / 0,1 en dur, pendant que le panneau de proprietes
#   passait, lui, les vraies valeurs. Deux autorites, celle de la boucle gagnait.
#   Mon premier critere ne le voyait pas : trois pas differents donnaient le meme
#   resultat, et je ne pouvais pas distinguer « le pas est lu » de « le pas vaut
#   toujours 0,5 ». Il a fallu reparer l'INSTRUMENT avant de pouvoir juger.
function Aimante([string]$nom, [string]$pas, [string]$on, [string]$axe) {
	$sortie = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_geste_$nom.txt"
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SEL = "n"
	$env:NK_CAM_YAW = "0"; $env:NK_CAM_PITCH = "0"; $env:NK_CAM_DIST = "4"
	$env:NK_SNAP_ON = $on; $env:NK_SNAP_STEP = $pas
	$env:NK_MODAL_OP = "move"; $env:NK_MODAL_DRAG = "120"; $env:NK_MODAL_DRAG_FRAMES = "6"
	$env:NK_MODAL_CONFIRM = "1"; $env:NK_AGENT_EXIT = "220"
	if ($axe) { $env:NK_MODAL_AXIS = $axe }
	if ($Mutation) { $env:NK_GLIBRE_AXEY = "1" }
	$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
		-RedirectStandardOutput $sortie
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
			"NK_CAM_YAW", "NK_CAM_PITCH", "NK_CAM_DIST", "NK_SNAP_ON", "NK_SNAP_STEP",
			"NK_MODAL_OP", "NK_MODAL_DRAG", "NK_MODAL_DRAG_FRAMES", "NK_MODAL_CONFIRM",
			"NK_AGENT_EXIT", "NK_MODAL_AXIS", "NK_GLIBRE_AXEY")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$m = @(Select-String -Path $sortie -Pattern "translation=\(")
	if ($m.Count -eq 0) { return -999.0 }
	$r = [regex]::Match($m[$m.Count - 1].Line, "translation=\((-?[0-9.e-]+),")
	if (-not $r.Success) { return -999.0 }
	return [double]$r.Groups[1].Value
}
$brut = Aimante "snap_off" "0.5" "0" "x"
$s05 = Aimante "snap_05" "0.5" "1" "x"
$s03 = Aimante "snap_03" "0.3" "1" "x"
$s2 = Aimante "snap_2" "2" "1" "x"
Write-Host "       sans aimantation : $brut  ·  pas 0,5 : $s05  ·  pas 0,3 : $s03  ·  pas 2 : $s2"

Dire "(h) LE ZERO : sans aimantation, la valeur n'est pas arrondie" ([Math]::Abs($brut - 1.04211) -lt 0.001) `
	"brut = $brut (exige la valeur non arrondie ; sinon « arrondi » ne voudrait rien dire)"
Dire "(i) l'aimantation AGIT pendant une modale" ([Math]::Abs($s05 - 1.0) -lt 0.001) `
	"pas 0,5 : $brut -> $s05 (exige 1,0 ; avant ce lot, la modale ignorait le pas)"
Dire "(j) et le PAS est vraiment lu : trois pas, trois resultats" (([Math]::Abs($s03 - 0.9) -lt 0.001) -and ([Math]::Abs($s2 - 2.0) -lt 0.001)) `
	"pas 0,3 -> $s03 (exige 0,9) · pas 2 -> $s2 (exige 2,0) ; s'ils etaient egaux, le pas ne serait pas lu"

# -- (k)(l)(m)(n) `S` LIBRE : LE RAPPORT DES DISTANCES AU PIVOT ------------
# Blender : eloigner le curseur du pivot agrandit, QUELLE QUE SOIT la direction,
# et le facteur est le rapport des distances avant/apres. Notre `S` lisait
# `(curX - startX)` : une composante HORIZONTALE. Mesure du 17/09 -- un geste
# VERTICAL de 120 px donnait ZERO, le geste ne faisait rien.
#
# ⚠ ET JE CORRIGE LE CRITERE CONVENU. On avait ecrit « a geste egal, le facteur
#   ne doit plus dependre de la DIRECTION ». Pris au pied de la lettre c'est FAUX,
#   et ce n'est pas ce que fait Blender : un geste RADIAL change beaucoup la
#   distance au pivot, un geste TANGENTIEL la change peu. Ce qui doit etre vrai,
#   c'est que le facteur ne depende plus que de la VARIATION DE DISTANCE AU
#   PIVOT -- et c'est verifiable exactement.
#
# ⚠ ET LE CRITERE DOIT ETRE INTERNE. `d0` depend de la position REELLE de la
#   souris au lancement, donc du bureau de l'utilisateur : un critere ecrit sur
#   la seule valeur finale varierait avec la main de celui qui mesure. On fait
#   donc imprimer d0 et d1 par l'application, et on exige `val == d1/d0 - 1`.
function Echelle([string]$nom, [string]$drag, [string]$axe, [string]$mut) {
	$sortie = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_geste_$nom.txt"
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SEL = "n"
	$env:NK_CAM_YAW = "0"; $env:NK_CAM_PITCH = "0"; $env:NK_CAM_DIST = "4"
	$env:NK_MODAL_OP = "scale"; $env:NK_MODAL_OBS = "1"
	$env:NK_MODAL_DRAG = $drag; $env:NK_MODAL_DRAG_FRAMES = "6"
	$env:NK_MODAL_CONFIRM = "1"; $env:NK_AGENT_EXIT = "220"
	if ($axe) { $env:NK_MODAL_AXIS = $axe }
	if ($mut) { $env:NK_SLIBRE_HORIZ = "1" }
	if ($Mutation) { $env:NK_GLIBRE_AXEY = "1" }
	$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
		-RedirectStandardOutput $sortie
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
			"NK_CAM_YAW", "NK_CAM_PITCH", "NK_CAM_DIST", "NK_MODAL_OP", "NK_MODAL_OBS",
			"NK_MODAL_DRAG", "NK_MODAL_DRAG_FRAMES", "NK_MODAL_CONFIRM", "NK_AGENT_EXIT",
			"NK_MODAL_AXIS", "NK_SLIBRE_HORIZ", "NK_GLIBRE_AXEY")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$val = -999.0; $d0 = -1.0; $d1 = -1.0
	$m = @(Select-String -Path $sortie -Pattern "valeur=")
	if ($m.Count -gt 0) {
		$r = [regex]::Match($m[$m.Count - 1].Line, "valeur=(-?[0-9.]+)")
		if ($r.Success) { $val = [double]$r.Groups[1].Value }
	}
	$o = @(Select-String -Path $sortie -Pattern "S LIBRE : pivot ecran")
	if ($o.Count -gt 0) {
		$r2 = [regex]::Match($o[$o.Count - 1].Line, "d0=([0-9.]+) d1=([0-9.]+)")
		if ($r2.Success) { $d0 = [double]$r2.Groups[1].Value; $d1 = [double]$r2.Groups[2].Value }
	}
	return [pscustomobject]@{ val = $val; d0 = $d0; d1 = $d1 }
}
$sh = Echelle "s_h" "120,0" "" ""
$sv = Echelle "s_v" "0,120" "" ""
$sr = Echelle "s_r" "-120,0" "" ""
$sm = Echelle "s_mut" "0,120" "" "x"
$sx = Echelle "s_axe" "120,0" "x" ""
$sxv = Echelle "s_axe_v" "0,120" "x" ""
Write-Host "       libre h $($sh.val) (d0=$($sh.d0) d1=$($sh.d1)) · v $($sv.val) · rapproche $($sr.val)"
Write-Host "       axe x : h $($sx.val) · v $($sxv.val)  ·  mutation (lecture horizontale) v : $($sm.val)"

Dire "(k) L'INSTRUMENT SAIT ROUGIR : en lecture horizontale, le VERTICAL ne fait RIEN" ([Math]::Abs($sm.val) -lt 0.001) `
	"NK_SLIBRE_HORIZ, drag vertical -> $($sm.val) (exige 0 : c'est le defaut, et il doit apparaitre AVANT)"
Dire "(l) un geste VERTICAL agit desormais" ([Math]::Abs($sv.val) -gt 0.01) `
	"drag vertical -> $($sv.val) (exige non nul ; il valait 0)"
# ⚠ ET JE CORRIGE CE CRITERE-CI AUSSI, POUR LA MEME RAISON QUE (n). Ecrit
#   « +120 px doit AGRANDIR », il supposait que le curseur est a GAUCHE du pivot.
#   Il a rougi des que la souris de Rodolf s'est retrouvee de l'autre cote : un
#   geste vers la droite RAPPROCHE alors du pivot, et retrecir est le comportement
#   JUSTE. Le critere vrai ne parle pas de direction mais de DISTANCE : le signe du
#   facteur suit le signe de (d1 - d0), quelle que soit la souris. Et les deux
#   gestes etant opposes, l'un des deux eloigne forcement -- les deux cas sont donc
#   couverts sans rien supposer.
$signeOk = ($sh.d0 -gt 0) -and ($sr.d0 -gt 0) -and
		   ([Math]::Sign([Math]::Round($sh.d1 - $sh.d0, 3)) -eq [Math]::Sign([Math]::Round($sh.val, 3))) -and
		   ([Math]::Sign([Math]::Round($sr.d1 - $sr.d0, 3)) -eq [Math]::Sign([Math]::Round($sr.val, 3))) -and
		   ([Math]::Sign([Math]::Round($sh.d1 - $sh.d0, 3)) -ne [Math]::Sign([Math]::Round($sr.d1 - $sr.d0, 3)))
Dire "(m) s'ELOIGNER agrandit, se RAPPROCHER retrecit" $signeOk `
	"geste +120 : d1-d0 = $([Math]::Round($sh.d1 - $sh.d0,1)) -> $($sh.val) · geste -120 : d1-d0 = $([Math]::Round($sr.d1 - $sr.d0,1)) -> $($sr.val) (exige MEME SIGNE, et les deux gestes opposes : l'un eloigne, l'autre rapproche)"
$interne = ($sh.d0 -gt 0) -and ([Math]::Abs(($sh.d1 / $sh.d0 - 1.0) - $sh.val) -lt 0.002) -and
		   ($sv.d0 -gt 0) -and ([Math]::Abs(($sv.d1 / $sv.d0 - 1.0) - $sv.val) -lt 0.002)
Dire "(n) CRITERE INTERNE : la valeur vaut EXACTEMENT d1/d0 - 1" $interne `
	"h : $($sh.d1)/$($sh.d0)-1 contre $($sh.val) · v : $($sv.d1)/$($sv.d0)-1 contre $($sv.val) (vrai quelle que soit la souris)"
Dire "(o) NEGATIF : S sur un AXE explicite reste lineaire" (([Math]::Abs($sx.val - 0.4) -lt 0.01) -and ([Math]::Abs($sxv.val) -lt 0.001)) `
	"axe x, h -> $($sx.val) (exige 0,4, inchange) · axe x, v -> $($sxv.val) (exige 0 : le geste reste horizontal)"

Write-Host "-----------------------------------------------------------------------"
if ($Mutation) {
	if ($rouges -gt 0) { Write-Host "MUTATION TUEE ($rouges rouge(s)) — les criteres mordent."; exit 1 }
	Write-Host "ECHEC DE LA SONDE : la mutation a SURVECU, les criteres ne testent rien."
	exit 2
}
if ($rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "ECHEC ($rouges rouge(s))"
exit 1
