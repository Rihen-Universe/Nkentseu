# Genere les icones d'application NKCraft (512 x 512), en DEUX versions :
#   Resources/nkcraft_icon.png        version SOMBRE -- celle que porte l'exe
#                                     (appicon() dans NKCraft.jenga)
#   Resources/nkcraft_icon_clair.png  version CLAIRE -- pour les supports clairs
#                                     (site, documents, fonds clairs)
#
# Logo PROVISOIRE : l'araignee (choix de Rihen, 29/09/2026), en attendant le
# designer. MEME dessin que Shell/NkModelerBrand.h (painter) et
# data/brand/nkcraft_araignee_{sombre,clair}.svg : repere 100 x 100, place ici
# avec une marge de 8 % sur un carre arrondi. Les couleurs sont celles des deux
# themes de la famille (UE5 Rihen) ; figees ici, car une icone n'a pas de theme.
#
# Lancer :  powershell -ExecutionPolicy Bypass -File Applications\NKCraft\Resources\gen_icon.ps1
# jenga convertit ensuite nkcraft_icon.png en .ico / mipmaps / favicon ; il lui
# faut Pillow dans le Python de jenga.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
function Col([string]$hex) { [System.Drawing.ColorTranslator]::FromHtml($hex) }

# ⚠ PowerShell ne distingue pas la casse des variables : $toile et $TOILE sont
#   la MEME variable. D'ou les prefixes (cFond, pToile...) au lieu de majuscules.
$variantes = @(
  @{ Nom = 'sombre'; Fichier = 'nkcraft_icon.png';
     cFond = '#141414'; cPatte = '#F5F5F5'; cToile = '#A2A6AD'; cCorps = '#F2980E'; cSommet = '#1177D1' },
  @{ Nom = 'clair';  Fichier = 'nkcraft_icon_clair.png';
     cFond = '#F5F5F5'; cPatte = '#141414'; cToile = '#5A5E66'; cCorps = '#C97A08'; cSommet = '#0E5FA6' }
)

$pattes = @(
  @(42,43, 28,26, 16,22), @(40,48, 22,40, 8,44),
  @(40,53, 22,60, 8,58),  @(42,58, 28,74, 16,80)
)

function Dessiner($v) {
  $S = 512
  $bmp = New-Object System.Drawing.Bitmap($S, $S)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.Clear([System.Drawing.Color]::Transparent)
  $cFond = Col $v.cFond; $cPatte = Col $v.cPatte; $cToile = Col $v.cToile
  $cCorps = Col $v.cCorps; $cSommet = Col $v.cSommet

  # Carre arrondi (rx = 19 % du cote, comme le SVG)
  $r = [single]($S * 0.19); $d = 2 * $r
  $fondPath = New-Object System.Drawing.Drawing2D.GraphicsPath
  $fondPath.AddArc(0, 0, $d, $d, 180, 90); $fondPath.AddArc($S - $d, 0, $d, $d, 270, 90)
  $fondPath.AddArc($S - $d, $S - $d, $d, $d, 0, 90); $fondPath.AddArc(0, $S - $d, $d, $d, 90, 90)
  $fondPath.CloseFigure()
  $g.FillPath((New-Object System.Drawing.SolidBrush($cFond)), $fondPath)

  # Repere 100 x 100 -> pixels : marge 8 %, echelle 0,84
  $U = $S * 0.84 / 100.0; $O = $S * 0.08
  $P = { param([double]$x, [double]$y) New-Object System.Drawing.PointF([single]($O + $x * $U), [single]($O + $y * $U)) }
  $Stylo = { param([System.Drawing.Color]$c, [double]$ep)
    $s = New-Object System.Drawing.Pen($c, [single]($ep * $U))
    $s.StartCap = 'Round'; $s.EndCap = 'Round'; $s.LineJoin = 'Round'; $s }

  $pToile = & $Stylo $cToile 1.4
  $pPatte = & $Stylo $cPatte 4
  foreach ($cote in 0,1) {
    $X = { param($val) if ($cote -eq 0) { $val } else { 100 - $val } }
    # 1. la toile, entre les bouts de pattes
    for ($i = 0; $i -lt 3; $i++) {
      $a = $pattes[$i]; $b = $pattes[$i + 1]
      $g.DrawLine($pToile, (& $P (& $X $a[4]) $a[5]), (& $P (& $X $b[4]) $b[5]))
    }
    # 2. les pattes, deux segments chacune
    foreach ($k in $pattes) {
      $pts = [System.Drawing.PointF[]]@((& $P (& $X $k[0]) $k[1]), (& $P (& $X $k[2]) $k[3]), (& $P (& $X $k[4]) $k[5]))
      $g.DrawLines($pPatte, $pts)
    }
  }
  # 3. le corps (cube en isometrie) et ses trois aretes visibles, couleur du fond
  $corps = [System.Drawing.PointF[]]@((& $P 50 36), (& $P 62 43), (& $P 62 57), (& $P 50 64), (& $P 38 57), (& $P 38 43))
  $g.FillPolygon((New-Object System.Drawing.SolidBrush($cCorps)), $corps)
  $pArete = & $Stylo $cFond 3
  $g.DrawLines($pArete, [System.Drawing.PointF[]]@((& $P 38 43), (& $P 50 50), (& $P 62 43)))
  $g.DrawLine($pArete, (& $P 50 50), (& $P 50 64))
  # 4. les sommets, au bout de chaque patte
  $bSommet = New-Object System.Drawing.SolidBrush($cSommet)
  $rs = 4 * $U
  foreach ($cote in 0,1) {
    foreach ($k in $pattes) {
      $vx = if ($cote -eq 0) { $k[4] } else { 100 - $k[4] }
      $pt = & $P $vx $k[5]
      $g.FillEllipse($bSommet, [single]($pt.X - $rs), [single]($pt.Y - $rs), [single](2 * $rs), [single](2 * $rs))
    }
  }

  $out = Join-Path $PSScriptRoot $v.Fichier
  $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Write-Output ("icone {0} ecrite : {1}" -f $v.Nom, $out)
}

foreach ($v in $variantes) { Dessiner $v }
