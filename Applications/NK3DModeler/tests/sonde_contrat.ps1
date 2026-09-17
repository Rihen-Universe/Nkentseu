# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_contrat.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — LE CONTRAT D'OUTILS DIT-IL LA VERITE ?
#
# Arbitrage de Rodolf (17/09) : « le plus important est la PERFORMANCE DES OUTILS
# qui sont associes au modele ». Le contrat est le document qu'on donnera a un
# modele distant, a Ilyana, ou a un modele de Rodolf. Un contrat FAUX est pire
# qu'un contrat absent : il fait echouer un modele qui a pourtant obei.
#
# ⚠ CE QUE CETTE SONDE MESURE, ET QUE LA RELECTURE NE PEUT PAS DONNER : que le
#   document et le pont disent LA MEME CHOSE. Ils sont pourtant construits sur la
#   meme table -- justement, c'est ce qu'il faut verifier, pas supposer : le jour
#   ou quelqu'un ajoutera un verbe ailleurs (un `if` en dur dans le viseur), le
#   document ne le connaitra pas et cette sonde le dira.
#
# LES CRITERES, ECRITS AVANT
# --------------------------
# (a) CHAQUE verbe annonce par le contrat est ACCEPTE par le pont. Un seul refus
#     rougit : le document promettrait un outil qui n'existe pas.
# (b) NEGATIF : un verbe ABSENT du contrat est REFUSE, avec son motif. Sans ce
#     cas, (a) passerait avec un pont qui accepte n'importe quoi.
# (c) le contrat annonce AU MOINS un parametre borne, et ses bornes sont celles
#     que l'application applique VRAIMENT (une valeur au-dela est ramenee).
# (d) LE ZERO : le contrat n'est pas vide, et il liste autant de verbes que la
#     table -- un document tronque en silence serait le pire des deux mondes.
#
# USAGE :  pwsh -File Applications/NK3DModeler/tests/sonde_contrat.ps1
# CODES :  0 tout vert · 1 au moins un rouge
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release"
)
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

$script:rouges = 0
function Dire([string]$nom, [bool]$ok, [string]$detail) {
	if ($ok) { Write-Host ("VERT   {0}  {1}" -f $nom, $detail) }
	else { Write-Host ("ROUGE  {0}  {1}" -f $nom, $detail); $script:rouges++ }
}

# ── 1. L'APPLICATION ECRIT SON CONTRAT (aucune fenetre, aucune carte) ─────────
$doc = Join-Path ([System.IO.Path]::GetTempPath()) "nk_contrat_outils.md"
if (Test-Path $doc) { Remove-Item $doc }
& $exe --contrat-outils $doc | Out-Null
if (-not (Test-Path $doc)) { Write-Host "ROUGE  le contrat n'a pas ete ecrit"; exit 1 }
$texte = Get-Content $doc -Raw

# Les verbes, LUS DANS LE DOCUMENT : c'est bien le document qu'on eprouve, pas
# une liste que la sonde aurait recopiee de son cote.
$verbes = @()
foreach ($m in [regex]::Matches($texte, '(?m)^\| `([a-z]+)` \|')) { $verbes += $m.Groups[1].Value }
Write-Host ("       le contrat annonce {0} verbes" -f $verbes.Count)

# ── 2. CHAQUE VERBE ANNONCE PASSE-T-IL LE PONT ? ──────────────────────────────
function Poser([string]$verbe) {
	$out = Join-Path ([System.IO.Path]::GetTempPath()) "nk_contrat_essai.txt"
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_AGENT_SCENE = "30"
	$env:NK_VP_ACTION = "$verbe,70"; $env:NK_AGENT_EXIT = "110"
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE",
			"NK_AGENT_SCENE", "NK_VP_ACTION", "NK_AGENT_EXIT")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	return (@(Select-String -Path $out -Pattern "nom inconnu").Count -gt 0)
}

$refuses = @()
foreach ($v in $verbes) { if (Poser $v) { $refuses += $v } }
Dire "(a) chaque verbe ANNONCE par le contrat est accepte par le pont" `
	(($verbes.Count -gt 0) -and ($refuses.Count -eq 0)) `
	("$($verbes.Count) verbes essayes, $($refuses.Count) refuse(s)" +
	 $(if ($refuses.Count) { " : " + ($refuses -join ', ') } else { "" }))

# ── 3. LE NEGATIF ─────────────────────────────────────────────────────────────
# « subdiviser » est le mot FRANCAIS de `subdivide` : le genre de verbe qu'un
# modele produit spontanement, et qui doit etre refuse NOMMEMENT plutot que
# devine. C'est aussi ce qui prouve que (a) ne passe pas par complaisance.
$inconnu = Poser "subdiviser"
Dire "(b) NEGATIF : un verbe ABSENT du contrat est refuse" $inconnu `
	"« subdiviser » (le mot francais) -> refuse=$inconnu (exige un refus : sinon (a) ne prouverait rien)"

# ── 4. LES BORNES ANNONCEES SONT CELLES QUI S'APPLIQUENT ──────────────────────
# Le contrat annonce une borne pour la largeur du biseau. On demande au-dela :
# l'application doit RAMENER a cette borne -- et c'est la borne du DOCUMENT qu'on
# verifie, lue dans son texte.
$mb = [regex]::Match($texte, '`Largeur[^`]*`[^0-9]*\(reel, ([0-9.]+)\.\.([0-9.]+)\)')
if (-not $mb.Success) { $mb = [regex]::Match($texte, '\(reel, ([0-9.]+)\.\.([0-9.]+)\)') }
if ($mb.Success) {
	$vmax = [double]::Parse($mb.Groups[2].Value, [System.Globalization.CultureInfo]::InvariantCulture)
	$out = Join-Path ([System.IO.Path]::GetTempPath()) "nk_contrat_borne.txt"
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_AGENT_SCENE = "30"; $env:NK_EDIT_SELMASK = "4"
	$env:NK_VP_ACTION = "selectall,60"; $env:NK_VP_ACTION2 = ("bevel:{0},70" -f ($vmax * 10))
	$env:NK_EDIT_REPORT = "100"; $env:NK_AGENT_EXIT = "130"
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_AGENT_SCENE",
			"NK_EDIT_SELMASK", "NK_VP_ACTION", "NK_VP_ACTION2", "NK_EDIT_REPORT", "NK_AGENT_EXIT")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$lu = -1.0
	$l = @(Select-String -Path $out -Pattern "EDIT RAPPORT")
	if ($l.Count -gt 0) {
		$r = [regex]::Match($l[$l.Count - 1].Line, "bevOff=([0-9.]+)")
		if ($r.Success) {
			$lu = [double]::Parse($r.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
		}
	}
	Dire "(c) la borne ANNONCEE est celle qui s'applique" ([Math]::Abs($lu - $vmax) -lt 0.001) `
		"le contrat annonce un maximum de $vmax ; demande a $($vmax * 10), l'application a retenu $lu"
} else {
	Dire "(c) la borne ANNONCEE est celle qui s'applique" $false `
		"aucune borne reelle trouvee dans le document : le contrat ne dit pas ses bornes"
}

# ── 5. LE ZERO ────────────────────────────────────────────────────────────────
$annonce = [regex]::Match($texte, 'Les (\d+) verbes')
$nDit = if ($annonce.Success) { [int]$annonce.Groups[1].Value } else { -1 }
Dire "(d) LE ZERO : le contrat n'est pas vide, et son compte est le sien" `
	(($verbes.Count -gt 0) -and ($nDit -eq $verbes.Count)) `
	"l'entete annonce $nDit verbes, le tableau en contient $($verbes.Count) (exige l'egalite : un document tronque en silence serait le pire des deux mondes)"

Write-Host "-----------------------------------------------------------------------"
Write-Host "  le contrat : $doc"
if ($script:rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "$($script:rouges) ROUGE(S)"
exit 1
