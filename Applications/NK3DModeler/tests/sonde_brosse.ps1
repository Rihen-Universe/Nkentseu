# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_brosse.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — LES RETOURS DE RODOLF DU 25/09
#
#   1. « on ne voit pas la taille de brosse, on ne peut pas modifier les
#      proprietes de dessin » -> rayon, force, durete, reglables, ET QUI
#      AGISSENT (mesure a deux rayons et deux forces, pas seulement « la valeur
#      a pris ») ;
#   2. « le curseur de brosse n'existe pas » -> le cercle sur la surface ;
#   3. « on ne voit pas ce qui est masque » -> la zone se peint sur la SURFACE.
#
# ⚠ CE QUI SE MESURE ICI EST LA CHAINE ENTIERE, pas la facade. NK_BRUSH_SET
#   passe par la MEME porte que la glissiere du panneau, et le trait part SANS
#   rayon ni force explicites (« dessiner:0:0:150 ») : si la resolution se
#   cassait entre le reglage et le tampon, le critere rougirait. Un banc qui
#   aurait passe le rayon dans le trait aurait mesure son propre argument.
#
# ⚠ ET LE TRAIT PART APRES LA SUBDIVISION. Premiere version de ce banc : le
#   trait partait a l'image 0, sur un cube encore NU (24 sommets, tous aux
#   coins), et « bouges=3 » ne bougeait pas d'un rayon a l'autre. Le defaut
#   etait dans la PHASE du banc, pas dans le rayon.
#
# AUCUNE INJECTION D'ENTREE : tout passe par les crochets d'agent, qui
# empruntent les memes portes que la souris. Fenetre titree
# « *** SONDE DE MESURE *** », transparente aux clics, entrees reelles ignorees.
#
# LES NEGATIFS, dans le MEME binaire (-Mutation les pose tous) :
#   NK_BROSSE_SANS_REGLAGE  les reglages sont ignores (etat d'avant)
#   NK_CURSEUR_SANS         le cercle ne se trace plus
#   NK_MASQUE_SANS_TEINTE   le masque n'est plus peint sur la surface
#   NK_MAT_EDIT_SANS        le maillage edite repart sans son materiau
#
# USAGE  : pwsh -File sonde_brosse.ps1 [-Config Release] [-Mutation]
# SORTIE : 0 tout vert · 1 un critere rouge · 2 la course n a pas tourne

param(
	[string]$Config = "Release",
	[switch]$Mutation,
	[string]$Arbre = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }
$journal = Join-Path $Arbre "logs\app.log"
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_brosse"
New-Item -ItemType Directory -Force $tmp | Out-Null

$rouges = 0
function Dire([string]$quoi, [bool]$ok, [string]$detail) {
	if ($ok) { Write-Host ("VERT   {0}  {1}" -f $quoi, $detail) }
	else { Write-Host ("ROUGE  {0}  {1}" -f $quoi, $detail); $script:rouges++ }
}
function Condition([string]$quoi, [bool]$ok, [string]$detail) {
	if (-not $ok) { Write-Host ("GRIS   {0} : condition non reunie -- {1}" -f $quoi, $detail) }
	return $ok
}

# Le sujet : un cube SUBDIVISE (1734 sommets). Un cube nu a 24 sommets tous aux
# coins : le rayon ne pourrait pas montrer qu'il elargit la zone touchee.
function Courir([string]$nom, [hashtable]$vars) {
	$out = Join-Path $tmp "$nom.txt"
	$log = Join-Path $tmp "$nom.log"
	$poses = @()
	$socle = @{ "NK_SONDE" = "1"; "NK_ADD_NODE" = "2,0,20"; "NK_EDIT_USER" = "99";
				"NK_EDIT_MODE" = "3,40"; "NK_EDIT_SEL" = "n"; "NK_EDIT_PRESUB" = "4";
				"NK_MODE_PROBE" = "1"; "NK_AGENT_SCENE" = "30"; "NK_AGENT_EXIT" = "260" }
	foreach ($k in $socle.Keys) { Set-Item "Env:\$k" $socle[$k]; $poses += $k }
	if ($Mutation) {
		foreach ($m in @("NK_BROSSE_SANS_REGLAGE", "NK_CURSEUR_SANS",
						 "NK_MASQUE_SANS_TEINTE", "NK_MAT_EDIT_SANS")) {
			Set-Item "Env:\$m" "1"; $poses += $m
		}
	}
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k]; $poses += $k }
	# ⚠ LE JOURNAL DOIT ETRE POSTERIEUR AU LANCEMENT. Un banc qui n'a pas pu
	#   mesurer doit le DIRE, jamais heriter du releve precedent -- faute deja
	#   payee par sonde_marqueurs, qui avait rendu trois verts sans course.
	$avant = Get-Date
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out -ErrorAction Stop | Out-Null
	foreach ($k in $poses) { if (Test-Path "Env:\$k") { Remove-Item "Env:\$k" } }
	if (-not (Test-Path $journal)) { Write-Host "ROUGE  aucun journal apres $nom"; exit 2 }
	if ((Get-Item $journal).LastWriteTime -lt $avant) {
		Write-Host "ROUGE  le journal de $nom est ANTERIEUR au lancement : rien n a tourne"
		exit 2
	}
	Copy-Item $journal $log -Force
	return [pscustomobject]@{ out = $out; log = $log }
}

# La ligne [SCULPT-MESURE] : ce que le trait a REELLEMENT deplace. Elle compare
# les positions avant et apres ; elle ne recalcule pas le deplacement attendu --
# un temoin qui refait la formule de la brosse serait d accord avec elle par
# construction, y compris quand elle est fausse.
function Trait($c) {
	$l = @(Select-String -Path $c.log -Pattern "\[SCULPT-MESURE\]")
	if ($l.Count -eq 0) { return $null }
	$m = [regex]::Match($l[-1].Line,
		"rayon=([0-9.]+) force=([0-9.]+) durete=([0-9.]+) pts=(\d+) -> bouges=(\d+) dmax=([0-9.e+-]+) dsomme=([0-9.e+-]+)")
	if (-not $m.Success) { return $null }
	return [pscustomobject]@{ r = [double]$m.Groups[1].Value; f = [double]$m.Groups[2].Value;
							  h = [double]$m.Groups[3].Value; bouges = [int]$m.Groups[5].Value;
							  dmax = [double]$m.Groups[6].Value; dsom = [double]$m.Groups[7].Value }
}

# Les lignes [MODE-SONDE] : curseur, teinte du masque, materiau du maillage.
function Vue($c) {
	$r = @()
	foreach ($l in @(Select-String -Path $c.log -Pattern "\[MODE-SONDE\]")) {
		$m = [regex]::Match($l.Line,
			"nv=(\d+).*masque=(\d+) .*curseur=(\d+) curseurSeg=(\d+) masqueTri=(\d+) matEdit=(\d+)")
		if ($m.Success) {
			$r += [pscustomobject]@{ nv = [int]$m.Groups[1].Value; masque = [int]$m.Groups[2].Value;
									 cur = [int]$m.Groups[3].Value; seg = [int]$m.Groups[4].Value;
									 mtri = [int]$m.Groups[5].Value; mat = [int]$m.Groups[6].Value }
		}
	}
	return $r
}
function Pic($lignes, [string]$champ) {
	$v = 0
	foreach ($l in $lignes) { if ($l.$champ -gt $v) { $v = $l.$champ } }
	return $v
}

Write-Host "======================================================================="
Write-Host " SONDE DE MESURE - brosse : reglages, curseur, masque visible  (25/09)"
Write-Host ("   binaire : {0}" -f (Get-Item $exe).LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss"))
if ($Mutation) { Write-Host "   MUTATION ACTIVE : les criteres (a) (b) (c) DOIVENT rougir" }
Write-Host "======================================================================="

# -- (a) LE REGLAGE ARRIVE JUSQU AU TAMPON, ET IL AGIT ----------------------
$t = @{}
foreach ($cas in @(@{ n = "r020_f050"; s = "0.20:0.50:" },
				   @{ n = "r040_f050"; s = "0.40:0.50:" },
				   @{ n = "r020_f100"; s = "0.20:1.00:" })) {
	# ⚠ LA BROSSE ACTIVE D ABORD : NK_BRUSH_SET regle celle qui est EN SERVICE.
	#   Sans cette ligne on reglait `creuser` (premiere du catalogue) et on
	#   tracait `dessiner` : le reglage prenait, le trait ne le voyait pas, et
	#   la mesure aurait accuse la chaine au lieu du banc.
	$c = Courir $cas.n @{ "NK_SCULPT_BRUSH" = "dessiner"; "NK_BRUSH_SET" = $cas.s;
						  "NK_SCULPT_STROKE" = "dessiner:0:0:150" }
	$t[$cas.n] = Trait $c
}
if (Condition "(a) reglages" (($null -ne $t["r020_f050"]) -and ($null -ne $t["r040_f050"]) -and
							  ($null -ne $t["r020_f100"])) `
		"une course n a pas produit de ligne [SCULPT-MESURE]") {
	$p1 = $t["r020_f050"]; $p2 = $t["r040_f050"]; $p3 = $t["r020_f100"]
	Dire "(a) le rayon regle ARRIVE dans le tampon" ([math]::Abs($p1.r - 0.20) -lt 1e-4) `
		("rayon du trait = {0} (demande 0,20 ; le fichier dit 0,25)" -f $p1.r)
	Dire "(a) la force reglee ARRIVE dans le tampon" ([math]::Abs($p3.f - 1.00) -lt 1e-4) `
		("force du trait = {0} (demande 1,00 ; le fichier dit 0,50)" -f $p3.f)
	# ⚠ CE QUI COMPTE N EST PAS LA VALEUR, C EST L EFFET. « Le rayon a pris » se
	#   dirait aussi d une valeur rangee dans un champ que personne ne lit.
	Dire "(a) un rayon DOUBLE touche PLUS de sommets" ($p2.bouges -gt $p1.bouges) `
		("sommets deplaces : {0} a r=0,20 -> {1} a r=0,40" -f $p1.bouges, $p2.bouges)
	$rap = $p3.dmax / [math]::Max($p1.dmax, 1e-9)
	Dire "(a) une force DOUBLE deplace DEUX FOIS plus loin" `
		(($p1.dmax -gt 0) -and ([math]::Abs($rap - 2.0) -lt 0.15)) `
		("dmax : {0} -> {1} (rapport {2:N4}, exige ~2)" -f $p1.dmax, $p3.dmax, $rap)
}

# -- (b) LE CURSEUR DE BROSSE -----------------------------------------------
# NK_BRUSH_HOVER pose des coordonnees SANS enfoncer le bouton : NK_SCULPT_AT
# simule un appui et sculpterait au passage -- on mesurerait autre chose.
$cSur = Courir "curseur_sur" @{ "NK_BRUSH_HOVER" = "480:300:80:150" }
$cHors = Courir "curseur_hors" @{ "NK_BRUSH_HOVER" = "40:40:80:150" }
$cSans = Courir "curseur_sans" @{}
$vSur = Vue $cSur; $vHors = Vue $cHors; $vSans = Vue $cSans
if (Condition "(b) curseur" (($vSur.Count -gt 0) -and ($vHors.Count -gt 0) -and ($vSans.Count -gt 0)) `
		"une course n a pas produit de ligne [MODE-SONDE]") {
	Dire "(b) le cercle se trace sous le curseur" ((Pic $vSur "seg") -gt 0) `
		("segments traces = {0} (deux cercles de 48, plus la normale)" -f (Pic $vSur "seg"))
	Dire "(b) il ne s invente pas de surface hors de l objet" ((Pic $vHors "seg") -eq 0) `
		("segments hors silhouette = {0} (exige 0)" -f (Pic $vHors "seg"))
	Dire "(b) et rien ne se trace sans survol" ((Pic $vSans "seg") -eq 0) `
		("segments sans survol = {0} (exige 0)" -f (Pic $vSans "seg"))
}

# -- (c) LE MASQUE SE VOIT SUR LA SURFACE -----------------------------------
$cMq = Courir "masque_peint" @{ "NK_SCULPT_STROKE" = "masquer:0.6:1.0:150" }
$vMq = Vue $cMq
if (Condition "(c) masque visible" ($vMq.Count -gt 0) "aucune ligne [MODE-SONDE]") {
	Dire "(c) le masque existe" ((Pic $vMq "masque") -gt 0) `
		("{0} sommet(s) masque(s)" -f (Pic $vMq "masque"))
	# ⚠ LE TEMOIN EST CE QUI PART AU TRACE, et non le nombre de sommets
	#   masques : celui-la existait deja quand Rodolf ne voyait rien.
	Dire "(c) et il est PEINT sur la surface" ((Pic $vMq "mtri") -gt 0) `
		("triangles de zone masquee traces = {0}" -f (Pic $vMq "mtri"))
	Dire "(c) rien n est peint quand rien n est masque" ((Pic $vSans "mtri") -eq 0) `
		("triangles sans masque = {0} (exige 0)" -f (Pic $vSans "mtri"))
}

Write-Host "-----------------------------------------------------------------------"
if ($rouges -gt 0) { Write-Host ("{0} ROUGE(S)" -f $rouges); exit 1 }
Write-Host "TOUT VERT (0 rouge)"
exit 0
