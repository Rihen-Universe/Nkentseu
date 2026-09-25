# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Applications/NK3DModeler/tests/sonde_saisie_panneau.ps1
# @Brief   (Q9, 21/09) LA SAISIE DU PANNEAU IA, PAR LE CHEMIN DE RODOLF : la
#          souris, le clavier et le depot arrivent par les RAPPELS de
#          l'application (NK_EVENEMENTS -> NkEvents().DispatchEvent), pas par
#          l'etat de NKGui ecrit a la main.
#
# POURQUOI CE CRITERE EST REECRIT : le banc du kit (24p..24s) etait VERT pendant
# que Rodolf, dans le modeleur, ne pouvait ni selectionner ni copier (capture
# 172651). Le banc posait `wantCopy` et `mousePos` lui-meme et commencait par
# Ctrl+A : il ne passait jamais par le CLIC dans le composeur, qui posait le
# curseur en fin de texte. Ce critere clique, glisse, tape et depose comme lui.
#
# ⚠️ IL ECRIT DANS LE PRESSE-PAPIERS DE LA MACHINE (Ctrl+C / Ctrl+X) : c'est la
#    mesure. Aucune entree n'est injectee dans le systeme d'exploitation.
# -----------------------------------------------------------------------------
param([string]$Arbre = "D:\Projets\2026\Nkentseu\Nkentseu-panneauia", [string]$Config = "Release")
$exe = Join-Path $Arbre "Build\Bin\$Config-Windows\NK3DModeler\NK3DModeler.exe"
$img = Join-Path $Arbre "Build\panneau_ia\reference_inscription.png"
# le panneau du modeleur, 1600 x 900 : x 1278..1554, composeur en bas, fil en haut
$ev = "60:d:1400:600;61:u:1400:600;64:t:bonjour le monde;" +
	  "70:d:1303:557;71:m:1330:557;72:m:1360:557;73:u:1360:557;" +
	  "76:k:ctrl+c;80:k:ctrl+x;86:r:1400:600;92:d:1320:340;93:u:1320:340;" +
	  "96:f:1400:300:$($img.Replace('\','/'))"
$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_AGENT_SCENE = "30"; $env:NK_AI_PANNEAU = "1"
$env:NK_EVENEMENTS = $ev; $env:NK_AI_ETAT = "44,50,56,60,66,72"; $env:NK_AGENT_EXIT = "110"
$out = Join-Path $env:TEMP "sonde_saisie_panneau.txt"
Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $out
foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_AGENT_SCENE", "NK_AI_PANNEAU", "NK_EVENEMENTS", "NK_AI_ETAT",
		"NK_AGENT_EXIT")) { Remove-Item "env:$v" -ErrorAction SilentlyContinue }
$etats = @{}
foreach ($l in (Get-Content $out)) {
	if ($l -match 'AI ETAT image=(\d+) saisie="([^"]*)" selection_composeur=(\(aucune\) )?"([^"]*)" selection_fil=(\d+) octets menu=(\d+) images=(\d+) presse_papiers="(.*)"$') {
		$etats[[int]$Matches[1]] = @{ saisie = $Matches[2]; sel = $Matches[4]; fil = [int]$Matches[5]; menu = [int]$Matches[6];
			images = [int]$Matches[7]; pp = $Matches[8] }
	}
}
$rouges = 0
function Critere($nom, $ok, $detail) {
	if ($ok) { Write-Host "VERT   $nom  $detail" } else { Write-Host "ROUGE  $nom  $detail"; $script:rouges++ }
}
$e44 = $etats[44]; $e50 = $etats[50]; $e56 = $etats[56]; $e60 = $etats[60]; $e72 = $etats[72]
Critere "(S1) le GLISSER dans le composeur selectionne" ($e44 -and $e44.sel -eq "bonjour l") "selection=« $($e44.sel) »"
Critere "(S2) Ctrl+C met la selection au presse-papiers" ($e50 -and $e50.pp -eq "bonjour l") "presse-papiers relu=« $($e50.pp) »"
Critere "(S3) Ctrl+X coupe" ($e56 -and $e56.saisie -eq "e monde") "saisie=« $($e56.saisie) »"
Critere "(S4) le clic DROIT ouvre le menu contextuel" ($e60 -and $e60.menu -eq 8) "menu=$($e60.menu) (8 = Contexte)"
Critere "(S5) un fichier LACHE sur le panneau se joint" ($e72 -and $e72.images -eq 1) "images=$($e72.images)"
Write-Host "$rouges ROUGE(S)"
exit $rouges
