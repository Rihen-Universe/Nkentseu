# Genere l'icone d'app Mou (mascotte Nana sur fond bleu) -> Resources/mou_icon.png
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$S = 512
$bmp = New-Object System.Drawing.Bitmap($S, $S)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)

function Col([int]$r,[int]$gg,[int]$b){ [System.Drawing.Color]::FromArgb(255,$r,$gg,$b) }
function ColA([int]$a,[int]$r,[int]$gg,[int]$b){ [System.Drawing.Color]::FromArgb($a,$r,$gg,$b) }

$INK   = Col 74 55 40
$BLUE  = Col 127 196 245
$BODY  = Col 255 201 60
$ORANGE= Col 255 146 43
$CHEEK = Col 255 143 163
$WHITE = [System.Drawing.Color]::White

# Fond carre arrondi
function RoundRect($x,$y,$w,$h,$r){
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = 2*$r
  $p.AddArc($x, $y, $d, $d, 180, 90)
  $p.AddArc($x+$w-$d, $y, $d, $d, 270, 90)
  $p.AddArc($x+$w-$d, $y+$h-$d, $d, $d, 0, 90)
  $p.AddArc($x, $y+$h-$d, $d, $d, 90, 90)
  $p.CloseFigure()
  return $p
}
$bgPath = RoundRect 0 0 $S $S 112
$g.FillPath((New-Object System.Drawing.SolidBrush($BLUE)), $bgPath)

# Nuages
$cloud = New-Object System.Drawing.SolidBrush (ColA 220 255 255 255)
$g.FillEllipse($cloud, 74, 74, 92, 44)
$g.FillEllipse($cloud, 358, 102, 76, 36)

# Helpers ellipse remplie + contour
function FillO($brush,$cx,$cy,$rx,$ry){ $g.FillEllipse($brush, $cx-$rx, $cy-$ry, 2*$rx, 2*$ry) }
function DrawO($pen,$cx,$cy,$rx,$ry){ $g.DrawEllipse($pen, $cx-$rx, $cy-$ry, 2*$rx, 2*$ry) }

$bodyB  = New-Object System.Drawing.SolidBrush($BODY)
$orB    = New-Object System.Drawing.SolidBrush($ORANGE)
$cheekB = New-Object System.Drawing.SolidBrush($CHEEK)
$whiteB = New-Object System.Drawing.SolidBrush($WHITE)
$inkB   = New-Object System.Drawing.SolidBrush($INK)

$penBody = New-Object System.Drawing.Pen($INK, 12.4); $penBody.LineJoin=[System.Drawing.Drawing2D.LineJoin]::Round
$penEye  = New-Object System.Drawing.Pen($INK, 7.8)
$penArm  = New-Object System.Drawing.Pen($INK, 15.5); $penArm.StartCap=[System.Drawing.Drawing2D.LineCap]::Round; $penArm.EndCap=[System.Drawing.Drawing2D.LineCap]::Round
$penSmile= New-Object System.Drawing.Pen($INK, 9.3);  $penSmile.StartCap=[System.Drawing.Drawing2D.LineCap]::Round; $penSmile.EndCap=[System.Drawing.Drawing2D.LineCap]::Round

# Pieds
FillO $orB 213 375 34 19;  DrawO $penBody 213 375 34 19
FillO $orB 300 375 34 19;  DrawO $penBody 300 375 34 19
# Bras (courbes)
$g.DrawCurve($penArm, ([System.Drawing.PointF[]]@(
  (New-Object System.Drawing.PointF(139,240)),(New-Object System.Drawing.PointF(120,255)),(New-Object System.Drawing.PointF(108,275)))))
$g.DrawCurve($penArm, ([System.Drawing.PointF[]]@(
  (New-Object System.Drawing.PointF(374,240)),(New-Object System.Drawing.PointF(393,255)),(New-Object System.Drawing.PointF(405,275)))))
# Corps
FillO $bodyB 256 233 121 121; DrawO $penBody 256 233 121 121
# Joues
FillO $cheekB 188 256 23 23
FillO $cheekB 325 256 23 23
# Yeux
FillO $whiteB 219 213 28 28; DrawO $penEye 219 213 28 28
FillO $whiteB 294 213 28 28; DrawO $penEye 294 213 28 28
FillO $inkB 225 217 12 12
FillO $inkB 300 217 12 12
FillO $whiteB 230 211 5 5
FillO $whiteB 305 211 5 5
# Sourire (bezier)
$g.DrawBezier($penSmile, 219,272, 240,309, 272,309, 294,272)
# Houppette
$tuft = New-Object System.Drawing.Drawing2D.GraphicsPath
$tuft.AddPolygon([System.Drawing.PointF[]]@(
  (New-Object System.Drawing.PointF(256,112)),(New-Object System.Drawing.PointF(244,76)),
  (New-Object System.Drawing.PointF(275,82)),(New-Object System.Drawing.PointF(266,108))))
$g.FillPath($orB, $tuft); $g.DrawPath($penBody, $tuft)

# Bordure par-dessus
$borderPath = RoundRect 14 14 484 484 100
$penBorder = New-Object System.Drawing.Pen($INK, 14)
$g.DrawPath($penBorder, $borderPath)

$out = Join-Path $PSScriptRoot 'mou_icon.png'
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output ("OK -> " + $out)
