# Genere l'icone d'application NKCraft -> Resources/nkcraft_icon.png (512 x 512).
#
# Logo PROVISOIRE : l'araignee (choix de Rihen, 29/09/2026), en attendant le
# designer. MEME dessin que Shell/NkModelerBrand.h (painter) et
# data/brand/nkcraft_araignee.svg : repere 100 x 100, place ici avec une marge
# de 8 % sur un carre arrondi sombre. Les couleurs sont figees : une icone
# d'application n'a pas de theme.
#
# Lancer :  powershell -ExecutionPolicy Bypass -File Applications\NKCraft\Resources\gen_icon.ps1
# jenga convertit ensuite ce PNG en .ico / mipmaps / favicon (appicon() dans
# NKCraft.jenga ; il faut Pillow dans le Python de jenga).
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$S = 512
$bmp = New-Object System.Drawing.Bitmap($S, $S)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)
function Col([string]$hex) { [System.Drawing.ColorTranslator]::FromHtml($hex) }
$cFond = Col '#141414'; $cPatte = Col '#F5F5F5'; $cToile = Col '#A2A6AD'
$cCorps = Col '#F2980E'; $cSommet = Col '#1177D1'

# Carre arrondi (rx = 19 % du cote, comme le SVG)
$r = [single]($S * 0.19); $d = 2 * $r
$fondPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$fondPath.AddArc(0, 0, $d, $d, 180, 90); $fondPath.AddArc($S - $d, 0, $d, $d, 270, 90)
$fondPath.AddArc($S - $d, $S - $d, $d, $d, 0, 90); $fondPath.AddArc(0, $S - $d, $d, $d, 90, 90)
$fondPath.CloseFigure()
$g.FillPath((New-Object System.Drawing.SolidBrush($cFond)), $fondPath)

# Repere 100 x 100 -> pixels : marge 8 %, echelle 0,84
$U = $S * 0.84 / 100.0; $O = $S * 0.08
function P([double]$x, [double]$y) { New-Object System.Drawing.PointF([single]($O + $x * $U), [single]($O + $y * $U)) }
function Pen([System.Drawing.Color]$c, [double]$ep) {
  $p = New-Object System.Drawing.Pen($c, [single]($ep * $U))
  $p.StartCap = 'Round'; $p.EndCap = 'Round'; $p.LineJoin = 'Round'; $p
}

$pattes = @(
  @(42,43, 28,26, 16,22), @(40,48, 22,40, 8,44),
  @(40,53, 22,60, 8,58),  @(42,58, 28,74, 16,80)
)
foreach ($cote in 0,1) {
  $X = { param($v) if ($cote -eq 0) { $v } else { 100 - $v } }
  # 1. la toile, entre les bouts de pattes
  $toile = Pen $cToile 1.4
  for ($i = 0; $i -lt 3; $i++) {
    $a = $pattes[$i]; $b = $pattes[$i + 1]
    $g.DrawLine($toile, (P (& $X $a[4]) $a[5]), (P (& $X $b[4]) $b[5]))
  }
  # 2. les pattes, deux segments chacune
  $patte = Pen $cPatte 4
  foreach ($k in $pattes) {
    $pts = [System.Drawing.PointF[]]@((P (& $X $k[0]) $k[1]), (P (& $X $k[2]) $k[3]), (P (& $X $k[4]) $k[5]))
    $g.DrawLines($patte, $pts)
  }
}
# 3. le corps (cube en isometrie) et ses trois aretes visibles
$corps = [System.Drawing.PointF[]]@((P 50 36), (P 62 43), (P 62 57), (P 50 64), (P 38 57), (P 38 43))
$g.FillPolygon((New-Object System.Drawing.SolidBrush($cCorps)), $corps)
$arete = Pen $cFond 3
$g.DrawLines($arete, [System.Drawing.PointF[]]@((P 38 43), (P 50 50), (P 62 43)))
$g.DrawLine($arete, (P 50 50), (P 50 64))
# 4. les sommets, au bout de chaque patte
$bSommet = New-Object System.Drawing.SolidBrush($cSommet)
$rs = 4 * $U
foreach ($cote in 0,1) {
  foreach ($k in $pattes) {
    $vx = if ($cote -eq 0) { $k[4] } else { 100 - $k[4] }
    $c = P $vx $k[5]
    $g.FillEllipse($bSommet, [single]($c.X - $rs), [single]($c.Y - $rs), [single](2 * $rs), [single](2 * $rs))
  }
}

$out = Join-Path $PSScriptRoot 'nkcraft_icon.png'
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "icone ecrite : $out"
