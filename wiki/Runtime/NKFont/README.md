# NKFont — documentation détaillée

Le module **NKFont**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKFont.md](../NKFont.md).

NKFont lit les polices TTF/OTF (et WOFF) sans bibliothèque externe : parser bas-niveau,
atlas multi-tailles, détection automatique de format, rastérisation/SDF/MSDF, extraction de
contours et triangulation des glyphes pour le rendu 2D et 3D.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Font.md](Font.md) | Charger une police TTF/OTF, construire un atlas et sa texture, métriques et mesure de chaînes UTF-8, cache multi-tailles, détection de format, rastérisation/SDF/MSDF, contours et maillages 3D extrudés. Parser bas-niveau `nkfont`. | `NkFont.h`, `Core/NkFontParser.h`, `Core/NkFontTypes.h`, `Core/NkFontSizeCache.h`, `Core/NkFontDetect.h`, `Core/NkUtils.h` |
| [Earcut.md](Earcut.md) | Trianguler un polygone (contour extérieur + trous) par ear-clipping avec `NkEarcut<T>`, conventions de winding (outer CCW / trous CW), indices globaux dans la flat-list, pièges (trous filtrés, garde `maxIter`). | `NkEarcut.h` |

[← Récap NKFont](../NKFont.md) · [← Couche Runtime](../README.md)
