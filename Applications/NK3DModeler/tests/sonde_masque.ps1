# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_masque.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — LE MASQUE DE SCULPTURE (25/09)
#
# CE QU'IL EST : un poids par sommet (0 libre, 1 protege) que TOUTES les brosses
# lisent. C'est le prealable de l'outil Transform de sculpture de Blender -- on
# masque, puis on deplace ce qui ne l'est pas.
#
# ⚠ LE POINT QUI SE RATE, et le coordinateur l'avait nomme d'avance : « toutes
#   les brosses respectent le masque -- mesure CHAQUE brosse, une par une, pas
#   seulement la premiere ». Ce banc mesure les QUATRE brosses du dossier
#   (dessiner, creuser, lisser, durcir), separement, avec pour chacune sa course
#   LIBRE et sa course MASQUEE. Une seule brosse mesuree aurait laisse les trois
#   autres au hasard de l'implantation.
#
# ⚠ ET IL MESURE AUSSI LE PARTIEL. « Protege » et « pas protege » ne suffisent
#   pas : un masque a mi-poids doit laisser passer une PART du deplacement. Un
#   correctif qui traiterait le masque comme un booleen passerait les criteres
#   binaires et raterait le degrade -- celui que l'oeil voit en premier.
#
# AUCUNE INJECTION D'ENTREE : tout passe par les crochets d'agent (le trait
# ecrit NK_SCULPT_STROKE / NK_SCULPT_STROKE2, le masque en bloc NK_MASK_ALL),
# qui empruntent les MEMES portes que la souris. Fenetre titree
# « *** SONDE DE MESURE *** », transparente aux clics, entrees reelles ignorees.
#
# LE NEGATIF : `-Mutation` pose NK_MASQUE_IGNORE=1, qui rend l'etat d'AVANT ce
# lot (les brosses ignorent le masque). Les criteres de protection DOIVENT
# rougir -- sans quoi « rien n'a bouge » pourrait venir d'un trait qui rate le
# maillage plutot que du masque.
#
# USAGE  : pwsh -File sonde_masque.ps1 [-Config Release] [-Mutation]
# SORTIE : 0 tout vert · 1 un critere rouge · 3 condition non reunie

param(
	[string]$Config = "Release",
	[switch]$Mutation,
	[string]$Arbre = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }
$journal = Join-Path $Arbre "logs\app.log"
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_masque"
New-Item -ItemType Directory -Force $tmp | Out-Null

# Le sujet : un cube SUBDIVISE (NK_EDIT_PRESUB=4 -> 1734 sommets). Un cube nu a
# 24 sommets tous aux coins : une brosse posee au centre d'une face n'en touche
# aucun, et « rien n'a bouge » ne voudrait plus rien dire.
$base = @{ "NK_ADD_NODE" = "2,0,20"; "NK_EDIT_USER" = "99"; "NK_EDIT_MODE" = "3,40";
		   "NK_EDIT_SEL" = "n"; "NK_EDIT_PRESUB" = "4"; "NK_MODE_PROBE" = "1";
		   "NK_AGENT_EXIT" = "220" }

function Courir([string]$nom, [hashtable]$vars) {
	$out = Join-Path $tmp "$nom.txt"
	$log = Join-Path $tmp "$nom.log"
	$poses = @("NK_SONDE", "NK_MASQUE_IGNORE", "NK_MASQUE_SANS_REPORT")
	$env:NK_SONDE = "1"
	# La mutation couvre les DEUX proprietes de ce banc : les brosses qui LISENT
	# le masque, et le REPORT du masque a travers une operation topologique.
	if ($Mutation) { $env:NK_MASQUE_IGNORE = "1"; $env:NK_MASQUE_SANS_REPORT = "1" }
	foreach ($k in $base.Keys) { Set-Item "Env:\$k" $base[$k]; $poses += $k }
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k]; $poses += $k }
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($k in $poses) { if (Test-Path "Env:\$k") { Remove-Item "Env:\$k" } }
	if (Test-Path $journal) { Copy-Item $journal $log -Force } else { Set-Content $log "" }
	return [pscustomobject]@{ out = $out; log = $log }
}

# Les lignes [MODE-SONDE] : positions (somme), masque (compte, somme, octets).
function Lignes($c) {
	$r = @()
	foreach ($l in @(Select-String -Path $c.log -Pattern "\[MODE-SONDE\]")) {
		$m = [regex]::Match($l.Line,
			"img=(\d+) .* nv=(\d+) somme=\((-?[0-9.e+-]+), (-?[0-9.e+-]+), (-?[0-9.e+-]+)\).*" +
			"masque=(\d+) sommeMasque=(-?[0-9.e+-]+) octetsMasque=(\d+)")
		if (-not $m.Success) { continue }
		$r += [pscustomobject]@{
			img = [int]$m.Groups[1].Value; nv = [int]$m.Groups[2].Value
			sx = [double]$m.Groups[3].Value; sy = [double]$m.Groups[4].Value; sz = [double]$m.Groups[5].Value
			masque = [int]$m.Groups[6].Value; somme = [double]$m.Groups[7].Value
			octets = [int]$m.Groups[8].Value
		}
	}
	return $r
}
# Le deplacement TOTAL du maillage pendant la course : la somme des positions
# est une fonctionnelle continue de toutes les positions -- deux formes
# differentes n'ont pas la meme, et un trait qui ne bouge rien la laisse egale.
function Bouge($L) {
	if ($L.Count -lt 2) { return -1.0 }
	$a = $L[0]; $b = $L[$L.Count - 1]
	return [Math]::Abs($b.sx - $a.sx) + [Math]::Abs($b.sy - $a.sy) + [Math]::Abs($b.sz - $a.sz)
}
function Dernier($L, [string]$champ) { if ($L.Count -eq 0) { return -1 } return $L[$L.Count - 1].$champ }
# LA VALEUR D'AVANT LE DERNIER CHANGEMENT, sans passer par un numero d'image.
# ⚠ Le compteur d'images de la sonde N'EST PAS le compteur d'images de l'agent
#   (il part quand le maillage s'ouvre) : un critere qui filtrerait sur une
#   fenetre d'images lirait, selon la machine, l'etat d'avant ou celui d'apres.
#   Ici on lit la SUITE des valeurs, et on prend le dernier palier distinct.
function PalierPrecedent($L, [string]$champ) {
	if ($L.Count -lt 2) { return -1 }
	$fin = $L[$L.Count - 1].$champ
	for ($i = $L.Count - 2; $i -ge 0; $i--) {
		if ($L[$i].$champ -ne $fin) { return $L[$i].$champ }
	}
	return $fin
}
function Max($L, [string]$champ) { $v = -1; foreach ($x in $L) { if ($x.$champ -gt $v) { $v = $x.$champ } }; return $v }

$rouges = 0; $conditions = 0
function Fusionner([hashtable]$a, [hashtable]$b) {
	$r = @{}
	foreach ($k in $a.Keys) { $r[$k] = $a[$k] }
	foreach ($k in $b.Keys) { $r[$k] = $b[$k] }
	return $r
}
function Dire([string]$nom, [bool]$vert, [string]$detail) {
	if ($vert) { Write-Host "VERT   $nom  $detail" } else { Write-Host "ROUGE  $nom  $detail"; $script:rouges++ }
}
function Condition([string]$nom, [bool]$ok, [string]$detail) {
	if (-not $ok) { Write-Host "COND   $nom  NON REUNIE : $detail"; $script:conditions++ }
	return $ok
}

Write-Host ""
Write-Host "SONDE DE MESURE — le masque de sculpture · mutation NK_MASQUE_IGNORE : $(if ($Mutation) { 'ACTIVE' } else { 'inactive' })"
Write-Host "-----------------------------------------------------------------------"

# ── (a) LE STOCKAGE ET SON COUT, MESURE ─────────────────────────────────────
# Vide = aucun masque = ZERO octet. C'est la propriete qui permet a un maillage
# de 250 000 sommets de ne rien payer pour une fonction qu'il n'utilise pas.
$c = Courir "cout" @{ "NK_MASK_ALL" = "1,120" }
$L = @(Lignes $c)
# AVANT = tout ce qui precede le PREMIER octet alloue, APRES = tout ce qui suit.
# ⚠ Decoupe par l'EVENEMENT, jamais par un numero d'image : le compteur de la
#   sonde ne demarre qu'a l'ouverture du maillage, et un seuil en dur lirait
#   l'etat d'apres en croyant lire celui d'avant (c'est ce qui a rougi ici a
#   la premiere version du banc).
$premier = $L.Count
for ($i = 0; $i -lt $L.Count; $i++) { if ($L[$i].octets -gt 0) { $premier = $i; break } }
$avant = @($L[0..([Math]::Max(0, $premier - 1))] | Where-Object { $_.octets -eq 0 })
$apres = @($L | Where-Object { $_.octets -gt 0 })
$nv = Dernier $L "nv"
if (Condition "(a) cout" (($avant.Count -gt 0) -and ($apres.Count -gt 0) -and ($nv -gt 100)) `
		"nv=$nv lignes avant=$($avant.Count) apres=$($apres.Count) (il faut le cube SUBDIVISE)") {
	Dire "(a) sans masque : ZERO octet" ((Max $avant "octets") -eq 0) `
		"octets max avant le premier geste = $(Max $avant 'octets') sur $nv sommets"
	Dire "(a) avec masque : EXACTEMENT 4 octets par sommet" ((Dernier $apres "octets") -eq (4 * $nv)) `
		"$(Dernier $apres 'octets') octets pour $nv sommets (exige $(4 * $nv) = 4 x nv)"
	Dire "(a) « tout masquer » protege TOUS les sommets" ((Dernier $apres "masque") -eq $nv) `
		"$(Dernier $apres 'masque') / $nv, somme $(Dernier $apres 'somme')"
}

# ── (b) LA BROSSE MASQUE : LOCALE, ET SON INVERSE LUI REPOND EXACTEMENT ─────
$c = Courir "pinceau" @{ "NK_SCULPT_STROKE" = "masquer:0.3:0.8:150" }
$L = @(Lignes $c)
if (Condition "(b) pinceau" ($L.Count -gt 2) "aucune ligne") {
	$nv = Dernier $L "nv"
	$n = Dernier $L "masque"
	Dire "(b) « masquer » peint une zone LOCALE (ni rien, ni tout)" (($n -gt 0) -and ($n -lt $nv)) `
		"$n sommet(s) protege(s) sur $nv, somme $(Dernier $L 'somme')"
	$sommePeinte = Dernier $L "somme"
	$c2 = Courir "pinceau_inverse" @{ "NK_MASK_ALL" = "1,120"; "NK_SCULPT_STROKE" = "demasquer:0.3:0.8:150" }
	$L2 = @(Lignes $c2)
	$retire = $nv - (Dernier $L2 "somme")
	$ecart = [Math]::Abs($retire - $sommePeinte)
	Dire "(b) « demasquer » RETIRE exactement ce que « masquer » pose" ($ecart -lt 0.01) `
		"pose $([Math]::Round($sommePeinte, 3)) · retire $([Math]::Round($retire, 3)) (ecart $([Math]::Round($ecart, 5)))"
}

# ── (c) LES GESTES EN BLOC, ET L'ANNULATION ────────────────────────────────
$c = Courir "inverser" @{ "NK_SCULPT_STROKE" = "masquer:0.3:1.0:120"; "NK_MASK_ALL" = "2,170" }
$L = @(Lignes $c)
if (Condition "(c) inverser" ($L.Count -gt 2) "aucune ligne") {
	$nv = Dernier $L "nv"
	$apresInv = Dernier $L "somme"
	$avantInv = PalierPrecedent $L "somme"
	# L'inverse de m vaut nv - m : c'est une EGALITE, pas une ressemblance.
	Dire "(c) inverser : somme = nv - somme precedente" ([Math]::Abs($apresInv - ($nv - $avantInv)) -lt 0.05) `
		"avant $([Math]::Round($avantInv, 3)) -> apres $([Math]::Round($apresInv, 3)) (exige $([Math]::Round($nv - $avantInv, 3)))"
}
$c = Courir "annuler" @{ "NK_SCULPT_STROKE" = "masquer:0.3:1.0:120"; "NK_VP_ACTION" = "undo,170" }
$L = @(Lignes $c)
if (Condition "(c) annuler" ($L.Count -gt 2) "aucune ligne") {
	$pose = (Max $L "masque")
	Dire "(c) Ctrl+Z defait le masque (c'est une COMMANDE, pas un etat a cote)" `
		(($pose -gt 0) -and ((Dernier $L "masque") -eq 0)) `
		"pose $pose sommet(s), puis $(Dernier $L 'masque') apres annulation"
}

# ── (d) LES QUATRE BROSSES, UNE PAR UNE ────────────────────────────────────
# Pour chacune : course LIBRE (le maillage DOIT bouger -- sinon la mesure
# masquee ne prouve rien) puis course TOUT MASQUE (il ne doit plus bouger).
foreach ($b in @("dessiner", "creuser", "lisser", "durcir")) {
	$libre = @(Lignes (Courir "libre_$b" @{ "NK_SCULPT_STROKE" = "${b}:0.3:0.8:150" }))
	$masq = @(Lignes (Courir "masque_$b" @{ "NK_MASK_ALL" = "1,120"; "NK_SCULPT_STROKE" = "${b}:0.3:0.8:170" }))
	$dLibre = Bouge $libre
	$dMasq = Bouge $masq
	if (Condition "(d) $b" (($libre.Count -gt 2) -and ($masq.Count -gt 2) -and ($dLibre -gt 1e-4)) `
			"le trait LIBRE n'a rien deplace ($([Math]::Round($dLibre, 6))) : rien a proteger, la mesure masquee ne dirait rien") {
		Dire "(d) $b : le masque la PROTEGE entierement" ($dMasq -eq 0.0) `
			"libre $([Math]::Round($dLibre, 5)) -> masque $([Math]::Round($dMasq, 5)) (exige 0 exactement)"
	}
}

# ── (e) LE PARTIEL : un masque a mi-poids laisse passer UNE PART ───────────
# Deux traits ecrits : le masque d'abord, la brosse ensuite, au MEME endroit.
$libre = @(Lignes (Courir "partiel_libre" @{ "NK_SCULPT_STROKE" = "dessiner:0.3:0.8:150" }))
$part = @(Lignes (Courir "partiel" @{ "NK_SCULPT_STROKE" = "masquer:0.3:1.0:120";
									  "NK_SCULPT_STROKE2" = "dessiner:0.3:0.8:170" }))
$dLibre = Bouge $libre
$dPart = Bouge $part
if (Condition "(e) partiel" (($dLibre -gt 1e-4) -and ($part.Count -gt 2)) "le trait libre n'a rien deplace") {
	Dire "(e) masque PARTIEL : il reste du mouvement, mais moins" (($dPart -gt 0.0) -and ($dPart -lt $dLibre)) `
		"libre $([Math]::Round($dLibre, 5)) -> partiellement masque $([Math]::Round($dPart, 5)) (exige 0 < x < libre)"
}

# ── (f) L'OUTIL TRANSFORM DE SCULPTURE : LE PREMIER CONSOMMATEUR DU MASQUE ──
# Blender : il deplace / tourne / met a l'echelle la partie NON MASQUEE, autour
# d'un pivot, avec la symetrie. C'est POUR LUI que le masque existe.
# La mesure est une EGALITE, pas une ressemblance : une translation de +0,5 en X
# sur nv sommets libres deplace la somme des positions de EXACTEMENT 0,5 x nv.
$libre = @(Lignes (Courir "xform_libre" @{ "NK_SCULPT_XFORM" = "0.5:0:0:0:0:0:1:1:1:0:150" }))
if (Condition "(f) transform" ($libre.Count -gt 2) "aucune ligne") {
	$nv = Dernier $libre "nv"
	$attendu = 0.5 * $nv
	$obtenu = Bouge $libre
	Dire "(f) sans masque : il deplace TOUT, exactement" ([Math]::Abs($obtenu - $attendu) -lt 0.01) `
		"deplacement de la somme = $([Math]::Round($obtenu, 3)) (exige $attendu = 0,5 x $nv)"

	$masq = @(Lignes (Courir "xform_masque" @{ "NK_MASK_ALL" = "1,120";
											   "NK_SCULPT_XFORM" = "0.5:0:0:0:0:0:1:1:1:0:170" }))
	Dire "(f) tout masque : il ne deplace RIEN" ((Bouge $masq) -eq 0.0) `
		"deplacement = $([Math]::Round((Bouge $masq), 5)) (exige 0 exactement)"

	$part = @(Lignes (Courir "xform_partiel" @{ "NK_SCULPT_STROKE" = "masquer:0.35:1.0:120";
											    "NK_SCULPT_XFORM" = "0.5:0:0:0:0:0:1:1:1:0:170" }))
	$dPart = Bouge $part
	Dire "(f) masque PARTIEL : il deplace moins, pas rien" (($dPart -gt 0.0) -and ($dPart -lt $obtenu)) `
		"libre $([Math]::Round($obtenu, 3)) -> partiel $([Math]::Round($dPart, 3)) (masque $(Dernier $part 'masque') sommets)"

	# LA SYMETRIE, ET LA COUTURE. Le cote negatif part en MIROIR : sur un cube
	# centre, la somme revient EXACTEMENT a zero. ⚠ Ce critere a trouve un vrai
	# defaut : les sommets POSES SUR LE PLAN suivaient le cote positif, et le
	# maillage se dechirait sur la couture (somme 34 au lieu de 0). Ils recoivent
	# desormais la MOYENNE des deux transformations.
	$sym = @(Lignes (Courir "xform_sym" @{ "NK_SCULPT_XFORM" = "0.5:0:0:0:0:0:1:1:1:1:150" }))
	Dire "(f) symetrie X : les deux cotes partent en miroir, la couture tient" ((Bouge $sym) -eq 0.0) `
		"deplacement de la somme = $([Math]::Round((Bouge $sym), 5)) (exige 0 : +0,5 d'un cote, -0,5 de l'autre, 0 sur le plan)"
}

# ── (g) LE GIZMO DU TRANSFORM RESPECTE LE MASQUE ────────────────────────────
# Le meme geste qu'a la souris : le crochet NK_SCULPT_DRAG attrape la poignee
# centrale AU PIVOT et tire de 120 px, par la structure de l'image -- aucune
# souris de la machine n'est touchee. C'est le chemin COMPLET (pick du gizmo,
# apercu par image, commande au relachement), pas l'operation appelee de cote.
$gzBase = @{ "NK_TOOL_CLIC" = "0,60"; "NK_SCULPT_DRAG" = "120,0,8,150"; "NK_AGENT_EXIT" = "280" }
$gLibre = @(Lignes (Courir "gz_libre" $gzBase))
$gTout = @(Lignes (Courir "gz_tout" (Fusionner $gzBase @{ "NK_MASK_ALL" = "1,120" })))
$gPart = @(Lignes (Courir "gz_part" (Fusionner $gzBase @{ "NK_SCULPT_STROKE" = "masquer:0.35:1.0:120" })))
$gSym = @(Lignes (Courir "gz_sym" (Fusionner $gzBase @{ "NK_SCULPT_SYM" = "1" })))
$dL = Bouge $gLibre
if (Condition "(g) gizmo" (($gLibre.Count -gt 2) -and ($dL -gt 1.0)) `
		"le glisser LIBRE n'a rien deplace ($([Math]::Round($dL, 5))) : le gizmo n'a pas ete attrape") {
	Dire "(g) gizmo, tout masque : il ne deplace RIEN" ((Bouge $gTout) -eq 0.0) `
		"libre $([Math]::Round($dL, 2)) -> masque $([Math]::Round((Bouge $gTout), 5)) (exige 0 exactement)"
	$dP = Bouge $gPart
	Dire "(g) gizmo, masque PARTIEL : il deplace moins, pas rien" (($dP -gt 0.0) -and ($dP -lt $dL)) `
		"libre $([Math]::Round($dL, 2)) -> partiel $([Math]::Round($dP, 2)) ($(Dernier $gPart 'masque') sommets proteges)"
	Dire "(g) gizmo, symetrie X : les deux cotes partent en miroir" ((Bouge $gSym) -eq 0.0) `
		"deplacement de la somme = $([Math]::Round((Bouge $gSym), 5)) (exige 0)"
}

# ── (h) LE MASQUE TRAVERSE LES OPERATIONS TOPOLOGIQUES ──────────────────────
# `BuildFromPolygons` renumerote les sommets : le masque, indexe par numero, est
# vide a l'arrivee. Il est REPORTE depuis le pre-etat -- chaque sommet neuf
# herite de ses plus proches A EGALITE, c'est-a-dire de ses parents (les deux
# extremites d'une arete coupee, les N coins d'une face).
#
# ⚠ CE BANC MESURE LES OPERATIONS UNE PAR UNE. « Ca marche sur la subdivision »
#   ne dit rien de l'extrusion : ce sont des maillages differents, et le report
#   est geometrique. Cinq operations, cinq courses.
#
# ⚠ ET IL NE MESURE PAS LA SOMME. Subdiviser AJOUTE des sommets dans la zone
#   protegee : leur poids s'ajoute, donc la somme CROIT -- c'est juste, et un
#   critere « la somme est conservee » rougirait sur un comportement correct. Ce
#   qui doit se conserver, c'est la PROPORTION masquee (la zone) et, surtout,
#   l'EFFET : la brosse ne doit toujours pas y agir. Le second critere est le
#   seul qui ne puisse pas etre satisfait par un report qui aurait tout barbouille.
# Le sujet est un cube pre-subdivise deux fois (150 sommets), en mode EDITION :
# c'est la que vivent les operations topologiques.
$baseTopo = @{ "NK_ADD_NODE" = "2,0,20"; "NK_EDIT_USER" = "99"; "NK_EDIT_MODE" = "1,40";
			   "NK_EDIT_SEL" = "a"; "NK_EDIT_PRESUB" = "2"; "NK_MODE_PROBE" = "1";
			   "NK_AGENT_EXIT" = "300" }
function CourirTopo([string]$nom, [hashtable]$vars) {
	$out = Join-Path $tmp "$nom.txt"
	$log = Join-Path $tmp "$nom.log"
	$poses = @("NK_SONDE", "NK_MASQUE_IGNORE", "NK_MASQUE_SANS_REPORT")
	$env:NK_SONDE = "1"
	if ($Mutation) { $env:NK_MASQUE_IGNORE = "1"; $env:NK_MASQUE_SANS_REPORT = "1" }
	foreach ($k in $baseTopo.Keys) { Set-Item "Env:\$k" $baseTopo[$k]; $poses += $k }
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k]; $poses += $k }
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $out | Out-Null
	foreach ($k in $poses) { if (Test-Path "Env:\$k") { Remove-Item "Env:\$k" } }
	if (Test-Path $journal) { Copy-Item $journal $log -Force } else { Set-Content $log "" }
	return [pscustomobject]@{ out = $out; log = $log }
}
foreach ($op in @("subdivide", "extrude", "inset", "bevel", "loopcut")) {
	$c = CourirTopo "topo_$op" @{ "NK_SCULPT_STROKE" = "masquer:0.35:1.0:120"; "NK_VP_ACTION" = "$op,180" }
	$L = @(Lignes $c)
	# AVANT = la derniere ligne ou le maillage a encore sa taille d'origine.
	$nvFin = Dernier $L "nv"
	$avant = @($L | Where-Object { $_.nv -ne $nvFin -and $_.masque -gt 0 })
	$apres = @($L | Where-Object { $_.nv -eq $nvFin })
	if (Condition "(h) $op" (($avant.Count -gt 0) -and ($apres.Count -gt 0) -and ($nvFin -gt 0)) `
			"l'operation n'a pas change le maillage (nv reste $nvFin) : il n'y a rien a traverser") {
		$nvAv = $avant[$avant.Count - 1].nv
		$mAv = $avant[$avant.Count - 1].masque
		$mAp = Dernier $L "masque"
		$fracAv = $mAv / $nvAv
		$fracAp = $mAp / $nvFin
		Dire "(h) $op : le masque SURVIT a l'operation" ($mAp -gt 0) `
			"$mAv sommet(s) sur $nvAv -> $mAp sur $nvFin"
		Dire "(h) $op : la ZONE protegee garde sa proportion" ([Math]::Abs($fracAp - $fracAv) -lt 0.05) `
			"fraction masquee $([Math]::Round($fracAv, 4)) -> $([Math]::Round($fracAp, 4)) (exige un ecart < 0,05 : ni perdu, ni barbouille)"
	}
}
# LE CRITERE D'EFFET : apres une subdivision, la brosse ne doit TOUJOURS pas
# agir dans la zone protegee. C'est le seul que ni une perte ni un barbouillage
# ne peuvent satisfaire.
$mq = @(Lignes (CourirTopo "topo_effet" @{ "NK_SCULPT_STROKE" = "masquer:0.35:1.0:120";
										   "NK_VP_ACTION" = "subdivide,180";
										   "NK_SCULPT_STROKE2" = "dessiner:0.35:0.8:230" }))
$sans = @(Lignes (CourirTopo "topo_effet_sans" @{ "NK_VP_ACTION" = "subdivide,180";
												  "NK_SCULPT_STROKE2" = "dessiner:0.35:0.8:230" }))
# On ne compare que ce qui suit l'operation : la subdivision elle-meme ne
# deplace aucun sommet, mais elle change `nv`, et la somme des positions avec.
function BougeApres($L) {
	$fin = Dernier $L "nv"
	$M = @($L | Where-Object { $_.nv -eq $fin })
	if ($M.Count -lt 2) { return -1.0 }
	$a = $M[0]; $b = $M[$M.Count - 1]
	return [Math]::Abs($b.sx - $a.sx) + [Math]::Abs($b.sy - $a.sy) + [Math]::Abs($b.sz - $a.sz)
}
$dSans = BougeApres $sans
$dAvec = BougeApres $mq
if (Condition "(h) effet" ($dSans -gt 0.01) "le trait TEMOIN n'a rien deplace apres subdivision ($([Math]::Round($dSans, 5)))") {
	Dire "(h) apres subdivision, le masque PROTEGE ENCORE" (($dAvec -ge 0.0) -and ($dAvec -lt ($dSans * 0.25))) `
		"sans masque $([Math]::Round($dSans, 4)) -> avec masque reporte $([Math]::Round($dAvec, 4)) (exige moins du quart)"
}

# ⚠ UN LANCEUR **NU** POUR CE CRITERE, ET LA PREMIERE VERSION S'Y EST FAIT
#   PRENDRE : le lanceur commun pose NK_ADD_NODE (il fabrique son cube). Dans la
#   course de RELECTURE, il creait donc un cube NEUF, et `NK_EDIT_USER=99`
#   entrait en edition sur LUI -- un noeud qui n'a jamais ete masque. Le banc
#   lisait 0 et accusait la persistance. Ici, le projet apporte sa propre scene :
#   le lanceur ne doit rien poser d'autre que ce qu'on lui demande.
function CourirNu([string]$nom, [hashtable]$vars) {
	$out = Join-Path $tmp "$nom.txt"
	$log = Join-Path $tmp "$nom.log"
	$poses = @("NK_SONDE", "NK_MODE_PROBE", "NK_MASQUE_IGNORE", "NK_MASQUE_SANS_REPORT")
	$env:NK_SONDE = "1"; $env:NK_MODE_PROBE = "1"
	if ($Mutation) { $env:NK_MASQUE_IGNORE = "1"; $env:NK_MASQUE_SANS_REPORT = "1" }
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k]; $poses += $k }
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $out | Out-Null
	foreach ($k in $poses) { if (Test-Path "Env:\$k") { Remove-Item "Env:\$k" } }
	if (Test-Path $journal) { Copy-Item $journal $log -Force } else { Set-Content $log "" }
	return [pscustomobject]@{ out = $out; log = $log }
}

# ── (i) LE MASQUE SURVIT A LA FERMETURE DU PROJET ───────────────────────────
# Le masque vit sur le NOEUD (pas sur l'index d'un maillage transitoire), et il
# part dans le `.nkgeo` avec la geometrie qu'il decrit -- meme objet, meme
# fichier : les separer donnerait deux fichiers a garder d'accord.
#
# ⚠ DEUX COURSES, ET C'EST LA SEULE FACON DE LE PROUVER. Une course qui
#   enregistre et relit dans le MEME processus mesurerait la memoire, pas le
#   fichier. Ici le second lancement ne partage rien avec le premier -- sauf le
#   disque.
#
# ⚠ LE PROJET VIT DANS UN DOSSIER A SON NOM (le produit le refuse autrement) :
#   <parent>/<nom>/<nom>.nk3dm.
$projRacine = Join-Path $tmp "projet_masque"
if (Test-Path $projRacine) { Remove-Item -Recurse -Force $projRacine }
New-Item -ItemType Directory -Force (Join-Path $projRacine "mq") | Out-Null
$projFichier = Join-Path $projRacine "mq\mq.nk3dm"

# COURSE A : creer, peindre le masque, SORTIR du mode (le masque remonte sur le
# noeud), enregistrer.
$a = CourirNu "projet_ecrire" @{ "NK_PROJECT" = $projFichier; "NK_ADD_NODE" = "2,0,40";
							   "NK_EDIT_USER" = "99"; "NK_EDIT_MODE" = "3,60"; "NK_EDIT_SEL" = "n";
							   "NK_EDIT_PRESUB" = "4"; "NK_SCULPT_STROKE" = "masquer:0.35:1.0:120";
							   "NK_VP_ACTION" = "toggleedit,200"; "NK_AGENT_SAVE" = "240";
							   "NK_AGENT_EXIT" = "320" }
$remonte = [regex]::Match((Get-Content -Raw $a.log), "MASQUE remonte sur le noeud (\d+) : (\d+) sommet")
$nkgeo = Get-ChildItem -Path $projRacine -Filter "*.nkgeo" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1

# COURSE B : rouvrir le projet dans un processus NEUF, entrer en Sculpture.
$b = CourirNu "projet_relire" @{ "NK_PROJECT" = $projFichier; "NK_EDIT_USER" = "99";
							   "NK_EDIT_MODE" = "3,120"; "NK_AGENT_EXIT" = "260" }
$Lb = @(Lignes $b)
$relu = Dernier $Lb "masque"
$sommeRelue = Dernier $Lb "somme"
if (Condition "(i) projet" (($remonte.Success) -and ($null -ne $nkgeo) -and ($Lb.Count -gt 0)) `
		"course A : masque remonte=$($remonte.Success) · fichier .nkgeo=$($null -ne $nkgeo) · course B : $($Lb.Count) ligne(s)$(if ($Mutation) { " -- ATTENDU sous mutation : le masque ne remonte pas sur le noeud, il n'y a donc rien a enregistrer" })") {
	$pose = [int]$remonte.Groups[2].Value
	Dire "(i) le masque est ECRIT dans le .nkgeo" ($nkgeo.Length -gt 0) `
		"$($nkgeo.Name), $([Math]::Round($nkgeo.Length / 1024.0, 1)) Ko"
	Dire "(i) rouvert dans un processus NEUF, le masque est LA" ($relu -gt 0) `
		"$pose sommet(s) a l'enregistrement -> $relu au retour (somme $([Math]::Round($sommeRelue, 3)))"
	Dire "(i) ... et c'est LE MEME (au sommet pres)" ($relu -eq $pose) `
		"$pose -> $relu (exige l'egalite : un masque qui change de taille en passant par le disque serait un autre masque)"
}

# ── (j) LA SYMETRIE DU MODE, BROSSE PAR BROSSE ──────────────────────────────
# Blender : la symetrie est un reglage du MODE (X, Y, Z independants), pas une
# propriete de la brosse -- le meme pinceau sculpte en miroir ou non selon un
# interrupteur qui n'est pas dans son fichier. Le trait est DEPLIE avant de
# partir : ce sont les tampons qui se multiplient, pas le code.
#
# ⚠ CE QUE CE BANC MESURE ICI, ET CE QU'IL LAISSE AU BANC DU MODULE. Ici : que
#   l'interrupteur ARRIVE jusqu'au geste, pour CHAQUE brosse, dans l'application
#   reelle. Le miroir champ par champ et LA COUTURE (un tampon pose sur le plan
#   ne doit pas recevoir la brosse deux fois) se mesurent en C++ dans
#   NKSculptHarness, ou l'on choisit les points -- ils ne se mesurent pas d'ici.
#
# Le trait ecrit tombe hors du plan x = 0 : sans symetrie la somme des positions
# bouge en X, avec symetrie elle revient EXACTEMENT a zero (le miroir compense)
# et le deplacement des deux autres axes DOUBLE (deux tampons au lieu d'un).
foreach ($b in @("dessiner", "creuser", "lisser", "durcir")) {
	$sans = @(Lignes (Courir "sym0_$b" @{ "NK_SCULPT_STROKE" = "${b}:0.3:0.8:150" }))
	$avec = @(Lignes (Courir "sym1_$b" @{ "NK_SCULPT_STROKE" = "${b}:0.3:0.8:150"; "NK_SCULPT_SYM" = "1" }))
	if (Condition "(j) $b" (($sans.Count -gt 1) -and ($avec.Count -gt 1) -and ((Bouge $sans) -gt 1e-4)) `
			"le trait sans symetrie n'a rien deplace : il n'y a pas de miroir a mesurer") {
		$dxS = [Math]::Abs((Dernier $sans "sx") - $sans[0].sx)
		$dxA = [Math]::Abs((Dernier $avec "sx") - $avec[0].sx)
		Dire "(j) $b : la symetrie X annule le deplacement en X" (($dxS -gt 1e-4) -and ($dxA -lt 1e-4)) `
			"sans symetrie |dx| = $([Math]::Round($dxS, 5)) · avec = $([Math]::Round($dxA, 5)) (exige un miroir)"
		Dire "(j) $b : et le geste a bien eu lieu DES DEUX COTES" ((Bouge $avec) -gt ((Bouge $sans) * 1.2)) `
			"deplacement total $([Math]::Round((Bouge $sans), 4)) -> $([Math]::Round((Bouge $avec), 4)) (deux tampons au lieu d'un)"
	}
}

Write-Host "-----------------------------------------------------------------------"
if ($rouges -gt 0) { Write-Host "ECHEC ($rouges rouge(s), $conditions condition(s) non reunie(s))"; exit 1 }
if ($conditions -gt 0) { Write-Host "CONDITION NON REUNIE ($conditions) -- ni vert ni rouge"; exit 3 }
Write-Host "TOUT VERT (0 rouge)"
exit 0
