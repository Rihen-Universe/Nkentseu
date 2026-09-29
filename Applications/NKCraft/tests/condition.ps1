# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/condition.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# LA GARDE DE CONDITION — « la condition n'etait pas reunie » n'est pas « le code
# est casse », et un banc qui ne sait pas dire la premiere accuse toujours le second.
#
# CE QUI L'A RENDUE NECESSAIRE, ET C'EST MESURE (17/09, ~18h52)
# -------------------------------------------------------------
# Juste apres une construction complete, trois bancs ont rendu ROUGE : `comptes`,
# `gestes`, `loopcut`. Leurs sorties etaient pleines de valeurs sentinelles --
# `-999`, `(rien)`, et surtout « v=0 e=0 f=0 (lu=0) » sur un cube qui a six faces.
# Un maillage a zero face n'est pas un defaut de geste : c'est UNE MESURE QUI
# N'A PAS EU LIEU. Les memes bancs, rejoues plus tard sur LE MEME BINAIRE, sont
# revenus tout verts -- deux fois.
#
# Le mecanisme : nos crochets de mesure se declenchent a des NUMEROS D'IMAGE
# fixes (`NK_ADD_NODE` a l'image 20, `NK_EDIT_MODE` a l'image 40). Ce qui prepare
# la scene, lui, ne suit pas le compteur d'images -- il depend du disque, des
# caches de nuanceurs, et d'une carte graphique que l'entrainement d'Ilyana
# occupe. Quand la preparation prend du retard, le crochet tire dans le vide et
# TOUT ce qui suit lit des sentinelles.
#
# ⚠️ ET LE PIRE N'EST PAS LE ROUGE : dans cet etat, deux criteres du banc des
#    gestes sont passes au VERT en comparant deux sentinelles entre elles
#    (« -1 px » contre « -1 px »). Un banc qui n'a rien mesure peut donc mentir
#    DANS LES DEUX SENS.
#
# LE CRITERE DE CONDITION, et pourquoi celui-la
# ---------------------------------------------
# Le marqueur est la ligne que l'hote ecrit quand il entre VRAIMENT en mode
# edition : « [Demo3D] EDIT MODE (utilisateur #N) ». Il ne vient pas du banc, il
# vient du produit, et il n'est ecrit que si la scene, l'objet et la topologie
# existent. Un banc d'edition dont la sortie ne le contient pas n'a pas mesure
# l'edition -- quoi qu'il affiche ensuite.
#
# Code de sortie 3 : NI vert (0) NI rouge (1). Un troisieme etat, parce qu'il y a
# trois reponses possibles et que les confondre est precisement le defaut.

function NkConditionReunie([string]$fichier) {
	if (-not (Test-Path $fichier)) { return $false }
	# ⚠ DEUX MARQUEURS, ET IL EN FALLAIT DEUX. Ma premiere version n exigeait que
	#   la ligne « EDIT MODE (utilisateur » -- une ligne du JOURNALISEUR. Or les
	#   bancs ne capturent pas tous le journal : `sonde_comptes` ne redirige que
	#   la sortie standard, et sa condition etait donc declaree NON REUNIE sur
	#   des lancements parfaitement sains. Un instrument qui se trompe dans ce
	#   sens est aussi nuisible que celui qu il remplace : il aurait appris a
	#   ignorer ses propres alertes.
	$lu1 = @(Select-String -Path $fichier -Pattern "\(lu=1\)").Count
	$lu0 = @(Select-String -Path $fichier -Pattern "\(lu=0\)").Count
	$edit = @(Select-String -Path $fichier -Pattern "EDIT MODE \(utilisateur").Count
	if ($lu1 -gt 0) { return $true }   # l hote avait bien un maillage a lire
	if ($lu0 -gt 0) { return $false }  # il a lu, et il n y avait rien : la scene manquait
	if ($edit -gt 0) { return $true }  # aucun rapport demande, mais l edition s est ouverte
	return $false                       # aucune preuve d aucun cote
}

# Rend $true si la condition manque, APRES l'avoir dit a l'ecran. L'appelant sort
# alors avec 3. On nomme le fichier fautif : sans lui, on relance a l'aveugle.
function NkConditionManque([string[]]$fichiers) {
	$mauvais = @()
	foreach ($f in $fichiers) {
		if (-not (NkConditionReunie $f)) { $mauvais += $f }
	}
	if ($mauvais.Count -eq 0) { return $false }
	Write-Host "-----------------------------------------------------------------------"
	Write-Host "CONDITION NON REUNIE — ce banc n'accuse pas le code, il dit qu'il n'a pas pu mesurer."
	Write-Host "  La scene n'etait pas prete quand les crochets ont tire (images 20 et 40)."
	Write-Host "  Causes connues : caches de nuanceurs froids apres une construction, et"
	Write-Host "  une carte graphique occupee (l'entrainement d'Ilyana en prend 4,3 Go)."
	foreach ($f in $mauvais) { Write-Host "  sans « EDIT MODE (utilisateur » ou avec « lu=0 » : $f" }
	Write-Host "  A REFAIRE tel quel : si la condition est reunie, le verdict sera vert ou rouge."
	Write-Host "-----------------------------------------------------------------------"
	return $true
}
