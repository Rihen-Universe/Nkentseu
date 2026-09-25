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
	$poses = @("NK_SONDE", "NK_MASQUE_IGNORE")
	$env:NK_SONDE = "1"
	if ($Mutation) { $env:NK_MASQUE_IGNORE = "1" }
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

Write-Host "-----------------------------------------------------------------------"
if ($rouges -gt 0) { Write-Host "ECHEC ($rouges rouge(s), $conditions condition(s) non reunie(s))"; exit 1 }
if ($conditions -gt 0) { Write-Host "CONDITION NON REUNIE ($conditions) -- ni vert ni rouge"; exit 3 }
Write-Host "TOUT VERT (0 rouge)"
exit 0
