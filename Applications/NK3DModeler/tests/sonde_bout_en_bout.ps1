# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_bout_en_bout.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — (b9) LE BANC DE BOUT EN BOUT, SUR LES DIX DEMANDES ECRITES
#                   A L'AVANCE (`demandes_ia.txt`).
#
# ⚠ AUCUNE INJECTION D'ENTREE. ⚠ AUCUN MODELE N'EST CHARGE : voir T1 ci-dessous.
#
# TROIS TAUX, MESURES SEPAREMENT — PARCE QU'ILS ECHOUENT POUR DES RAISONS
# DIFFERENTES, ET QU'UN TAUX UNIQUE MELANGERAIT TROIS CAUSES
# ------------------------------------------------------------------------
#  T1  LE MODELE REND-IL QUELQUE CHOSE ?
#      ⚠ NON MESURE ICI, ET C'EST UN CHOIX QUE J'ASSUME. Le 7B quantifie occupe
#        4 444 Mo sur une carte de 8 192 (mesure du 14/09). Le charger pendant
#        que Rodolf travaille n'est pas une mesure que je prends sans qu'il le
#        sache. La commande qui le mesure est donnee en fin de fichier : elle
#        s'execute en une fois, et elle est a LUI.
#        ⚠ NE PAS confondre « non mesure » et « zero ». Le taux est INCONNU.
#
#  T2  LA REPONSE SE TRADUIT-ELLE EN UN VERBE ET DES PARAMETRES CONNUS ?
#      Mesure ici sur les verbes attendus de `demandes_ia.txt` : la chaine
#      reconnait-elle ce qu'un modele CORRECT aurait rendu ? Et, pour les trois
#      demandes hors portee, REFUSE-t-elle en nommant son motif ? Ce taux ne
#      depend d'aucun modele : il mesure NOTRE cote du contrat.
#
#  T3  L'OPERATION S'EXECUTE-T-ELLE ET MODIFIE-T-ELLE LE MAILLAGE ?
#      Le nombre de faces doit changer -- ou, pour `undo`, revenir a l'etat
#      d'avant. Un verbe reconnu qui ne ferait rien est le quatrieme etat d'une
#      commande, celui qui rapporte un succes : c'est exactement ce que (b6) a
#      trouve sur `submodeface`, et la raison pour laquelle T3 existe a part.
#
# ⚠ POURQUOI SEPARER T2 ET T3. Si un seul taux etait publie, une chaine qui
#   reconnait tout et n'execute rien rendrait le meme chiffre qu'une chaine qui
#   execute tout et ne reconnait rien. Ce sont deux pannes opposees.
#
# USAGE :  pwsh -File Applications/NK3DModeler/tests/sonde_bout_en_bout.ps1
# CODES :  0 les deux taux mesurables sont a 100 % · 1 sinon
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release"
)

$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 1 }

# Les dix demandes, LUES depuis le fichier versionne. Elles ne sont pas recopiees
# ici : deux listes ecrites separement finissent toujours par diverger, et ce
# banc perdrait precisement ce qui fait sa valeur -- des demandes figees avant.
$fichier = Join-Path $Arbre "Applications\NK3DModeler\tests\demandes_ia.txt"
if (-not (Test-Path $fichier)) { Write-Host "ROUGE  demandes_ia.txt introuvable"; exit 1 }
$cas = @()
foreach ($ligne in (Get-Content $fichier -Encoding UTF8)) {
	if ($ligne -match '^\s*#' -or $ligne -notmatch '\|') { continue }
	$p = $ligne -split '\|'
	if ($p.Count -lt 3) { continue }
	$cas += [pscustomobject]@{
		num = $p[0].Trim(); texte = $p[1].Trim(); verbe = $p[2].Trim()
	}
}
if ($cas.Count -ne 10) { Write-Host "ROUGE  $($cas.Count) demandes lues, 10 attendues"; exit 1 }

# Un essai : on donne a la chaine CE QUE LE MODELE AURAIT DU RENDRE (le verbe
# attendu), ou, pour les cas hors portee, LA DEMANDE EN FRANCAIS telle quelle --
# c'est exactement ce qu'un modele complaisant renverrait, et la chaine doit le
# refuser en nommant son motif.
$script:essai = 0
function Lancer([string]$verbe, [bool]$avecUndo) {
	$script:essai++
	$out = Join-Path ([System.IO.Path]::GetTempPath()) ("nk_bout_{0}.txt" -f $script:essai)
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SELMASK = "4"
	$env:NK_VP_ACTION = "selectall,60"
	# `undo` a besoin de quelque chose A annuler : on subdivise d'abord.
	if ($avecUndo) { $env:NK_VP_ACTION2 = "subdivide,80"; $env:NK_VP_ACTION3 = "$verbe,120" }
	else { $env:NK_VP_ACTION2 = "$verbe,90" }
	$env:NK_EDIT_REPORT = "70,150"
	$env:NK_AGENT_EXIT = "190"
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait `
		-RedirectStandardOutput $out | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_EDIT_MODE",
			"NK_EDIT_SELMASK", "NK_VP_ACTION", "NK_VP_ACTION2", "NK_VP_ACTION3",
			"NK_EDIT_REPORT", "NK_AGENT_EXIT")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	$r = [pscustomobject]@{ f0 = -1; f1 = -1; refuse = $false }
	$r.refuse = (@(Select-String -Path $out -Pattern "nom inconnu").Count -gt 0)
	$n = 0
	foreach ($x in @(Select-String -Path $out -Pattern "EDIT RAPPORT")) {
		$m = [regex]::Match($x.Line, "f=(\d+)")
		if (-not $m.Success) { continue }
		if ($n -eq 0) { $r.f0 = [int]$m.Groups[1].Value } else { $r.f1 = [int]$m.Groups[1].Value }
		$n++
	}
	return $r
}

Write-Host "===== LES DIX DEMANDES, ECRITES AVANT (demandes_ia.txt) ================"
$t2ok = 0; $t3ok = 0; $t3applicables = 0
foreach ($c in $cas) {
	$horsPortee = ($c.verbe -eq "-")
	# Hors portee : on soumet LA DEMANDE EN FRANCAIS, ce qu'un modele complaisant
	# renverrait. Dans le cas normal : le verbe attendu.
	$soumis = if ($horsPortee) { ($c.texte -replace '[^a-zA-Z0-9]', '') } else { $c.verbe }
	$estUndo = ($c.verbe -eq "undo")
	$r = Lancer $soumis $estUndo

	# ── T2 : reconnu, ou refuse AVEC motif quand c'est hors portee ──────────
	$t2 = if ($horsPortee) { $r.refuse } else { -not $r.refuse }
	if ($t2) { $t2ok++ }

	# ── T3 : le maillage a-t-il change comme annonce ? ───────────────────────
	# Hors portee : T3 ne s'applique pas (rien ne devait s'executer). On ne le
	# compte pas dans le denominateur -- un taux calcule sur des cas qui n'ont
	# pas lieu d'etre mesure autre chose que ce qu'il annonce.
	$t3 = $null
	if (-not $horsPortee) {
		$t3applicables++
		# `undo` doit RAMENER a l'etat d'avant ; les autres doivent le CHANGER.
		$t3 = if ($estUndo) { $r.f1 -eq $r.f0 } else { $r.f1 -ne $r.f0 }
		if ($t3) { $t3ok++ }
	}
	$marque = if ($t2) { "ok " } else { "NON" }
	$m3 = if ($t3 -eq $null) { " - " } elseif ($t3) { "ok " } else { "NON" }
	Write-Host ("  {0,-3} {1,-38} -> {2,-20} T2={3} T3={4}  (faces {5} -> {6})" -f `
		$c.num, $c.texte, $soumis, $marque, $m3, $r.f0, $r.f1)
}

Write-Host "-----------------------------------------------------------------------"
Write-Host "  T1  le modele rend-il quelque chose  : NON MESURE (aucun modele charge)"
Write-Host "      ⚠ inconnu, et surtout PAS zero. La commande est en fin de fichier."
Write-Host ("  T2  traduit en verbe connu, ou refuse avec motif : {0}/10  ({1} %)" -f `
	$t2ok, [Math]::Round(100.0 * $t2ok / 10.0))
Write-Host ("  T3  l'operation s'execute et modifie le maillage  : {0}/{1}  ({2} %)" -f `
	$t3ok, $t3applicables, [Math]::Round(100.0 * $t3ok / [Math]::Max(1, $t3applicables)))
Write-Host "-----------------------------------------------------------------------"
Write-Host "  POUR MESURER T1 (a lancer par Rodolf, quand sa carte est libre) :"
Write-Host "    NKDesignLLM --invite invite.txt --sortie reponse.txt"
Write-Host "  Le 7B quantifie prend ~4 444 Mo sur les 8 192 de la carte ; le modeleur"
Write-Host "  en prend ~162 de plus sur une scene simple. Les deux tiennent ensemble,"
Write-Host "  mais une scene lourde mangera la marge."

if ($t2ok -eq 10 -and $t3ok -eq $t3applicables) { Write-Host "TOUT VERT (0 rouge)"; exit 0 }
Write-Host "IL RESTE DES CAS NON TENUS"
exit 1
