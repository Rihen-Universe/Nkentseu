# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_loopcut.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — (b7) LES QUATRE ETATS DE Ctrl+R.
#
# « Quatre etats chez Blender, pas une commande. » Chez nous il n'y avait qu'UNE
# phase : la souris choisissait l'anneau ET faisait coulisser, en meme temps.
# Chez Blender ce sont DEUX phases separees par un clic, et Echap n'y veut pas
# dire la meme chose :
#     phase 0 (CHOIX)   Echap -> AUCUNE boucle
#     phase 1 (GLISSER) Echap -> la boucle RESTE, remise au milieu
#
# ⚠ C'EST L'ECHAP DE LA PHASE 1 QUI PORTE TOUT LE SUJET. Il annulait tout : un
#   utilisateur qui placait sa boucle puis se ravisait sur le coulissement
#   PERDAIT sa boucle.
#
# ── LES COURSES, ET CE QUE CHACUNE DOIT MONTRER ─────────────────────────────
#   (d) phase 0 + Echap        -> faces INCHANGEES, pile d'annulation VIDE
#   (e) phase 0 + clic         -> phase GLISSEMENT ouverte, anneau FIGE
#   (f) phase 1 + Echap        -> la boucle RESTE : faces AUGMENTEES, pile a 1
#   (g) validation normale     -> meme compte qu'en (f) : Echap en phase 1 ne
#                                 coute pas la boucle
#
# ⚠ AUCUNE INJECTION D'ENTREE : la modale se lance et se termine par les crochets
#   d'API (NK_MODAL_OP, NK_MODAL_CONFIRM, NK_MODAL_CANCEL), c'est-a-dire par le
#   chemin du bouton. La fenetre porte *** SONDE DE MESURE ***.
#
# ── LA MUTATION ─────────────────────────────────────────────────────────────
#   -Mutation pose NK_LOOP_UNEPHASE=1, qui REFUSIONNE les deux phases (le
#   comportement d'avant). (f) doit alors rougir : la boucle repart avec Echap.
#
# USAGE : pwsh -File sonde_loopcut.ps1 [-Config Debug] [-Mutation]
# SORTIE : 0 tout vert · 1 un critere rouge · 2 la mutation a survecu
#          3 la COURSE n'a pas pu avoir lieu

param(
	[string]$Config = "Debug",
	[switch]$Mutation,
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs"
)
# ── LA GARDE DE CONDITION ──────────────────────────────────────────────────
# Un banc qui n a pas pu mesurer doit le DIRE, au lieu d accuser le code : le
# 17/09 celui-ci a rendu rouge sur une scene qui n etait pas prete, et deux de
# ses criteres sont meme passes VERT en comparant deux sentinelles entre elles.
. (Join-Path $PSScriptRoot "condition.ps1")
$script:fichiers = @()

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

function Courir([string]$nom, [hashtable]$vars) {
	$sortie = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_loop_$nom.txt"
	$script:fichiers += $sortie
	$env:NK_SONDE = "1"
	$env:NK_ADD_NODE = "2,0,20"
	$env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"
	$env:NK_EDIT_SEL = "n"
	$env:NK_EDIT_SELMASK = "2"
	$env:NK_EDIT_PICK_EDGE = "0,-1,0,70"
	$env:NK_EDIT_REPORT = "100,230"
	$env:NK_AGENT_EXIT = "260"
	if ($Mutation) { $env:NK_LOOP_UNEPHASE = "1" } else { Remove-Item Env:\NK_LOOP_UNEPHASE -ErrorAction SilentlyContinue }
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k] }
	# ⚠ LE VERDICT N'EST PAS TOUJOURS LA OU L'ON REGARDE, ET CETTE SONDE L'A PAYE
	#   LE 17/09. Elle ne lisait que la console. Or le produit ecrit par DEUX
	#   canaux : `printf` va sur la console, `logger.Info` va dans logs/app.log --
	#   et en Release il N'ECHO PAS sur la console, alors qu'en Debug il le fait.
	#   Resultat : sept criteres VERTS en Debug et ROUGES en Release, sur le MEME
	#   code. Le banc ne mesurait pas le produit, il mesurait la configuration.
	#   Le journal est reecrit a chaque lancement : on le releve donc APRES la
	#   course, et on verifie qu'il lui est POSTERIEUR -- sinon on heriterait du
	#   journal de la course precedente, ce qui rendrait des verts empruntes.
	$journal = Join-Path $Arbre "logs\app.log"
	$avant = Get-Date
	$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
		-RedirectStandardOutput $sortie
	if ((Test-Path $journal) -and ((Get-Item $journal).LastWriteTime -ge $avant)) {
		Add-Content -Path $sortie -Value (Get-Content $journal -Raw)
	}
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
			"NK_EDIT_SELMASK", "NK_EDIT_PICK_EDGE", "NK_EDIT_REPORT", "NK_AGENT_EXIT",
			"NK_LOOP_UNEPHASE", "NK_MODAL_OP", "NK_MODAL_CONFIRM", "NK_MODAL_CANCEL",
			"NK_MODAL_DRAG", "NK_MODAL_DRAG_FRAMES")) {
		Remove-Item "Env:\$v" -ErrorAction SilentlyContinue
	}
	return $sortie
}
function Rap([string]$f, [int]$idx) {
	$m = @(Select-String -Path $f -Pattern "EDIT RAPPORT")
	if ($m.Count -le $idx) { return $null }
	return $m[$idx].Line
}
function Lire([string]$l, [string]$cle) {
	if ($null -eq $l) { return -999 }
	$m = [regex]::Match($l, [regex]::Escape($cle) + "=(-?\d+)")
	if ($m.Success) { return [int]$m.Groups[1].Value }
	return -999
}
$rouges = 0
function Dire([string]$nom, [bool]$vert, [string]$detail) {
	if ($vert) { Write-Host "VERT   $nom  $detail" } else { Write-Host "ROUGE  $nom  $detail"; $script:rouges++ }
}

Write-Host ""
Write-Host "SONDE DE MESURE — (b7) les quatre etats de Ctrl+R · mutation NK_LOOP_UNEPHASE : $(if ($Mutation) { 'ACTIVE' } else { 'inactive' })"
Write-Host "-----------------------------------------------------------------------"

# ── (d) PHASE 0 + ECHAP : AUCUNE BOUCLE. C'est le zero de la phase 0. ──────
$fd = Courir "phase0_echap" @{ "NK_MODAL_OP" = "loopcut"; "NK_MODAL_CANCEL" = "1" }
$d0 = Rap $fd 0; $d1 = Rap $fd 1
$fA = Lire $d0 "f"; $fB = Lire $d1 "f"; $unD = Lire $d1 "annuler"
Dire "(d) phase 0 + Echap : AUCUNE boucle" (($fA -gt 0) -and ($fB -eq $fA) -and ($unD -eq 0)) `
	"faces $fA -> $fB (exige EGAL et > 0), pile d'annulation=$unD (exige 0)"

# ── (f) PHASE 1 + ECHAP : LA BOUCLE RESTE. LE critere de (b7). ────────────
# Le premier clic passe en phase 1 ; l'Echap qui suit ne doit annuler que le
# glissement. NK_MODAL_CONFIRM confirme a la premiere occasion -> c'est lui qui
# joue le PREMIER clic ; NK_MODAL_CANCEL joue l'Echap d'apres.
$ff = Courir "phase1_echap" @{ "NK_MODAL_OP" = "loopcut"; "NK_MODAL_CONFIRM" = "1"; "NK_MODAL_CANCEL" = "12" }  # UN seul clic, puis Echap
$f0 = Rap $ff 0; $f1 = Rap $ff 1
$gA = Lire $f0 "f"; $gB = Lire $f1 "f"; $unF = Lire $f1 "annuler"
$phase = @(Select-String -Path $ff -Pattern "anneau FIGE").Count
$reste = @(Select-String -Path $ff -Pattern "la boucle RESTE").Count
Dire "(e) le premier clic FIGE l'anneau et ouvre le glissement" ($phase -ge 1) `
	"« anneau FIGE » journalise $phase fois (exige >= 1)"
# ⚠ LA REFERENCE EST LE CUBE MESURE, PAS LE RELEVE 0 DE CETTE COURSE. La modale
#   demarre AVANT le premier releve, donc comparer les deux releves d'une meme
#   course comparerait deux grandeurs deja egales -- et le critere ne pourrait
#   pas echouer. `$refCube` vient de la course (d), celle qui n'insere rien.
$refCube = $fA
Dire "(f) phase 1 + Echap : la boucle RESTE" (($gB -gt $refCube) -and ($unF -ge 1) -and ($reste -ge 1)) `
	"faces $gB contre $refCube pour le cube nu (exige PLUS), pile d'annulation=$unF (exige >= 1), « la boucle RESTE » x$reste"

# ── (g) VALIDATION NORMALE : meme compte qu'en (f). ───────────────────────
# Si Echap en phase 1 coutait la boucle, (f) et (g) differeraient.
$fg = Courir "valide" @{ "NK_MODAL_OP" = "loopcut"; "NK_MODAL_CONFIRM" = "2" }  # DEUX clics
$g0 = Rap $fg 0; $g1 = Rap $fg 1
$hA = Lire $g0 "f"; $hB = Lire $g1 "f"
Dire "(g) Echap en phase 1 donne le MEME maillage qu'une validation" (($hB -eq $gB) -and ($hB -gt $refCube)) `
	"validation (2 clics) : $hB faces · Echap en phase 1 : $gB · cube nu : $refCube (exige les deux EGAUX et > cube)"


# ── (h) LE CRITERE QUI PORTE VRAIMENT LA SEPARATION ───────────────────────
# Les comptes de faces ne distinguent PAS les deux comportements : dans les deux
# cas la boucle finit par etre posee. Ce qui differe est LE GLISSEMENT AU MOMENT
# DE LA VALIDATION.
#   nouveau : le glisser tombe en phase 0, ou la souris NE GLISSE PAS -> 0
#   ancien  : le glisser compte tout de suite -> la boucle est DECALEE
# On injecte donc un glisser par l'API (NK_MODAL_DRAG, aucune souris touchee) et
# on lit la valeur que la confirmation journalise.
$fh = Courir "glisse" @{ "NK_MODAL_OP" = "loopcut"; "NK_MODAL_DRAG" = "120"; "NK_MODAL_DRAG_FRAMES" = "6"; "NK_MODAL_CONFIRM" = "1"; "NK_MODAL_CANCEL" = "12" }
$conf = @(Select-String -Path $fh -Pattern "CONFIRME \(valeur=")
$val = -999.0
if ($conf.Count -gt 0) {
	$m = [regex]::Match($conf[$conf.Count - 1].Line, "valeur=(-?[0-9.]+)")
	if ($m.Success) { $val = [double]$m.Groups[1].Value }
}
$absVal = [Math]::Abs($val)
Dire "(h) un glisser en phase 0 est IGNORE : validation a glissement nul" ($absVal -lt 0.001) `
	"glissement a la validation = $val (exige 0 ; sans la separation, le glisser compte deja)"

# -- (i)(j)(k)(l) LA SAISIE NUMERIQUE : DEUX PHASES, DEUX SENS, MEMES BORNES --
# Blender : apres Ctrl+R, taper 3 donne TROIS coupes ; une fois l'anneau fige, le
# nombre devient le FACTEUR DE GLISSEMENT. NK_MODAL_NUM="<n1>,<n2>" joue les deux
# frappes -- n2 ne pouvait pas exister avant, la phase 1 n'etant pas ouverte.
# ⚠ LES BORNES SONT CELLES DU PILOTAGE SOURIS. La saisie ne passait par AUCUNE
#   borne : taper 50 donnait un facteur de 50 la ou la souris plafonne a 1, et le
#   defaut ne se voyait QUE si l'on tapait un nombre -- presque jamais.
function ValeurDe([string]$nums, [string]$mutBorne) {
	$e = @{ "NK_MODAL_OP" = "loopcut"; "NK_MODAL_NUM" = $nums; "NK_MODAL_CONFIRM" = "2" }
	if ($mutBorne) { $e["NK_NUM_SANSBORNE"] = "1" }
	$f = Courir ("num_" + ($nums -replace "[^0-9]", "_") + $mutBorne) $e
	if (Test-Path "Env:\NK_NUM_SANSBORNE") { Remove-Item -Path "Env:\NK_NUM_SANSBORNE" }
	$c = @(Select-String -Path $f -Pattern "CONFIRME")
	$v = -999.0; $sg = -999
	if ($c.Count -gt 0) {
		$m = [regex]::Match($c[$c.Count - 1].Line, "valeur=(-?[0-9.]+) segments=([0-9]+)")
		if ($m.Success) { $v = [double]$m.Groups[1].Value; $sg = [int]$m.Groups[2].Value }
	}
	return @($v, $sg)
}
$r1 = ValeurDe "3,50" ""
$r2 = ValeurDe "3,50" "x"
$r3 = ValeurDe "3,0.4" ""
$r4 = ValeurDe "3,-50" ""
Dire "(i) le nombre de la phase 0 est un nombre de COUPES" ($r1[1] -eq 3) `
	"NUM=3,50 -> segments=$($r1[1]) (exige 3)"
Dire "(j) LE ZERO DES BORNES : une valeur DEJA valide n'est pas deformee" ([Math]::Abs($r3[0] - 0.4) -lt 0.001) `
	"NUM=3,0.4 -> glissement=$($r3[0]) (exige 0,4 inchange ; sinon la borne deformerait tout)"
Dire "(k) la saisie de phase 1 est bornee COMME LA SOURIS" (([Math]::Abs($r1[0] - 1.0) -lt 0.001) -and ([Math]::Abs($r4[0] + 1.0) -lt 0.001)) `
	"NUM=3,50 -> $($r1[0]) et NUM=3,-50 -> $($r4[0]) (exige +1 et -1 : la souris ne depasse jamais 1)"
Dire "(l) MUTATION des bornes : 50 passe tel quel" ([Math]::Abs($r2[0] - 50.0) -lt 0.001) `
	"NUM=3,50 sans borne -> $($r2[0]) (exige 50 : c'est exactement ce que la borne empeche)"

# ── (m) (n) (o) : LE VERBE, POUR QU'UN MODELE PUISSE DEMANDER UNE BOUCLE ───
# ⚠ AJOUTES LE 17/09, ET POUR UNE RAISON PRECISE : le contrat d'outils a montre
#   que `loopcut` n'y figurait pas. L'outil existait, prouve en deux phases par
#   les criteres (d) a (l) ci-dessus, et AUCUN modele ne pouvait le demander.
#   Mais un verbe simplement ACCEPTE ne prouve rien : ce chantier a deja vu des
#   verbes accueillis qui ne faisaient rien, et c'est pire qu'un verbe absent,
#   parce que l'appelant croit avoir agi.
# ⚠ L'ATTENDU DE (m) NE PORTE AUCUN NOMBRE ABSOLU. Il compare DEUX courses :
#   l'effet de deux boucles doit valoir exactement le double de l'effet d'une.
#   Un attendu en dur (« 14 faces ») se perimerait au premier changement de
#   maillage de depart ; celui-ci se derive de la course elle-meme.
$fm1 = Courir "verbe_1" @{ "NK_VP_ACTION" = "loopcut:1:0" }
$fm2 = Courir "verbe_2" @{ "NK_VP_ACTION" = "loopcut:2:0.4" }
function Faces([string]$f, [int]$i) { return (Lire (Rap $f $i) "f") }
$av1 = Faces $fm1 0; $ap1 = Faces $fm1 1
$av2 = Faces $fm2 0; $ap2 = Faces $fm2 1
$dl1 = $ap1 - $av1
$dl2 = $ap2 - $av2
Dire "(m) LE VERBE AGIT, et son effet suit le nombre demande" (($dl1 -gt 0) -and ($dl2 -eq (2 * $dl1))) `
	"une boucle : +$dl1 faces · deux boucles : +$dl2 (exige exactement le double, derive et non dicte)"

# Le glissement est LU dans le rapport, pas suppose : un parametre qu'aucune
# mesure ne voit est un parametre qu'on croit sur parole.
$glisL = Rap $fm2 1
$glis = -99.0
if ($null -ne $glisL) {
	$mg = [regex]::Match($glisL, "glis=(-?[\d.]+)")
	if ($mg.Success) { $glis = [double]$mg.Groups[1].Value }
}
Dire "(n) le GLISSEMENT demande par le verbe est reellement POSE" ([Math]::Abs($glis - 0.4) -lt 0.001) `
	"loopcut:2:0.4 -> glis=$glis (exige 0,4 : sinon le verbe accepterait un parametre qu'il jette)"

# ⚠ ET LE REFUS EST NOMME. Deux etats qui ne se confondent pas : hors mode
#   Edition, l'outil n'a pas de maillage ouvert ; dans l'edition, l'anneau peut
#   ne pas se fermer. Un seul message pour les deux ne permettrait a personne de
#   se corriger.
# ⚠ CETTE COURSE N'ENTRE PAS DANS LA GARDE COMMUNE, ET C'EST LA GARDE QUI ME
#   L'A APPRIS : elle exige « EDIT MODE (utilisateur » dans chaque fichier, or
#   celle-ci est VOLONTAIREMENT hors du mode Edition -- c'est tout son sujet.
#   L'y inscrire faisait declarer « je n'ai pas pu mesurer » a un banc qui
#   venait justement de mesurer. Elle porte donc SA condition : l'application
#   doit avoir tourne et parle, sinon (o) serait rouge pour la mauvaise raison.
$sortieObj = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_loop_refus.txt"
$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
$env:NK_AGENT_SCENE = "30"; $env:NK_VP_ACTION = "loopcut"; $env:NK_AGENT_EXIT = "160"
Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $sortieObj | Out-Null
foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_AGENT_SCENE", "NK_VP_ACTION", "NK_AGENT_EXIT")) {
	Remove-Item "Env:\$v" -ErrorAction SilentlyContinue
}
if (@(Select-String -Path $sortieObj -Pattern "\[nk3d\]").Count -eq 0) {
	Write-Host "CONDITION NON REUNIE pour (o) : l'application n'a rien dit -- elle n'a pas tourne."
	exit 3
}
$refus = @(Select-String -Path $sortieObj -Pattern "loopcut REFUS : .*mode Edition")
Dire "(o) hors du mode Edition, loopcut REFUSE en se NOMMANT" ($refus.Count -ge 1) `
	"lignes de refus nomme = $($refus.Count) (exige au moins 1 : un echec muet ferait croire au succes)"

Write-Host "-----------------------------------------------------------------------"
if ($Mutation) {
	if ($rouges -gt 0) { Write-Host "MUTATION TUEE ($rouges rouge(s)) — les criteres mordent."; exit 1 }
	Write-Host "ECHEC DE LA SONDE : la mutation a SURVECU, les criteres ne testent rien."
	exit 2
}
# ⚠ LE VERDICT NE VAUT QUE SI LA CONDITION ETAIT REUNIE. Sinon, ni vert ni
#   rouge : code 3, et les lignes ci-dessus ne sont pas des verdicts.
if (NkConditionManque $script:fichiers) {
	Write-Host "  ⚠ LES LIGNES CI-DESSUS NE SONT PAS DES VERDICTS."
	exit 3
}
if ($rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "ECHEC ($rouges rouge(s))"
exit 1
