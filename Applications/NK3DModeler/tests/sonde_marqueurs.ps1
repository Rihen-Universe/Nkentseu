# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_marqueurs.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — LE DESSIN DES SOMMETS : trace-t-on, ou peint-on ?
#
# ⚠ AUCUNE INJECTION D'ENTREE : ni souris, ni clavier. Tout passe par des
#   crochets d'API dans le binaire (NK_*), la fenetre porte
#   « *** SONDE DE MESURE *** » et elle se ferme seule (NK_AGENT_EXIT).
#   Les images mesurees sont celles de CETTE fenetre, prises par l'application
#   elle-meme (NK_AGENT_SHOT) : jamais l'ecran de Rodolf.
#
# CE QU'ELLE TRANCHE, ET POURQUOI CES CRITERES-LA
# -----------------------------------------------
# Rodolf : « le mode sommet du modeleur ne donne pas la meme forme que dans
# renderdemo ; celui de renderdemo est VALIDE et ressemble a Blender ».
# Les deux sites sont LE MEME CODE PORTE (mesure du 17/09) : memes quads
# face-camera, memes demi-cotes (1,5 / 1,8 / 2,0 px + 0,7 de lisere), memes
# couleurs, meme DrawDebugTriangle. La question n'est donc pas « partager ou
# recopier » -- c'est deja partage -- mais : LE MEME CODE, POURQUOI NE REND-IL
# PAS LA MEME CHOSE ?
#
# Deux causes opposees, un seul symptome :
#   (i)  on ne TRACE pas   -> un filtre ecarte les marqueurs avant l'appel ;
#   (ii) on trace et ca ne PEINT pas -> l'appel part, et l'etat de la passe
#        l'efface.
# Rien dans une image ne les separe. Les compteurs, si -- et l'image, ensuite,
# dit laquelle des deux a gagne.
#
# LE TEMOIN PARTAGE TOUT SAUF LA VARIABLE ETUDIEE. Le gizmo de l'objet part par
# le MEME appel (DrawDebugTriangle), vers le MEME tampon, dans la MEME frame ;
# sa seule difference est overlay = true, la ou les marqueurs passent
# overlay = editXray. Rodolf VOIT ce gizmo -- il a meme pris ses poignees de
# plan pour des sommets. Un temoin pareil elimine d'un coup la scene, la
# camera, le device et la passe de rendu.
#
# ⚠ LE CRITERE (c) EXISTE PARCE QU'UN NEGATIF A DEJA ETE CRU SANS LUI. Le
#   17/09, le rayon X a ete bascule par la commande du shell, aucun marqueur
#   n'est apparu, et on en a conclu que le filtre d'orientation etait hors de
#   cause -- SANS AVOIR VERIFIE QUE LE RAYON X S'ETAIT ALLUME. Un negatif dont
#   la condition n'est pas prouvee ne refute rien. Ici l'etat est POSE puis
#   IMPRIME par le binaire, et relu dans la boucle de dessin.
#
# ⚠ ET ON QUITTE L'ACCUEIL (NK_AGENT_SCENE). Sans cela on mesure un etat que
#   l'utilisateur ne rencontre jamais : la vue 3D n'est pas composee, et une
#   capture rend l'ecran d'accueil. Ce chantier a deja paye cette erreur une
#   fois -- onze criteres mesures au lanceur.
#
# LE ZERO : en mode OBJET, aucun marqueur ne doit etre trace. Et il se
# distingue de « la sonde n'a rien mesure » : on exige que la frame ait bien
# rendu (lignes DebugTri presentes) ET qu'aucune ligne MARQ n'existe. Un
# compteur dont le zero n'est pas un zero a deja coute une soiree ici.
#
# USAGE :  pwsh -File Applications/NK3DModeler/tests/sonde_marqueurs.ps1
# CODES :  0 tout vert · 1 au moins un rouge · 2 la mesure n'a pas pu se faire
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release",
	[string]$Sortie = ""
)

$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 2 }
Write-Host ("binaire : {0}" -f (Get-Item $exe).LastWriteTime.ToString("dd/MM HH:mm:ss"))
if (-not $Sortie) { $Sortie = [System.IO.Path]::GetTempPath() }
New-Item -ItemType Directory -Force -Path $Sortie | Out-Null

$script:rouges = 0
function Dire([string]$nom, [bool]$ok, [string]$detail) {
	if ($ok) { Write-Host ("VERT   {0}  {1}" -f $nom, $detail) }
	else { Write-Host ("ROUGE  {0}  {1}" -f $nom, $detail); $script:rouges++ }
}

# Une course = une scene neuve, un cube a l'utilisateur, un mode, un rapport.
# NK_EDIT_USER=99 vise l'objet de L'UTILISATEUR : viser un objet de demo
# marque supprime avait deja coute une soiree a ce chantier.
function Courir([string]$nom, [bool]$edition, [string]$masque, [string]$xray, [string]$png) {
	$out = Join-Path $Sortie "nk_marq_$nom.txt"
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_MARQ_PROBE = "1"; $env:NK_DEBUGTRI_PROBE = "1"
	$env:NK_AGENT_SCENE = "30"; $env:NK_AGENT_EXIT = "160"
	if ($edition) { $env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SEL = "a"; $env:NK_EDIT_SELMASK = $masque }
	if ($xray) { $env:NK_EDIT_XRAY = $xray }
	if ($png) { $env:NK_AGENT_SHOT = "140"; $env:NK_TOAST_PROBE = $png }
	# ⚠ LA GARDE QUI MANQUAIT, ET ELLE A DEJA MENTI UNE FOIS CE SOIR. Le
	#   repertoire de sortie n'existait pas, Start-Process a echoue, AUCUNE
	#   application n'a demarre -- et la sonde a rendu VERT sur trois criteres,
	#   parce qu'elle lisait le logs/app.log de la course PRECEDENTE. Un banc
	#   qui n'a pas pu mesurer doit le DIRE, jamais heriter du passe.
	$journal = Join-Path $Arbre "logs\app.log"
	$avant = Get-Date
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $out -ErrorAction Stop | Out-Null
	if (-not (Test-Path $journal)) { Write-Host "ROUGE  aucun journal apres la course $nom"; exit 2 }
	if ((Get-Item $journal).LastWriteTime -lt $avant) {
		Write-Host "ROUGE  le journal de $nom est ANTERIEUR au lancement : la course n a pas tourne"
		exit 2
	}
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_MARQ_PROBE", "NK_DEBUGTRI_PROBE",
			"NK_AGENT_SCENE", "NK_AGENT_EXIT", "NK_EDIT_MODE", "NK_EDIT_SEL", "NK_EDIT_SELMASK",
			"NK_EDIT_XRAY", "NK_AGENT_SHOT", "NK_TOAST_PROBE")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$txt = if (Test-Path $out) { Get-Content $out -Raw } else { "" }
	# ⚠ LE VERDICT N'EST PAS TOUJOURS LA OU L'ON REGARDE : la console ne porte
	#   que quelques lignes, le journal porte le reste. On lit les deux plutot
	#   que d'annoncer un silence qu'on n'a pas cherche.
	$txt = $txt + (Get-Content $journal -Raw)
	return $txt
}

function DernierNombre([string]$txt, [string]$motif) {
	$m = [regex]::Matches($txt, $motif)
	if ($m.Count -eq 0) { return -1 }
	return [int]$m[$m.Count - 1].Groups[1].Value
}

# ---- 1. MODE EDITION, SOUS-MODE SOMMET, RAYON X ETEINT --------------------
$png1 = Join-Path $Sortie "marq_eteint.png"
$t1 = Courir "sommet" $true "1" $null $png1
$marq1 = DernierNombre $t1 "triangles\(marqueurs\)=(\d+)"
$pts1 = DernierNombre $t1 "sommets\(traces\)=(\d+)"
$rej1 = DernierNombre $t1 "par l'orientation\)=(\d+)"
$prof1 = DernierNombre $t1 "dont depth-teste=(\d+)"
$over1 = DernierNombre $t1 "dont depth-teste=\d+ overlay=(\d+)"
$pipeP = DernierNombre $t1 "pipeline\(depth\)=(\d+)"
$pipeO = DernierNombre $t1 "pipeline\(overlay\)=(\d+)"
if ($marq1 -lt 0) { Write-Host "ROUGE  la sonde n'a rien mesure (aucune ligne MARQ)"; exit 2 }

Dire "(a) les marqueurs sont TRACES en sous-mode sommet" ($marq1 -gt 0) ("triangles marqueurs={0} sommets retenus={1} rejetes par l'orientation={2}" -f $marq1, $pts1, $rej1)
Dire "(b) TEMOIN : des triangles OVERLAY partent dans la MEME image" ($over1 -gt 0) ("overlay={0} (le gizmo de l'objet, que Rodolf voit)" -f $over1)
Dire "(d) rayon X eteint : les marqueurs partent DEPTH-TESTE, pipelines valides" (($prof1 -gt 0) -and ($pipeP -eq 1) -and ($pipeO -eq 1)) ("depth-teste={0} overlay={1} pipeline depth={2} overlay={3}" -f $prof1, $over1, $pipeP, $pipeO)

# ---- 2. LE MEME, RAYON X ALLUME PAR LE CROCHET ----------------------------
$png2 = Join-Path $Sortie "marq_allume.png"
$t2 = Courir "xray" $true "1" "1" $png2
$etat2 = DernierNombre $t2 "crochet NK_EDIT_XRAY : editXray = (\d+)"
$xr2 = DernierNombre $t2 "xray=(\d+)"
$marq2 = DernierNombre $t2 "triangles\(marqueurs\)=(\d+)"
$rej2 = DernierNombre $t2 "par l'orientation\)=(\d+)"
$prof2 = DernierNombre $t2 "dont depth-teste=(\d+)"
$over2 = DernierNombre $t2 "dont depth-teste=\d+ overlay=(\d+)"

Dire "(c) le rayon X est REELLEMENT allume, et le binaire le DIT" (($etat2 -eq 1) -and ($xr2 -eq 1)) ("crochet={0} etat relu dans la boucle de dessin={1}" -f $etat2, $xr2)
Dire "(e) rayon X allume : plus AUCUN rejet par l'orientation" ($rej2 -eq 0) ("rejetes={0} (eteint : {1})" -f $rej2, $rej1)
Dire "(f) rayon X allume : les marqueurs basculent en OVERLAY" (($over2 -gt $over1) -and ($prof2 -lt $prof1)) ("depth-teste {0}->{1}  overlay {2}->{3}  marqueurs traces={4}" -f $prof1, $prof2, $over1, $over2, $marq2)

# ---- 3. LE ZERO : MODE OBJET ---------------------------------------------
$t3 = Courir "objet" $false "" $null ""
$marq3 = DernierNombre $t3 "triangles\(marqueurs\)=(\d+)"
$dbg3 = ([regex]::Matches($t3, "DebugTri")).Count
# ⚠ « pas de ligne MARQ » ne vaut zero QUE si la frame a rendu. Sans la
#   seconde moitie, une course morte rendrait ce critere vert.
Dire "(g) LE ZERO : en mode objet, aucun marqueur trace (et la frame a bien rendu)" (($marq3 -lt 0) -and ($dbg3 -gt 0)) ("lignes MARQ=0 lignes DebugTri={0}" -f $dbg3)

# ---- 4. ET L'IMAGE TRANCHE ------------------------------------------------
# Les compteurs disent qu'on TRACE. Seule l'image dit si ca PEINT.
#
# ⚠ LE SEUIL DE COULEUR N'EST PAS CHOISI, IL EST MESURE. Les sommets sont tous
#   selectionnes (NK_EDIT_SEL=a), donc leur coeur est peint en (255,140,13).
#   Le dorsal le rend a l'ecran en (214,193,58) -- il encode, et ce n'est pas
#   un defaut ici. Le remplissage de face, lui, sort a (218,194,113) : proche
#   en rouge et en vert, MAIS SON BLEU EST DEUX FOIS PLUS HAUT, parce qu'il est
#   translucide et laisse passer le gris de la surface. C'est donc le BLEU, et
#   lui seul, qui separe un marqueur d'un remplissage. Un seuil pris sur le
#   rouge aurait compte les deux et rendu un vert qui ne mesure rien.
function Coeurs([string]$png) {
	if (-not (Test-Path $png)) { return -1 }
	Add-Type -AssemblyName System.Drawing
	$bmp = [System.Drawing.Bitmap]::FromFile($png)
	$n = 0
	# Zone du cube seulement : le reste de la fenetre porte l'interface.
	for ($y = 345; $y -lt [Math]::Min(495, $bmp.Height); $y += 1) {
		for ($x = 685; $x -lt [Math]::Min(825, $bmp.Width); $x += 1) {
			$c = $bmp.GetPixel($x, $y)
			if (($c.R -gt 180) -and ($c.G -gt 150) -and ($c.G -lt 215) -and ($c.B -lt 90) -and
				(($c.G - $c.B) -gt 90)) { $n++ }
		}
	}
	$bmp.Dispose()
	return $n
}
$o1 = Coeurs $png1
$o2 = Coeurs $png2
Dire "(h) L'IMAGE : rayon X allume, les huit marqueurs se VOIENT" ($o2 -gt 60) ("coeurs de marqueur = {0} px (8 coins)" -f $o2)
# ⚠ CE VERDICT EST ECRIT POUR BASCULER. Il est VERT tant que le defaut est la.
#   Rayon X eteint, le marqueur n'est pas absent : il est ROGNE par la surface
#   du cube -- la moitie du quad face-camera qui plonge dans le volume perd le
#   test de profondeur, et seule la moitie qui depasse la silhouette survit.
#   C'est CA que Rodolf voit comme « pas la meme forme ».
#   CONDITION DE RETRAIT : le jour ou les marqueurs se verront entiers rayon X
#   eteint, ce critere doit devenir « (i) les marqueurs ne sont plus rognes »
#   avec l'attendu superieur a 60. Le laisser vert des deux cotes ne mesurerait
#   plus rien.
Dire "(i) [DEFAUT] rayon X eteint : les marqueurs sont ROGNES par la surface" (($o1 -gt 5) -and ($o1 -lt ($o2 / 2))) ("coeurs restants = {0} px contre {1} allume -- moins de la moitie" -f $o1, $o2)

Write-Host ("-- {0} rouge(s) --" -f $script:rouges)
if ($script:rouges -gt 0) { exit 1 } else { exit 0 }
