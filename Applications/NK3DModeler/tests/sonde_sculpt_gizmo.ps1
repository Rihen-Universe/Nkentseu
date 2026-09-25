# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_sculpt_gizmo.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — SCULPTURE : LE GIZMO, TOUT COMME BLENDER (21/09)
#
# LA QUESTION DE RODOLF : « en mode sculpting, a-t-on le droit de manipuler les
# vertices avec le gizmo comme en mode edition ? » -- puis « tout comme Blender ».
# Blender : en Sculpt Mode, ni selection de sommets, ni gizmo de sommets, ni G/R/S
# sur des sommets. On deforme avec des brosses.
#
# MESURE AVANT CORRECTIF (meme binaire, sans le correctif) -- Sculpture (3) ET
# Sculpture 2.5D (2) donnaient exactement l'Edition :
#   gizmo de sommets trace : 54 triangles · 12 marqueurs · 12 aretes de cage
#   G (modale)             : les 24 sommets deplaces (somme des positions 0 -> 58,7)
#   glisser du gizmo       : deplace les sommets (en 3, PUIS un trait de brosse
#                            part au relachement : un geste, deux actions)
#   clic                   : selectionne 12 sommets
#   E / X                  : 24 -> 48 sommets / le maillage ENTIER supprime
#
# CE QUE LE BANC FAIT : il ouvre un cube utilisateur dans chaque mode, par les
# crochets d'agent (aucune souris ni clavier de la machine n'est touche), et lit
# la ligne `[MODE-SONDE]` que la vue ecrit a partir de ce qui PART AU TRACE
# (triangles envoyes, marqueurs poses) et de la somme des positions du maillage.
#
# ⚠ LA CONDITION D'ABORD. Un zero ne vaut que la ou la question se pose : les
#   criteres « rien ne se dessine » exigent une selection NON VIDE (selV > 0),
#   sans quoi le gizmo se tairait aussi avec l'ancien code. Condition absente =
#   sortie 3, ni vert ni rouge.
#
# LE NEGATIF : `-Mutation` pose NK_SCULPT_GIZMO_MUTE=1, qui rend l'ancienne
# regle (`editMode` seul) dans la vue ET dans le shell. Le banc DOIT alors
# rougir, et d'abord sur le gizmo de sommets en Sculpture.
#
# USAGE  : pwsh -File sonde_sculpt_gizmo.ps1 [-Config Release] [-Mutation] [-Images <dossier>]
# SORTIE : 0 tout vert · 1 un critere rouge · 3 condition non reunie

param(
	[string]$Config = "Release",
	[switch]$Mutation,
	[string]$Images = "",
	[string]$Arbre = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }
$journal = Join-Path $Arbre "logs\app.log"
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_sculpt_gizmo"
New-Item -ItemType Directory -Force $tmp | Out-Null

# UNE course : variables posees, application lancee et attendue, variables retirees.
# Le journal de l'application est COPIE aussitot : la course suivante l'ecrase.
function Courir([string]$nom, [hashtable]$vars) {
	$out = Join-Path $tmp "$nom.txt"
	$log = Join-Path $tmp "$nom.log"
	$poses = @("NK_SONDE", "NK_MODE_PROBE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_SCULPT_GIZMO_MUTE",
			  "NK_BOITE_FAUSSE")
	$env:NK_SONDE = "1"; $env:NK_MODE_PROBE = "1"
	$env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	# La mutation porte les DEUX negatifs de ce banc : l'ancienne regle des modes,
	# et une boite publiee SANS CIBLE (NK_BOITE_FAUSSE).
	if ($Mutation) { $env:NK_SCULPT_GIZMO_MUTE = "1"; $env:NK_BOITE_FAUSSE = "1" }
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k]; $poses += $k }
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($k in $poses) { if (Test-Path "Env:\$k") { Remove-Item "Env:\$k" } }
	if (Test-Path $journal) { Copy-Item $journal $log -Force } else { Set-Content $log "" }
	return [pscustomobject]@{ out = $out; log = $log }
}

# Les lignes [MODE-SONDE] d'une course, pour le mode demande, maillage charge.
function Lignes($c, [int]$mode) {
	$r = @()
	foreach ($l in @(Select-String -Path $c.log -Pattern "\[MODE-SONDE\]")) {
		$m = [regex]::Match($l.Line, "img=(\d+) uiMode=(\d+) editMode=(\d) selV=(\d+) gizmoSel=(\d) " +
			"gizmoTri=(\d+) drag=(\d) marqPts=(\d+) cage=(\d+) nv=(\d+) somme=\((-?[0-9.e+-]+), (-?[0-9.e+-]+), (-?[0-9.e+-]+)\)")
		if (-not $m.Success) { continue }
		if ([int]$m.Groups[2].Value -ne $mode) { continue }
		$r += [pscustomobject]@{
			img = [int]$m.Groups[1].Value; selV = [int]$m.Groups[4].Value
			gizmoSel = [int]$m.Groups[5].Value; gizmoTri = [int]$m.Groups[6].Value
			drag = [int]$m.Groups[7].Value; marq = [int]$m.Groups[8].Value
			cage = [int]$m.Groups[9].Value; nv = [int]$m.Groups[10].Value
			sx = [double]$m.Groups[11].Value; sy = [double]$m.Groups[12].Value; sz = [double]$m.Groups[13].Value
		}
	}
	return $r
}
function Delta($L) {
	if ($L.Count -lt 2) { return -1.0 }
	$a = $L[0]; $b = $L[$L.Count - 1]
	return [Math]::Abs($b.sx - $a.sx) + [Math]::Abs($b.sy - $a.sy) + [Math]::Abs($b.sz - $a.sz)
}
function Max($L, [string]$champ) {
	$v = -1; foreach ($x in $L) { if ($x.$champ -gt $v) { $v = $x.$champ } }; return $v
}

$rouges = 0; $conditions = 0
function Dire([string]$nom, [bool]$vert, [string]$detail) {
	if ($vert) { Write-Host "VERT   $nom  $detail" } else { Write-Host "ROUGE  $nom  $detail"; $script:rouges++ }
}
function Condition([string]$nom, [bool]$ok, [string]$detail) {
	if (-not $ok) { Write-Host "COND   $nom  NON REUNIE : $detail"; $script:conditions++ }
	return $ok
}
function Image([string]$nom) {
	if (-not $Images) { return @{} }
	New-Item -ItemType Directory -Force $Images | Out-Null
	return @{ "NK_AGENT_SCENE" = "5"; "NK_FRAME_EDIT" = "0,100"; "NK_AGENT_SHOT" = "150";
			  "NK_TOAST_PROBE" = (Join-Path $Images "$nom.png") }
}
function Fusion([hashtable]$a, [hashtable]$b) { $r = @{}; foreach ($k in $a.Keys) { $r[$k] = $a[$k] }; foreach ($k in $b.Keys) { $r[$k] = $b[$k] }; return $r }

Write-Host ""
Write-Host "SONDE DE MESURE — Sculpture : le gizmo, tout comme Blender · mutation NK_SCULPT_GIZMO_MUTE : $(if ($Mutation) { 'ACTIVE' } else { 'inactive' })"
Write-Host "-----------------------------------------------------------------------"

# ── LES DEUX MODES A BROSSES ─────────────────────────────────────────────────
$noms = @{ 3 = "Sculpture"; 2 = "Sculpture 2.5D" }
foreach ($mode in @(3, 2)) {
	$n = $noms[$mode]
	# (a) CE QUI SE DESSINE, avec une selection NON VIDE posee par le crochet.
	$c = Courir "aff_$mode" (Fusion @{ "NK_EDIT_MODE" = "$mode,40"; "NK_EDIT_SEL" = "a"; "NK_AGENT_EXIT" = "170" } (Image "mode$mode"))
	$L = @(Lignes $c $mode | Where-Object { $_.img -ge 20 })
	if (Condition "$n (a)" (($L.Count -gt 0) -and ($L[0].nv -gt 0) -and ((Max $L "selV") -gt 0)) "nv=$(if ($L.Count) { $L[0].nv } else { '?' }) selV=$(Max $L 'selV') (il faut un maillage ET une selection)") {
		Dire "$n (a) gizmo de sommets : RIEN au trace" ((Max $L "gizmoTri") -eq 0) "triangles de gizmo max=$(Max $L 'gizmoTri') avec selV=$(Max $L 'selV') (avant correctif : 54)"
		Dire "$n (a) marqueurs de sommets et cage : RIEN" (((Max $L "marq") -eq 0) -and ((Max $L "cage") -eq 0)) "marqueurs max=$(Max $L 'marq') cage max=$(Max $L 'cage') (avant : 12 et 12)"
	}
	# (b) G PAR LE SHELL (la porte du clavier), avec un glisser et une validation.
	$c = Courir "g_$mode" @{ "NK_EDIT_MODE" = "$mode,40"; "NK_EDIT_SEL" = "a"; "NK_VP_ACTION" = "modalmove,120";
							 "NK_MODAL_DRAG" = "120,0"; "NK_MODAL_DRAG_FRAMES" = "6"; "NK_MODAL_CONFIRM" = "1"; "NK_AGENT_EXIT" = "220" }
	$L = @(Lignes $c $mode | Where-Object { $_.img -ge 20 })
	if (Condition "$n (b)" (($L.Count -gt 1) -and ((Max $L "selV") -gt 0)) "aucune ligne ou selection vide") {
		$d = Delta $L
		Dire "$n (b) G ne deplace AUCUN sommet" ($d -lt 1e-4) "variation de la somme des positions = $([Math]::Round($d, 5)) (avant : 58,7)"
	}
	# (c) LE GLISSER DU GIZMO : on appuie AU PIVOT, la ou la poignee centrale serait.
	$c = Courir "drag_$mode" @{ "NK_EDIT_MODE" = "$mode,40"; "NK_EDIT_SEL" = "a"; "NK_EDIT_DRAG" = "80,0,8,100"; "NK_AGENT_EXIT" = "200" }
	$L = @(Lignes $c $mode | Where-Object { $_.img -ge 20 })
	$appui = @(Select-String -Path $c.log -Pattern "NK_EDIT_DRAG appui").Count
	if (Condition "$n (c)" (($L.Count -gt 1) -and ($appui -gt 0)) "l'appui synthetique n'a pas eu lieu") {
		$d = Delta $L
		# Seuil 0,5 et non zero : en Sculpture l'appui est AUSSI un trait de brosse
		# (c'est voulu), qui bouge la somme de quelques millimes. Le gizmo, lui,
		# deplacait les 24 sommets (26,9).
		Dire "$n (c) le gizmo n'est JAMAIS attrape, rien ne glisse" (((Max $L "drag") -eq 0) -and ($d -lt 0.5)) "drag max=$(Max $L 'drag') variation=$([Math]::Round($d, 5)) (avant : drag=1, 26,9)"
	}
	# (d) LE CLIC DE SELECTION (meme porte que la souris), sur une selection vide.
	$c = Courir "clic_$mode" @{ "NK_EDIT_MODE" = "$mode,40"; "NK_EDIT_SEL" = "n"; "NK_EDIT_SELMASK" = "4"; "NK_EDIT_PICK" = "0.5,0.5,100"; "NK_AGENT_EXIT" = "160" }
	$L = @(Lignes $c $mode | Where-Object { $_.img -ge 20 })
	$pick = @(Select-String -Path $c.out -Pattern "NK_EDIT_PICK \(").Count
	if (Condition "$n (d)" (($L.Count -gt 0) -and ($pick -gt 0)) "le clic ecrit n'a pas ete pose") {
		Dire "$n (d) le clic ne selectionne AUCUN sommet" ((Max $L "selV") -eq 0) "selV max=$(Max $L 'selV') (avant : 12)"
	}
	# (e) LES OPERATIONS D'ELEMENTS PAR LE SHELL : extruder, supprimer.
	foreach ($op in @("extrude", "delete")) {
		$c = Courir "${op}_$mode" @{ "NK_EDIT_MODE" = "$mode,40"; "NK_EDIT_SEL" = "a"; "NK_VP_ACTION" = "$op,120"; "NK_AGENT_EXIT" = "180" }
		$L = @(Lignes $c $mode | Where-Object { $_.img -ge 20 })
		if (Condition "$n (e) $op" ($L.Count -gt 1) "aucune ligne") {
			$nv0 = $L[0].nv; $nv1 = $L[$L.Count - 1].nv
			Dire "$n (e) « $op » ne touche pas le maillage" (($nv0 -gt 0) -and ($nv1 -eq $nv0)) "sommets $nv0 -> $nv1 (avant : 24 -> $(if ($op -eq 'extrude') { '48' } else { '0' }))"
		}
	}
}

# ── LA SCULPTURE ELLE-MEME AGIT TOUJOURS ─────────────────────────────────────
# Le trait ecrit (NK_SCULPT_AT) passe par la MEME porte que la souris. Il vise
# le centre du cube cadre ; « touche=0 » = condition non reunie, pas un defaut.
# Le cube est SUBDIVISE d'abord (NK_EDIT_PRESUB) : ses 24 sommets de coin sont
# tous hors du rayon d'une brosse posee au centre d'une face, et un trait qui
# ne touche aucun sommet rend ok=0 -- mesure du 21/09, pas un defaut.
$c = Courir "trait_3" @{ "NK_EDIT_MODE" = "3,40"; "NK_EDIT_SEL" = "n"; "NK_EDIT_PRESUB" = "4"; "NK_FRAME_EDIT" = "0,80"; "NK_SCULPT_AT" = "524:300:8:110"; "NK_AGENT_EXIT" = "190" }
$L = @(Lignes $c 3 | Where-Object { $_.img -ge 20 })
$touche = @(Select-String -Path $c.log -Pattern "SCULPT rayon .* touche=1").Count
if (Condition "Sculpture (f)" (($L.Count -gt 1) -and ($touche -gt 0)) "le rayon du trait ecrit n'a pas touche le maillage") {
	$ok = @(Select-String -Path $c.log -Pattern "SCULPT souris : .* ok=1").Count
	Dire "Sculpture (f) la brosse deforme toujours le maillage" (($ok -gt 0) -and ((Delta $L) -gt 1e-6)) "trait ok=$ok variation=$([Math]::Round((Delta $L), 6))"
}

# ── EDITION ET OBJET : INCHANGES ─────────────────────────────────────────────
# Les memes gestes en Edition DOIVENT agir : sans ces temoins, « rien ne bouge »
# en Sculpture pourrait venir d'un crochet casse, pas du correctif.
$c = Courir "aff_1" (Fusion @{ "NK_EDIT_MODE" = "1,40"; "NK_EDIT_SEL" = "a"; "NK_AGENT_EXIT" = "170"; "NK_GIZMO_DIAG" = "1" } (Image "mode1"))
$L = @(Lignes $c 1 | Where-Object { $_.img -ge 20 })
if (Condition "Edition (a)" (($L.Count -gt 0) -and ((Max $L "selV") -gt 0)) "aucune ligne ou selection vide") {
	Dire "Edition (a) gizmo de sommets, marqueurs et cage traces" (((Max $L "gizmoTri") -gt 0) -and ((Max $L "marq") -gt 0) -and ((Max $L "cage") -gt 0)) "gizmo=$(Max $L 'gizmoTri') marqueurs=$(Max $L 'marq') cage=$(Max $L 'cage')"
	$g1 = @(Select-String -Path $c.log -Pattern "GIZMO-DIAG editMode=1 .* -> 1 gizmo\(s\)").Count
	$g2 = @(Select-String -Path $c.log -Pattern "GIZMO-DIAG editMode=1 .* -> 2 gizmo\(s\)").Count
	Dire "Edition (a) UN seul gizmo (les deux gizmos du 20/09 ne reviennent pas)" (($g2 -eq 0) -and ($g1 -gt 0)) "lignes « 1 gizmo »=$g1 · « 2 gizmos »=$g2"
}
$c = Courir "g_1" @{ "NK_EDIT_MODE" = "1,40"; "NK_EDIT_SEL" = "a"; "NK_VP_ACTION" = "modalmove,120"; "NK_MODAL_DRAG" = "120,0"; "NK_MODAL_DRAG_FRAMES" = "6"; "NK_MODAL_CONFIRM" = "1"; "NK_AGENT_EXIT" = "220" }
$L = @(Lignes $c 1 | Where-Object { $_.img -ge 20 })
if (Condition "Edition (b)" ($L.Count -gt 1) "aucune ligne") { Dire "Edition (b) G deplace les sommets" ((Delta $L) -gt 1.0) "variation=$([Math]::Round((Delta $L), 4))" }
$c = Courir "drag_1" @{ "NK_EDIT_MODE" = "1,40"; "NK_EDIT_SEL" = "a"; "NK_EDIT_DRAG" = "80,0,8,100"; "NK_AGENT_EXIT" = "200" }
$L = @(Lignes $c 1 | Where-Object { $_.img -ge 20 })
if (Condition "Edition (c)" ($L.Count -gt 1) "aucune ligne") { Dire "Edition (c) le gizmo s'attrape et deplace" (((Max $L "drag") -eq 1) -and ((Delta $L) -gt 1.0)) "drag max=$(Max $L 'drag') variation=$([Math]::Round((Delta $L), 4))" }
$c = Courir "clic_1" @{ "NK_EDIT_MODE" = "1,40"; "NK_EDIT_SEL" = "n"; "NK_EDIT_SELMASK" = "4"; "NK_EDIT_PICK" = "0.5,0.5,100"; "NK_AGENT_EXIT" = "160" }
$L = @(Lignes $c 1 | Where-Object { $_.img -ge 20 })
if (Condition "Edition (d)" ($L.Count -gt 0) "aucune ligne") { Dire "Edition (d) le clic selectionne" ((Max $L "selV") -gt 0) "selV max=$(Max $L 'selV')" }
$c = Courir "extrude_1" @{ "NK_EDIT_MODE" = "1,40"; "NK_EDIT_SEL" = "a"; "NK_VP_ACTION" = "extrude,120"; "NK_AGENT_EXIT" = "180" }
$L = @(Lignes $c 1 | Where-Object { $_.img -ge 20 })
if (Condition "Edition (e)" ($L.Count -gt 1) "aucune ligne") { Dire "Edition (e) E extrude" ($L[$L.Count - 1].nv -gt $L[0].nv) "sommets $($L[0].nv) -> $($L[$L.Count - 1].nv)" }

# OBJET : on entre en Edition puis on en SORT par TAB (la porte du shell) ; le
# cube reste selectionne et c'est le gizmo D'OBJET qui doit se dessiner, seul.
$c = Courir "objet" (Fusion @{ "NK_EDIT_MODE" = "1,40"; "NK_EDIT_SEL" = "a"; "NK_VP_ACTION" = "toggleedit,90"; "NK_AGENT_EXIT" = "170"; "NK_GIZMO_DIAG" = "1" } (Image "mode0"))
$o1 = @(Select-String -Path $c.log -Pattern "GIZMO-DIAG editMode=0 : emptyGizmo dessine=1 .* -> 1 gizmo\(s\)").Count
if (Condition "Objet" ($o1 -gt 0) "aucune ligne GIZMO-DIAG en mode Objet avec une selection") {
	Dire "Objet : le gizmo d'OBJET, seul, et plus aucun trace d'edition" ($o1 -gt 0) "lignes « emptyGizmo dessine=1 -> 1 gizmo »=$o1"
}

function Barre($c) {
	$g = @(Select-String -Path $c.out -Pattern "OUTILS-TRANSFORM clic ecrit bouton=1 mode=(\d+) grises=(\d) -> outil=(\d+)")
	if ($g.Count -eq 0) { return $null }
	$m = $g[0].Matches[0]
	return [pscustomobject]@{ mode = [int]$m.Groups[1].Value; grises = [int]$m.Groups[2].Value; outil = [int]$m.Groups[3].Value }
}
# ── (g) LES BOUTONS DE LA BARRE, ET LE GIZMO DU TRANSFORM DE SCULPTURE ─────
# 21/09 : grises, parce que l'outil n'existait pas. 25/09 : ACTIFS, parce qu'il
# existe -- masque, pivot, symetrie, et son gizmo. C'est la MEME regle qui a
# decide les deux fois : un bouton s'allume quand il agit.
#
# ⚠ LE CRITERE PORTE SUR L'EFFET, PAS SUR L'APPARENCE. « Le bouton est actif »
#   se satisferait d'un bouton qui s'allume pour rien -- exactement ce qu'on
#   refusait le 21/09. On exige donc DEUX choses ensemble : choisir l'outil fait
#   APPARAITRE le gizmo (triangles traces > 0), et le glisser DEPLACE la matiere.
#   Le negatif est dans la meme course : sans choisir l'outil, le meme glisser ne
#   trace rien et ne deplace rien.
function Gizmo($c, [int]$mode) {
	$L = @(Lignes $c $mode | Where-Object { $_.img -ge 20 })
	$d = Delta $L
	return [pscustomobject]@{ lignes = $L.Count; tri = (Max $L "gizmoTri"); bouge = $d }
}
$dragBase = @{ "NK_EDIT_USER" = "99"; "NK_EDIT_SEL" = "n"; "NK_EDIT_PRESUB" = "4";
			   "NK_SCULPT_DRAG" = "120,0,8,150"; "NK_AGENT_EXIT" = "280" }
foreach ($mode in @(3, 2)) {
	$n = $noms[$mode]
	$avec = Gizmo (Courir "gz_outil_$mode" (Fusion $dragBase @{ "NK_EDIT_MODE" = "$mode,40"; "NK_TOOL_CLIC" = "0,60" })) $mode
	$sans = Gizmo (Courir "gz_sans_$mode" (Fusion $dragBase @{ "NK_EDIT_MODE" = "$mode,40" })) $mode
	if (Condition "$n (g)" (($avec.lignes -gt 2) -and ($sans.lignes -gt 2)) "courses incompletes") {
		Dire "$n (g) l'outil choisi : le gizmo se trace ET le glisser deplace" `
			(($avec.tri -gt 0) -and ($avec.bouge -gt 1.0)) `
			"triangles=$($avec.tri) deplacement=$([Math]::Round($avec.bouge, 3))"
		Dire "$n (g) NEGATIF : sans l'outil, aucun gizmo et rien ne bouge" `
			(($sans.tri -eq 0) -and ($sans.bouge -eq 0.0)) `
			"triangles=$($sans.tri) deplacement=$([Math]::Round($sans.bouge, 5)) (le meme glisser, sans avoir choisi l'outil)"
		# ⚠ ET CE QUI REMPLACE LE GIZMO PAR DEFAUT DOIT AGIR : entrer dans un mode a
		#   brosses arme la BROSSE (l'entree de barre du 25/09). Sans ce critere,
		#   « rien ne bouge sans l'outil » serait satisfait par un mode ou plus rien
		#   ne marche -- le pire des verts.
		$brosse = @(Lignes (Courir "gz_brosse_$mode" (Fusion $dragBase @{ "NK_EDIT_MODE" = "$mode,40";
																		 "NK_SCULPT_STROKE" = "creuser:0.3:0.8:150" })) $mode |
					Where-Object { $_.img -ge 20 })
		Dire "$n (g) ... et la BROSSE, elle, agit par defaut" ((Delta $brosse) -gt 1e-4) `
			"deplacement d'un trait a l'entree dans le mode = $([Math]::Round((Delta $brosse), 5)) (outil par defaut : la brosse)"
	}
}
# EDITION INCHANGEE : le bouton choisit l'outil du gizmo de sommets, comme avant.
$b = Barre (Courir "barre_1" @{ "NK_EDIT_MODE" = "1,40"; "NK_TOOL_CLIC" = "1,120"; "NK_AGENT_EXIT" = "160" })
if (Condition "Edition (g)" (($null -ne $b) -and ($b.mode -eq 1)) "le clic ecrit n'a pas eu lieu en Edition") {
	Dire "Edition (g) les boutons sont ACTIFS : Rotation se choisit" (($b.grises -eq 0) -and ($b.outil -eq 3)) `
		"grises=$($b.grises) outil=$($b.outil)"
}

# ── (h) AUCUNE BOITE PUBLIEE SANS CIBLE, AU CLIC DU CURSEUR 3D ──────────────
# Defaut signale par Rodolf le 24/09 : « en cliquant sur le curseur de l'univers,
# une boite englobante apparait brievement et disparait, a chaque clic ». Ce
# critere ne juge pas l'image : il lit ce que la vue PUBLIE au masque de
# silhouette. `sans_cible` compte les soumissions dont le maillage est INVALIDE --
# une boite qui ne designe rien. Le clic du curseur 3D est ECRIT (NK_CURSOR_CLIC),
# l'outil Curseur est arme par le crochet du bouton (NK_TOOL_CLIC=4).
# ⚠ CONDITION : il faut qu'une boite soit publiee ce tour-la (un noeud cerne), et
#   que le clic ait eu lieu -- sinon le zero ne dirait rien.
$c = Courir "boite" @{ "NK_AGENT_SCENE" = "5"; "NK_BOITE_SONDE" = "1"; "NK_TOOL_CLIC" = "4,60";
					   "NK_CURSOR_CLIC" = "500:400:100:20"; "NK_AGENT_EXIT" = "200" }
$lignes = @(Select-String -Path $c.log -Pattern "\[BOITE-SONDE\]")
$clics = @(Select-String -Path $c.log -Pattern "NK_CURSOR_CLIC : clic ecrit").Count
$cernes = @($lignes | Where-Object { $_.Line -match "cerne_user=([1-9])" }).Count
$sans = @($lignes | Where-Object { $_.Line -match "sans_cible=([1-9])" }).Count
if (Condition "Curseur 3D (h)" (($lignes.Count -gt 0) -and ($clics -gt 0) -and ($cernes -gt 0)) "lignes=$($lignes.Count) clics=$clics images avec une boite=$cernes") {
	Dire "Curseur 3D (h) aucune boite publiee SANS CIBLE" ($sans -eq 0) "$sans image(s) sur $($lignes.Count) publient une boite sans maillage, sur $clics clic(s) du curseur"
}

Write-Host "-----------------------------------------------------------------------"
if ($rouges -gt 0) { Write-Host "ECHEC ($rouges rouge(s), $conditions condition(s) non reunie(s))"; exit 1 }
if ($conditions -gt 0) { Write-Host "CONDITION NON REUNIE ($conditions) -- ni vert ni rouge"; exit 3 }
Write-Host "TOUT VERT (0 rouge)"
exit 0
