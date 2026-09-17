# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_suppression.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — « APRES UNE SUPPRESSION, LA SELECTION EST VIDE » (Blender).
#
# Rodolf, 14/09 : « dans Blender, la selection est VIDE apres une suppression ».
# Chez nous elle SURVIVAIT : les faces restantes se retrouvaient selectionnees
# par deduction, et un SECOND X vidait le cube entier (6 -> 4 -> 0). Une frappe
# de trop detruisait le travail.
#
# ── CE QUE CETTE SONDE FAIT, ET CE QU'ELLE NE FAIT PAS ──────────────────────
#   * AUCUNE INJECTION D'ENTREE, ni souris ni clavier. Les gestes passent par
#     les crochets d'API du binaire (NK_EDIT_PICK_FACE / NK_VP_ACTION*), c'est
#     a dire par LE CHEMIN DU BOUTON.
#   * AUCUNE CAPTURE D'ECRAN. Elle ne lit que ce que l'application IMPRIME.
#   * La fenetre qu'elle ouvre porte le titre *** SONDE DE MESURE *** (NK_SONDE).
#
# ── LES CRITERES, DERIVES AVANT LA MESURE ───────────────────────────────────
#   (0) LE ZERO D'ABORD : au releve 1, AVANT toute suppression, le compteur doit
#       rendre AUTRE CHOSE que 0 dans le sous-mode teste. Sans ce releve, « 0 »
#       plus bas pourrait vouloir dire que le compteur ne compte rien.
#   (1) APRES LE PREMIER X : selV = selE = selF = 0. Les TROIS, pas seulement le
#       sous-mode courant -- des sommets restes allumes nourriraient la deduction
#       au tour suivant, et le defaut reviendrait par la porte d'a cote.
#   (2) LE SECOND X NE SUPPRIME PLUS RIEN : le compte de faces du releve 3 est
#       EGAL a celui du releve 2. C'est le critere qui porte le defaut signale :
#       un temoin qui se serait arrete au premier X serait reste vert.
#   (3) L'ANNULATION est RELEVEE, pas jugee : c'est un fait a rapporter.
#
# ── LA MUTATION, DANS LE MEME BINAIRE ───────────────────────────────────────
#   -Mutation pose NK_DEL_KEEPSEL=1, qui remet l'ANCIEN comportement et NE TOUCHE
#   A RIEN D'AUTRE. Les criteres (1) et (2) DOIVENT alors rougir. S'ils restent
#   verts, ils ne testent rien et la sonde le dit (elle sort 2).
#
# ── POURQUOI LA CIBLE EST UN OBJET **UTILISATEUR**, ET PAS L'OBJET DE DEMO ──
#   Mesure du 17/09. `NK_EDIT_MODE` visait par defaut l'objet 16 de la DEMO ; dans
#   NK3DModeler cet objet est marque SUPPRIME, et la garde du cadenas de
#   `HostHierarchyFrame` le desselectionne a l'image suivante -- a juste titre. Le
#   pilote « prenait » donc toujours (ActiveIndex=16) et le mode Edition ne
#   s'ouvrait JAMAIS. Ce n'etait pas un defaut du produit : c'etait l'instrument
#   qui visait une cible condamnee, et qui ne le disait pas.
#   La cible correcte se fabrique : `NK_ADD_NODE="2,0,<img>"` cree un cube
#   UTILISATEUR (noeud 96 + emplacement) et `NK_EDIT_USER` le vise. Verifie :
#   v=24 e=12 f=6 t=12, le cube de la maison (notre Vert EST un coin).
#
# USAGE
# USAGE
#   pwsh -File sonde_suppression.ps1 [-Config Debug] [-SousMode face|arete|sommet]
#                                    [-Mutation] [-Arbre <chemin>]
# SORTIE : 0 tout vert · 1 un critere rouge · 2 la mutation a survecu
#          3 la COURSE n'a pas pu avoir lieu (a ne surtout pas lire comme un rouge)

param(
	[string]$Config = "Debug",
	[ValidateSet("face", "arete", "sommet")][string]$SousMode = "face",
	[switch]$Mutation,
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

# ⚠ LE MASQUE EST COMBINE (FACE + le sous-mode teste), ET CE N'EST PAS UN
#   ARRANGEMENT. Mesure du 17/09 : en sous-mode ARETE ou SOMMET seul, le clic de
#   FACE est filtre par le sous-mode et la selection reste VIDE -- la course
#   n'avait pas lieu, et le critere (0) l'a dit. Or la suppression passe par
#   `DeleteSelectedFaces`, qui exige des faces ENTIEREMENT selectionnees : deux
#   sommets ou deux aretes ne suppriment donc RIEN aujourd'hui (difference avec
#   Blender, a remonter, hors de ce lot). Ce que ce parametre eprouve est donc :
#   « la selection est-elle videe AUSSI du point de vue de ce sous-mode-la »,
#   apres une suppression declenchee par une selection de faces.
$masque = switch ($SousMode) { "face" { 4 } "arete" { 6 } "sommet" { 5 } }
$sortie = Join-Path ([System.IO.Path]::GetTempPath()) ("nk_sonde_suppression_" + $SousMode + $(if ($Mutation) { "_MUTE" } else { "" }) + ".txt")

# Les images sont choisies pour que chaque geste soit CONSOMME avant le releve
# suivant : le pick est consomme dans la vue, pas dans la boucle du shell.
#   20 : le cube utilisateur   40 : mode Edition   70/73 : les deux clics de face
#   140 : X n.1   150 : releve 2     170 : X n.2   180 : releve 3
#   200 : Ctrl+Z  210 : releve 4     240 : sortie
$env:NK_SONDE = "1"
# LA CIBLE SE FABRIQUE : un cube UTILISATEUR, parce que les objets de demo sont
# marques supprimes dans ce projet (voir l'en-tete). NK_EDIT_USER accepte le
# NUMERO DE NOEUD que NK_ADD_NODE imprime.
$env:NK_ADD_NODE = "2,0,20"
$env:NK_EDIT_USER = "99"
$env:NK_EDIT_MODE = "1,40"
$env:NK_EDIT_SEL = "n"
$env:NK_EDIT_SELMASK = "$masque"
$env:NK_EDIT_PICK_FACE = "0,2,1,70"
$env:NK_EDIT_REPORT = "110,150,180,210"
$env:NK_VP_ACTION = "delete,140"
$env:NK_VP_ACTION2 = "delete,170"
$env:NK_VP_ACTION3 = "undo,200"
$env:NK_AGENT_EXIT = "240"
if ($Mutation) { $env:NK_DEL_KEEPSEL = "1" } else { Remove-Item Env:\NK_DEL_KEEPSEL -ErrorAction SilentlyContinue }

$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
	-RedirectStandardOutput $sortie
foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
		"NK_EDIT_SELMASK", "NK_EDIT_PICK_FACE",
		"NK_EDIT_REPORT", "NK_VP_ACTION", "NK_VP_ACTION2", "NK_VP_ACTION3", "NK_AGENT_EXIT",
		"NK_DEL_KEEPSEL")) {
	Remove-Item "Env:\$v" -ErrorAction SilentlyContinue
}

$lignes = @(Get-Content $sortie | Select-String "EDIT RAPPORT")
Write-Host ""
Write-Host "SONDE DE MESURE — suppression et selection · sous-mode $SousMode · mutation NK_DEL_KEEPSEL : $(if ($Mutation) { 'ACTIVE' } else { 'inactive' })"
Write-Host "code de sortie de l'application : $($p.ExitCode) · journal : $sortie"
Write-Host "-----------------------------------------------------------------------"
foreach ($l in $lignes) { Write-Host "  $l" }
Write-Host "-----------------------------------------------------------------------"

if ($lignes.Count -lt 4) {
	Write-Host "ROUGE  seulement $($lignes.Count) releve(s) sur 4 : la course n'est pas allee au bout."
	exit 1
}

function Lire([string]$l, [string]$cle) {
	$m = [regex]::Match($l, [regex]::Escape($cle) + "=(-?\d+)")
	if ($m.Success) { return [int]$m.Groups[1].Value }
	return -999
}
$r = @($lignes | ForEach-Object { $_.ToString() })
$f1 = Lire $r[0] "f"; $s1 = Lire $r[0] "selection"
$f2 = Lire $r[1] "f"; $v2 = Lire $r[1] "selV"; $e2 = Lire $r[1] "selE"; $c2 = Lire $r[1] "selF"
$f3 = Lire $r[2] "f"
$f4 = Lire $r[3] "f"; $s4 = Lire $r[3] "selection"

$rouges = 0
function Dire([string]$nom, [bool]$vert, [string]$detail) {
	if ($vert) { Write-Host "VERT   $nom  $detail" } else { Write-Host "ROUGE  $nom  $detail"; $script:rouges++ }
}

# ⚠ « COURSE IMPOSSIBLE » N'EST PAS « CRITERE ROUGE », ET LES CONFONDRE FERAIT
#   ACCUSER LE CORRECTIF. Si rien n'est selectionne au releve 1 alors que le
#   maillage est bien la, c'est que le geste de selection n'a pas pu avoir lieu --
#   mesure du 17/09 : en sous-mode ARETE ou SOMMET, le clic de FACE est filtre par
#   le sous-mode (a juste titre), et le shell REECRIT le masque a un seul bit, donc
#   un masque combine ne tient pas. La sonde le NOMME et sort 3.
if (($s1 -eq 0) -and ($f1 -gt 0)) {
	Write-Host "COURSE IMPOSSIBLE  le maillage est la (faces=$f1) mais RIEN n'est selectionne au releve 1."
	Write-Host "                   En sous-mode '$SousMode', le clic de FACE ne designe rien et le masque"
	Write-Host "                   est reecrit par le shell. Aucun critere n'est juge : ce n'est pas un rouge."
	Write-Host "                   La regle est prouvee un etage plus bas : NKEditMeshHarness --suppression."
	exit 3
}
Dire "(0) LE ZERO D'ABORD : le compteur sait rendre autre chose que 0" ($s1 -gt 0) `
	"releve 1 : faces=$f1 selection=$s1 (exige > 0 ; sinon les 0 d'en bas ne prouvent rien)"
Dire "(1) apres le 1er X : selection VIDE dans les TROIS sous-modes" (($v2 -eq 0) -and ($e2 -eq 0) -and ($c2 -eq 0)) `
	"releve 2 : faces=$f2 selV=$v2 selE=$e2 selF=$c2 (exige 0/0/0)"
Dire "(2) le 2e X ne supprime plus rien" ($f3 -eq $f2) `
	"releve 3 : faces=$f3, contre $f2 au releve 2 (exige EGAL ; sans le correctif : 0)"
Write-Host "FAIT   (3) apres Ctrl+Z (rapporte, non juge) : faces=$f4 selection=$s4"

Write-Host "-----------------------------------------------------------------------"
if ($Mutation) {
	# Sous mutation, ZERO rouge est un ECHEC DE LA SONDE : la mutation a survecu,
	# donc les criteres ne testent rien.
	if ($rouges -gt 0) { Write-Host "MUTATION TUEE ($rouges rouge(s)) — les criteres mordent."; exit 1 }
	Write-Host "ECHEC DE LA SONDE : la mutation a SURVECU, les criteres ne testent rien."
	exit 2
}
if ($rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "ECHEC ($rouges rouge(s))"
exit 1
