# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_pont_ia.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — (b9) LE PONT « UN VERBE ET SES PARAMETRES -> UNE OPERATION ».
#
# ⚠ AUCUNE INJECTION D'ENTREE : ni souris, ni clavier. Tout passe par les crochets
#   d'API du binaire. La fenetre porte « *** SONDE DE MESURE *** » dans son titre.
#
# CE QUE LE PONT DOIT TENIR, ET POURQUOI CHAQUE CRITERE EXISTE
# ------------------------------------------------------------
# Le pont est ce qui decide si une pastille IA vaut quelque chose : un panneau
# sans pont n'est qu'une boite de dialogue. Une IA ecrit « biseaute a 0,2 avec
# 4 segments » ; il faut que cela devienne une commande REELLE, bornee, annulable,
# et qu'un verbe invente se refuse en DISANT pourquoi.
#
# (a) le verbe SEUL agit                    subdivide        -> le maillage change
# (b) le PARAMETRE agit sur le maillage     subdivide:3      -> BEAUCOUP plus de faces
# (c) deux parametres, dans l'ordre         bevel:0.2:4      -> largeur ET segments
# (d) hors bornes : RAMENE, pas refuse      bevel:99         -> 10 (le vmax de la table)
# (e) NEGATIF : sans parametre, rien ne     subdivide        -> subdiv reste 1
#     se remet a zero
# (f) un parametre EN TROP se dit           bevel:1:2:3      -> motif imprime
# (g) un verbe inconnu se refuse AVEC son   fusionnerlesetoiles -> « nom inconnu »
#     motif
# (h) TOUT PASSE PAR L'ANNULATION           apres l'operation : annuler=1, et un
#                                           undo rend les comptes d'AVANT
#
# ⚠ POURQUOI (d) RAMENE AU LIEU DE REFUSER. `Demo3DHostOpParamSet` porte le clamp,
#   et son commentaire dit pourquoi : « LE CLAMP EST ICI ET NULLE PART AILLEURS.
#   Un champ regle par deux chemins avec deux bornes differentes laisse entrer par
#   l'un ce que l'autre refuse. » Le pont ne borne donc RIEN lui-meme. Et une IA
#   qui demande un biseau de 99 doit obtenir le plus large possible, pas un echec
#   muet -- c'est aussi ce que fait deja la molette.
#
# ⚠ POURQUOI (h) COMPTE AUTANT QUE LE RESTE. Une operation declenchee par une IA
#   qui ne s'annulerait pas comme une operation de la main rendrait l'outil
#   inessayable : on n'ose pas demander a une machine de modifier son travail si
#   on ne peut pas revenir en arriere d'un geste.
#
# MUTATION :  -Mutation pose NK_PARAM_IGNORE=1 -- les parametres sont LUS puis
#             JETES. (b)(c)(d) doivent alors rougir. Une mutation qui ne change
#             rien rend un faux verdict EN VERT : la sonde sort en ECHEC (code 2).
#
# USAGE :  pwsh -File Applications/NK3DModeler/tests/sonde_pont_ia.ps1 [-Mutation]
# CODES :  0 tout vert · 1 au moins un rouge · 2 la mutation n'a pas pris
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release",
	[switch]$Mutation
)

$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

$script:rouges = 0
function Dire([string]$nom, [bool]$ok, [string]$detail) {
	if ($ok) { Write-Host ("VERT   {0}  {1}" -f $nom, $detail) }
	else { Write-Host ("ROUGE  {0}  {1}" -f $nom, $detail); $script:rouges++ }
}

# ⚠ LE NOM DE FICHIER NE PEUT PAS PORTER LE VERBE TEL QUEL : « bevel:0.2 » contient
#   un deux-points, que Windows refuse dans un chemin. Premiere version de cette
#   sonde : trois essais silencieusement perdus, et leurs criteres auraient ete
#   juges sur des fichiers absents. On assainit, et on numerote.
$script:essai = 0
function Essai([string]$verbe, [string]$undo) {
	$script:essai++
	$out = Join-Path ([System.IO.Path]::GetTempPath()) ("nk_pont_{0}.txt" -f $script:essai)
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SELMASK = "4"
	$env:NK_VP_ACTION = "selectall,60"
	if ($verbe) { $env:NK_VP_ACTION2 = "$verbe,90" }
	if ($undo) { $env:NK_VP_ACTION3 = "undo,140" }
	if ($Mutation) { $env:NK_PARAM_IGNORE = "1" }
	$env:NK_EDIT_REPORT = $(if ($undo) { "80,120,170" } else { "80,120" })
	$env:NK_AGENT_EXIT = "200"
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE",
			"NK_EDIT_SELMASK", "NK_VP_ACTION", "NK_VP_ACTION2", "NK_VP_ACTION3",
			"NK_PARAM_IGNORE", "NK_EDIT_REPORT", "NK_AGENT_EXIT")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$r = [pscustomobject]@{ avant = $null; apres = $null; apresUndo = $null
							trop = $false; inconnu = $false }
	$r.trop = (@(Select-String -Path $out -Pattern "parametre en trop").Count -gt 0)
	$r.inconnu = (@(Select-String -Path $out -Pattern "nom inconnu").Count -gt 0)
	$n = 0
	foreach ($x in @(Select-String -Path $out -Pattern "EDIT RAPPORT")) {
		$m = [regex]::Match($x.Line,
			"f=(\d+).*annuler=(\d+).*bevOff=([-0-9.]+) bevSeg=(\d+) subdiv=(\d+) loop=(\d+)")
		if (-not $m.Success) { continue }
		$e = [pscustomobject]@{ f = [int]$m.Groups[1].Value; annuler = [int]$m.Groups[2].Value
								# ⚠ PAS DE CONVERSION DE VIRGULE ICI, ET C'EST LE PIEGE INVERSE DE
								#   CELUI QU'ON CONNAISSAIT. On sait que PowerShell ECRIT la virgule
								#   decimale en fr-FR ; mais le CAST [double] lit, lui, en culture
								#   INVARIANTE. Remplacer le point par une virgule donnait donc 200
								#   pour « 0.200 » et 10000 pour « 10.000 » -- deux rouges qui
								#   accusaient le produit alors que l'application imprimait juste.
								#   Le point du printf C se lit tel quel.
								bevOff = [double]::Parse($m.Groups[3].Value, [System.Globalization.CultureInfo]::InvariantCulture)
								bevSeg = [int]$m.Groups[4].Value
								subdiv = [int]$m.Groups[5].Value; loop = [int]$m.Groups[6].Value }
		if ($n -eq 0) { $r.avant = $e } elseif ($n -eq 1) { $r.apres = $e } else { $r.apresUndo = $e }
		$n++
	}
	return $r
}

$sd1 = Essai "subdivide" ""
$sd3 = Essai "subdivide:3" ""
$bv2 = Essai "bevel:0.2:4" ""
$bv9 = Essai "bevel:99" ""
$trp = Essai "bevel:1:2:3" ""
$inc = Essai "fusionnerlesetoiles" ""
$und = Essai "subdivide:3" "undo"

Write-Host ("       subdivide nu -> f={0} · subdivide:3 -> f={1} · bevel:0.2:4 -> off={2} seg={3}" -f `
	$sd1.apres.f, $sd3.apres.f, $bv2.apres.bevOff, $bv2.apres.bevSeg)

Dire "(a) le verbe SEUL agit sur le maillage" ($sd1.apres.f -gt $sd1.avant.f) `
	"faces $($sd1.avant.f) -> $($sd1.apres.f)"
Dire "(b) le PARAMETRE agit sur le maillage" ($sd3.apres.f -gt $sd1.apres.f) `
	"subdivide:3 -> $($sd3.apres.f) faces contre $($sd1.apres.f) sans parametre (exige plus)"
Dire "(c) DEUX parametres, dans l'ordre de la table" `
	(([Math]::Abs($bv2.apres.bevOff - 0.2) -lt 0.001) -and ($bv2.apres.bevSeg -eq 4)) `
	"bevel:0.2:4 -> largeur=$($bv2.apres.bevOff) segments=$($bv2.apres.bevSeg)"
Dire "(d) hors bornes : RAMENE au maximum, pas refuse" `
	(([Math]::Abs($bv9.apres.bevOff - 10.0) -lt 0.001) -and ($bv9.apres.f -gt $bv9.avant.f)) `
	"bevel:99 -> largeur=$($bv9.apres.bevOff) (le vmax de la table) et l'operation s'execute : f=$($bv9.apres.f)"
Dire "(e) NEGATIF : sans parametre, rien ne se remet a zero" `
	(($sd1.apres.subdiv -eq 1) -and ($sd1.apres.bevOff -eq 0)) `
	"subdivide nu -> subdiv=$($sd1.apres.subdiv) bevOff=$($sd1.apres.bevOff) (les reglages courants sont conserves)"
Dire "(f) un parametre EN TROP se dit" $trp.trop `
	"bevel:1:2:3 -> motif imprime, et les deux premiers sont poses (off=$($trp.apres.bevOff) seg=$($trp.apres.bevSeg))"
Dire "(g) un verbe INCONNU se refuse AVEC son motif" `
	($inc.inconnu -and ($inc.apres.f -eq $inc.avant.f)) `
	"« nom inconnu, aucune action posee » ; faces inchangees $($inc.avant.f) -> $($inc.apres.f)"
Dire "(h) TOUT PASSE PAR L'ANNULATION" `
	(($und.apres.annuler -eq 1) -and ($und.apresUndo -ne $null) -and ($und.apresUndo.f -eq $und.avant.f)) `
	"annuler=$($und.apres.annuler) ; faces $($und.avant.f) -> $($und.apres.f) -> $(if ($und.apresUndo) { $und.apresUndo.f } else { 'PAS DE RELEVE' }) apres undo"

Write-Host "-----------------------------------------------------------------------"
if ($Mutation) {
	if ($script:rouges -eq 0) {
		Write-Host "ECHEC  NK_PARAM_IGNORE n'a RIEN change : verdict non concluant"
		exit 2
	}
	Write-Host "MUTATION OK : $($script:rouges) rouge(s), les parametres sont bien la cause"
	exit 0
}
if ($script:rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "$($script:rouges) ROUGE(S)"
exit 1
