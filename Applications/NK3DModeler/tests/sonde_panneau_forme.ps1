# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_panneau_forme.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — (b9) LA FORME DU PANNEAU, celle que Rodolf a tranchee le 17/09
# (echanges/PANNEAU_IA_SPEC.md : « ils doivent ressembler a celui de VSCode »).
#
# ⚠ AUCUNE INJECTION D'ENTREE au sens du systeme : les clics passent par le
#   crochet d'API `NK_AGENT_CLICK`, qui pose une position et un appui DANS la
#   boucle, comme tous les lots precedents. Aucune souris reelle ne bouge.
#
# ⚠ ET LES COORDONNEES NE SONT PAS CALCULEES PAR LA SONDE. Elles sont LUES dans
#   la trace `AI BLOC`, ou la PEINTURE a ecrit les rectangles qu'elle vient de
#   dessiner. Une sonde qui refabriquerait la mise en page de son cote
#   mesurerait SA formule : le jour ou le panneau bouge d'un pixel, elle
#   cliquerait a cote en restant verte. D'ou les deux temps : on lit, puis on
#   clique la ou c'est.
#
# LES CRITERES, ECRITS AVANT LE CODE (canal R38)
# ----------------------------------------------
# (A) ancre a DROITE, pleine hauteur, et il COUVRE la pastille de proprietes --
#     donc il est dans l'overlay. Derive de la mise en page, pas d'un seuil.
# (B) une barre d'onglets par fournisseur, LOCAL premier et actif par defaut.
# (D) chaque operation porte son effet MESURE, et le meme chiffre se lit dans
#     DEUX instruments sans code commun : le bloc du panneau et la ligne
#     « EDIT RAPPORT », qui ne sait rien du panneau.
# (E) « Annuler cette action » n'existe que sur la DERNIERE operation, et il
#     ramene vraiment les comptes d'avant.
# (F) deplier n'execute RIEN : le bloc s'ouvre, les comptes ne bougent pas.
# (G) un refus est un bloc a part, avec son motif, maillage intact.
# (H) le zero : champ vide -> aucun bloc, rien ne s'execute.
#
# (C) « meme operation par le champ que par la sonde » et la mutation du pont
#     restent mesures par `sonde_panneau_ia.ps1` : les redoubler ici ferait deux
#     ecritures du meme critere, qui divergeraient a la premiere correction.
#
# USAGE :  pwsh -File Applications/NK3DModeler/tests/sonde_panneau_forme.ps1 [-Image]
# CODES :  0 tout vert · 1 au moins un rouge · 3 condition non reunie
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release",
	[switch]$Image
)
. (Join-Path $PSScriptRoot "condition.ps1")
$script:fichiers = @()

$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

$script:rouges = 0
function Dire([string]$nom, [bool]$ok, [string]$detail) {
	if ($ok) { Write-Host ("VERT   {0}  {1}" -f $nom, $detail) }
	else { Write-Host ("ROUGE  {0}  {1}" -f $nom, $detail); $script:rouges++ }
}

# Un lancement : une scene neuve, un cube, le mode edition, tout selectionne,
# une demande, une trace du panneau, et deux rapports de comptes.
function Lancer([string]$nom, [string]$demande, [string]$clic, [int]$trace, [switch]$Shot, [switch]$Accueil) {
	$out = Join-Path ([System.IO.Path]::GetTempPath()) ("nk_forme_{0}.txt" -f $nom)
	$script:fichiers += $out
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SELMASK = "4"
	$env:NK_VP_ACTION = "selectall,60"
	# ⚠ ON QUITTE L'ACCUEIL DANS TOUS LES CAS SAUF -Accueil. La regle de Rodolf
	#   du 17/09 interdit le panneau au lanceur : mesurer le panneau la-bas
	#   mesurerait un etat que l'application n'a plus le droit d'avoir.
	if (-not $Accueil) { $env:NK_AGENT_SCENE = "30" }
	if ($null -ne $demande) { $env:NK_AI_DEMANDE = "$demande,90" }
	if ($clic) { $env:NK_AGENT_CLICK = $clic }
	$env:NK_AI_TRACE = "$trace"
	$env:NK_EDIT_REPORT = "80,200"
	$env:NK_AGENT_EXIT = "230"
	if ($Shot) { $env:NK_AGENT_SHOT = "205" }
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE",
			"NK_EDIT_SELMASK", "NK_VP_ACTION", "NK_AI_DEMANDE", "NK_AGENT_CLICK",
			"NK_AI_TRACE", "NK_EDIT_REPORT", "NK_AGENT_EXIT", "NK_AGENT_SHOT",
			"NK_AGENT_SCENE")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$r = [pscustomobject]@{
		x = -1.0; y = -1.0; w = -1.0; h = -1.0; fenW = -1.0; fenH = -1.0
		px = -1.0; py = -1.0; pw = -1.0; ph = -1.0
		onglet = -1; fournisseur = ""; blocs = @(); f0 = -1; f1 = -1; vide = $false
		etat = ""; survole = ""; bloque = -1; absent = $false; motifAbsence = ""; refus = ""

	}
	$p = Select-String -Path $out -Pattern "AI PANNEAU rect=\(([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)\) fenetre=\((\d+),(\d+)\) props=\(([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)\) onglet=(\d+) fournisseur=(\w+) blocs=(\d+)" |
		Select-Object -First 1
	if ($p) {
		$g = $p.Matches[0].Groups
		$r.x = [double]$g[1].Value; $r.y = [double]$g[2].Value
		$r.w = [double]$g[3].Value; $r.h = [double]$g[4].Value
		$r.fenW = [double]$g[5].Value; $r.fenH = [double]$g[6].Value
		$r.px = [double]$g[7].Value; $r.py = [double]$g[8].Value
		$r.pw = [double]$g[9].Value; $r.ph = [double]$g[10].Value
		$r.onglet = [int]$g[11].Value; $r.fournisseur = $g[12].Value
	}
	$blocs = @()
	foreach ($l in @(Select-String -Path $out -Pattern "AI BLOC (\d+) type=(\d+) replie=(\d+) mesure=(\d+) ligne=\(([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)\) annuler=\(([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)\) v=(-?\d+)->(-?\d+) f=(-?\d+)->(-?\d+) texte=""([^""]*)"" out=""([^""]*)"" motif=""([^""]*)""")) {
		$g = $l.Matches[0].Groups
		$blocs += [pscustomobject]@{
			i = [int]$g[1].Value; type = [int]$g[2].Value; replie = [int]$g[3].Value
			mesure = [int]$g[4].Value
			lx = [double]$g[5].Value; ly = [double]$g[6].Value
			lw = [double]$g[7].Value; lh = [double]$g[8].Value
			ux = [double]$g[9].Value; uy = [double]$g[10].Value
			uw = [double]$g[11].Value; uh = [double]$g[12].Value
			vA = [int]$g[13].Value; vB = [int]$g[14].Value
			fA = [int]$g[15].Value; fB = [int]$g[16].Value
			texte = $g[17].Value; sortie = $g[18].Value; motif = $g[19].Value
		}
	}
	$r.blocs = $blocs
	$n = 0
	foreach ($x in @(Select-String -Path $out -Pattern "EDIT RAPPORT")) {
		$m = [regex]::Match($x.Line, "f=(\d+)")
		if (-not $m.Success) { continue }
		if ($n -eq 0) { $r.f0 = [int]$m.Groups[1].Value } else { $r.f1 = [int]$m.Groups[1].Value }
		$n++
	}
	$ab = Select-String -Path $out -Pattern 'AI PANNEAU absent=1 ouvert=(\d+) accueil=(\d+) motif=(\S+)' |
		Select-Object -First 1
	if ($ab) { $r.absent = $true; $r.motifAbsence = $ab.Matches[0].Groups[3].Value }
	$rf = Select-String -Path $out -Pattern 'AI (?:RESULTAT : .+ -> REFUS|DEMANDE .+ -> refusee) : (.+)$' | Select-Object -First 1
	if ($rf) { $r.refus = $rf.Matches[0].Groups[1].Value.Trim() }
	$sv = Select-String -Path $out -Pattern 'AI SURVOL souris=\([-\d.]+,[-\d.]+\) survole="([^"]*)" bloque=(\d+) etat="([^"]*)"' |
		Select-Object -First 1
	if ($sv) {
		$r.survole = $sv.Matches[0].Groups[1].Value
		$r.bloque = [int]$sv.Matches[0].Groups[2].Value
		$r.etat = $sv.Matches[0].Groups[3].Value
	}
	$r.vide = (@(Select-String -Path $out -Pattern "refusee : .*demande est vide").Count -gt 0)
	return $r
}

Write-Host ""
Write-Host "SONDE DE MESURE — (b9) LA FORME DU PANNEAU (spec du 17/09)"
Write-Host "-----------------------------------------------------------------------"

# ── TEMPS 1 : une demande acceptee, on LIT ce que le panneau a dessine ─────────
$a = Lancer "accepte" "subdivide:3" "" 120

Write-Host ("       panneau=({0},{1},{2}x{3}) fenetre={4}x{5} pastille=({6},{7},{8}x{9})" -f `
	$a.x, $a.y, $a.w, $a.h, $a.fenW, $a.fenH, $a.px, $a.py, $a.pw, $a.ph)

Dire "(A) ancre a DROITE, pleine hauteur, et il COUVRE la pastille" `
	(($a.x + $a.w -eq $a.fenW) -and ($a.x -lt $a.px) -and ($a.y -le $a.py) -and `
	 (($a.y + $a.h) -ge ($a.py + $a.ph))) `
	("bord droit $($a.x + $a.w) = fenetre $($a.fenW) · gauche $($a.x) < pastille $($a.px) · " +
	 "haut $($a.y) <= $($a.py) · bas $($a.y + $a.h) >= $($a.py + $a.ph)")

Dire "(B) le LOCAL est le premier onglet ET l'actif par defaut" `
	(($a.onglet -eq 0) -and ($a.fournisseur -eq "Local")) `
	"onglet=$($a.onglet) fournisseur=$($a.fournisseur)"

$op = @($a.blocs | Where-Object { $_.type -eq 1 }) | Select-Object -Last 1
$dem = @($a.blocs | Where-Object { $_.type -eq 0 })
Dire "(D) l'effet est MESURE, et DEUX instruments donnent le meme chiffre" `
	(($null -ne $op) -and ($op.mesure -eq 1) -and ($op.fB -eq $a.f1) -and ($op.fA -eq $a.f0)) `
	("bloc du panneau : faces $($op.fA) -> $($op.fB) · EDIT RAPPORT (qui ignore le panneau) : " +
	 "$($a.f0) -> $($a.f1)")

Dire "(F-zero) les blocs naissent REPLIES" `
	(($null -ne $op) -and ($op.replie -eq 1)) `
	"le bloc d'operation a replie=$($op.replie) (exige 1 : c'est la forme de la capture)"

Dire "(E-zero) la demande de l'utilisateur est un bloc VISIBLE, jamais replie" `
	(($dem.Count -ge 1) -and ($dem[0].replie -eq 0)) `
	"$($dem.Count) bloc(s) de demande, replie=$($dem[0].replie) (exige 0 : c'est la question)"

# ── TEMPS 2 : on clique LA OU LE PANNEAU A DESSINE « Annuler cette action » ────
if ($null -ne $op -and $op.uw -gt 0) {
	$cx = [int]($op.ux + $op.uw / 2); $cy = [int]($op.uy + $op.uh / 2)
	$b = Lancer "annule" "subdivide:3" "150,$cx,$cy" 170
	$ops = @($b.blocs | Where-Object { $_.type -eq 1 })
	$anciens = @($ops | Select-Object -SkipLast 1)
	$eteints = @($anciens | Where-Object { $_.uw -eq 0 }).Count
	Dire "(E) le clic sur « Annuler cette action » RAMENE les comptes d'avant" `
		(($b.f1 -eq $b.f0) -and ($b.f0 -gt 0)) `
		"faces $($b.f0) avant la demande, $($b.f1) apres l'annulation (exige EGAL et > 0)"
	Dire "(E-bis) seule la DERNIERE operation porte le bouton" `
		(($ops.Count -ge 2) -and ($eteints -eq $anciens.Count)) `
		"$($ops.Count) operations, $eteints ancienne(s) sur $($anciens.Count) avec le bouton eteint"
} else {
	Dire "(E) le clic sur « Annuler cette action » RAMENE les comptes d'avant" $false `
		"le bouton n'a pas ete dessine ACTIF : rien a cliquer (uw=$($op.uw))"
	Dire "(E-bis) seule la DERNIERE operation porte le bouton" $false "non mesure"
}

# ── TEMPS 3 : DEPLIER, et il ne doit RIEN executer ────────────────────────────
if ($null -ne $op) {
	$cx = [int]($op.lx + $op.lw / 2); $cy = [int]($op.ly + $op.lh / 2)
	$c = Lancer "deplie" "subdivide:3" "150,$cx,$cy" 170
	$opc = @($c.blocs | Where-Object { $_.type -eq 1 }) | Select-Object -Last 1
	Dire "(F) DEPLIER ouvre le bloc et n'execute RIEN" `
		(($null -ne $opc) -and ($opc.replie -eq 0) -and ($c.f1 -eq $opc.fB)) `
		"replie=$($opc.replie) apres le clic (exige 0) · faces a la fin $($c.f1), inchangees depuis l'operation ($($opc.fB))"
}

# ── TEMPS 4 : LE REFUS, ET LE ZERO ────────────────────────────────────────────
$d = Lancer "refus" "rends ce modele plus beau" "" 120
$ref = @($d.blocs | Where-Object { $_.type -eq 2 }) | Select-Object -Last 1
Dire "(G) un REFUS est un bloc a part, avec son motif, maillage intact" `
	(($null -ne $ref) -and ($ref.motif -ne "") -and ($d.f1 -eq $d.f0)) `
	"bloc de refus, motif : « $($ref.motif) » · faces $($d.f0) -> $($d.f1)"

Dire "(I) la ligne d'etat nomme le dorsal ET dit que le modele N'EST PAS CHARGE" `
	(($a.etat -like "*NKDesignLLM*") -and ($a.etat -like "*NON CHARGE*")) `
	("« " + $a.etat + " » (exige le nom du dorsal ET « NON CHARGE » : le modele ne l'est pas," +
	 " et un cout affiche sans cette reserve se lit comme une mesure de l'instant)")

$e = Lancer "zero" "" "" 120
Dire "(H) LE ZERO : champ vide -> aucun bloc, rien ne s'execute" `
	($e.vide -and ($e.blocs.Count -eq 0) -and ($e.f1 -eq $e.f0)) `
	"« refusee (vide) »=$($e.vide) · $($e.blocs.Count) bloc(s) · faces $($e.f0) -> $($e.f1)"

# ── TEMPS 5 : LE LANCEUR. Pas de panneau, et le refus le DIT ──────────────────
$l = Lancer "lanceur" "subdivide:3" "" 120 -Accueil
Dire "(J) AU LANCEUR : aucun panneau, et la demande est REFUSEE avec son motif" `
	($l.absent -and ($l.motifAbsence -eq "ecran-d-accueil") -and ($l.blocs.Count -eq 0) -and `
	 ($l.refus -ne "")) `
	("absent=$($l.absent) motif=$($l.motifAbsence) · $($l.blocs.Count) bloc(s) · refus : " +
	 "« $($l.refus) » (exige un refus NOMME plutot qu'une coquille vide)")

Dire "(K) EN PROJET : le panneau est la, au meme montage a un seul detail pres" `
	((-not $a.absent) -and ($a.w -gt 0) -and ($a.blocs.Count -gt 0)) `
	"le meme lancement, sans -Accueil : panneau de $($a.w) px, $($a.blocs.Count) bloc(s)"

if ($Image) {
	Write-Host "-----------------------------------------------------------------------"
	$avant = @(Get-ChildItem (Join-Path $Arbre "captures") -Filter "*.png" -ErrorAction SilentlyContinue |
		ForEach-Object { $_.FullName })
	$null = Lancer "image" "subdivide:3" "" 210 -Shot
	$apres = @(Get-ChildItem (Join-Path $Arbre "captures") -Filter "*.png" -ErrorAction SilentlyContinue |
		ForEach-Object { $_.FullName })
	$neuf = @($apres | Where-Object { $avant -notcontains $_ })
	if ($neuf.Count -eq 1) { Write-Host "  IMAGE -> $($neuf[0])" }
	else { Write-Host "  AUCUNE IMAGE PRODUITE ($($neuf.Count) fichiers neufs)" }
}

Write-Host "-----------------------------------------------------------------------"
# ⚠ LE VERDICT NE VAUT QUE SI LA CONDITION ETAIT REUNIE.
if (NkConditionManque $script:fichiers) {
	Write-Host "  ⚠ LES LIGNES CI-DESSUS NE SONT PAS DES VERDICTS."
	exit 3
}
if ($script:rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "$($script:rouges) ROUGE(S)"
exit 1
