# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Applications/NK3DModeler/tests/sonde_image_creation.ps1
# @Brief   (Q9, 21/09) L'IMAGE JOINTE ARRIVE A LA CREATION, DANS LES DEUX ORDRES,
#          par le chemin de Rodolf : clic dans le composeur, frappe, depot du
#          fichier sur le panneau, Entree -- rejoues par les rappels de
#          l'application (NK_EVENEMENTS). Aucune entree injectee dans l'OS.
#
# LE DEFAUT QU'IL GARDE : le relais vers `NkCreaJoindreImage` avait lieu une
# image APRES la soumission -- la demande partait sans son image, et la
# suivante la consommait. Le panneau le fait maintenant AVANT `NkAiSoumettre`.
# (A) image puis texte ; (B) texte puis image deposee. Le critere : la trace
# « IMAGE -> creation AVANT la demande » precede « [crea] ENVOI », et la vision
# de la creation lit CETTE image.
# -----------------------------------------------------------------------------
param([string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-panneauia", [string]$Config = "Release")
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
$img = (Join-Path $Arbre "Build\panneau_ia\reference_inscription.png").Replace('\', '/')
$rouges = 0
function Lancer($nom, $joindre, $ev) {
	$env:NK_SONDE = "1"; $env:NK_AGENT_SCENE = "30"; $env:NK_AI_PANNEAU = "1"; $env:NK_AGENT_EXIT = "4000"
	$env:NK_EVENEMENTS = $ev
	if ($joindre) { $env:NK_AI_JOINDRE = $img } else { Remove-Item env:NK_AI_JOINDRE -ErrorAction SilentlyContinue }
	$out = Join-Path $env:TEMP "sonde_image_creation_$nom.txt"
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $out
	foreach ($v in @("NK_SONDE", "NK_AGENT_SCENE", "NK_AI_PANNEAU", "NK_AGENT_EXIT", "NK_EVENEMENTS", "NK_AI_JOINDRE")) {
		Remove-Item "env:$v" -ErrorAction SilentlyContinue }
	return Get-Content $out
}
function Juger($nom, $lignes) {
	$iRelais = -1; $iEnvoi = -1; $vision = $false
	for ($i = 0; $i -lt $lignes.Count; $i++) {
		if ($iRelais -lt 0 -and $lignes[$i] -match "IMAGE -> creation AVANT la demande : .*reference_inscription\.png") { $iRelais = $i }
		if ($iEnvoi -lt 0 -and $lignes[$i] -match "\[crea\] ENVOI") { $iEnvoi = $i }
		if ($lignes[$i] -match "\[crea\] VISION : image '.*reference_inscription\.png'") { $vision = $true }
	}
	$ok = ($iRelais -ge 0) -and ($iEnvoi -gt $iRelais) -and $vision
	if ($ok) { Write-Host "VERT   $nom  relais ligne $iRelais, envoi ligne $iEnvoi, la vision lit l'image" }
	else { Write-Host "ROUGE  $nom  relais=$iRelais envoi=$iEnvoi vision=$vision"; $script:rouges++ }
}
Juger "(A) image jointe PUIS texte" (Lancer "a" $true "60:d:1400:600;61:u:1400:600;64:t:une chaise;70:k:enter")
Juger "(B) texte PUIS image deposee" (Lancer "b" $false "60:d:1400:600;61:u:1400:600;64:t:une chaise;80:f:1400:300:$img;90:d:1400:600;91:u:1400:600;95:k:enter")
Write-Host "$rouges ROUGE(S)"
exit $rouges
