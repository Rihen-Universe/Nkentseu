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

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

function Courir([string]$nom, [hashtable]$vars) {
	$sortie = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_loop_$nom.txt"
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
	$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
		-RedirectStandardOutput $sortie
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

Write-Host "-----------------------------------------------------------------------"
if ($Mutation) {
	if ($rouges -gt 0) { Write-Host "MUTATION TUEE ($rouges rouge(s)) — les criteres mordent."; exit 1 }
	Write-Host "ECHEC DE LA SONDE : la mutation a SURVECU, les criteres ne testent rien."
	exit 2
}
if ($rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "ECHEC ($rouges rouge(s))"
exit 1
