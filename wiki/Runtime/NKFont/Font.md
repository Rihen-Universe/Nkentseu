# Le texte : de la police aux pixels

> Couche **Runtime** · NKFont · Transformer un fichier de police (TTF, OTF, WOFF) en
> **glyphes rastérisés** rangés dans un atlas GPU — du parser bas niveau `NkFontFaceInfo`
> jusqu'au `NkFontAtlas` haut niveau, en passant par le SDF, le cache multi-tailles et même
> le **texte 3D extrudé**.

Afficher du texte est l'un de ces problèmes qui semblent triviaux jusqu'à ce qu'on les
regarde de près. Un fichier `.ttf` ne contient pas des images de lettres : il contient des
**contours vectoriels** (des courbes de Bézier), des tables de métriques, une *cmap* qui
relie chaque caractère Unicode à un glyphe. Pour dessiner « Bonjour » à l'écran, il faut
décoder ces contours, les **rastériser** à la bonne taille, les empaqueter dans une texture,
et calculer pour chaque lettre son quad et ses coordonnées UV. NKFont fait tout cela, *à
partir de zéro* — aucun FreeType, aucune dépendance externe.

Le module s'organise en **deux étages**. En haut, l'API « atlas-centric » (`NkFontAtlas`,
`NkFont`, `NkFontGlyph`) : on empile des polices, on appelle `Build()`, on récupère une
texture prête à uploader. En bas, le **parser** (`nkentseu::nkfont`) qui lit réellement les
octets du fichier, décode les contours, et rastérise un glyphe isolé. La plupart des
applications ne touchent que l'étage haut ; les outils, les générateurs d'atlas et le texte
3D descendent à l'étage bas.

- **Namespaces** : `nkentseu` (API haut niveau, cache, détection) et `nkentseu::nkfont`
  (parser TTF/OTF/WOFF)
- **Header parapluie** : `#include "NKFont/NkFont.h"`
- **Headers** : `NKFont/Core/NkFontTypes.h`, `Core/NkFontParser.h`, `Core/NkFontSizeCache.h`,
  `Core/NkFontDetect.h`, `Core/NkUtils.h`

---

## L'atlas, conteneur central : `NkFontAtlas`

Le cœur du module est `NkFontAtlas`. L'idée tient en une image : plutôt que de garder une
texture par lettre (catastrophe pour le GPU, qui déteste changer de texture à chaque
caractère), on **empaquète toutes les lettres de toutes les polices dans une seule grande
texture**. C'est le mot *atlas* : une planche unique où chaque glyphe occupe un petit
rectangle, repéré par ses coordonnées UV.

Le cycle de vie est toujours le même, et il faut le mémoriser : on **empile** N polices avec
`AddFontFromFile` / `AddFontFromMemory`, on appelle **`Build()`** une fois (qui rastérise tout
et range les glyphes dans la texture), puis on **récupère les pixels** avec
`GetTexDataAsAlpha8` ou `GetTexDataAsRGBA32` pour les uploader sur le GPU et noter l'identifiant
de texture dans `texID`.

```cpp
NkFontAtlas atlas;
atlas.AddFontFromFile("Roboto.ttf", 24.f);     // empile, ne rastérise pas encore
atlas.AddFontFromFile("Emoji.ttf", 24.f);      // une 2e police dans le même atlas
atlas.Build();                                 // ICI tout est rastérisé

nkft_uint8* pixels; nkft_int32 w, h;
atlas.GetTexDataAsAlpha8(&pixels, &w, &h);     // 1 octet/px, prêt pour l'upload
atlas.texID = monGPU.UploadAlpha(pixels, w, h);
```

Ce n'est **pas** un objet qu'on copie : `NkFontAtlas` interdit explicitement la copie et
l'assignation (`= delete`). Un atlas détient ses polices, ses buffers de données, sa texture —
le dupliquer n'aurait pas de sens. On le manipule **par pointeur ou référence**. Et attention,
ce n'est **pas** non plus un `NKIResource` : le module documente une doctrine *multi-resource*
où l'atlas regroupe plusieurs polices, ce qui ne colle pas au modèle « une ressource = un
fichier » de `NKIResource`.

Deux réglages comptent dès le départ. `texGlyphPadding` (défaut 2) sépare les glyphes dans la
texture : sans marge, le filtrage bilinéaire « saigne » d'un glyphe sur le voisin (*bleeding*)
— gardez-le ≥ 2. Et le **mode SDF** (`sdfMode = true`, `sdfSpread` = rayon en pixels) change
toute la nature de l'atlas, on y revient plus bas.

> **En résumé.** `NkFontAtlas` empaquète toutes les polices dans **une seule texture**.
> Idiome immuable : `AddFontFrom*` (×N) → `Build()` → `GetTexDataAsAlpha8/RGBA32` → upload GPU
> → `texID`. **Non copiable**, manipuler par pointeur. Ce n'est pas un `NKIResource`. Réglez
> `texGlyphPadding` ≥ 2 contre le bleeding.

---

## La police rastérisée : `NkFont` et `NkFontGlyph`

Chaque appel à `AddFontFrom*` renvoie un `NkFont*` — une police **à une taille donnée**,
détenue par l'atlas. C'est un point crucial : un `NkFont` ne se charge **pas seul**, il n'a
pas de constructeur de fichier, il pointe vers son `containerAtlas` et n'existe que parce que
l'atlas l'a créé. Demander « Roboto en 24 px » et « Roboto en 48 px » donne **deux** `NkFont`
distincts dans le même atlas.

Une fois l'atlas construit, le travail de tous les jours est de **trouver le glyphe d'un
caractère** et de **mesurer du texte**. `FindGlyph(codepoint)` renvoie le `NkFontGlyph`
correspondant (ou le *fallback* si le caractère est absent) ; `FindGlyphNoFallback` ne ment
pas et renvoie `nullptr` si le glyphe manque vraiment. Un `NkFontGlyph` porte tout ce qu'il
faut pour dessiner : le quad en pixels écran (`x0,y0,x1,y1`, relatif au curseur), les UV dans
l'atlas (`u0,v0,u1,v1`), et l'`advanceX` (de combien avancer le curseur après cette lettre).

```cpp
NkFont* font = atlas.AddFontFromFile("Roboto.ttf", 24.f);
atlas.Build();

float cursorX = 0.f;
const char* s = "Salut";
while (*s) {
    NkFontCodepoint cp = NkFont::DecodeUTF8(&s, nullptr);   // décode + avance
    const NkFontGlyph* g = font->FindGlyph(cp);
    DessinerQuad(cursorX + g->x0, g->y0, g->u0, g->v0, ...); // un quad texturé
    cursorX += g->advanceX;                                  // curseur suivant
}
```

Pour mesurer sans dessiner, `CalcTextSizeX(text)` donne la largeur d'une chaîne UTF-8 en un
seul passage (`O(n)`) — indispensable pour centrer un titre, dimensionner un bouton, ou faire
un retour à la ligne. Notez que tout est **UTF-8** : `DecodeUTF8` (méthode statique) ou les
fonctions libres `NkFontDecodeUTF8` / `NkFontEncodeUTF8` font la conversion codepoint ↔ octets.

> **En résumé.** `NkFont` = une police à **une taille**, détenue par l'atlas (jamais
> autonome). `FindGlyph` (avec fallback) ou `FindGlyphNoFallback` donne le `NkFontGlyph` (quad
> écran + UV atlas + advance). `CalcTextSizeX` mesure en `O(n)`. Tout est UTF-8 via `DecodeUTF8`.

---

## Configurer l'ajout d'une police : `NkFontConfig`

Le troisième argument de `AddFontFrom*` est un `NkFontConfig*` optionnel — la boîte à
réglages de *comment* la police entre dans l'atlas. Sans lui, on a des valeurs par défaut
raisonnables ; avec lui, on contrôle finement le rendu.

Deux familles de réglages dominent. D'abord la **qualité de rastérisation** :
`oversampleH`/`oversampleV` rastérisent à une résolution supérieure puis réduisent (du
super-sampling, qui adoucit les bords — 2×1 par défaut), et `pixelSnapH` aligne sur la grille
pixel pour la netteté. Ensuite la **sélection des caractères** : `glyphRanges` est un tableau
de plages Unicode à inclure (par défaut seul l'ASCII de base), et `glyphFallback` (défaut
`?`) désigne le caractère affiché quand un codepoint manque.

```cpp
NkFontConfig cfg;
cfg.oversampleH = 3;                              // bords plus doux
cfg.glyphRanges = NkFontAtlas::GetGlyphRangesCyrillic();
atlas.AddFontFromFile("DejaVu.ttf", 18.f, &cfg);
```

Le réglage le plus subtil est `mergeMode`. À `true`, la police n'est **pas** ajoutée comme une
nouvelle entrée mais **fusionnée dans la précédente** : ses glyphes viennent compléter ceux
qui manquaient. C'est exactement ce qu'il faut pour ajouter une police d'emojis ou un jeu de
caractères CJK *par-dessus* une police latine, sous un seul `NkFont` logique.

Reste la question délicate de **qui possède le buffer de données**. `fontData` /
`fontDataSize` / `fontDataOwned` règlent l'ownership. La règle suit les trois portes d'entrée :
`AddFontFromFile` charge et possède ; `AddFontFromMemory` ne **possède pas** (l'appelant garde
le buffer vivant au moins jusqu'au `Build`) ; `AddFontFromMemoryOwned` **possède** (l'atlas
libérera). Mélanger les deux derniers est le piège classique de double-libération.

> **En résumé.** `NkFontConfig` règle la rastérisation (`oversample*`, `pixelSnapH`) et la
> sélection de glyphes (`glyphRanges`, `glyphFallback`). `mergeMode = true` empile une police
> **sur** la précédente (emojis/CJK sur du latin). Attention à l'ownership :
> `AddFontFromMemory` ne possède pas, `…Owned` possède — ne double-libérez pas.

---

## Le SDF : du texte net à toutes les tailles

Un atlas classique rastérise chaque lettre à une taille fixe. Agrandir la texture donne du
flou, réduire donne de l'aliasing — on est prisonnier de la taille rastérisée. Le **Signed
Distance Field** (SDF) résout cela : au lieu de stocker l'opacité du glyphe, chaque texel
encode la **distance au contour** le plus proche. Le shader reconstruit ensuite un bord net
*à n'importe quelle échelle*, à partir d'une seule rastérisation.

On l'active sur l'atlas avant `Build` : `sdfMode = true`, et `sdfSpread` (4 à 8 px) fixe le
rayon sur lequel la distance est encodée. La convention de sortie est simple : valeur > 127 =
intérieur, = 127 = sur le contour, < 127 = extérieur. Côté GPU, le module fournit même les
deux shaders prêts à l'emploi : `kNkFontFragNormal` (rendu classique, lit le canal alpha) et
`kNkFontFragSDF` (lit le canal rouge, applique un `smoothstep(0.5 - s, 0.5 + s, dist)`).

```cpp
atlas.sdfMode = true;
atlas.sdfSpread = 6;
atlas.AddFontFromFile("Roboto.ttf", 32.f);
atlas.Build();                          // texture SDF : nette à toute taille
// shader = kNkFontFragSDF, uSmoothing ≈ 0.1 / fontSize
```

C'est l'outil des titres qui zooment, des étiquettes 3D vues de près comme de loin, des UI où
une seule police sert à toutes les tailles. Le coût : la génération SDF est plus lourde au
`Build`, et le rendu suppose le bon shader.

> **En résumé.** `sdfMode = true` + `sdfSpread` (4-8 px) produit un atlas de **distances**, net
> à **toute échelle** depuis une seule rastérisation. Convention : >127 intérieur, =127 contour.
> Shaders fournis : `kNkFontFragNormal` (alpha) et `kNkFontFragSDF` (`smoothstep`).

---

## Le texte en relief : extrusion 3D

NKFont ne s'arrête pas aux pixels plats. À partir des contours vectoriels d'un glyphe, il sait
construire un **maillage 3D extrudé** — une lettre épaisse avec une face avant, une face
arrière et des faces latérales, prête à être éclairée et tournée dans une scène 3D. C'est ce
qui permet un logo en volume, un titre cinématique, du texte flottant dans un monde.

`GenerateGlyphMesh3D` produit le mesh d'un caractère, `GenerateTextMesh3D` celui d'une chaîne
entière (les caractères sont concaténés, la matrice translatée pour chacun). On passe l'échelle,
la profondeur d'extrusion, une matrice monde et une couleur, et on récupère un
`NkFontGlyphMesh3D` (sommets `NkFontGlyph3DVertex` + indices + métriques).

```cpp
NkFontGlyphMesh3D mesh = font->GenerateTextMesh3D(
    "NKENTSEU", /*scale*/0.01f, /*depth*/0.2f, worldMatrix, NkVec4f{1,1,1,1});
monGPU.Draw(mesh.vertices.Data(), mesh.vertices.Size());
```

Pour les cas sans allocation, les variantes `ForEachGlyph3DVertex` / `ForEachText3DVertex`
**streament** les sommets vers un callback (`NkFont3DVertexCallback`) au lieu de bâtir un
`NkVector`. Idéal quand on remplit directement un buffer GPU mappé, sans tampon intermédiaire.

> **En résumé.** `GenerateGlyphMesh3D` / `GenerateTextMesh3D` extrudent un glyphe ou une chaîne
> en **maillage 3D** (face avant/arrière/latérale). Les variantes `ForEach…3DVertex` streament
> les sommets vers un callback, sans allocation. Pour logos, titres en volume, texte dans une scène.

---

## Choisir et gérer les tailles : détection et cache

Deux outils complètent le tableau. D'abord la **détection automatique** (`NkFontDetector`) :
toutes les polices ne se traitent pas pareil. Une police *bitmap* (ProggyClean, Terminus) a une
taille native et doit être affichée nette au pixel près ; une police *vectorielle* (Roboto)
supporte n'importe quelle taille. `NkFontDetector::Analyze` examine un échantillon de glyphes,
distingue les courbes des segments, et renvoie un `NkFontProfile` ; `ApplyOptimalConfig` en
déduit un `NkFontConfig` réglé pour cette police.

```cpp
NkFontProfile profile = NkFontDetector::AnalyzeFile("Terminus.ttf");
NkFontConfig cfg;
NkFontDetector::ApplyOptimalConfig(profile, 16.f, &cfg);   // règle oversample, snap, filtre
atlas.AddFontFromFile("Terminus.ttf", 16.f, &cfg);
```

Ensuite le **cache multi-tailles** (`NkFontSizeCache`) : une UI demande la même police à des
tailles variées et changeantes. Le cache gère jusqu'à 16 tailles sur un même atlas, avec
éviction LRU et rebuild à la demande. Mais attention au piège central : quand on demande une
taille **absente**, `GetFont` l'ajoute, marque un rebuild, et **retourne `nullptr` jusqu'au
prochain appel** après `BuildAtlasIfNeeded()`. Le rebuild invalide toute la texture GPU — il
faut donc re-uploader (`NeedsGpuUpload()` / `ClearGpuUploadFlag()`).

L'alternative légère est `NkFontScaleRenderer` : pas de rebuild du tout, on rastérise à une
taille de référence puis on **scale par matrice** (net en réduction, flou en agrandissement).

> **En résumé.** `NkFontDetector` analyse une police (bitmap vs vectorielle) et règle un
> `NkFontConfig` optimal. `NkFontSizeCache` gère 16 tailles avec LRU + rebuild — piège :
> `GetFont` d'une taille neuve renvoie `nullptr` jusqu'au `BuildAtlasIfNeeded()` suivant, et il
> faut re-uploader. `NkFontScaleRenderer` = même police scalée par matrice, sans rebuild.

---

## Aperçu de l'API

Tous les éléments publics du module, par fichier. Les complexités sont indiquées entre crochets
quand elles sont déductibles ; aucun `noexcept`/`nodiscard` n'est déclaré dans ces headers.

### Types de base (`NkFontTypes.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias primitifs | `nkft_uint8/16/32/64`, `nkft_int8/16/32/64`, `nkft_float32/64`, `nkft_bool`, `nkft_size` | Alias directs des types `nkentseu::*`. |
| Alias publics | `NkFontCodepoint`, `NkGlyphId` | Codepoint Unicode / index de glyphe (`uint32`). |
| Alias math | `NkRecti`, `NkRectf` | `math::NkIntRect` / `NkFloatRect`. |
| Constantes | `NKFONT_INVALID_GLYPH_ID`, `NKFONT_CODEPOINT_REPLACEMENT`, `NKFONT_CODEPOINT_MAX` | Sentinelle glyphe / U+FFFD / max Unicode. |
| Macros | `NK_FONT_UNUSED`, `NK_FONT_ASSERT`, `NK_FONT_LIKELY`, `NK_FONT_UNLIKELY` | Anti-warning, assertion, hints de branchement (overridables). |

### API haut niveau (`NkFont.h`, namespace `nkentseu`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Glyphe | `struct NkFontGlyph` | Quad écran (`x0..y1`) + UV atlas (`u0..v1`) + `advanceX` + `visible`. |
| Config | `struct NkFontConfig` | Réglages d'ajout : data/ownership, `sizePixels`, `oversample*`, `glyphRanges`, `mergeMode`… |
| Rect custom | `struct NkFontAtlasCustomRect` | Rectangle réservé dans l'atlas (`width/height/x/y`, UV, `isPacked`). |
| Atlas | `struct NkFontAtlas` | Conteneur central (non copiable). |
| Atlas / ajout | `AddFontFromFile`, `AddFontFromMemory`, `AddFontFromMemoryOwned` | Empile une police → `NkFont*` (`nullptr` si plein). |
| Atlas / rects | `AddCustomRect`, `GetCustomRect` | Réserve / relit un rect custom. |
| Atlas / plages | `GetGlyphRangesDefault/LatinExtA/Cyrillic/Greek/ChineseFull` | Plages Unicode prédéfinies (statics). |
| Atlas / build | `Build`, `GetTexDataAsAlpha8`, `GetTexDataAsRGBA32` | Rastérise tout / récupère les pixels (alpha8 / RGBA32). |
| Atlas / état | `ClearTexData`, `Clear`, `IsBuilt` | Libère pixels CPU / réinitialise / construit ? |
| Atlas / constantes | `NK_FONT_ATLAS_MAX_FONTS` (16), `NK_FONT_ATLAS_MAX_CUSTOM_RECTS` (64) | Capacités fixes. |
| Police | `struct NkFont` | Police à une taille, détenue par l'atlas. |
| Police / recherche | `FindGlyph`, `FindGlyphNoFallback`, `GetCharAdvance`, `CalcTextSizeX` `[O(n)]` | Glyphe (avec/sans fallback), advance, largeur de chaîne. |
| Police / UTF-8 | `DecodeUTF8` (static) | Décode un codepoint et avance le pointeur. |
| Police / contours | `GetGlyphOutlinePoints` | Contours aplatis (segments, tolérance 0.35 px). |
| Police / table | `BuildLookupTable`, `AddGlyph`, `AddGlyphSorted`, `FindGlyphIndex` | Construction interne de la table de lookup. |
| Police / 3D | `GenerateGlyphMesh3D`, `GenerateTextMesh3D`, `ForEachGlyph3DVertex`, `ForEachText3DVertex` | Mesh 3D extrudé / streaming vers callback. |
| Mesh 3D | `struct NkFontOutlineVertex`, `NkFontGlyph3DVertex`, `NkFontGlyphMesh3D` | Sommet de contour / sommet 3D / mesh complet. |
| Mesh 3D | `NkFont3DVertexCallback`, `NK_FONT_MAX_GLYPHS` (4096), `NK_FONT_INDEX_SIZE` (256) | Callback streaming + constantes. |
| UTF-8 libre | `NkFontDecodeUTF8`, `NkFontEncodeUTF8` | Décode (avance ptr) / encode (nb octets). |
| Shaders | `kNkFontFragNormal`, `kNkFontFragSDF` | Fragment shaders GLSL prêts (alpha / SDF). |

### Parser bas niveau (`NkFontParser.h`, namespace `nkentseu::nkfont`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Lecteurs BE | `NkReadU8/U16/U32`, `NkReadI16/I32` | Lecture big-endian (inline). |
| Vue mémoire | `struct NkFontDataSpan` (`IsValid`, `At`) | Vue bornée sur un buffer. |
| Face | `struct NkFontFaceInfo` | État parsé d'une face (offsets tables, métriques, cmap, WOFF). |
| Sommet | `struct NkFontVertex` | Sommet de contour brut (line/curve/cubic). |
| Sommet | `struct NkFontVertexBuffer` (`Push` `[O(1)]`, `Clear`) | Buffer fixe de 2048 sommets. |
| Constantes | `NK_FONT_VERTEX_MOVE/LINE/CURVE/CUBIC`, `NK_FONT_MAX_VERTICES` (2048) | Types de sommet + capacité. |
| Cycle de vie | `NkInitFontFace`, `NkFreeFontFace` | Init depuis buffer (WOFF auto) / libère (Create/Destroy). |
| Glyphes | `NkFindGlyphIndex`, `NkGetGlyphHMetrics`, `NkGetGlyphBox`, `NkGetGlyphKernAdvance`, `NkGetGlyphShape` | cmap, métriques h, bbox, kerning, contours. |
| Police | `NkScaleForPixelHeight`, `NkScaleForEmToPixels`, `NkGetFontVMetrics`, `NkGetFontName` | Échelles, métriques v, table name. |
| Raster | `NkGetGlyphBitmapBox`, `NkMakeGlyphBitmap` | Bbox bitmap / rastérise un glyphe (alpha 8 bits). |
| SDF | `NkMakeSDFFromBitmap` | SDF depuis bitmap alpha (spread, défaut 6). |

### Cache (`NkFontSizeCache.h`, namespace `nkentseu`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Entrée | `struct NkFontSizeEntry` | Une taille cachée (path, sizePx, font, profile, LRU). |
| Cache | `class NkFontSizeCache` (non copiable, `MAX_CACHED_SIZES` = 16) | N tailles sur un atlas. |
| Cache / API | `Init`, `GetFont`, `BuildAtlasIfNeeded`, `NeedsGpuUpload`, `ClearGpuUploadFlag` | Attache / récupère (piège `nullptr`) / rebuild / flag GPU. |
| Cache / accès | `GetAtlas`, `GetCachedCount`, `GetProfile`, `ShouldUseNearestFilter` | Atlas, compte, profil, filtre recommandé. |
| Scale | `struct NkFontScaleRenderer` (statics) | Taille variable **sans** rebuild (scale par matrice). |
| Scale / API | `ComputeScale`, `CalcTextSizeX`, `GetScaledGlyphQuad` | Facteur d'échelle, largeur scalée, quad scalé. |

### Détection (`NkFontDetect.h`, namespace `nkentseu`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `enum class NkFontKind` (`Unknown/Bitmap/Vector/VectorCFF`) | Nature de la police. |
| Profil | `struct NkFontProfile` | Résultat d'analyse (kind, oversample, tailles, stats courbes). |
| Détecteur | `class NkFontDetector` (statics) | Analyse automatique. |
| Détecteur / API | `Analyze`, `AnalyzeBuffer`, `AnalyzeFile`, `ApplyOptimalConfig`, `RecommendsNearestFilter`, `SnapSize` | Analyse (face/buffer/fichier), config optimale, filtre, snap. |

### Utilitaires raster/SDF/MSDF (`NkUtils.h`, namespace `nkentseu`) — ⚠ utilise `std::vector`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Edge / Shape | `struct NkEdge`, `struct NkShape` | Arête (`p0,p1`) / contour (`std::vector<NkEdge>`). |
| Rasterizer | `class NkRasterizer` (`RasterizeCoverage`, `PointInShape`) | Coverage AA / test inside (non-zero winding). |
| MSDF | `struct NkMSDFPixel`, `class NkMSDFGenerator` (`GenerateMSDF`) | Pixel RGB / génération MSDF. |
| Dilation | `class NkDilation` (`Apply`) | Dilatation de pixels. |
| Shape libre | `nkfont::NkBuildShapeFromGlyph` | Décode un glyphe en liste d'edges pour raster/SDF/MSDF. |

---

## Référence complète

Chaque élément repris en détail. Les types triviaux (alias, lecteurs, accès) sont brefs ; les
pièces importantes (atlas, glyphe, parser, SDF, 3D) sont traitées à fond, avec leurs usages
dans les différents domaines du temps réel.

### Les types de base : `NkFontTypes.h`

Le module ouvre sur un jeu d'alias `nkft_*` qui sont des **alias directs** des types
`nkentseu::*` (`nkft_uint32` = `nkentseu::uint32`, etc.) : une cohérence interne, sans rien de
nouveau à apprendre. Deux alias publics structurent tout le reste : `NkFontCodepoint` (un
caractère Unicode, `uint32`) et `NkGlyphId` (l'index d'un glyphe dans la fonte, `uint32`) — la
distinction est fondamentale, car la *cmap* est précisément la table qui traduit l'un en
l'autre. Les sentinelles `NKFONT_INVALID_GLYPH_ID` (0xFFFFFFFF), `NKFONT_CODEPOINT_REPLACEMENT`
(U+FFFD, le losange « ? » des caractères manquants) et `NKFONT_CODEPOINT_MAX` bornent ces
domaines. Les macros (`NK_FONT_UNUSED`, `NK_FONT_ASSERT`, `NK_FONT_LIKELY/UNLIKELY`) sont des
utilitaires de compilation overridables — sans intérêt applicatif direct.

### `NkFontGlyph` — l'unité dessinable

C'est la structure que vous lisez à chaque caractère affiché. Elle ne contient pas de pixels :
elle contient les **coordonnées** pour aller chercher les pixels dans l'atlas et les **placer**
à l'écran. Le quad `(x0,y0)-(x1,y1)` est exprimé **relativement au curseur** (et non en
absolu), de sorte qu'on dessine en ajoutant la position courante ; les UV `(u0,v0)-(u1,v1)`
pointent dans la texture de l'atlas en [0..1]. `advanceX` est la valeur dont on avance le
curseur **après** la lettre — c'est elle qui crée l'espacement naturel, plus large pour un
« m » que pour un « i ». Le booléen `visible` distingue les glyphes dessinables des espaces.

- **UI / 2D** — chaque label, bouton, champ de texte se réduit à une boucle « FindGlyph →
  dessiner le quad UV → avancer de `advanceX` ». C'est le cœur du rendu de texte.
- **Rendu** — les quads se batchent : on accumule tous les glyphes d'un paragraphe dans un seul
  *vertex buffer* (un `NkVector<NkVertex>`) et on émet un seul *draw call* texturé sur l'atlas.
- **Outils / éditeur** — la même structure sert au *hit-testing* (où a-t-on cliqué dans le
  texte ?) en parcourant les `advanceX`.

### `NkFontConfig` — le profil d'ajout

`NkFontConfig` paramètre *comment* une police entre dans l'atlas. Ses champs se regroupent par
intention. La **source et l'ownership** (`fontData`, `fontDataSize`, `fontDataOwned`,
`fontIndex` pour les fichiers TTC à plusieurs faces) décident d'où viennent les octets et qui
les libère. La **qualité** (`oversampleH`/`oversampleV` = super-sampling pour adoucir les
bords, `pixelSnapH` pour la netteté, `rasterizerMultiply` pour épaissir/affiner le trait). La
**géométrie** (`glyphOffset` décale tous les glyphes, `glyphMinAdvanceX`/`glyphMaxAdvanceX`
forcent un pas mini/maxi — utile pour rendre une police *monospace* —, `glyphExtraSpacing`
ajoute de l'interlettrage). La **sélection** (`glyphRanges` choisit les plages Unicode,
`glyphFallback` le caractère de secours). Et `mergeMode`, le bouton qui fusionne dans la police
précédente.

- **UI / internationalisation** — `glyphRanges` + `mergeMode` empilent latin + cyrillique +
  CJK + emojis sous un même `NkFont`, sans multiplier les textures.
- **Outils** — forcer `glyphMinAdvanceX == glyphMaxAdvanceX` produit une grille monospace pour
  un éditeur de code ou une console.
- **IO** — `AddFontFromMemory` (non possédé) sert quand la police vient d'un *asset pack* déjà
  en mémoire ; `…Owned` quand on a alloué le buffer exprès pour l'atlas.

### `NkFontAtlas` — le conteneur, à fond

L'atlas est l'objet autour duquel tout gravite. Ses champs publics exposent son état : le côté
GPU (`texID`, `texWidth/Height`, `texPixels`, `texReady`), les paramètres de packing
(`texDesiredWidth`, `texGlyphPadding`), les réglages SDF (`sdfMode`, `sdfSpread`), et les
tableaux fixes de polices (`fonts[16]`, `configs[16]`) et de rects custom
(`customRects[64]`). Tout est en capacité **fixe** — 16 polices, 64 rects — ce qui évite toute
allocation dynamique de la structure elle-même.

**Le cycle de vie est non négociable.** Les `AddFontFrom*` ne font qu'**empiler** une
description ; rien n'est rastérisé. C'est `Build()` qui, en une passe, rastérise toutes les
polices empilées et les range dans la texture (il renvoie `false` en cas d'échec). Ensuite
seulement `GetTexDataAsAlpha8` (1 octet/pixel, le format compact pour du texte monochrome) ou
`GetTexDataAsRGBA32` (converti en interne, stocké dans le membre privé `mTexPixelsRGBA`)
livrent les pixels à uploader. `ClearTexData` rend la mémoire CPU une fois la texture sur le
GPU ; `Clear` repart de zéro ; `IsBuilt()` (inline) interroge `texReady`.

**Les plages prédéfinies** (`GetGlyphRangesDefault/LatinExtA/Cyrillic/Greek/ChineseFull`) sont
des statics renvoyant un tableau terminé, à passer dans `NkFontConfig::glyphRanges` — un raccourci
pour les alphabets courants sans écrire les plages à la main.

**Les rects custom** (`AddCustomRect` / `GetCustomRect`) réservent de la place dans l'atlas pour
**autre chose que des glyphes** : un curseur, une icône, un *checkbox tick*. On empaquète ainsi
ses propres petites images dans la même texture que le texte, donc dans le même *draw call*.

- **Rendu / UI** — c'est l'atlas qui rend le texte « gratuit » côté GPU : une texture, un
  *draw call* pour des milliers de glyphes. Les rects custom y ajoutent les icônes d'UI.
- **Outils / éditeur** — un éditeur charge plusieurs polices (interface, code, titres) dans un
  ou quelques atlas et bascule entre les `NkFont*` selon le widget.
- **GPU** — `GetTexDataAsAlpha8` minimise la bande passante (1 canal) pour du texte plat ;
  `GetTexDataAsRGBA32` sert quand le pipeline n'accepte que du RGBA.
- **Threading** — `Build()` est lourd : on peut le faire sur un thread de chargement, puis
  uploader la texture sur le thread GPU. Mais l'atlas n'étant **pas copiable** et sans garantie
  de thread-safety documentée, on n'y touche que depuis un propriétaire à la fois.

### `NkFont` — la police rastérisée, à fond

`NkFont` représente une police **figée à une taille**. Ses métriques (`ascent`, `descent`,
`lineAdvance`, `scale`, `fontSize`) servent à la **mise en page verticale** : `lineAdvance` est
l'espacement entre deux lignes, `ascent`/`descent` placent la *baseline*. Le tableau
`glyphs[NK_FONT_MAX_GLYPHS]` (4096) stocke les glyphes rastérisés, `indexLookup[256]` accélère
les codepoints bas (ASCII), et les tables triées (`glyphSortedCodepoints` /
`glyphSortedIndices`) servent la recherche au-delà.

La **recherche** est le service principal. `FindGlyph(cp)` applique le fallback (renvoie le
glyphe de secours si `cp` manque) ; `FindGlyphNoFallback(cp)` renvoie `nullptr` franchement.
`GetCharAdvance(cp)` (inline) donne l'avance, et `CalcTextSizeX(text, end)` mesure la largeur
d'une chaîne UTF-8 en `O(n)`. En interne, `FindGlyphIndex` opère sur la table triée
`glyphSortedCodepoints` — vraisemblablement par dichotomie (`O(log n)`), même si le header ne le
garantit pas. Les méthodes `BuildLookupTable`, `AddGlyph`, `AddGlyphSorted` sont publiques mais
bas niveau : c'est l'atlas qui les pilote pendant le `Build`.

`GetGlyphOutlinePoints(cp, out)` extrait les **contours aplatis** d'un glyphe en segments
(tolérance 0.35 px) — il faut que `m_FaceInfo` (le stockage parser interne) soit présent. C'est
le pont vers le vectoriel quand on veut tracer une lettre autrement qu'en bitmap.

- **UI / 2D** — `CalcTextSizeX` centre et dimensionne ; `lineAdvance` gère les paragraphes et le
  *word wrap* ; `FindGlyph` boucle sur chaque caractère.
- **Gameplay** — texte de dégâts flottant, sous-titres, dialogues : tout passe par `FindGlyph` +
  `advanceX`.
- **Outils** — `GetGlyphOutlinePoints` permet à un éditeur de tracer les contours d'une lettre
  (visualiseur de police, export SVG).
- **Animation** — animer caractère par caractère (machine à écrire, *bounce*) revient à appliquer
  une transformation par glyphe pendant la boucle de rendu.

### Le texte 3D : `NkFontGlyph3DVertex`, `NkFontGlyphMesh3D` et les générateurs

L'extrusion 3D transforme les contours plats en **volume**. `NkFontOutlineVertex` est un sommet
de contour aplati (position 2D + drapeau `isEndOfContour`). `NkFontGlyph3DVertex` est le sommet
du maillage final : position et normale 3D, UV, couleur, `glyphIndex`, et surtout `faceType` (0
= face avant, 1 = arrière, 2 = latérale) — ce qui permet d'éclairer différemment le devant et
les tranches. `NkFontGlyphMesh3D` agrège le tout (`NkVector<NkFontGlyph3DVertex> vertices`,
indices optionnels, métriques `advanceX`/`ascent`/`descent`).

Les générateurs viennent en deux saveurs. `GenerateGlyphMesh3D` / `GenerateTextMesh3D`
**allouent** et renvoient un mesh complet — simple, mais une allocation par appel. Les variantes
`ForEachGlyph3DVertex` / `ForEachText3DVertex` **streament** chaque sommet vers un callback
`NkFont3DVertexCallback` sans tampon intermédiaire — pour remplir directement un buffer GPU mappé.

- **Rendu** — un logo ou un titre en relief, éclairé par le pipeline 3D normal grâce aux normales
  par face.
- **Gameplay / 3D** — texte du monde (panneaux, étiquettes flottantes, HUD diégétique) qui
  réagit à la lumière et à la caméra.
- **GPU** — `ForEach…3DVertex` évite l'allocation : idéal pour pousser les sommets dans un buffer
  *streaming* sans passer par un `NkVector` temporaire.
- **Outils / éditeur** — prévisualiser un titre 3D, exporter un mesh de texte vers un format
  d'asset.

### Le parser : `NkFontParser.h` (namespace `nkentseu::nkfont`)

C'est l'étage qui lit réellement le fichier de police. Les formats des polices étant **big-endian**,
les lecteurs inline `NkReadU8/U16/U32/I16/I32` décodent chaque champ dans le bon ordre d'octets.
`NkFontDataSpan` est une vue bornée (`data` + `size`) avec `IsValid(offset, len)` pour vérifier
qu'on ne sort pas du buffer et `At(offset)` pour pointer dedans — la garde anti-débordement de
tout le parser.

`NkFontFaceInfo` est le **résultat du parsing** d'une face : les offsets de toutes les tables
(`cmap`, `loca`, `head`, `glyf`, `hhea`, `hmtx`, `kern`, `gpos`, `os2`, `name_`, `cff`), les
métriques (`unitsPerEm`, `ascent`, `descent`, `numGlyphs`…), la *cmap* sélectionnée
(`cmapTableOffset`, `cmapFormat`), le drapeau `isCFF` (police OTF/CFF Type 2) et l'éventuel
buffer WOFF décompressé (`woffBuffer`).

**Le cycle de vie est strict** (règle Create/Destroy du moteur) : `NkInitFontFace(info, data,
size, faceIndex)` initialise depuis un buffer TTF/OTF/WOFF — il **décompresse automatiquement**
le WOFF (zlib intégré) dans `info->woffBuffer` —, et **doit** être suivi de
`NkFreeFontFace(info)` pour libérer cette mémoire. Oublier le `Free` fuit le buffer WOFF.

Une fois la face initialisée, les fonctions de requête décrivent un glyphe : `NkFindGlyphIndex`
traduit codepoint → `NkGlyphId` via la cmap ; `NkGetGlyphHMetrics` donne l'advance et le *left
side bearing* ; `NkGetGlyphBox` la bbox en unités fonte ; `NkGetGlyphKernAdvance` le crénage
(kern format 0) entre deux glyphes ; `NkGetGlyphShape` décode les **contours** (TTF simple,
composite, ou CFF Type 2) dans un `NkFontVertexBuffer`. Côté police entière,
`NkScaleForPixelHeight` / `NkScaleForEmToPixels` calculent les facteurs d'échelle,
`NkGetFontVMetrics` les métriques verticales, et `NkGetFontName` lit la table *name* (nom de la
police).

`NkFontVertex` est un sommet de contour brut (coordonnées entières + points de contrôle Bézier
`cx/cy/cx1/cy1` + `type`), où `type` vaut `NK_FONT_VERTEX_MOVE` (déplacement), `LINE` (segment),
`CURVE` (Bézier quadratique) ou `CUBIC` (Bézier cubique CFF). `NkFontVertexBuffer` est un buffer
fixe de 2048 sommets avec `Push` (`O(1)`, `false` si plein) et `Clear`.

Limites assumées : **WOFF2 n'est pas supporté** (il exige Brotli), seuls cmap formats 4 et 12 et
kern format 0 sont gérés.

- **Outils / éditeur** — un visualiseur de police, un générateur d'atlas custom, un export de
  contours s'appuient directement sur ce parser.
- **IO / réseau** — charger une police compressée WOFF reçue (la décompression zlib est intégrée)
  sans dépendance externe.
- **Rendu** — c'est la source de vérité des contours que l'atlas rastérise et que l'extrusion 3D
  transforme en volume.

### Le rastériseur et le SDF (free functions)

`NkGetGlyphBitmapBox` calcule la boîte bitmap d'un glyphe à une échelle/décalage donnés (pour
dimensionner le buffer de sortie), et `NkMakeGlyphBitmap` **rastérise** le glyphe en **alpha 8
bits** dans le buffer fourni (largeur, hauteur, stride, scale x/y, shift x/y). C'est l'étape qui
transforme des courbes en pixels d'opacité.

`NkMakeSDFFromBitmap(alpha, w, h, sdfOut, spread)` convertit un bitmap alpha en **Signed Distance
Field** de même taille : chaque texel encode la distance au contour (> 127 intérieur, = 127
contour, < 127 extérieur), sur un rayon `spread` (défaut 6). Combiné au shader fourni
`kNkFontFragSDF` (`smoothstep(0.5 - s, 0.5 + s, sdf/255)`), il donne du texte net à toute échelle.

- **Rendu / UI** — le SDF est l'arme des polices qui zooment (titres animés, étiquettes 3D, UI à
  DPI variable) sans re-rastériser.
- **Outils** — pré-calculer des atlas SDF hors-ligne pour les charger directement au runtime.

### Le cache de tailles : `NkFontSizeCache` et `NkFontScaleRenderer`

`NkFontSizeCache` répond à un besoin d'UI : afficher la **même police à plein de tailles** qui
changent (zoom, niveaux de titre, accessibilité). Il gère jusqu'à 16 tailles
(`MAX_CACHED_SIZES`) sur un atlas, chacune décrite par un `NkFontSizeEntry` (chemin, taille,
`NkFont*`, profil, `lastUsedFrame` pour le LRU). On l'attache avec `Init(atlas, fontPath,
preloadSizes, preloadCount)` — en préchauffant éventuellement des tailles.

Le **piège central** est dans `GetFont(sizePx, frameIdx)`. Si la taille existe, on récupère le
`NkFont*`. Mais si elle est **absente**, le cache l'ajoute, marque un rebuild nécessaire, et
**renvoie `nullptr`** — il faudra appeler `BuildAtlasIfNeeded()` (qui renvoie `true` s'il a
reconstruit) puis **re-uploader la texture GPU**, et seulement au prochain `GetFont` on obtiendra
la fonte. Les drapeaux `NeedsGpuUpload()` / `ClearGpuUploadFlag()` (inline) pilotent ce
re-upload. `ShouldUseNearestFilter()` délègue au détecteur pour choisir le filtrage. Le cache
est **non copiable**.

`NkFontScaleRenderer` est l'alternative **sans rebuild** : on rastérise à une taille de référence,
et on **scale par matrice** (statics inline). `ComputeScale(ref, target)` donne le facteur
`target/ref` ; `CalcTextSizeX(font, text, scale)` la largeur scalée ; `GetScaledGlyphQuad`
calcule les coins du quad mis à l'échelle et l'advance. Le compromis assumé : net en réduction,
flou en agrandissement.

- **UI / éditeur** — un éditeur où l'on zoome le texte (Ctrl+molette) : le cache fournit des
  rendus nets à chaque palier ; le scale-renderer évite le rebuild quand le flou est acceptable.
- **Threading / GPU** — le rebuild invalide toute la texture : on planifie le re-upload via les
  drapeaux plutôt que de re-pousser à chaque frame.
- **Gameplay** — texte qui grossit (pop-up de score, transition) : `NkFontScaleRenderer` anime la
  taille sans toucher l'atlas.

### La détection : `NkFontKind`, `NkFontProfile`, `NkFontDetector`

Toutes les polices ne veulent pas le même traitement, et `NkFontDetector` automatise le choix.
`NkFontKind` classe la police : `Bitmap` (taille native, à afficher au pixel, filtre *nearest*),
`Vector` (TrueType à courbes, toute taille), `VectorCFF` (OTF/CFF, marqué « non supporté pour
l'instant » dans le commentaire), `Unknown` (non analysée). `NkFontProfile` porte le verdict :
le `kind`, les réglages déduits (`oversampleH/V`, `pixelSnapH`, `useNearestFilter`), les tailles
recommandées (`nativeSizePx`, min/max), et les statistiques de courbes (`curveRatio`) qui ont
servi à décider.

Le détecteur est **tout statique**. `Analyze(faceInfo, testCps, testCount)` examine un échantillon
de glyphes (jeu ASCII par défaut si `nullptr`) pour distinguer courbes et lignes ; `AnalyzeBuffer`
et `AnalyzeFile` font le même travail depuis un buffer ou un fichier. `ApplyOptimalConfig(profile,
sizePx, outConfig)` **écrit** dans un `NkFontConfig` les réglages optimaux. `RecommendsNearestFilter`
indique le filtre, et `SnapSize` ajuste une taille à la native pour une police bitmap (inchangée
pour le vectoriel).

- **Outils / éditeur** — importer une police inconnue et appliquer **automatiquement** le bon
  oversampling et le bon filtre, sans réglage manuel.
- **UI** — éviter le piège classique d'une police bitmap rendue floue (mauvais filtre) ou d'une
  vectorielle rendue trop nette (pas d'AA).

### Les utilitaires `NkUtils.h` — l'anomalie zéro-STL

Ce header est explicitement signalé comme **non conforme** au reste du module : il inclut
`<vector>`, `<cmath>`, `<cstdint>` et utilise `std::vector` (dans `NkShape::edges`). À traiter à
part dans un codebase zéro-STL. Son `NkShape` est en outre un **homonyme** du `NkShape` de
NKCanvas, mais un type **distinct** (ici, un contour de glyphe).

`NkEdge` est une arête (`p0`, `p1`), `NkShape` une liste d'arêtes. `NkRasterizer::RasterizeCoverage`
produit un coverage **anti-aliasé** par sur-échantillonnage (4×4 par défaut) ; `PointInShape` teste
si un point est à l'intérieur (règle non-zero winding). `NkMSDFGenerator::GenerateMSDF` calcule un
**MSDF** (multi-channel SDF, `NkMSDFPixel{r,g,b}`) qui préserve mieux les coins que le SDF simple.
`NkDilation::Apply` dilate des pixels. Enfin `nkfont::NkBuildShapeFromGlyph` décode un glyphe (via
`GetGlyphShape`), convertit les courbes en segments, et produit la liste d'edges — le pont entre le
parser et ces algorithmes raster/SDF/MSDF.

- **Outils** — un générateur d'atlas hors-ligne haute qualité (MSDF pour des coins nets,
  dilatation pour des contours).
- **Rendu** — le MSDF est la meilleure option pour du texte vectoriel à très grande échelle (coins
  préservés là où le SDF simple les arrondit).

### Les shaders et l'UTF-8

Côté GPU, `kNkFontFragNormal` et `kNkFontFragSDF` sont deux fragment shaders GLSL **fournis prêts à
l'emploi** : le premier lit le canal alpha de l'atlas (`discard` si quasi nul), le second lit le
canal rouge et applique le `smoothstep` du SDF. On les colle directement dans son pipeline selon
que `sdfMode` est actif. Côté texte, `NkFontDecodeUTF8` (décode un codepoint et avance le pointeur)
et `NkFontEncodeUTF8` (encode, renvoie le nombre d'octets) sont les fonctions libres de conversion
— l'équivalent des `DecodeUTF8` statiques de `NkFont`, indispensables pour itérer correctement sur
du texte multi-octets.

### Note de fiabilité

Aucun `noexcept` ni `nodiscard` n'est déclaré dans ces headers. Les complexités ne sont pas
documentées ; seules celles déductibles le sont (`Push`/`Clear` du vertex buffer en `O(1)`,
`CalcTextSizeX` en `O(n)`). Attention enfin aux exemples *aspirationnels* des commentaires
(`atlas.GetFont(...)`, `NkFontScaleRenderer::DrawText2D`) : ces signatures **n'existent pas** dans
l'API réelle — ne pas s'y fier.

---

### Exemple récapitulatif

```cpp
#include "NKFont/NkFont.h"
using namespace nkentseu;

// 1. Détecter la police et appliquer les réglages optimaux.
NkFontProfile profile = NkFontDetector::AnalyzeFile("Roboto.ttf");
NkFontConfig cfg;
NkFontDetector::ApplyOptimalConfig(profile, 24.f, &cfg);

// 2. Empiler dans l'atlas, fusionner une 2e police (emojis) par-dessus.
NkFontAtlas atlas;
atlas.texGlyphPadding = 2;                              // anti-bleeding
NkFont* font = atlas.AddFontFromFile("Roboto.ttf", 24.f, &cfg);
NkFontConfig emoji; emoji.mergeMode = true;            // fusion dans Roboto
atlas.AddFontFromFile("NotoEmoji.ttf", 24.f, &emoji);

// 3. Rastériser TOUT en une passe, récupérer les pixels, uploader.
atlas.Build();
nkft_uint8* px; nkft_int32 w, h;
atlas.GetTexDataAsAlpha8(&px, &w, &h);
atlas.texID = monGPU.UploadAlpha(px, w, h);            // shader = kNkFontFragNormal

// 4. Mesurer puis dessiner une chaîne UTF-8.
float largeur = font->CalcTextSizeX("Bonjour 👋");      // O(n), pour centrer
float x = (ecranW - largeur) * 0.5f;
const char* s = "Bonjour 👋";
while (*s) {
    NkFontCodepoint cp = NkFont::DecodeUTF8(&s, nullptr);
    const NkFontGlyph* g = font->FindGlyph(cp);         // fallback automatique
    if (g->visible) DessinerQuad(x + g->x0, g->y0, g->u0, g->v0, g->u1, g->v1);
    x += g->advanceX;
}

// 5. Bonus : un titre en relief 3D.
NkFontGlyphMesh3D mesh =
    font->GenerateTextMesh3D("NKENTSEU", 0.01f, 0.2f, worldMatrix, NkVec4f{1,1,1,1});
monGPU.Draw3D(mesh.vertices.Data(), mesh.vertices.Size());
```

---

[← Index NKFont](README.md) · [Récap NKFont](../NKFont.md) · [Couche Runtime](../README.md)
