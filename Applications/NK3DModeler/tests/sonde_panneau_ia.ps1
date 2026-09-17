# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_panneau_ia.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — (b9) LE PANNEAU DE L'ASSISTANT.
#
# ⚠ AUCUNE INJECTION D'ENTREE : ni souris, ni clavier. `NK_AI_DEMANDE` entre au
#   MEME point que le bouton « Envoyer » (la fonction `NkAiSoumettre`), pas plus
#   loin : une sonde qui entrerait apres mesurerait un chemin que Rodolf
#   n'emprunte jamais.
#   ⚠ CE QUI N'EST DONC PAS MESURE, ET JE LE DIS : la FRAPPE elle-meme. Ce qui
#     est mesure va du texte SOUMIS a l'operation APPLIQUEE.
#
# LES CRITERES, ECRITS AVANT
# --------------------------
# (a) LE PANNEAU ET LA SONDE FONT LA MEME CHOSE. La meme demande, par les deux
#     chemins, doit donner le MEME maillage. Le second instrument est le compteur
#     de faces, qui ne sait rien d'aucun des deux chemins.
#
# (b) ⚠ ET LA PREUVE QUE LE PANNEAU EMPRUNTE BIEN LE PONT N'EST PAS (a).
#     Deux chemins independants qui donneraient le meme resultat par hasard
#     passeraient (a) sans rien prouver. On mute donc LE PONT (`NK_PARAM_IGNORE`)
#     et on verifie que LE PANNEAU EN SOUFFRE AUTANT QUE LA SONDE : si le panneau
#     avait sa propre implementation, la mutation du pont ne l'atteindrait pas.
#     C'est la seule facon de distinguer « ils s'accordent » de « c'est le meme
#     code ».
#
# (c) UN REFUS PORTE SON MOTIF, ET PAS SEULEMENT AU JOURNAL. Le motif est ecrit
#     dans `st.aiMotif`, que le panneau AFFICHE. Rodolf n'a pas de console : un
#     outil qui refuse sans le dire a l'ecran se lit comme un outil casse.
#
# (d) LE ZERO. Champ vide -> RIEN ne part, rien ne s'execute, et on le dit.
#     Sans cette garde, un clic distrait soumettrait la chaine vide, que la table
#     refuserait en disant « je ne connais pas "" » : exact et incomprehensible.
#
# (e) NEGATIF : une demande qui n'a PAS ete soumise ne s'execute pas toute seule.
#     Une demande consommee ne se rejoue pas a l'image suivante.
#
# USAGE :  pwsh -File Applications/NK3DModeler/tests/sonde_panneau_ia.ps1 [-Image]
# CODES :  0 tout vert · 1 au moins un rouge
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release",
	[switch]$Image
)

$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

$script:rouges = 0
function Dire([string]$nom, [bool]$ok, [string]$detail) {
	if ($ok) { Write-Host ("VERT   {0}  {1}" -f $nom, $detail) }
	else { Write-Host ("ROUGE  {0}  {1}" -f $nom, $detail); $script:rouges++ }
}

$script:n = 0
# $par = "panneau" (NK_AI_DEMANDE) ou "sonde" (NK_VP_ACTION2) -- LES DEUX CHEMINS.
function Lancer([string]$par, [string]$texte, [bool]$mut, [bool]$shot) {
	$script:n++
	$out = Join-Path ([System.IO.Path]::GetTempPath()) ("nk_panneau_{0}.txt" -f $script:n)
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SELMASK = "4"
	$env:NK_VP_ACTION = "selectall,60"
	if ($par -eq "panneau") { $env:NK_AI_DEMANDE = "$texte,90" }
	elseif ($texte) { $env:NK_VP_ACTION2 = "$texte,90" }
	if ($mut) { $env:NK_PARAM_IGNORE = "1" }
	if ($shot) {
		# ⚠ TROIS CHOSES, ET IL EN MANQUAIT DEUX. Une capture seule montrait
		#   l ECRAN D ACCUEIL : il faut AUSSI passer a la scene (NK_AGENT_SCENE)
		#   et DEPLIER le groupe (replie par defaut, comme tous les autres).
		#   Une image qui ne montre pas ce qu elle prouve ne prouve rien.
		$env:NK_AGENT_SHOT = "150"; $env:NK_AGENT_SCENE = "30"
		$env:NK_DEPLIER = "prop.g.edai"
	}
	$env:NK_EDIT_REPORT = "80,130"
	$env:NK_AGENT_EXIT = "190"
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE",
			"NK_EDIT_SELMASK", "NK_VP_ACTION", "NK_VP_ACTION2", "NK_AI_DEMANDE",
			"NK_PARAM_IGNORE", "NK_AGENT_SHOT", "NK_AGENT_SCENE", "NK_DEPLIER",
			"NK_EDIT_REPORT", "NK_AGENT_EXIT")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$r = [pscustomobject]@{ f0 = -1; f1 = -1; refus = ""; soumise = $false; vide = $false }
	$n2 = 0
	foreach ($x in @(Select-String -Path $out -Pattern "EDIT RAPPORT")) {
		$m = [regex]::Match($x.Line, "f=(\d+)")
		if (-not $m.Success) { continue }
		if ($n2 -eq 0) { $r.f0 = [int]$m.Groups[1].Value } else { $r.f1 = [int]$m.Groups[1].Value }
		$n2++
	}
	foreach ($x in @(Select-String -Path $out -Pattern "AI RESULTAT")) {
		$m = [regex]::Match($x.Line, "REFUS : (.+)$")
		if ($m.Success) { $r.refus = $m.Groups[1].Value.Trim() }
	}
	$r.soumise = (@(Select-String -Path $out -Pattern "-> soumise").Count -gt 0)
	$r.vide = (@(Select-String -Path $out -Pattern "refusee \(vide\)").Count -gt 0)
	return $r
}

$pan = Lancer "panneau" "subdivide:3" $false $false
$son = Lancer "sonde"   "subdivide:3" $false $false
$panM = Lancer "panneau" "subdivide:3" $true $false
$sonM = Lancer "sonde"   "subdivide:3" $true $false
$ref = Lancer "panneau" "rends ce modele plus beau" $false $false
$zero = Lancer "panneau" "" $false $false
$rien = Lancer "sonde"   "" $false $false

Write-Host ("       panneau -> {0} faces · sonde -> {1} faces · sous mutation : {2} / {3}" -f `
	$pan.f1, $son.f1, $panM.f1, $sonM.f1)

Dire "(a) le PANNEAU et la SONDE donnent le MEME maillage" `
	(($pan.f1 -eq $son.f1) -and ($pan.f1 -gt $pan.f0)) `
	"panneau $($pan.f0) -> $($pan.f1) · sonde $($son.f0) -> $($son.f1)"

Dire "(b) MUTER LE PONT atteint le PANNEAU autant que la sonde" `
	(($panM.f1 -eq $sonM.f1) -and ($panM.f1 -ne $pan.f1)) `
	"sous NK_PARAM_IGNORE : panneau $($panM.f1) · sonde $($sonM.f1) (contre $($pan.f1) sans mutation) -- le panneau n'a donc pas sa propre implementation"

Dire "(c) un REFUS porte son MOTIF" `
	(($ref.refus -ne "") -and ($ref.f1 -eq $ref.f0)) `
	"motif rendu : « $($ref.refus) » ; faces inchangees $($ref.f0) -> $($ref.f1)"

Dire "(d) LE ZERO : champ vide, rien ne part" `
	($zero.vide -and (-not $zero.soumise) -and ($zero.f1 -eq $zero.f0)) `
	"« refusee (vide) », rien n'est soumis, faces $($zero.f0) -> $($zero.f1)"

Dire "(e) NEGATIF : sans demande, rien ne s'execute" `
	(($rien.f1 -eq $rien.f0) -and ($rien.refus -eq "")) `
	"aucune demande -> faces $($rien.f0) -> $($rien.f1), aucun refus (le panneau ne rejoue rien)"

if ($Image) {
	Write-Host "-----------------------------------------------------------------------"
	Write-Host "  IMAGE : une capture est prise a la frame 150, apres une demande acceptee."
	$img = Lancer "panneau" "subdivide:3" $false $true
	$derniere = Get-ChildItem (Join-Path $Arbre "captures") -Filter "*.png" -ErrorAction SilentlyContinue |
		Sort-Object LastWriteTime -Descending | Select-Object -First 1
	if ($derniere) { Write-Host "  -> $($derniere.FullName)" }
	else { Write-Host "  -> AUCUNE IMAGE PRODUITE (le crochet n'a pas ecrit)" }
}

Write-Host "-----------------------------------------------------------------------"
if ($script:rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "$($script:rouges) ROUGE(S)"
exit 1
