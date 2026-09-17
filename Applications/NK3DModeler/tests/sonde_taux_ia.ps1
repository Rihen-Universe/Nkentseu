# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_taux_ia.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — LES TROIS TAUX, AVEC CONTRAT CONTRE SANS
#
# ⚠ AUCUNE FENETRE. Tout est console : le contrat s'imprime avec
#   `--contrat-outils`, et les demandes passent par la MEME commande que le
#   panneau lance (`Tools/Genia/ia_verbe.py`). La carte n'est sollicitee que par
#   le service de modele, jamais par un rendu.
#
# CE QU'ELLE MESURE, ET POURQUOI CES TROIS TAUX-LA
# ------------------------------------------------
#   T1  le modele REPOND quelque chose de LISIBLE  (une ligne qui ressemble a
#       une commande : ni phrase, ni politesse, ni bloc de code)
#   T2  ce qu'il rend APPARTIENT AU CONTRAT        (le pont l'accepterait)
#   T3  ce qu'il rend est CELUI QU'ON ATTENDAIT    (verite de reference)
#
# Les trois sont emboites : T3 <= T2 <= T1. Un seul taux ne dirait pas OU la
# chaine casse -- un modele qui repond joliment sans jamais nommer un verbe du
# contrat aurait T1 = 100 % et T2 = 0.
#
# ⚠ LA VERITE DE REFERENCE N'EST PAS RETAPEE ICI. Elle est LUE dans
#   `demandes_ia.txt`, ecrit et date AVANT qu'aucun modele ne soit interroge.
#   La retaper reviendrait a pouvoir l'ajuster apres coup, et le taux ne dirait
#   plus rien. Les DIX comptent, y compris les trois HORS PORTEE : un banc dont
#   tous les cas passent ne mesure pas une limite, il la cache.
#
# ⚠ LES TROIS HORS PORTEE SE JUGENT A L'ENVERS : leur verbe attendu est `-`, et
#   la bonne reponse est un REFUS. Un verbe invente y compte comme un ECHEC, et
#   c'est le plus grave des echecs : il modifierait le travail de Rodolf sans
#   qu'il l'ait demande.
#
# LA COMPARAISON QUI COMPTE : avec contrat contre sans. Elle ne mesure pas le
# modele, elle mesure CE QUE NOTRE OUTILLAGE LUI APPORTE.
#
# USAGE :  pwsh -File sonde_taux_ia.ps1 [-Modele qwen2.5:7b-instruct]
# CODES :  0 mesure faite · 2 la mesure n'a pas pu se faire
param(
	[string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-actifs",
	[string]$Config = "Release",
	[string]$Modele = "qwen2.5:7b-instruct",
	[string]$Sortie = ""
)

$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
if (-not (Test-Path $exe)) { Write-Host "ROUGE  binaire introuvable : $exe"; exit 2 }
Write-Host ("binaire : {0}" -f (Get-Item $exe).LastWriteTime.ToString("dd/MM HH:mm:ss"))
if (-not $Sortie) { $Sortie = [System.IO.Path]::GetTempPath() }
New-Item -ItemType Directory -Force -Path $Sortie | Out-Null
$env:NK_IA_MODELE = $Modele

# ── LE SERVICE REPOND-IL ? Sans cette garde, dix echecs de connexion
#    passeraient pour dix echecs du modele -- un taux de 0 % qui accuserait le
#    mauvais coupable.
try { $null = Invoke-RestMethod -Uri "http://127.0.0.1:11434/api/tags" -TimeoutSec 5 }
catch { Write-Host "CONDITION NON REUNIE : le service de modele ne repond pas. Ce n'est pas un verdict."; exit 2 }

# ── LE CONTRAT, ECRIT PAR L'APPLICATION ────────────────────────────────────
$contratPath = Join-Path $Sortie "taux_contrat.md"
& $exe --contrat-outils $contratPath | Out-Null
if (-not (Test-Path $contratPath)) { Write-Host "ROUGE  le contrat n'a pas ete ecrit"; exit 2 }
$contrat = Get-Content $contratPath -Raw
# Les verbes ANNONCES, lus dans le document -- jamais recopies. Le jour ou un
# verbe sera ajoute ailleurs, ce banc le verra par le document ou pas du tout.
$verbes = @()
foreach ($m in [regex]::Matches($contrat, '^\| `([a-z0-9]+)`', 'Multiline')) { $verbes += $m.Groups[1].Value }
if ($verbes.Count -lt 5) { Write-Host "ROUGE  le contrat n'annonce que $($verbes.Count) verbe(s)"; exit 2 }
Write-Host ("le contrat annonce {0} verbes" -f $verbes.Count)

# ── LES DIX DEMANDES, LUES ET NON RETAPEES ─────────────────────────────────
$demandes = @()
foreach ($l in Get-Content (Join-Path $Arbre "Applications\NK3DModeler\tests\demandes_ia.txt")) {
	if ($l -match '^\s*(\d+)\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|') {
		$demandes += [pscustomobject]@{ n = [int]$Matches[1]; texte = $Matches[2].Trim(); attendu = $Matches[3].Trim() }
	}
}
if ($demandes.Count -ne 10) { Write-Host "ROUGE  $($demandes.Count) demandes lues, 10 attendues"; exit 2 }

function Interroger([string]$texte, [bool]$avecContrat) {
	$inv = Join-Path $Sortie "taux_invite.txt"
	$rep = Join-Path $Sortie "taux_reponse.txt"
	if (Test-Path $rep) { Remove-Item $rep -Force }
	# ⚠️ L'INVITE EST ECRITE PAR LE PRODUIT, PAS PAR LA SONDE. La premiere
	#    version composait ici sa propre en-tete a cote de celle du panneau :
	#    elle mesurait donc un texte que Rodolf n'envoie jamais, et ses taux ne
	#    parlaient pas de notre outillage. C'est la faute « mesurer un chemin que
	#    l'utilisateur n'emprunte pas », commise sur l'invite au lieu du clic.
	#    `--invite-ia` rend EXACTEMENT ce que le panneau enverrait, et la
	#    mutation du contrat agit des deux cotes.
	if ($avecContrat) { Remove-Item "Env:\NK_IA_SANS_CONTRAT" -ErrorAction SilentlyContinue }
	else { $env:NK_IA_SANS_CONTRAT = "1" }
	& $exe --invite-ia $texte $inv | Out-Null
	if (-not (Test-Path $inv)) { return "" }
	& python (Join-Path $Arbre "Tools\Genia\ia_verbe.py") $inv $rep 2>$null | Out-Null
	if (-not (Test-Path $rep)) { return "" }
	$t = (Get-Content $rep -Raw).Trim()
	# LA MEME EXTRACTION QUE LE PRODUIT : premiere ligne, ornements retires.
	$t = ($t -split "`n")[0].Trim()
	$t = $t.Trim('`', '"', "'", ' ', '.', ';', '-', '*')
	return $t.ToLower()
}

function Mesurer([bool]$avecContrat) {
	$t1 = 0; $t2 = 0; $t3 = 0
	Write-Host ""
	Write-Host ("--- CONTRAT {0} ---" -f $(if ($avecContrat) { "DONNE" } else { "ABSENT" }))
	foreach ($d in $demandes) {
		$r = Interroger $d.texte $avecContrat
		# T1 : lisible = une ligne sans espace ni accent, faite de lettres,
		# chiffres et separateurs. C'est le test du produit, pas un autre.
		$lisible = ($r.Length -gt 0) -and ($r -match '^[a-z0-9:._+\-]+$')
		$dansContrat = $lisible -and ($verbes -contains ($r -split ':')[0])
		$horsPortee = ($d.attendu -eq '-')
		# ⚠ LES HORS PORTEE SE JUGENT A L'ENVERS : la bonne reponse est un refus,
		#   donc « pas un verbe du contrat » compte comme une REUSSITE.
		$juste = if ($horsPortee) { -not $dansContrat } else { $r -eq $d.attendu }
		if ($lisible) { $t1++ }
		if ($dansContrat -or $horsPortee) { $t2++ }
		if ($juste) { $t3++ }
		$marque = if ($juste) { "ok " } else { "NON" }
		Write-Host ("  {0} {1,-2} « {2,-38} » attendu={3,-12} rendu={4}" -f $marque, $d.n, $d.texte, $d.attendu, $r)
	}
	Write-Host ("  T1 lisible = {0}/10   T2 au contrat (ou refus juste) = {1}/10   T3 exact = {2}/10" -f $t1, $t2, $t3)
	return @($t1, $t2, $t3)
}

$avec = Mesurer $true
$sans = Mesurer $false
Remove-Item Env:\NK_IA_MODELE -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "-----------------------------------------------------------------------"
Write-Host ("LES TROIS TAUX, modele {0}" -f $Modele)
Write-Host ("  avec contrat : T1={0}/10  T2={1}/10  T3={2}/10" -f $avec[0], $avec[1], $avec[2])
Write-Host ("  sans contrat : T1={0}/10  T2={1}/10  T3={2}/10" -f $sans[0], $sans[1], $sans[2])
# ⚠ C'EST L'ECART QUI PARLE DE NOTRE OUTILLAGE, pas le taux absolu : celui-ci
#   melangerait ce que le modele sait deja et ce que le contrat lui apprend.
Write-Host ("  ce que le CONTRAT apporte : T3 {0:+#;-#;0} point(s) sur 10" -f ($avec[2] - $sans[2]))
Write-Host "-----------------------------------------------------------------------"
exit 0
