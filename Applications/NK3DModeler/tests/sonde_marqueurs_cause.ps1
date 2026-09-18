# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_marqueurs_cause.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — LE ROGNAGE DES MARQUEURS : est-ce le MODELEUR, ou le MARQUEUR ?
#
# ⚠ AUCUNE INJECTION D'ENTREE : ni souris, ni clavier. Crochets d'API seulement.
#   La fenetre du modeleur porte *** SONDE DE MESURE *** et se ferme seule ;
#   l'etalon tourne en capture headless. Jamais l'ecran de Rodolf.
#
# CE QUI EST DEJA ETABLI (sonde_marqueurs, 9 criteres verts)
# ----------------------------------------------------------
# Rayon X eteint, le marqueur de sommet du modeleur est COUPE NET par la surface
# du cube. Il n'est pas absent : le quad face-camera plonge pour moitie dans le
# volume, et cette moitie perd le test de profondeur malgre le biais de -1,5.
#
# CE QUE CETTE SONDE TRANCHE
# --------------------------
# Le code du dessin est IDENTIQUE dans renderdemo, que Rodolf a VALIDE. Donc :
#   - si l'etalon rogne PAREIL, ce n'est pas un defaut du modeleur mais une
#     PROPRIETE DU MARQUEUR -- et c'est alors l'etalon qu'il faudrait changer,
#     donc une DECISION DE RODOLF, pas un correctif discret ;
#   - si l'etalon ne rogne PAS, le modeleur est seul en cause, et la partie A
#     dit si l'echelle de la camera l'explique.
#
# ⚠ LE SUSPECT « FORMAT DE PROFONDEUR » EST MORT PAR LECTURE : la cible hors
#   ecran du modeleur (NkDemo3D.cpp:13699) et la chaine d'echange de l'etalon
#   (NkISwapchain.h:31) sont toutes deux en NK_D32_FLOAT. Un suspect elimine par
#   lecture n'a coute ni fenetre ni watt.
#
# ⚠ ET LE LEVIER DE LA PARTIE A EST SUSPECT D'ETRE INERTE. Le code le dit
#   lui-meme (NkDemo3D.cpp:7214) : sous NK_FIX_CAM, NK_CAM_DIST=3 et =6 donnent
#   des captures IDENTIQUES AU BIT PRES, parce que la pose figee est appliquee
#   plus tard et ecrase le rayon. « UN INSTRUMENT INERTE CONFIRME LE DEFAUT
#   QU'ON LUI SOUMET, QUEL QU'IL SOIT. » Le critere (A0) verifie donc que le
#   levier AGIT avant qu'aucun des suivants ne soit interprete.
#
# USAGE :  pwsh -File sonde_marqueurs_cause.ps1 [-Sortie <dossier>]
# CODES :  0 mesure faite · 1 une prediction dementie · 2 la mesure n'a pas pu
#          se faire (levier inerte, course morte, compte hors d'ordre de grandeur)
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release",
	[string]$Sortie = ""
)

$exeMod = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
$exeEta = Join-Path $Arbre "Build\Bin\$Config-Windows\renderdemo\renderdemo.exe"
foreach ($e in @($exeMod, $exeEta)) {
	if (-not (Test-Path $e)) { Write-Host "ROUGE  binaire introuvable : $e"; exit 2 }
}
Write-Host ("modeleur : {0}" -f (Get-Item $exeMod).LastWriteTime.ToString("dd/MM HH:mm:ss"))
Write-Host ("etalon   : {0}" -f (Get-Item $exeEta).LastWriteTime.ToString("dd/MM HH:mm:ss"))
if (-not $Sortie) { $Sortie = [System.IO.Path]::GetTempPath() }
New-Item -ItemType Directory -Force -Path $Sortie | Out-Null

$script:rouges = 0
function Dire([string]$nom, [bool]$ok, [string]$detail) {
	if ($ok) { Write-Host ("VERT   {0}  {1}" -f $nom, $detail) }
	else { Write-Host ("ROUGE  {0}  {1}" -f $nom, $detail); $script:rouges++ }
}

Add-Type -AssemblyName System.Drawing

# ── LE COMPTEUR D'IMAGE ────────────────────────────────────────────────────
# ⚠️ IL A ETE REECRIT APRES AVOIR PRODUIT UNE CONCLUSION SUR DES NOMBRES FAUX.
#    La premiere version selectionnait le coeur du marqueur PAR SA COULEUR, en
#    s'appuyant sur le BLEU : coeur (213,193,57) contre remplissage de face
#    (218,196,121) dans le modeleur -- une separation nette. Mesure faite dans
#    l'ETALON : le meme remplissage y sort a (207,175,49) et le coeur a
#    (225,199,53). LE BLEU NE LES SEPARE PLUS. Le modeleur tourne en DirectX 11,
#    l'etalon en OpenGL, et ce depot a deja ecrit que les dorsaux n'encodent pas
#    pareil : un remplissage TRANSLUCIDE ne donne pas les memes pixels.
#    Resultat : le filtre a compte TOUT LE CUBE de l'etalon -- 3 293 px la ou
#    huit marqueurs de 5 px de cote en valent au plus 200 -- et la sonde a
#    imprime une conclusion. Elle se trouvait juste, ce qui est PIRE : un
#    instrument faux qui tombe juste ne se fait pas corriger.
#
# ON NE DEMANDE DONC PLUS A LA COULEUR DE SEPARER, ON DEMANDE A LA FORME : le
# coeur du marqueur est CERNE d'un lisere quasi noir (0,0,0,0.9) ; le
# remplissage de face n'en a pas. Un pixel clair ne compte que s'il a du
# presque-noir a deux ou trois pixels de la. Cela reste vrai sur les deux
# dorsaux, parce que ca ne depend d'aucun encodage.
function Coeurs([string]$png, [int[]]$boite) {
	$r = [pscustomobject]@{ n = -1; largeur = -1 }
	if (-not (Test-Path $png)) { return $r }
	$bmp = [System.Drawing.Bitmap]::FromFile($png)
	$rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
	$data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
		[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
	$oct = New-Object byte[] ($data.Stride * $bmp.Height)
	[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $oct, 0, $oct.Length)
	$bmp.UnlockBits($data)
	# LockBits plutot que GetPixel : des centaines de milliers d'appels COM par
	# image mettraient la sonde a genoux, et une sonde lente cesse d'etre lancee.
	$st = $data.Stride
	$dx = @(-3, 3, 0, 0, -2, 2, -2, 2); $dy = @(0, 0, -3, 3, -2, 2, 2, -2)
	$n = 0; $mnx = $bmp.Width; $mxx = 0
	$yA = [Math]::Max(3, $boite[1]); $yB = [Math]::Min($boite[3], $bmp.Height - 4)
	$xA = [Math]::Max(3, $boite[0]); $xB = [Math]::Min($boite[2], $bmp.Width - 4)
	for ($y = $yA; $y -lt $yB; $y++) {
		for ($x = $xA; $x -lt $xB; $x++) {
			$i = $y * $st + $x * 4
			$b = $oct[$i]; $g = $oct[$i + 1]; $rr = $oct[$i + 2]
			# ⚠️ « CHAUD », PAS « VERT ELEVE ». Ce test exigeait g > 140 : il etait
			#    cale sur l'ANCIENNE couleur du marqueur, dont le vert sortait a 202.
			#    Le jour ou la couleur s'est alignee sur Blender -- vert 121 --, la
			#    sonde a rendu -1 px partout et accuse le levier de camera d'etre
			#    INERTE. Un attendu en dur se perime, et celui-la a accuse le CODE
			#    d'un changement qui etait le sien.
			#    Le critere est maintenant la CHALEUR de la teinte -- rouge tres
			#    superieur au bleu -- qui ne depend pas de la valeur exacte du vert.
			#    Il retient l'ancienne couleur (231,202,0) comme la nouvelle
			#    (227,121,1), et rejette le gris de surface (140,142,143), dont le
			#    rouge et le bleu sont egaux.
			#    ⚠️ CE QUI SERAIT MIEUX, et n'est pas fait : que la sonde se
			#       CALIBRE sur l'image -- prendre la couleur AUX ADRESSES publiees
			#       par [MARQPOS] dans la course rayon X allume, et s'en servir pour
			#       juger la course eteinte. Elle serait alors immunisee contre tout
			#       changement de teinte a venir.
			if (-not (($rr -gt 170) -and (($rr - $b) -gt 80) -and ($g -gt 60))) { continue }
			$v = 0
			for ($k = 0; $k -lt 8; $k++) {
				$j = ($y + $dy[$k]) * $st + ($x + $dx[$k]) * 4
				if (($oct[$j + 2] -lt 70) -and ($oct[$j + 1] -lt 70) -and ($oct[$j] -lt 70)) { $v++ }
			}
			if ($v -ge 2) {
				$n++
				if ($x -lt $mnx) { $mnx = $x }; if ($x -gt $mxx) { $mxx = $x }
			}
		}
	}
	$bmp.Dispose()
	$r.n = $n
	$r.largeur = if ($n -gt 0) { $mxx - $mnx } else { -1 }
	return $r
}

# ⚠️ L'ORDRE DE GRANDEUR ATTENDU, DERIVE ET NON OBSERVE. Huit marqueurs dont le
#    coeur fait au plus 5 px de cote valent au plus ~200 px, et au moins ~20 si
#    l'on n'en voit que deux. Hors de cette fourchette, le compte ne porte PAS
#    sur des marqueurs, et la sonde doit REFUSER DE CONCLURE plutot que de
#    diviser deux nombres qui parlent d'autre chose. C'est exactement le
#    garde-fou qui manquait quand 3 293 px sont passes pour huit marqueurs.
function Plausible([int]$n) { return ($n -ge 20) -and ($n -le 400) }

# ⚠️ LES BOITES SONT LUES SUR LES CAPTURES, ET JE L'ECRIS PLUTOT QUE DE LES
#    DEGUISER EN FRACTIONS « portables ». Une fraction de la fenetre aurait
#    englobe, dans l'etalon, la rangee de cubes JAUNES de la scene de demo --
#    c'est-a-dire exactement la contamination qu'on vient de corriger. Une boite
#    posee a la main et DECLAREE vaut mieux qu'une boite automatique qui ramasse
#    autre chose en silence. Si la camera de l'etalon change, le garde-fou
#    d'ordre de grandeur le dira : le compte sortira de la fourchette.
$boiteMod = @(640, 320, 880, 540)   # modeleur 1600x900, cube centre en (750,420)
$boiteEta = @(570, 290, 715, 430)   # etalon 1280x720, cube edite (obj #16)

# ── UNE COURSE DU MODELEUR ─────────────────────────────────────────────────
function CourirMod([string]$nom, [string]$xray, [string]$dist, [string]$dorsal) {
	$png = Join-Path $Sortie "cause_mod_$nom.png"
	$out = Join-Path $Sortie "cause_mod_$nom.txt"
	if (Test-Path $png) { Remove-Item $png -Force }
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_MARQ_PROBE = "1"; $env:NK_AGENT_SCENE = "30"; $env:NK_AGENT_EXIT = "160"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SEL = "a"; $env:NK_EDIT_SELMASK = "1"
	$env:NK_AGENT_SHOT = "140"; $env:NK_TOAST_PROBE = $png
	if ($xray) { $env:NK_EDIT_XRAY = $xray }
	if ($dist) { $env:NK_CAM_DIST = $dist }
	# ⚠️ LE DORSAL EST UN FACTEUR CONFONDANT, ET ON LE MESURE AU LIEU DE LE
	#    SUBIR. Releve dans les journaux : le modeleur a tourne en DirectX 11 et
	#    l'etalon en OpenGL, tous deux en « auto ». Comparer deux applications sur
	#    deux dorsaux, c'est deja ce qui a fait mentir le filtre de couleur. Le
	#    temoin de la partie C refait la mesure du modeleur en OpenGL : si la
	#    survie ne bouge pas, le dorsal est hors de cause et la comparaison entre
	#    applications tient. Sinon elle ne tient pas, et il faut le dire.
	if ($dorsal) { $env:NK_GFX_BACKEND = $dorsal }
	# ⚠️ LA CAMERA EST FIGEE, ET C'EST LE CORRECTIF LE PLUS IMPORTANT DE CE
	#    FICHIER. Deux courses identiques -- meme binaire, memes variables, trois
	#    minutes d'ecart -- ont rendu 10 px puis 181 px. En comparant les deux
	#    captures : 13 116 pixels differents sur 13 200 echantillonnes. Ce
	#    n'etait pas du bruit de mesure, C'ETAIT UNE AUTRE SCENE : cube a une
	#    autre place, autre orientation, autre ombre. La sonde mesurait une CIBLE
	#    MOUVANTE, et toutes ses boites relevees a la main portaient a cote.
	#    `NK_FIX_CAM` fige la pose ET le temps. Sans lui, aucun chiffre de cette
	#    sonde n'est reproductible -- et un chiffre non reproductible ne prouve
	#    rien, meme quand il tombe sur la bonne conclusion.
	#    ⚠️ LE PRIX EST DIT : sous NK_FIX_CAM, `NK_CAM_DIST` est INERTE (le code
	#       le documente, NkDemo3D.cpp:7214). La partie A perd donc son levier.
	#       C'est un echange assume : la question de l'echelle de camera a deja
	#       ete tranchee (R66/R67, prediction dementie), tandis que la
	#       reproductibilite manquait a TOUT le reste.
	$env:NK_FIX_CAM = "1"
	$journal = Join-Path $Arbre "logs\app.log"
	$avant = Get-Date
	Start-Process -FilePath $exeMod -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $out -ErrorAction Stop | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_MARQ_PROBE", "NK_AGENT_SCENE",
			"NK_AGENT_EXIT", "NK_EDIT_MODE", "NK_EDIT_SEL", "NK_EDIT_SELMASK", "NK_AGENT_SHOT",
			"NK_TOAST_PROBE", "NK_EDIT_XRAY", "NK_CAM_DIST", "NK_GFX_BACKEND", "NK_FIX_CAM")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	# La course a-t-elle seulement eu lieu ? Un journal anterieur au lancement
	# serait celui de la course precedente -- des verts empruntes.
	if (-not (Test-Path $journal) -or ((Get-Item $journal).LastWriteTime -lt $avant)) {
		Write-Host "ROUGE  la course modeleur '$nom' n'a pas tourne (journal absent ou anterieur)"
		exit 2
	}
	if (-not (Test-Path $png)) { Write-Host "ROUGE  aucune image pour la course '$nom'"; exit 2 }
	return (Coeurs $png $boiteMod)
}

# ── UNE COURSE DE L'ETALON ─────────────────────────────────────────────────
# ⚠ LA DEMO SE DESIGNE PAR SON NOM. `--demo=3D` est resolu dans la table
#   (main.cpp:506) ; `--demo=2` est un INDICE, et un indice se decale a chaque
#   fusion -- ce depot a deja pose deux bancs differents sur le meme numero.
function CourirEta([string]$nom, [string]$xray) {
	$png = Join-Path $Sortie "cause_eta_$nom.png"
	$out = Join-Path $Sortie "cause_eta_$nom.txt"
	if (Test-Path $png) { Remove-Item $png -Force }
	# ⚠️ L'ETALON EST FIGE LUI AUSSI, ET C'EST LE PREALABLE DE TOUT LE RESTE.
	#    Sans cela, deux de ses courses rendaient 44 px puis 70 px. La cause est
	#    lisible dans son code : `st->angle += dt * 0.45f` -- l'angle d'orbite
	#    avance avec le TEMPS REEL, donc avec la charge de la machine. A la
	#    frame 120, deux lancements ne regardent pas le meme cube.
	#    `NK_FIX_CAM` pose `st->angle = 0.6f` et, du meme geste, arrete
	#    l'animation du spot (`&& !fixcam`, Demo3D.cpp:7066). `NK_LIGHT_ANIM=0`
	#    est pose EN PLUS, explicitement : une garde implicite se fait retirer
	#    par qui ne la voit pas.
	#    ⚠️ ET ON NE LE CROIT PAS SUR PAROLE : le critere (F) rejoue la meme
	#       course et exige les MEMES nombres.
	$env:NK_FIX_CAM = "1"; $env:NK_LIGHT_ANIM = "0"
	$env:NK_MARQ_PROBE = "1"; $env:NK_EDIT_MODE = "1"; $env:NK_EDIT_SEL = "all"
	$env:NK_EDIT_SELMASK = "1"; $env:NK_MAXFRAMES = "140"
	$env:NK_CAPTURE = "120"; $env:NK_CAPTURE_PATH = $png
	if ($xray) { $env:NK_EDIT_XRAY = $xray }
	Start-Process -FilePath $exeEta -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-ArgumentList "--demo=3D" -RedirectStandardOutput $out -ErrorAction Stop | Out-Null
	foreach ($v in @("NK_MARQ_PROBE", "NK_EDIT_MODE", "NK_EDIT_SEL", "NK_EDIT_SELMASK",
			"NK_MAXFRAMES", "NK_CAPTURE", "NK_CAPTURE_PATH", "NK_EDIT_XRAY",
			"NK_FIX_CAM", "NK_LIGHT_ANIM")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	if (-not (Test-Path $png)) { Write-Host "ROUGE  aucune image de l'etalon pour '$nom'"; exit 2 }
	return (Coeurs $png $boiteEta)
}

function Rapport([double]$eteint, [double]$allume) {
	if ($allume -le 0) { return -1.0 }
	return [Math]::Round(100.0 * $eteint / $allume, 1)
}

Write-Host ""
Write-Host "PARTIE A — RETIREE, et voici pourquoi"
Write-Host "-----------------------------------------------------------------------"
# ⚠️ ELLE MESURAIT UNE QUESTION DEJA TRANCHEE, AVEC UN LEVIER DEVENU INERTE.
#    (A1) demandait si le rognage s'attenue quand la camera s'approche. La
#    reponse est NON, etablie deux fois : 22,6 % a d=4 contre 18 % a d=7, un
#    ecart trop faible ; puis le decalage vers la camera, qui a corrige le
#    rognage sans toucher a la distance.
#    Et depuis que (F) exige `NK_FIX_CAM` -- sans quoi RIEN n'est reproductible
#    -- le levier `NK_CAM_DIST` est INERTE : le code le documente lui-meme
#    (NkDemo3D.cpp:7214), la pose figee etant appliquee plus tard et ecrasant le
#    rayon. Les deux exigences sont donc INCOMPATIBLES, et c'est la
#    reproductibilite qui gagne.
# ⚠️ ON LA RETIRE AU LIEU DE LA LAISSER ROUGIR. Un critere qui ne peut plus
#    etre mesure dans les conditions qu'on exige n'accuse plus le code : il
#    accuse le montage, et il finirait par etre ignore -- ou pire, par faire
#    ignorer les autres, puisqu'il arretait la sonde avant les parties B, C et F.
# CONDITION DE REOUVERTURE : le jour ou l'on saura figer la pose SANS rendre
#    `NK_CAM_DIST` inerte, (A1) redevient mesurable telle qu'elle est ecrite.
Write-Host "       (A) retiree : le levier NK_CAM_DIST est inerte sous NK_FIX_CAM, que (F)"
Write-Host "       exige. La question -- l'echelle de camera explique-t-elle le rognage ? --"
Write-Host "       a ete tranchee NON, deux fois. Condition de reouverture ecrite dans le banc."
$rProche = -1.0; $rLoin = -1.0; $rDX = -1.0

Write-Host "PARTIE C — LE TEMOIN DE DORSAL : le modeleur, en OpenGL comme l'etalon"
Write-Host "-----------------------------------------------------------------------"
# ⚠️ LE TEMOIN COURT A LA DISTANCE PAR DEFAUT, ET PAS A d=4. Premiere
#    version : il reutilisait le d=4 de la partie A et rendait VERT sur 8 px
#    contre 1 px -- c'est-a-dire sur des comptes que la sonde venait, deux
#    lignes plus haut, de declarer HORS D'ORDRE DE GRANDEUR. Un garde-fou qui
#    protege un critere et pas son voisin ne protege rien : le chiffre faux
#    ressort par la porte d'a cote. A la distance par defaut, le cadrage est
#    celui pour lequel la boite a ete relevee, et les comptes redeviennent
#    plausibles.
$gl_on = CourirMod "gl_on" "1" $null "opengl"
$gl_off = CourirMod "gl_off" $null $null "opengl"
$dx_on = CourirMod "dx_on" "1" $null $null
$dx_off = CourirMod "dx_off" $null $null $null
$rGL = Rapport $gl_off.n $gl_on.n
$rDX = Rapport $dx_off.n $dx_on.n
Write-Host ("       modeleur OpenGL     : eteint {0} px / allume {1} px  ->  {2} %" -f $gl_off.n, $gl_on.n, $rGL)
Write-Host ("       modeleur DirectX 11 : eteint {0} px / allume {1} px  ->  {2} %" -f $dx_off.n, $dx_on.n, $rDX)
# Le meme garde-fou qu'en (A1), et pour la meme raison.
if ((Plausible $gl_on.n) -and (Plausible $dx_on.n)) {
	Dire "(C) le DORSAL ne change pas la survie des marqueurs" (($rGL -gt 0) -and ($rDX -gt 0) -and ([Math]::Abs($rGL - $rDX) -lt 15)) `
		("{0} % en OpenGL contre {1} % en DirectX 11, meme cadrage (exige un ecart faible ; sinon comparer deux applications sur deux dorsaux ne veut rien dire)" -f $rGL, $rDX)
}
else {
	Write-Host ("       (C) NON MESURABLE : {0} px en OpenGL et {1} px en DirectX 11, hors de" -f $gl_on.n, $dx_on.n)
	Write-Host "       l'ordre de grandeur de huit marqueurs. Ce n'est PAS un verdict sur le code."
}

Write-Host ""
Write-Host "PARTIE B — L'ETALON, MEME MONTAGE, MEME FILTRE DE FORME"
Write-Host "-----------------------------------------------------------------------"
$eta_on = CourirEta "on" "1"
$eta_off = CourirEta "off" $null
if (-not (Plausible $eta_on.n)) {
	Write-Host ("ARRET : etalon, compte hors d'ordre de grandeur ({0} px)." -f $eta_on.n)
	Write-Host "La sonde REFUSE de conclure : le filtre ne compte pas des marqueurs."
	exit 2
}
# ⚠️ (F) L'ETALON EST-IL VRAIMENT FIGE ? On rejoue la MEME course et on exige
#    les memes nombres. Sans ce critere, « j'ai pose NK_FIX_CAM » serait une
#    intention, pas une mesure -- et ce chantier a deja paye un levier inerte.
$eta_on2 = CourirEta "on2" "1"
# ⚠️ ET SURTOUT LE CAS ETEINT, car c'est LUI qui dansait : 44 px puis 70 px.
#    Le cas allume ne prouve que la scene ; le cas eteint met en jeu le TEST DE
#    PROFONDEUR sur des marqueurs poses exactement sur la surface. Si, scene
#    figee, il continue de varier, alors l'instabilite n'est pas un defaut de
#    la sonde : c'est le z-fighting lui-meme, et c'est un RESULTAT.
$eta_off2 = CourirEta "off2" $null
$rEta = Rapport $eta_off.n $eta_on.n
Write-Host ("       etalon : eteint {0} px / allume {1} px  ->  {2} %" -f $eta_off.n, $eta_on.n, $rEta)
Dire "(F) L'ETALON EST FIGE : deux courses identiques rendent les MEMES nombres" (($eta_on2.n -eq $eta_on.n) -and ($eta_off2.n -eq $eta_off.n)) `
	("allume : {0} puis {1} px  ·  ETEINT : {2} puis {3} px (exige l'EGALITE des DEUX ; l'eteint est celui qui dansait, 44 puis 70, et c'est lui qui met en jeu le test de profondeur)" -f $eta_on.n, $eta_on2.n, $eta_off.n, $eta_off2.n)
Write-Host "       ⚠ CE CHIFFRE EST FRAGILE, et il faut le dire a cote de lui : dans"
Write-Host "       l'etalon les aretes sont CLAIRES, et le filtre de forme en ramasse des"
Write-Host "       fragments. En retirant les amas fusionnes avec une arete, ce meme"
Write-Host "       comptage donne 110 % de survie -- une impossibilite. Ce qui etablit le"
Write-Host "       rognage de l'etalon, ce sont les IMAGES, pas ce pourcentage."

Write-Host ""
Write-Host "-----------------------------------------------------------------------"
Write-Host "LA DECISION, DERIVEE — elle etait ecrite avant la course :"
if ($rEta -lt 0 -or $rProche -lt 0) {
	Write-Host "  INDECIDABLE : une des deux mesures manque."
	exit 2
}
elseif ($rEta -lt 80) {
	Write-Host ("  L'ETALON ROGNE LUI AUSSI : {0} % de ses coeurs survivent rayon X eteint" -f $rEta)
	# ⚠️ ON AFFICHE LA VALEUR MESURABLE DU MODELEUR, PAS CELLE DE d=4. Celle-ci
	#    vient de la partie C, au cadrage pour lequel la boite a ete relevee, et
	#    elle passe le garde-fou d'ordre de grandeur. Imprimer a cote de la
	#    conclusion un chiffre que la sonde vient de declarer NON MESURABLE, ce
	#    serait le remettre en circulation par la bande.
	Write-Host ("  (modeleur, cadrage par defaut : {0} % en DirectX 11, {1} % en OpenGL)." -f $rDX, $rGL)
	Write-Host "  => Ce n'est PAS un defaut du modeleur : c'est une PROPRIETE DU MARQUEUR,"
	Write-Host "     presente des deux cotes. Changer le marqueur toucherait l'ETALON que"
	Write-Host "     Rodolf a valide : c'est un ARBITRAGE, pas un correctif discret."
}
else {
	Write-Host ("  L'ETALON NE ROGNE PAS ({0} % contre {1} % pour le modeleur)." -f $rEta, $rDX)
	Write-Host "  => Le modeleur est seul en cause. (A1) dit si l'echelle de camera l'explique."
}
Write-Host "-----------------------------------------------------------------------"
if ($script:rouges -gt 0) { exit 1 } else { exit 0 }
