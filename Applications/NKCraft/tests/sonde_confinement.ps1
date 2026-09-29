# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_confinement.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — LE CONFINEMENT DU CURSEUR PENDANT UNE MODALE (b5).
#
# POURQUOI IL EXISTE : quand une modale est lancee au CLAVIER (G, R, S), aucun
# bouton n'est tenu. Des que le curseur sort de la fenetre, Windows cesse
# d'envoyer des WM_MOUSEMOVE : la position hors bornes n'est JAMAIS vue, et le
# rebouclage ne se declencherait jamais sur un bord qui est aussi le bord de la
# fenetre. Blender confine exactement de la meme facon.
#
# ── LES TROIS GARDES, QUI SONT LES CRITERES ─────────────────────────────────
#   (i)   UNE SEULE FONCTION pose et retire, et elle SYNCHRONISE a chaque image
#         au lieu de « prendre » ici et « relacher » la. La modale a QUATRE
#         chemins de sortie ; un relachement reparti sur quatre sites a quatre
#         chances d'etre oublie. Mesure : prises == relachements en fin de
#         course, et le confinement n'est plus actif.
#   (ii)  Il est retire a la PERTE DE FOCUS, pas seulement a la validation et a
#         l'annulation. Le service, seul a connaitre la fenetre, refuse et
#         relache. (Mesure : voir la limite ci-dessous.)
#   (iii) Il ne s'active JAMAIS sans modale, et ca se prouve par le ZERO : une
#         course sans aucune operation modale doit laisser `prises` a 0.
#
# ── LA LIMITE, ANNONCEE PLUTOT QUE MAQUILLEE ────────────────────────────────
#   CETTE SONDE NE PROUVE PAS QUE LE CURSEUR EST RETENU. Le prouver voudrait
#   dire le deplacer vers l'exterieur et constater qu'il ne sort pas — c'est
#   l'injection de souris interdite, sans exception. Elle mesure donc les
#   APPELS (pris / relache / equilibre) et le ZERO, pas l'effet physique.
#   La garde (ii) est dans le meme cas : provoquer une perte de focus demande
#   d'activer une autre fenetre, donc d'agir sur le bureau de Rodolf. Elle est
#   LUE dans le code (le service rend faux sans focus et appelle
#   ClipMouseToClient(false)) et NON mesuree ici. Je le dis.
#
# ── LA MUTATION ─────────────────────────────────────────────────────────────
#   -Mutation pose NK_CLIP_OFF=1, qui retire le mecanisme et rien d'autre. Le
#   cas (1) doit alors rendre 0 prise, donc ROUGIR. S'il reste vert, il ne teste
#   rien et la sonde sort 2.
#
# USAGE : pwsh -File sonde_confinement.ps1 [-Config Debug] [-Mutation]
# SORTIE : 0 tout vert · 1 un critere rouge · 2 la mutation a survecu

param(
	[string]$Config = "Debug",
	[switch]$Mutation,
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

function Courir([string]$nom, [hashtable]$vars) {
	$sortie = Join-Path ([System.IO.Path]::GetTempPath()) "nk_sonde_clip_$nom.txt"
	$env:NK_SONDE = "1"
	$env:NK_ADD_NODE = "2,0,20"
	$env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"
	$env:NK_EDIT_SEL = "n"
	$env:NK_EDIT_SELMASK = "4"
	$env:NK_EDIT_PICK_FACE = "0,2,1,70"
	$env:NK_AGENT_EXIT = "240"
	if ($Mutation) { $env:NK_CLIP_OFF = "1" } else { Remove-Item Env:\NK_CLIP_OFF -ErrorAction SilentlyContinue }
	foreach ($k in $vars.Keys) { Set-Item "Env:\$k" $vars[$k] }
	$p = Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -PassThru -Wait `
		-RedirectStandardOutput $sortie
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE", "NK_EDIT_SEL",
			"NK_EDIT_SELMASK", "NK_EDIT_PICK_FACE", "NK_AGENT_EXIT", "NK_CLIP_OFF",
			"NK_VP_ACTION", "NK_VP_ACTION2", "NK_CLIP_REPORT")) {
		Remove-Item "Env:\$v" -ErrorAction SilentlyContinue
	}
	return $sortie
}

function Dernier([string]$f, [string]$motif) {
	$m = @(Select-String -Path $f -Pattern $motif)
	if ($m.Count -eq 0) { return $null }
	return $m[$m.Count - 1].Line
}

$rouges = 0
function Dire([string]$nom, [bool]$vert, [string]$detail) {
	if ($vert) { Write-Host "VERT   $nom  $detail" } else { Write-Host "ROUGE  $nom  $detail"; $script:rouges++ }
}

Write-Host ""
Write-Host "SONDE DE MESURE — confinement du curseur pendant une modale · mutation NK_CLIP_OFF : $(if ($Mutation) { 'ACTIVE' } else { 'inactive' })"
Write-Host "⚠ Cette sonde ne prouve PAS que le curseur est retenu : le prouver serait une injection de souris."
Write-Host "-----------------------------------------------------------------------"

# ── (0) LE ZERO : aucune modale, donc AUCUNE prise ──────────────────────────
# Sans ce cas, « il ne s'active jamais a tort » ne serait qu'une affirmation.
$f0 = Courir "zero" @{ "NK_CLIP_REPORT" = "230" }
$l0 = Dernier $f0 "CLIP ETAT"
Dire "(0) ZERO : sans aucune modale, aucune prise" ($l0 -match "prises=0") `
	"course complete sans operation modale -> $l0"

# ── (1) UNE MODALE PREND, ET LA VALIDATION RELACHE ─────────────────────────
# G lance au clavier (par le chemin du BOUTON : l'action du shell), puis la
# validation. Le confinement doit avoir ete pris AU MOINS une fois, et etre
# relache a la fin.
$f1 = Courir "modale" @{ "NK_VP_ACTION" = "modalmove,120"; "NK_VP_ACTION2" = "modalconfirm,160"; "NK_CLIP_REPORT" = "230" }
$l1 = Dernier $f1 "CLIP ETAT"
$pris = [regex]::Match("$l1", "prises=(\d+)")
$rel = [regex]::Match("$l1", "relaches=(\d+)")
$act = [regex]::Match("$l1", "actif=(\d+)")
$np = if ($pris.Success) { [int]$pris.Groups[1].Value } else { -1 }
$nr = if ($rel.Success) { [int]$rel.Groups[1].Value } else { -1 }
$na = if ($act.Success) { [int]$act.Groups[1].Value } else { -1 }
Dire "(1) une modale PREND le confinement" ($np -ge 1) "prises=$np (exige >= 1 ; sans le mecanisme : 0)"
Dire "(2) il est RELACHE a la fin de la course" (($na -eq 0) -and ($nr -ge $np)) `
	"actif=$na relaches=$nr contre prises=$np (exige actif=0 et relaches >= prises)"

Write-Host "-----------------------------------------------------------------------"
Write-Host "LU, NON MESURE : le relachement a la PERTE DE FOCUS. Le provoquer demanderait"
Write-Host "                 d'activer une autre fenetre, donc d'agir sur le bureau."
Write-Host "-----------------------------------------------------------------------"
if ($Mutation) {
	if ($rouges -gt 0) { Write-Host "MUTATION TUEE ($rouges rouge(s)) — les criteres mordent."; exit 1 }
	Write-Host "ECHEC DE LA SONDE : la mutation a SURVECU, les criteres ne testent rien."
	exit 2
}
if ($rouges -eq 0) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "ECHEC ($rouges rouge(s))"
exit 1
