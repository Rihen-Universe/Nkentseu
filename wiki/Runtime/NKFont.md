# NKFont

> Couche **Runtime** · Polices TTF/OTF lues from-scratch : parsing bas-niveau, types,
> atlas multi-tailles avec cache, détection automatique de format, rastérisation/SDF,
> extraction de contours et triangulation (earcut) pour le rendu de glyphes 2D et 3D.

Dès qu'il faut **afficher du texte**, c'est NKFont qui transforme un fichier de police
brut en quelque chose de dessinable : une texture d'atlas remplie de glyphes, des
métriques de positionnement, des contours vectoriels, voire des maillages 3D extrudés.
Le module ne dépend d'aucune bibliothèque externe (pas de FreeType) : le parser TTF/OTF/WOFF,
le rastériseur, le générateur SDF/MSDF et la triangulation sont tous écrits à la main.

L'idiome central est l'**atlas** : on empile une ou plusieurs polices à des tailles données
dans un `NkFontAtlas`, on appelle `Build()`, et on récupère une texture unique plus les
métriques de chaque glyphe. Le parser bas-niveau (`nkfont`) reste accessible pour les usages
avancés (extraction de contours, rastérisation manuelle, kerning), et `NkEarcut` triangule
ces contours quand on veut du texte géométrique plutôt que texturé.

- **Namespaces** : `nkentseu` (API haut-niveau atlas/font, détection, cache) et
  `nkentseu::nkfont` (parser bas-niveau TTF/OTF/WOFF)
- **Header parapluie** : `#include "NKFont/NkFont.h"`
- **Version** : V2.1 (SDF, WOFF/OTF-CFF, extraction de contours, mesh 3D extrudé)

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Charger une police TTF/OTF et obtenir une texture d'atlas | [Police](NKFont/Font.md) |
| Récupérer les métriques d'un glyphe / mesurer une chaîne UTF-8 | [Police](NKFont/Font.md) |
| Gérer plusieurs tailles d'une même police (cache, rebuild) | [Police](NKFont/Font.md) |
| Détecter automatiquement le type de police (bitmap / vectoriel) | [Police](NKFont/Font.md) |
| Rastériser un glyphe ou générer un SDF/MSDF à la main | [Police](NKFont/Font.md) |
| Extraire les contours d'un glyphe ou un maillage 3D extrudé | [Police](NKFont/Font.md) |
| Transformer des contours de glyphe en triangles (rendu géométrique) | [Triangulation (earcut)](NKFont/Earcut.md) |

---

## Aperçu des familles

- **API haut-niveau atlas** (`NkFont.h`, namespace `nkentseu`) — `NkFontAtlas` empile N
  polices/tailles dans une texture unique (`Build` → `GetTexDataAsAlpha8/RGBA32`), `NkFont`
  expose les glyphes rastérisés, leurs métriques (`FindGlyph`, `GetCharAdvance`,
  `CalcTextSizeX`), l'extraction de contours (`GetGlyphOutlinePoints`) et l'extrusion 3D
  (`GenerateGlyphMesh3D`, `GenerateTextMesh3D`, callbacks `ForEach…3DVertex`). Plages de
  glyphes prédéfinies, rects custom, plus deux fragment shaders GLSL prêts (`kNkFontFragNormal`,
  `kNkFontFragSDF`). **Ni `NkFontAtlas` ni `NkFont` n'implémentent `NKIResource`** (doctrine
  atlas-centric) ; `NkFont` est toujours détenu par son `containerAtlas`.
- **Types de base** (`NkFontTypes.h`) — alias primitifs `nkft_*`, `NkFontCodepoint`,
  `NkGlyphId`, alias rect (`NkRecti`/`NkRectf`), constantes sentinelles
  (`NKFONT_INVALID_GLYPH_ID`, `NKFONT_CODEPOINT_REPLACEMENT`) et macros gardées
  (`NK_FONT_ASSERT`, `NK_FONT_LIKELY`…).
- **Parser bas-niveau** (`NkFontParser.h`, namespace `nkfont`) — lecteurs big-endian,
  `NkFontFaceInfo` (état parsé d'une face), décodage de contours (`NkGetGlyphShape`),
  métriques (`NkGetGlyphHMetrics`, kerning), rastériseur (`NkMakeGlyphBitmap`) et SDF
  (`NkMakeSDFFromBitmap`). Formats : TTF simple/composite, TTC, OTF glyf, OTF/CFF Type 2,
  WOFF (zlib). **WOFF2 non supporté**. Règle Create/Destroy : `NkInitFontFace` +
  `NkFreeFontFace`.
- **Cache de tailles** (`NkFontSizeCache.h`) — `NkFontSizeCache` gère N tailles d'une police
  sur un atlas avec rebuild à la demande (`GetFont` / `BuildAtlasIfNeeded` / `NeedsGpuUpload`),
  `NkFontScaleRenderer` affiche à taille variable sans rebuild (scale par matrice).
- **Détection de format** (`NkFontDetect.h`) — `NkFontDetector` analyse un échantillon de
  glyphes (`Analyze`/`AnalyzeFile`) pour classer la police (`NkFontKind` bitmap/vectoriel/CFF)
  et produire un `NkFontConfig` optimal (`ApplyOptimalConfig`, `RecommendsNearestFilter`,
  `SnapSize`).
- **Utilitaires raster/SDF/MSDF** (`NkUtils.h`) — `NkRasterizer` (coverage AA),
  `NkMSDFGenerator`, `NkDilation`, conversion glyphe → `NkShape` (`NkBuildShapeFromGlyph`).
  ⚠ **Anomalie zéro-STL** : ce header inclut `<vector>` (`NkShape::edges`) ; à considérer
  comme non conforme au reste du module.
- **Triangulation** (`NkEarcut.h`) — `NkEarcut<T>`, fonction libre header-only qui triangule
  un polygone (contour extérieur + trous) par ear-clipping, conçue pour transformer les
  contours d'un glyphe en triangles.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkFont.h` | Parapluie + API haut-niveau (`NkFontAtlas`, `NkFont`, mesh 3D, shaders GLSL). | [Police](NKFont/Font.md) |
| `Core/NkFontTypes.h` | Alias `nkft_*`, `NkFontCodepoint`, `NkGlyphId`, constantes, macros. | [Police](NKFont/Font.md) |
| `Core/NkFontParser.h` | Parser bas-niveau TTF/OTF/WOFF (`nkfont`), rastériseur, SDF. | [Police](NKFont/Font.md) |
| `Core/NkFontSizeCache.h` | `NkFontSizeCache`, `NkFontScaleRenderer`. | [Police](NKFont/Font.md) |
| `Core/NkFontDetect.h` | `NkFontDetector`, `NkFontProfile`, `NkFontKind`. | [Police](NKFont/Font.md) |
| `Core/NkUtils.h` | `NkRasterizer`, `NkMSDFGenerator`, `NkDilation` (⚠ utilise `std::vector`). | [Police](NKFont/Font.md) |
| `NkEarcut.h` | `NkEarcut<T>` (triangulation header-only). | [Triangulation (earcut)](NKFont/Earcut.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
