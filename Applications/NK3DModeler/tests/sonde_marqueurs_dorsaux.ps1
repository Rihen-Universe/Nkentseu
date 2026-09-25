# -----------------------------------------------------------------------------
# @File    Applications/NK3DModeler/tests/sonde_marqueurs_dorsaux.ps1
# @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# SONDE DE MESURE — LE DÉCALAGE DES MARQUEURS, SUR TOUS LES DORSAUX
#
# ⚠ AUCUNE INJECTION D'ENTREE. Crochets d'API seulement, fenetre titree
#   *** SONDE DE MESURE ***, fermee par NK_AGENT_EXIT.
#
# POURQUOI ELLE EXISTE
# --------------------
# Le correctif du rognage a ete mesure sur DEUX dorsaux -- DirectX 11 et OpenGL --
# et le resultat « 33 % -> 79/83 % » a ete publie comme s'il valait pour tous.
# Rodolf : « et sur Vulkan, DX12 et Metal ? »
#
# ⚠ ET LE DEPOT SAIT POURQUOI CA COMPTE : « un vrai defaut ne frappe pas GL et VK
#   a l'identique ». L'accord de plusieurs dorsaux est un signal SUSPECT autant
#   que rassurant -- deux dorsaux d'accord peuvent l'etre parce que le code est
#   commun, ou parce que la mesure ne voit ni l'un ni l'autre.
#
# LES TROIS ISSUES, TOUTES INFORMATIVES, ECRITES AVANT LA COURSE
# ---------------------------------------------------------------
#   (i)   les quatre s'accordent          -> le correctif vit dans le code COMMUN,
#                                            et c'est ce qu'on attend d'un decalage
#                                            calcule avant l'envoi au dorsal ;
#   (ii)  DX11+DX12 contre GL+VK          -> c'est une affaire d'API, pas de code ;
#   (iii) un seul diverge                 -> defaut de CE dorsal, et il est trouve.
#
# ⚠ METAL N'EST PAS MESURE ICI, ET CE N'EST PAS UN OUBLI. Il n'existe que sur les
#   plateformes Apple ; le kit le REFUSE sur Windows avec son motif plutot que de
#   lancer autre chose en silence (`NkEditorGfxApiSupported`). La sonde le declare
#   NON MESURABLE au lieu de laisser croire qu'un chiffre couvre cinq dorsaux
#   quand il en couvre quatre.
#   CONDITION DE REOUVERTURE : une course sur macOS, par les Actions GitHub.
#
# LE CRITERE : la survie des coeurs de marqueur, rayon X eteint contre allume.
# ⚠ Son PLAFOND n'est pas 100 % mais ~7/8 = 87 % : rayon X allume, les 24 sommets
#   du cube se dessinent a 8 positions ; eteint, le filtre d'orientation n'en
#   garde que 12, a 7 positions. Le compte de pixels suit les POSITIONS, pas les
#   sommets -- trois marqueurs superposes ne couvrent pas trois fois plus de
#   pixels. Un attendu de 100 % serait inatteignable par construction.
#
# USAGE :  pwsh -File sonde_marqueurs_dorsaux.ps1
# CODES :  0 mesure faite · 1 un dorsal diverge · 2 la mesure n'a pas pu se faire
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

Add-Type -AssemblyName System.Drawing

# ⚠ LE FILTRE EST « CHAUD », PAS « VERT ELEVE ». Un seuil pris sur le vert s'est
#   perime le jour meme ou la couleur du marqueur s'est alignee sur Blender. La
#   chaleur -- rouge tres superieur au bleu -- ne depend pas de la teinte exacte.
function Coeurs([string]$png, [int[]]$boite) {
	if (-not (Test-Path $png)) { return -1 }
	$bmp = [System.Drawing.Bitmap]::FromFile($png)
	$rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
	$data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
		[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
	$oct = New-Object byte[] ($data.Stride * $bmp.Height)
	[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $oct, 0, $oct.Length)
	$bmp.UnlockBits($data)
	$st = $data.Stride
	$dx = @(-3, 3, 0, 0, -2, 2, -2, 2); $dy = @(0, 0, -3, 3, -2, 2, 2, -2)
	$n = 0
	for ($y = [Math]::Max(3, $boite[1]); $y -lt [Math]::Min($boite[3], $bmp.Height - 4); $y++) {
		for ($x = [Math]::Max(3, $boite[0]); $x -lt [Math]::Min($boite[2], $bmp.Width - 4); $x++) {
			$i = $y * $st + $x * 4
			$b = $oct[$i]; $g = $oct[$i + 1]; $rr = $oct[$i + 2]
			if (-not (($rr -gt 170) -and (($rr - $b) -gt 80) -and ($g -gt 60))) { continue }
			$v = 0
			for ($k = 0; $k -lt 8; $k++) {
				$j = ($y + $dy[$k]) * $st + ($x + $dx[$k]) * 4
				if (($oct[$j + 2] -lt 70) -and ($oct[$j + 1] -lt 70) -and ($oct[$j] -lt 70)) { $v++ }
			}
			if ($v -ge 2) { $n++ }
		}
	}
	$bmp.Dispose()
	return $n
}

# La boite du cube dans la fenetre du modeleur, relevee sur les captures.
$boite = @(640, 320, 880, 540)

function Courir([string]$dorsal, [string]$xray) {
	$png = Join-Path $Sortie ("dors_{0}_{1}.png" -f $dorsal, $(if ($xray) { "on" } else { "off" }))
	$out = Join-Path $Sortie ("dors_{0}_{1}.txt" -f $dorsal, $(if ($xray) { "on" } else { "off" }))
	if (Test-Path $png) { Remove-Item $png -Force }
	$env:NK_SONDE = "1"; $env:NK_ADD_NODE = "2,0,20"; $env:NK_EDIT_USER = "99"
	$env:NK_AGENT_SCENE = "30"; $env:NK_AGENT_EXIT = "160"; $env:NK_FIX_CAM = "1"
	$env:NK_EDIT_MODE = "1,40"; $env:NK_EDIT_SEL = "a"; $env:NK_EDIT_SELMASK = "1"
	$env:NK_AGENT_SHOT = "140"; $env:NK_TOAST_PROBE = $png
	$env:NK_GFX_BACKEND = $dorsal
	if ($xray) { $env:NK_EDIT_XRAY = "1" }
	$journal = Join-Path $Arbre "logs\app.log"
	$avant = Get-Date
	Start-Process -FilePath $exe -WorkingDirectory $Arbre -NoNewWindow -Wait -RedirectStandardOutput $out -ErrorAction Stop | Out-Null
	foreach ($v in @("NK_SONDE", "NK_ADD_NODE", "NK_EDIT_USER", "NK_AGENT_SCENE", "NK_AGENT_EXIT",
			"NK_FIX_CAM", "NK_EDIT_MODE", "NK_EDIT_SEL", "NK_EDIT_SELMASK", "NK_AGENT_SHOT",
			"NK_TOAST_PROBE", "NK_GFX_BACKEND", "NK_EDIT_XRAY")) {
		if (Test-Path "Env:\$v") { Remove-Item -Path "Env:\$v" }
	}
	# ⚠ LE DORSAL RETENU EST LU DANS LE JOURNAL, PAS SUPPOSE. Demander `vulkan`
	#   et obtenir DX11 par un repli silencieux rendrait deux colonnes identiques
	#   et « prouverait » un accord qui n'existe pas. Le kit refuse un dorsal
	#   indisponible avec son motif ; encore faut-il le LIRE.
	$retenu = "?"
	if (Test-Path $journal) {
		$m = Select-String -Path $journal -Pattern "Device RHI cree: ([A-Za-z0-9 ]+)" | Select-Object -Last 1
		if ($m) { $retenu = $m.Matches[0].Groups[1].Value.Trim() }
	}
	return [pscustomobject]@{ n = (Coeurs $png $boite); retenu = $retenu; png = $png }
}

$rouges = 0
Write-Host ""
Write-Host "LE DECALAGE DES MARQUEURS, DORSAL PAR DORSAL"
Write-Host "-----------------------------------------------------------------------"
Write-Host "dorsal     retenu        allume   eteint   survie     plafond ~87 %"
$res = @{}
foreach ($d in @("opengl", "vulkan", "dx11", "dx12")) {
	$on = Courir $d "1"
	$off = Courir $d $null
	$s = if ($on.n -gt 0) { [Math]::Round(100.0 * $off.n / $on.n, 1) } else { -1 }
	$res[$d] = $s
	$alerte = ""
	# ⚠ DEUX GARDES, ET LA SECONDE EST CELLE QUI ATTRAPE UN REPLI SILENCIEUX.
	if ($on.n -lt 15) { $alerte = "  <- COMPTE TROP FAIBLE : non concluant" }
	if ($on.retenu -notmatch $d.Replace("dx", "DirectX ").Replace("opengl", "OpenGL").Replace("vulkan", "Vulkan")) {
		$alerte = "  <- DORSAL RETENU DIFFERENT DE CELUI DEMANDE"
	}
	Write-Host ("{0,-10} {1,-13} {2,6}  {3,7}  {4,7} %{5}" -f $d, $on.retenu, $on.n, $off.n, $s, $alerte)
}
Write-Host ""
# ⚠ METAL : DECLARE, PAS DEVINE.
Write-Host "metal      NON MESURABLE ICI : Metal n'existe que sur les plateformes Apple, et le"
Write-Host "           kit le REFUSE sur Windows avec son motif plutot que de lancer autre chose"
Write-Host "           en silence (NkEditorGfxApiSupported). Condition de reouverture : une"
Write-Host "           course sur macOS, par les Actions GitHub."
Write-Host ""
$vals = @($res.Values | Where-Object { $_ -gt 0 })
if ($vals.Count -lt 2) { Write-Host "ARRET : moins de deux dorsaux mesurables."; exit 2 }
$ecart = ([Math]::Round(($vals | Measure-Object -Maximum).Maximum - ($vals | Measure-Object -Minimum).Minimum, 1))
Write-Host ("ecart entre le meilleur et le pire dorsal : {0} points" -f $ecart)
if ($ecart -lt 15) {
	Write-Host "  => LES DORSAUX S'ACCORDENT. Le decalage est calcule AVANT l'envoi au dorsal"
	Write-Host "     (une position monde, pas un etat de pipeline) : c'est l'issue attendue."
	Write-Host "     ⚠ Et l'accord ne PROUVE pas le correctif : il prouve que rien ne le"
	Write-Host "       contredit selon l'API. Ce qui le prouve, c'est le passage de 33 % a ~80 %."
} else {
	Write-Host "  => UN OU PLUSIEURS DORSAUX DIVERGENT. Ce n'est plus le code commun :"
	Write-Host "     c'est une affaire d'API ou de dorsal, et la colonne le dit."
	$rouges++
}
if ($rouges -gt 0) { exit 1 } else { exit 0 }
