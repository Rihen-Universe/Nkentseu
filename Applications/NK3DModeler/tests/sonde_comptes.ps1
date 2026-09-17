# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_comptes.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — (b10) LES COMPTES, et (b6) LES TROIS SOUS-MODES.
#
# ⚠ AUCUNE INJECTION D'ENTREE : ni souris, ni clavier. Tout passe par des
#   crochets d'API dans le binaire (NK_*), et la fenetre porte « *** SONDE DE
#   MESURE *** » dans son titre. Aucune capture de l'ecran.
#
# CE QU'ELLE PROUVE, ET POURQUOI CES CRITERES-LA
# ----------------------------------------------
# (b10) Le critere n'est PAS « Blender dit 8 » : c'est la COHERENCE INTERNE du
#       maillage. La caracteristique d'Euler d'un volume ferme vaut
#           V - E + F = 2
#       Avant correctif, la barre d'etat publiait 24 sommets (des COINS) a cote
#       de 12 aretes (deja soudees par POSITION) : 24 - 12 + 6 = 18, impossible.
#       Apres : 8 - 12 + 6 = 2.
#       Ce critere ne depend d'AUCUNE machine, d'aucune souris, d'aucun bureau --
#       et c'est precisement ce que je cherche depuis que la position de la souris
#       de Rodolf a fait rougir deux sondes (--sonde-warp, puis sonde_gestes (m)).
#       La topologie se juge elle-meme.
#
# (b6)  Un verbe ACCEPTE qui ne fait rien est pire qu'un verbe absent. Avant
#       correctif, `submodeface` et `submodeedge` rendaient masque=1 (inchange)
#       tandis que `submodevert`, ABSENT de la table, se refusait avec son motif.
#       L'absent se comportait mieux que les presents, parce qu'il le disait.
#
# MUTATIONS (dans le MEME binaire, une par critere) :
#   -Mutation        NK_VERT_COINS=1 : les comptes redeviennent des coins.
#                    (b10) doit alors rougir, et Euler redire 18.
#
# USAGE :  pwsh -File Applications/NK3DModeler/tests/sonde_comptes.ps1 [-Mutation]
# CODES :  0 tout vert · 1 au moins un rouge · 2 la mutation n'a pas pris
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release",
	[switch]$Mutation
)
# ── LA GARDE DE CONDITION ──────────────────────────────────────────────────
# Un banc qui n a pas pu mesurer doit le DIRE, au lieu d accuser le code : le
# 17/09 celui-ci a rendu rouge sur une scene qui n etait pas prete, et deux de
# ses criteres sont meme passes VERT en comparant deux sentinelles entre elles.
. (Join-Path $PSScriptRoot "condition.ps1")
$script:fichiers = @()

$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

$script:rouges = 0
function Dire([string]$nom, [bool]$ok, [string]$detail) {
	if ($ok) { Write-Host ("VERT   {0}  {1}" -f $nom, $detail) }
	else { Write-Host ("ROUGE  {0}  {1}" -f $nom, $detail); $script:rouges++ }
}

# Un lancement = une scene neuve, un cube a l'utilisateur, le mode edition, un
# rapport. `NK_EDIT_USER=99` vise l'objet de L'UTILISATEUR : viser un objet de
# demo marque supprime=1 avait deja coute une soiree (la hierarchie le
# deselectionnait a juste titre, et j'avais accuse le produit).
function Relever([string]$nom, [string]$masque, [string]$pick, [string]$action) {
	$out = Join-Path ([System.IO.Path]::GetTempPath()) "nk_comptes_$nom.txt"
	$script:fichiers += $out
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SEL = "n"; $env:NK_EDIT_SELMASK = $masque
	if ($pick) { $env:NK_EDIT_PICK_VERT = $pick }
	if ($action) { $env:NK_VP_ACTION = $action }
	if ($Mutation) { $env:NK_VERT_COINS = "1" }
	$env:NK_EDIT_REPORT = "110"; $env:NK_AGENT_EXIT = "140"
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
			"NK_EDIT_SELMASK", "NK_EDIT_PICK_VERT", "NK_VP_ACTION", "NK_EDIT_REPORT",
			"NK_AGENT_EXIT", "NK_VERT_COINS")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$res = [pscustomobject]@{ v = -1; e = -1; f = -1; t = -1; selV = -1; selE = -1; selF = -1;
							  masque = -1; refuse = $false }
	$l = @(Select-String -Path $out -Pattern "EDIT RAPPORT")
	if ($l.Count -gt 0) {
		$m = [regex]::Match($l[0].Line,
			"v=(\d+) e=(\d+) f=(\d+) t=(\d+) \| selection=-?\d+ \| selV=(\d+) selE=(\d+) selF=(\d+) \| masque=(\d+)")
		if ($m.Success) {
			$res.v = [int]$m.Groups[1].Value; $res.e = [int]$m.Groups[2].Value
			$res.f = [int]$m.Groups[3].Value; $res.t = [int]$m.Groups[4].Value
			$res.selV = [int]$m.Groups[5].Value; $res.selE = [int]$m.Groups[6].Value
			$res.selF = [int]$m.Groups[7].Value; $res.masque = [int]$m.Groups[8].Value
		}
	}
	$res.refuse = (@(Select-String -Path $out -Pattern "nom inconnu").Count -gt 0)
	return $res
}

Write-Host "----- (b10) LES COMPTES ------------------------------------------------"
$un   = Relever "un_sommet" "1" "0,-1,0,70" ""
$tout = Relever "tout_somm" "1" "" "selectall,70"
$euler = $tout.v - $tout.e + $tout.f
Write-Host ("       cube : v=$($tout.v) e=$($tout.e) f=$($tout.f) t=$($tout.t) · un clic -> selV=$($un.selV)")

# ⚠ LE CRITERE INTERNE, ET C'EST LE PRINCIPAL. Il ne cite aucun chiffre attendu
#   de l'exterieur : il verifie que le maillage est coherent AVEC LUI-MEME.
Dire "(a) EULER : V - E + F = 2 sur un volume ferme" ($euler -eq 2) `
	"$($tout.v) - $($tout.e) + $($tout.f) = $euler (exige 2 ; valait 18 quand les sommets etaient des coins)"

# Blender dit 1, et c'est ce que Rodolf voit. Mais ce critere-ci depend d'une
# convention exterieure : il vient APRES celui d'Euler, pas avant.
Dire "(b) un clic sur un coin compte pour UN sommet" ($un.selV -eq 1) `
	"un clic -> selV=$($un.selV) (exige 1 ; valait 3, les trois copies coincidentes)"

# ⚠ (c) NE DISTINGUE PAS LE DEFAUT DE (b10), ET JE LE DIS PLUTOT QUE DE LE
#   LAISSER CROIRE. Sous la mutation NK_VERT_COINS il reste VERT : « selV == v »
#   est vrai dans les DEUX unites (24 == 24 comme 8 == 8). Il ne prouve donc pas
#   que les sommets sont soudes -- c'est (a) et (b) qui le font.
#   Ce qu'il prouve, et c'est autre chose : que les DEUX LECTEURS (le compteur de
#   selection et Demo3DHostStats) lisent bien la MEME autorite. Il rougirait le
#   jour ou l'un des deux serait corrige sans l'autre, et c'est precisement le
#   defaut que la barre d'etat s'interdit dans son propre commentaire.
Dire "(c) les DEUX LECTEURS lisent la meme autorite" ($tout.selV -eq $tout.v) `
	"selV=$($tout.selV) contre v=$($tout.v) (exige l'egalite : la selection et son total dans la MEME unite)"

# NEGATIF : la correction ne devait toucher QUE la branche sommet.
Dire "(d) NEGATIF : aretes, faces et triangles INCHANGES" `
	(($tout.selE -eq 12) -and ($tout.selF -eq 6) -and ($tout.t -eq 12)) `
	"selE=$($tout.selE) (12) · selF=$($tout.selF) (6) · t=$($tout.t) (12)"

Write-Host "----- (b6) LES TROIS SOUS-MODES ----------------------------------------"
$face = Relever "sm_face" "1" "" "submodeface,70"
$edge = Relever "sm_edge" "1" "" "submodeedge,70"
$vert = Relever "sm_vert" "4" "" "submodevert,70"
$neg  = Relever "sm_maj"  "1" "" "maj+submodeface,70"
Write-Host ("       face -> $($face.masque) · edge -> $($edge.masque) · vert -> $($vert.masque) · maj+face -> $($neg.masque)")

Dire "(e) le verbe AGIT : submodeface pose le bit face" ($face.masque -eq 4) `
	"masque=$($face.masque) (exige 4 ; le verbe etait ACCEPTE et rendait 1 : il ne faisait rien)"
Dire "(f) le verbe AGIT : submodeedge pose le bit arete" ($edge.masque -eq 2) `
	"masque=$($edge.masque) (exige 2)"
Dire "(g) submodevert EXISTE et agit" (($vert.masque -eq 1) -and (-not $vert.refuse)) `
	"masque=$($vert.masque) refuse=$($vert.refuse) (il manquait a la table : on pouvait quitter le sous-mode Sommet, jamais y revenir)"

# ⚠ CE NEGATIF A EVITE UNE REGRESSION REELLE, et c'est pour ca qu'il est ecrit
#   ici plutot que raconte. La garde etait `!st.pendingShift` ; or les
#   modificateurs sont CONSOMMES avec l'action dix lignes plus haut
#   (`majAct = st.pendingShift; st.pendingShift = false;`), donc la garde etait
#   TOUJOURS vraie et ecrasait le masque y compris sur Maj+1/2/3 au CLAVIER.
#   Sans ce negatif, la regression partait. La table accepte « maj+<nom> » : le
#   modificateur passe par le chemin du bouton SANS injecter d'evenement clavier.
Dire "(h) NEGATIF : avec Maj, le verbe n'ECRASE PAS la combinaison" ($neg.masque -eq 1) `
	"maj+submodeface -> masque=$($neg.masque) (exige 1, inchange : le viseur seul sait COMBINER par XOR)"

Write-Host "-----------------------------------------------------------------------"
if ($Mutation) {
	# Sous NK_VERT_COINS, (a)(b)(c) DOIVENT rougir. Si tout est vert, la mutation
	# n'a pas pris -- et une mutation qui ne mute rien rend un faux verdict.
	if ($script:rouges -eq 0) {
		Write-Host "ECHEC  la mutation NK_VERT_COINS n'a RIEN change : verdict non concluant"
		exit 2
	}
	Write-Host "MUTATION OK : $($script:rouges) rouge(s), le defaut d'origine est bien reproduit"
	exit 0
}
# ⚠ LE VERDICT NE VAUT QUE SI LA CONDITION ETAIT REUNIE. Sinon, ni vert ni
#   rouge : code 3, et les lignes ci-dessus ne sont pas des verdicts.
if (NkConditionManque $script:fichiers) {
	Write-Host "  ⚠ LES LIGNES CI-DESSUS NE SONT PAS DES VERDICTS."
	exit 3
}
if ($script:rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "$($script:rouges) ROUGE(S)"
exit 1
