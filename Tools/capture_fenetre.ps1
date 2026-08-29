# =============================================================================
#  capture_fenetre.ps1 -- lance une application, LEVE sa fenetre, la photographie
# =============================================================================
#  Usage :
#    .\Tools\capture_fenetre.ps1 -Exe <chemin> -Sortie <png> [-AppArgs "..."]
#                                [-Attente 6] [-Delai 2500]
#
#  TROIS PIEGES DEJA PAYES SUR CE DEPOT, ET ILS SONT TOUS LES TROIS ICI :
#
#  1. `$Args` EST UNE VARIABLE AUTOMATIQUE DE POWERSHELL. Un parametre de script
#     nomme `$Args` est ecrase et arrive VIDE -- `Start-Process -ArgumentList`
#     echoue alors, et le script part en cascade de nulls. D'ou `$AppArgs`.
#
#  2. `SetForegroundWindow` ECHOUE SI L'APPELANT N'A PAS DEJA LE FOCUS. Mesure du
#     18/08 : la garde est passee ROUGE deux fois de suite alors que
#     l'application tournait parfaitement. Ce n'etait pas un defaut de
#     l'application, c'etait le script qui ne savait pas lever la fenetre.
#     Deverrouillage : presser/relacher ALT autour de l'appel, plus
#     `ShowWindow(SW_RESTORE)`.
#
#  3. LA GARDE PID A EU RAISON DE REFUSER. Elle a prefere AUCUNE capture a une
#     capture d'autre chose -- c'est exactement ce qu'on lui demande. On ne
#     photographie que la fenetre du PROCESSUS QU'ON A LANCE, jamais « la
#     fenetre au premier plan ».
#
#  ET UN QUATRIEME, PROPRE A WINDOWS 10/11 : `GetWindowRect` inclut l'ombre
#  portee du gestionnaire de fenetres, ce qui donne une bande noire autour de la
#  capture. On prend `DWMWA_EXTENDED_FRAME_BOUNDS`, qui rend le cadre reel.
# =============================================================================

param(
	[Parameter(Mandatory = $true)][string]$Exe,
	[Parameter(Mandatory = $true)][string]$Sortie,
	[string[]]$AppArgs = @(),
	[int]$Attente = 8,      # secondes max d'attente de la fenetre
	[int]$Delai = 2500      # ms laisses a l'application pour dessiner
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$sig = @"
using System;
using System.Runtime.InteropServices;
public class NkWin {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int s);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    public const int SW_RESTORE = 9;
    public const int DWMWA_EXTENDED_FRAME_BOUNDS = 9;
    public static void LeverAvecAlt(IntPtr h) {
        // ALT enfonce/relache : deverrouille SetForegroundWindow (piege 2).
        keybd_event(0x12, 0, 0, UIntPtr.Zero);
        keybd_event(0x12, 0, 2, UIntPtr.Zero);
        ShowWindow(h, SW_RESTORE);
        SetForegroundWindow(h);
    }
}
"@
Add-Type -TypeDefinition $sig

Write-Host "[capture] lancement : $Exe [$($AppArgs -join '] [')]"
# =============================================================================
#  PIEGE 5, ET IL A FAILLI LIVRER UNE IMAGE QUI MENT
# =============================================================================
#  `Start-Process -ArgumentList` NE CITE PAS les elements qui contiennent un
#  espace -- ni en chaine, ni en TABLEAU. Il les recolle avec des espaces, et le
#  systeme redecoupe. `--theme=GitHub Light Pro` arrive donc a l'application en
#  TROIS morceaux : `--theme=GitHub`, `Light`, `Pro`.
#
#  Et l'application se comporte exactement comme elle le doit : elle ne connait
#  pas le theme « GitHub », elle le DIT dans son journal, et elle garde son
#  defaut. Le defaut, lui, ne se voit pas sur la capture.
#
#  MESURE DU 2026-08-29 -- et c'est la seule raison pour laquelle ca s'est vu :
#    deux captures « des deux themes » etaient PIXEL POUR PIXEL IDENTIQUES,
#    et le releve d'introspection de la course de capture donnait
#    `theme.bgPrimary = 1 4 9` -- ni le sombre (13 17 23), ni le clair
#    (255 255 255), mais une TROISIEME valeur, celle du defaut.
#
#  ⚠️ LE JOURNAL DISAIT LA VERITE ET PERSONNE NE LE LISAIT : la ligne
#     « theme 'GitHub' INCONNU : le defaut est garde » etait ecrite a chaque
#     lancement. Une capture ne lit pas les journaux -- c'est pour ca qu'un
#     instrument qui MESURE ce que le processus a applique (le releve) vaut
#     mieux qu'un oeil sur une image.
#
#  C'est aussi, mot pour mot, la porte « le contenu d'abord, la destination
#  ensuite » : LE TRANSPORT A TOUCHE AU CONTENU. On cite donc soi-meme, avant
#  de confier quoi que ce soit a Start-Process.
# ==============================================================================
if ($AppArgs.Count -gt 0) {
	# Chaque element qui contient un ESPACE est cite ICI, a la main.
	$cites = $AppArgs | ForEach-Object { if ($_ -match ' ') { '"' + $_ + '"' } else { $_ } }
	$proc = Start-Process -FilePath $Exe -ArgumentList $cites -PassThru
} else {
	$proc = Start-Process -FilePath $Exe -PassThru
}

# -- Attente de la fenetre, DU PROCESSUS QU'ON A LANCE (piege 3) --------------
$fin = (Get-Date).AddSeconds($Attente)
$h = [IntPtr]::Zero
while ((Get-Date) -lt $fin) {
	Start-Sleep -Milliseconds 250
	try { $proc.Refresh() } catch { }
	if ($proc.HasExited) {
		Write-Host "[capture] ECHEC : le processus s'est termine (code $($proc.ExitCode))."
		exit 3
	}
	if ($proc.MainWindowHandle -ne [IntPtr]::Zero -and [NkWin]::IsWindowVisible($proc.MainWindowHandle)) {
		$h = $proc.MainWindowHandle
		break
	}
}
if ($h -eq [IntPtr]::Zero) {
	Write-Host "[capture] ECHEC : aucune fenetre visible pour le PID $($proc.Id) en $Attente s."
	try { $proc.Kill() } catch { }
	exit 2
}

Start-Sleep -Milliseconds $Delai
[NkWin]::LeverAvecAlt($h) | Out-Null
Start-Sleep -Milliseconds 700

# -- Le cadre REEL, sans l'ombre du gestionnaire de fenetres -----------------
$r = New-Object NkWin+RECT
$ok = [NkWin]::DwmGetWindowAttribute($h, [NkWin]::DWMWA_EXTENDED_FRAME_BOUNDS, [ref]$r, 16)
if ($ok -ne 0) {
	Write-Host "[capture] ECHEC : DwmGetWindowAttribute a rendu $ok."
	try { $proc.Kill() } catch { }
	exit 4
}
$w = $r.R - $r.L
$hh = $r.B - $r.T
if ($w -le 0 -or $hh -le 0) {
	Write-Host "[capture] ECHEC : cadre degenere ($w x $hh)."
	try { $proc.Kill() } catch { }
	exit 5
}

$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $hh))
$g.Dispose()
$bmp.Save($Sortie, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Host "[capture] OK : $Sortie ($w x $hh), PID $($proc.Id)"
try { $proc.CloseMainWindow() | Out-Null; Start-Sleep -Milliseconds 800 } catch { }
try { if (-not $proc.HasExited) { $proc.Kill() } } catch { }
exit 0
