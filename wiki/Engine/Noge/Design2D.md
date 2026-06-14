# Le dessin 2D : illustration vectorielle et raster

> Couche **Engine** · Noge · La famille **Design2D** : un atelier complet de dessin 2D — peinture
> numérique tile-based (`NkRasterCanvas`, `NkBrushEngine`), illustration vectorielle Illustrator-like
> (`NkVectorPath`, `NkVectorDocument`), documents hybrides Procreate/CSP (`NkHybridDocument`,
> `NkLayerStack`), gestion colorimétrique perceptuelle (`NkColorManager`) et typographie sur chemin
> (`NkTextPath`).

Un moteur qui veut servir un **éditeur** — Nogee, ou n'importe quel outil auteur — a besoin, à un
moment, de **dessiner pour de vrai** : poser de la peinture pixel par pixel, tracer une courbe de
Bézier nette à n'importe quelle échelle, empiler des calques avec des modes de fusion, gérer des
couleurs justes. La famille Design2D de Noge est cette ambition : reproduire, en zéro-STL et au-dessus
de NKRenderer 2D, ce que Photoshop, Illustrator, Procreate et Clip Studio Paint offrent. Elle se lit
comme un atelier à quatre établis : le **raster** (peinture), le **vectoriel** (tracé), le **document**
(qui assemble), et la **couleur** (qui sous-tend tout).

> **STATUT — lire d'abord.** Ces sept en-têtes sont des fichiers de **spécification**. La quasi-totalité
> des méthodes y sont *déclarées sans corps* (les `.cpp` ne sont pas garantis) ; seules les méthodes
> définies **inline** existent réellement aujourd'hui (factories, accesseurs, `Play`/`Stop`/`Pause`,
> destructeurs). Les commentaires d'en-tête (« Limité », « Nécessite ffmpeg », « CONCEPT ») confirment
> ce statut. Traitez cette page comme une **API cible** : la forme est arrêtée, l'implémentation viendra.

- **Namespace** : `nkentseu` (chaque header fait `using namespace math;` → `NkVec2f`, `NkVec4f`,
  `NkMat3f`, `NkAABB2f`, `NkRectF`, `NkIRect`, `NkIVec2`, `NkColor` sans qualification). Le renderer est
  dans le sous-namespace `nkentseu::renderer` (`renderer::NkRender2D`).
- **Headers** : `Noge/Design/NkLayerStack.h`, `Design/Raster/NkRasterCanvas.h`,
  `Design/Vector/NkVectorPath.h`, `Doc/NkHybridDocument.h`, `Doc/NkVectorDocument.h`,
  `Color/NkColorManager.h`, `Text/NkTextPath.h`.

> **Deux dettes à connaître d'emblée.** (1) **Ownership en pointeurs bruts + `delete`/`delete[]` CRT** :
> calques, tuiles, frames, layers vectoriels et symboles libèrent leur mémoire avec `delete`, ce qui
> **viole la règle dure NKMemory** (Create/Destroy via `NkAlloc`/`NkFree`) — risque de heap-corruption
> `c0000374` si on mélange. (2) **Usages `std::`** ponctuels (`std::pair`, `std::strncpy`, `std::swap`)
> en contradiction avec le zéro-STL du projet. Ces deux points sont signalés à chaque endroit concerné.

---

## La peinture raster : `NkRasterCanvas` et `NkBrushEngine`

Peindre, c'est **poser des pixels**. Mais une toile de 4096×4096 en RGBA32F pèse 256 Mo : la garder
entière en mémoire et la re-téléverser au GPU à chaque coup de pinceau serait ruineux.
`NkRasterCanvas` résout cela par un stockage **tile-based** : la toile est découpée en tuiles de
**64×64** (`NkCanvasTile::kSize`), et seules les tuiles **réellement touchées** sont marquées *dirty*
puis synchronisées au GPU. Vous peignez sur le CPU, vous appelez `FlushDirtyTiles`, et seules les
quelques tuiles modifiées remontent.

La profondeur de couleur se choisit à la création via `NkPixelDepth` : `Depth8` (RGBA8, 4 o/px, le
défaut), `Depth16` (RGBA16, 8 o/px) ou `Depth32` (RGBA32F, 16 o/px, pour le HDR et le compositing
précis). Astuce de design : la valeur de l'enum **est** le facteur de demi-mots, donc
`BytesPerPixel() = uint32(depth) * 4`.

```cpp
NkRasterCanvas canvas;
canvas.Create(2048, 2048, NkPixelDepth::Depth8);

NkBrushEngine brush;
brush.SetCanvas(&canvas);
brush.SetPreset(NkBrushPreset::SoftRound());
brush.SetColor(NkColorRGBA(30, 60, 200));

brush.PointerDown({100.f, 100.f}, /*pressure*/0.8f);
brush.PointerMove({140.f, 130.f}, 1.0f);
brush.PointerUp();

canvas.FlushDirtyTiles(cmd);            // remonte UNIQUEMENT les tuiles touchées
canvas.DrawToRender2D(r2d, dstRect);    // affiche
```

Le pinceau n'est **pas** un simple disque de couleur. `NkBrushEngine` accumule des **tampons**
(`NkBrushDab`) le long du trait, espacés selon le preset, avec dynamique de pression (taille,
opacité), *jitter*, *scatter*, grain et stabilisation du tracé. Un `NkBrushPreset` capture toute
cette configuration ; sept presets prêts à l'emploi existent (`HardRound`, `SoftRound`, `Pencil`,
`Ink`, `Watercolor`, `Airbrush`, `Eraser`).

> **Le workflow obligatoire.** Peindre sur le CPU (`PaintDab`/`Brush`) → `FlushDirtyTiles(cmd)` →
> `DrawToRender2D`. Les accès par tuile (`GetTilePixels`, `MarkTileDirty`) prennent des coordonnées
> **en tuiles** (÷64), pas en pixels — c'est le piège classique.

---

## Le tracé vectoriel : `NkVectorPath`

Là où le raster fige des pixels, le vectoriel décrit des **formes mathématiques** — des courbes qui
restent nettes à n'importe quel zoom. `NkVectorPath` est le chemin à la Illustrator/SVG : une suite
de commandes (`MoveTo`, `LineTo`, `CubicTo`, `QuadTo`, `ArcTo`, `Close`) qu'on enchaîne en **API
fluide** (chaque méthode renvoie `NkVectorPath&`), exactement comme l'attribut `d` d'un `<path>` SVG.

```cpp
NkVectorPath star;
star.AddStar(200.f, 200.f, /*outerR*/80.f, /*innerR*/35.f, /*points*/5);

NkVectorPath card;
card.MoveTo(0, 0).LineTo(100, 0).LineTo(100, 60).LineTo(0, 60).Close();

NkPaint fill = NkPaint::LinearGrad({0,0}, {0,60},
                                   NkVec4f{1,1,1,1}, NkVec4f{0.8f,0.9f,1,1});
star.Draw(r2d, fill, NkPaint::Black(), NkStrokeStyle{});   // fill + contour
```

Le remplissage est porté par `NkPaint` (couleur unie, dégradés linéaire/radial/conique, motif,
image), le contour par `NkStrokeStyle` (largeur, *caps*, *joins*, pointillés). Au-delà du tracé brut,
`NkVectorPath` offre des **formes haut niveau** (`AddRect`, `AddCircle`, `AddEllipse`, `AddStar`,
`AddRoundedRect`), des **transformations in-place** chaînables (`Translate`, `Scale`, `Rotate`,
`Transform`), des **opérations booléennes** statiques (`Union`, `Subtract`, `Intersect`, `Exclude`),
des **métriques** (boîte englobante, longueur, point/tangente à une abscisse curviligne, test de
contenance) et la **sérialisation SVG** (`ToSVGPath`/`FromSVGPath`).

> **En résumé.** `NkVectorPath` = un chemin résolution-indépendante, construit en API fluide
> (`path.MoveTo(...).LineTo(...).Close()`), peint via `NkPaint` (fill) + `NkStrokeStyle` (stroke).
> Pour des formes courantes, les helpers `Add*` ; pour combiner, les booléennes statiques. Le tout est
> aujourd'hui de la **spec** (méthodes non-inline sans corps), sauf les factories inline.

---

## L'empilement : `NkLayerStack`

Un dessin sérieux n'est jamais une seule couche : c'est une **pile**. `NkLayerStack` empile des
`NkLayer` du **bas vers le haut** et les **composite** au GPU, chacun avec son **mode de fusion**
(`NkBlendMode`), son **opacité**, sa **visibilité**, son **masque**, et la possibilité d'être
**écrêté** sur la couche du dessous ou **groupé**. C'est le cœur de Photoshop/Procreate.

Une couche a un **type** (`NkLayer::Type` : `Raster`, `Vector`, `Text`, `Adjustment`, `Group`,
`Reference`) qui dicte ses données : un `NkRasterCanvas*` pour le raster, des chemins vectoriels pour
le vectoriel, un `NkLayerAdjustment*` pour un **filtre non-destructif** (luminosité/contraste,
courbes, teinte/saturation, flou…).

```cpp
NkLayerStack stack;
NkLayer* bg    = stack.AddRasterLayer("Fond");
NkLayer* line  = stack.AddVectorLayer("Encrage");
NkLayer* tone  = stack.AddAdjustmentLayer(NkAdjustmentType::HueSaturation, "Teinte");

line->blendMode = NkBlendMode::Multiply;
line->opacity   = 0.9f;

stack.Composite(r2d, cmd, dstRect);    // bottom → top
```

> **Piège fondamental : `id` ≠ `index`.** Les méthodes de gestion et de sélection
> (`DeleteLayer`, `MoveLayer`*, `DuplicateLayer`, `MergeDown`, `MarkDirty`, `SelectLayer`,
> `FindLayer`) prennent un **`id`** (identifiant stable). En revanche `MoveLayer`'s second argument et
> `GetLayer(index)` prennent un **index** dans l'ordre bottom-to-top. Ne pas les confondre.

> **Deux `BlendMode` distincts.** `NkBlendMode` (global, 27 valeurs + `COUNT`) sert au compositing des
> **calques**. Mais `NkBrushDab::BlendMode` (imbriqué, 14 valeurs) sert au mélange des **tampons de
> pinceau** et de `Blit`. Toujours qualifier : `NkBlendMode::Multiply` *vs*
> `NkBrushDab::BlendMode::Multiply`.

---

## Les documents : `NkHybridDocument` et `NkVectorDocument`

Au sommet, deux types de **fichiers**. `NkHybridDocument` est un document **raster + vectoriel** à la
Procreate/Clip Studio : une `NkLayerStack`, plus tout un atelier d'assistants — symétrie
(`NkSymmetryTool`), perspective (`NkPerspectiveGuide`), stabilisation du tracé (`NkStabilizer`), et
animation image-par-image avec *onion skin* (`NkAnimationAssist`). Il s'enregistre en `.nkart` et
exporte en PNG/PSD.

`NkVectorDocument` est l'équivalent **purement vectoriel** à la Illustrator/Affinity, organisé en
hiérarchie : `NkVectorDocument → NkArtboard → NkVectorLayer → NkVectorObject`. Plusieurs **plans de
travail** (artboards), des guides, une grille, des symboles réutilisables, un presse-papier, et
l'export SVG/PDF/PNG.

```cpp
NkVectorDocument doc;
NkArtboard& board = doc.AddArtboard("A4", 595.f, 842.f);
NkVectorLayer& layer = board.AddLayer("Dessin");

NkVectorPath logo; logo.AddCircle(100, 100, 40);
layer.AddPath(logo, NkPaint::Solid(NkVec4f{0.1f,0.4f,0.9f,1.f}), "Disque");

doc.ExportSVG("logo.svg", /*artboardIdx*/0);
```

> **En résumé.** `NkHybridDocument` = raster+vecteur+animation, fichier `.nkart`. `NkVectorDocument` =
> vectoriel multi-artboards, hiérarchie à quatre niveaux. Tous deux exposent `Composite`/`Draw` (rendu),
> `Save/LoadToFile`, et des exports — **tous non-inline, donc spec**. Attention à l'ownership : voir
> ci-dessous, certains conteneurs sont *owned*, d'autres non.

---

## La couleur : `NkColorManager`

Tout le reste repose sur des **couleurs justes**. `NkColor` stocke en interne du **Linear RGB**
[0..1] + alpha, ce qui est le bon espace pour mélanger et composer. Autour de lui, `NkColorManager`
fournit conversions (sRGB, HSL/HSV, LAB/LCH, CMYK, OKLab…), ajustements perceptuels, métriques
d'accessibilité (contraste WCAG, ΔE), palettes (`NkPalette`, `NkSwatch`), harmonies (`NkHarmony`) et
un état de sélecteur (`NkColorPicker`).

```cpp
NkColor brand = NkColor::FromHex("#2D7FF9");          // depuis un hex écran
NkColor onText = brand.BestContrast(NkColor::Black(), NkColor::White());
bool ok = brand.IsAccessible(NkColor::White(), 4.5f); // WCAG AA ?

auto triade = NkHarmony::Triadic(brand);              // 3 couleurs à 120°
```

> **Piège colorimétrique majeur.** Le constructeur brut `NkColor(r, g, b, a)` interprète ses valeurs
> comme du **Linear RGB**, *pas* du sRGB. Pour des valeurs venues de l'écran (un color picker, un hex),
> utilisez **`FromSRGB`** ou **`FromU8`** — sinon les couleurs sortiront trop sombres.

---

## Le texte : `NkTextPath`

Enfin, `NkTextPath` apporte la **typographie** : du texte multi-style riche (`NkRichText` : plusieurs
`NkTextRun`, chacun avec sa `NkFontStyle`), et surtout du **texte le long d'un chemin**
(`NkTextOnPath`) — un titre qui suit une courbe, un label sur un arc. C'est le complément naturel du
vectoriel pour l'affiche, le logo, l'illustration éditoriale.

---

## Aperçu de l'API

Tous les éléments publics, regroupés par fichier. Le détail (cas d'usage, formules, statut) suit dans
la « Référence complète ».

### `NkLayerStack.h` — pile de calques et compositing

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkBlendMode` (27 + `COUNT`) | Modes de fusion des calques (`Normal`, `Multiply`, `Screen`, `Overlay`, … `Erase`). |
| Enum | `NkAdjustmentType` (13) | Types de filtre non-destructif (`BrightnessContrast`, `CurvesRGB`, `GaussianBlur`, …). |
| Struct | `NkLayerAdjustment` | Paramètres d'un filtre (courbes 5 points, flou…). |
| Struct | `NkLayer` (+ `Type`, `VectorData`) | Une couche : type, blend, opacité, masque, groupe, données owned. |
| Création | `AddRasterLayer`, `AddVectorLayer`, `AddAdjustmentLayer`, `AddGroup` | Ajoutent au-dessus de la sélection ; renvoient `NkLayer*`. |
| Gestion | `DeleteLayer`, `MoveLayer`, `DuplicateLayer`, `MergeDown`, `FlattenAll` | Suppression / déplacement / duplication / fusion. |
| Sélection | `SelectLayer`, `GetActiveLayer`, `FindLayer`, `LayerCount` | Couche active, recherche par **id**, compte. |
| Rendu | `Composite`, `MarkDirty`, `MarkAllDirty` | Compositing bottom→top ; invalidation. |
| Accès UI | `GetLayer(index)` | Accès par **index**. |

### `NkRasterCanvas.h` — peinture tile-based

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Struct | `NkColorRGBA` | Couleur 8-bit/canal + `FromFloat`/`ToFloat`/`Black`/`White`/`Transparent`. |
| Enum | `NkPixelDepth` (`Depth8/16/32`) | Profondeur ; valeur = facteur de demi-mots. |
| Struct | `NkCanvasTile` | Tuile CPU 64×64 (stockage interne). |
| Struct | `NkBrushDab` (+ `BlendMode` 14) | Tampon individuel : taille, dureté, flux, rotation… |
| Classe | `NkRasterCanvas` | Toile : `Create`/`Destroy`, peinture CPU, transforms, pixels, GPU sync, I/O. |
| Struct | `NkBrushPreset` (+ 7 presets) | Config pinceau complète (`HardRound`, `SoftRound`, `Pencil`, …). |
| Classe | `NkBrushEngine` | Moteur de trait : `Set*` + `PointerDown/Move/Up`, stabilisation. |

### `NkVectorPath.h` — chemin vectoriel

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkFillRule`, `NkStrokeCap`, `NkStrokeJoin`, `NkPathCmdType` | Règle de remplissage, terminaisons, jonctions, types de commande. |
| Struct | `NkGradientStop`, `NkPaint` (+ `Type`), `NkStrokeStyle`, `NkPathCmd` | Arrêt de dégradé, remplissage, style de contour, commande. |
| Construction | `MoveTo`, `LineTo`, `CubicTo`, `QuadTo`, `ArcTo`, `Close` | API fluide SVG-like (chaînable). |
| Formes | `AddRect`, `AddCircle`, `AddEllipse`, `AddStar`, `AddRoundedRect` | Primitives haut niveau. |
| Transform | `Transform`, `Translate`, `Scale`, `Rotate` | In-place, chaînables. |
| Booléennes | `Union`, `Subtract`, `Intersect`, `Exclude` (statiques) | Combinent deux chemins. |
| Métriques | `GetBoundingBox`, `GetLength`, `PointAtLength`, `TangentAtLength`, `Contains` | Mesures géométriques. |
| Rendu | `DrawFill`, `DrawStroke`, `Draw` | Tessellation + rendu. |
| SVG | `ToSVGPath`, `FromSVGPath` | Sérialisation chemin. |

### `NkHybridDocument.h` — document raster+vecteur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Classe | `NkSymmetryTool` (+ `Mode`) | Symétrie miroir/radiale/kaléidoscope/mandala. |
| Struct/Classe | `NkVanishingPoint`, `NkPerspectiveGuide` (+ `Mode`) | Points de fuite ; guide 1/2/3 points + iso. |
| Classe | `NkStabilizer` (+ `Mode`) | Lissage du tracé (Chaikin, LazyNezumi, Kalman…). |
| Struct/Classe | `NkAnimationFrame`, `NkAnimationAssist` | Animation image-par-image + onion skin + exports. |
| Classe | `NkHybridDocument` | Le document : `.nkart`, layerStack, outils, exports. |
| Struct | `NkExportPreset` (+ `Target`, `OutputFormat`) | Presets d'export (Web/Print/Social). |

### `NkVectorDocument.h` — document vectoriel multi-artboards

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkVectorObjectType` | `Path`, `Text`, `Image`, `Group`, `Symbol`. |
| Struct | `NkVectorObject`, `NkVectorLayer` | Objet (transform, apparence, masque) ; calque (objets owned). |
| Struct | `NkGuide`, `NkGrid`, `NkSymbol` | Guides, grille, symboles réutilisables. |
| Classe | `NkArtboard` | Plan de travail : calques, guides, sélection, hit-test. |
| Classe | `NkVectorDocument` | Document : artboards, symboles, unités, presse-papier, exports. |

### `NkColorManager.h` — gestion colorimétrique

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkColorSpace` (10) | Espaces colorimétriques (`LinearRGB`, `sRGB`, `LAB`, `OKLab`, …). |
| Classe | `NkColor` | Couleur Linear RGB : factories `From*`, conversions `To*`, ajustements, métriques, blend. |
| Struct/Classe | `NkSwatch`, `NkPalette` (+ 5 presets) | Échantillon nommé ; palette + I/O `.ase/.aco/.json`. |
| Classe | `NkHarmony` | Harmonies (complémentaire, triade, tétrade, analogues…). |
| Struct | `NkColorPicker` | État UI d'un sélecteur de couleur. |
| Singleton | `NkColorManager` | `Global()` : fg/bg, palettes, historique, pipette écran. |

### `NkTextPath.h` — texte et typographie

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkTextAlign`, `NkVerticalAlign`, `NkTextDecoration` | Alignements et décoration. |
| Struct | `NkFontStyle`, `NkTextRun` | Style typographique ; portion de texte stylée. |
| Classe | `NkRichText` | Texte multi-style : `AddRun`, `Draw`, `MeasureSize`. |
| Classe | `NkTextOnPath` | Texte le long d'un `NkVectorPath`. |

---

## Référence complète

Chaque élément est repris ici à fond, avec ses cas d'usage dans les différents domaines. Le **statut
spec** est rappelé là où il compte : seules les méthodes **inline** sont garanties.

### `NkBlendMode` et `NkAdjustmentType` — fusionner et corriger

`NkBlendMode` (`uint8`) énumère **27 modes** plus `COUNT` (le commentaire d'en-tête annonce « 24 »,
mais l'enum en liste 27 — à savoir). L'ordre exact : `Normal=0, Dissolve, Darken, Multiply,
ColorBurn, LinearBurn, DarkerColor, Lighten, Screen, ColorDodge, LinearDodge` (alias *Add*)`,
LighterColor, Overlay, SoftLight, HardLight, VividLight, LinearLight, PinLight, HardMix, Difference,
Exclusion, Subtract, Divide, Hue, Saturation, Color, Luminosity, Erase` (soustrait l'alpha)`, COUNT`.

- **Rendu / compositing** — `Multiply` assombrit (encrage, ombres), `Screen`/`LinearDodge` éclaircit
  (lumières, effets de feu), `Overlay`/`SoftLight` contraste, `Erase` perce l'alpha pour gommer.
- **UI / éditeur** — un panneau de calques propose la liste ; `COUNT` borne l'itération pour peupler
  une combo-box.

`NkAdjustmentType` (`uint8`, sans valeurs explicites, sans `COUNT`) liste les **filtres
non-destructifs** : `BrightnessContrast, LevelsRGB, CurvesRGB, HueSaturation, ColorBalance,
GradientMap, Exposure, Vibrance, Posterize, Threshold, Invert, GaussianBlur, Sharpen`. C'est le menu
d'un calque de réglage (un *adjustment layer* qui modifie tout ce qui est dessous sans détruire les
pixels).

### `NkLayerAdjustment` et `NkLayer` — l'anatomie d'une couche

`NkLayerAdjustment` est une **struct de paramètres** (aucune méthode) : `type`, `brightness`/`contrast`
[-1..1], `hue` [-180..180]/`saturation`/`lightness`, trois courbes RGB (`curveR/G/B`, chacune
`kCurvePoints = 5` points `NkVec2f` initialisés sur l'identité {0,0}…{1,1}), et `blurRadius`. C'est la
donnée qu'un calque `Adjustment` porte.

`NkLayer` décrit **une couche**. Outre son `name` (jusqu'à `kMaxName = 128`) et son `id`, elle porte :

- **Type** — `NkLayer::Type` (`uint8`) : `Raster, Vector, Text, Adjustment, Group, Reference` (six
  valeurs ; `Reference` est absent du commentaire d'en-tête mais bien présent).
- **Compositing** — `blendMode`, `opacity` [0..1], `visible`, `locked`, `alphaSelf` (preserve
  transparency), `clipped` (calque écrêté sur le dessous).
- **Données owned** (pointeurs bruts) — `rasterCanvas`, `vectorData` (struct imbriquée `VectorData` :
  `paths`, `fill`, `stroke`, `strokeStyle`), `adjustment`.
- **Groupe** — `isGroup`, `passThrough`, `collapsed`, `parentId` (0 = racine).
- **Masque** — `maskCanvas`, `maskEnabled`, `maskInvert`.
- **GPU** — `offscreenRT` (handle `NkOffscreenTarget`), `dirty`.

Le constructeur est `default`, le destructeur libère les données owned, et la couche est
**non-copiable** (copie/affectation `= delete`). Cas d'usage : un calque `Raster` pour la peinture, un
`Vector` pour l'encrage net, un `Adjustment` pour une correction globale, un `Group` pour organiser,
un masque pour révéler/cacher localement.

> Statut : `NkLayer` est de la **donnée** (champs publics) ; son destructeur existe mais utilise
> `delete` (dette NKMemory).

### `NkLayerStack` — composer la pile

`NkLayerStack` empile les `NkLayer` **du bas vers le haut** et les composite au GPU. Non-copiable,
constructeur/destructeur `default`, toutes méthodes `noexcept`.

- **Création** (renvoient `NkLayer*`, ajoutent au-dessus de la sélection) — `AddRasterLayer`,
  `AddVectorLayer`, `AddAdjustmentLayer(type, name)`, `AddGroup`.
- **Gestion** — `DeleteLayer(id)`, `MoveLayer(id, toIndex)`, `DuplicateLayer(id)`, `MergeDown(id)`
  (fusionne avec la couche du dessous), `FlattenAll`.
- **Sélection** — `SelectLayer(id)`, `GetActiveLayer()` (+ surcharge const), `FindLayer(id)`,
  `LayerCount()`.
- **Rendu** — `Composite(r2d, cmd, dst)` (bottom→top, via `DrawImage`), `MarkDirty(id)`, `MarkAllDirty`.
- **Accès UI** — `GetLayer(index)` (+ const).

Domaines : **éditeur** (panneau de calques, drag-and-drop d'ordre via `MoveLayer`), **rendu** (le
compositing GPU avec offscreen targets), **animation** (chaque frame peut être une pile compositée).

> **Le piège central à retenir** : `id` (stable, pour `Delete/Move/Duplicate/Merge/Mark/Select/Find`)
> ≠ `index` (position bottom-to-top, pour `MoveLayer`'s destination et `GetLayer`). Mélanger les deux
> est l'erreur la plus probable.

> Statut : toutes ces méthodes sont **déclarées sans corps** → spec. Aucune n'est garantie tant qu'un
> `.cpp` n'est pas trouvé.

### `NkColorRGBA`, `NkPixelDepth`, `NkCanvasTile`, `NkBrushDab` — les briques du raster

`NkColorRGBA` est une couleur **8-bit/canal** (`r,g,b` à 0, `a` à 255 par défaut). Le constructeur
`constexpr(r,g,b,a=255)`, plus les factories inline `Black/White/Transparent` et la conversion
`ToFloat()` (÷255). **Attention** : `FromFloat(r,g,b,a=1)` multiplie par 255 **sans clamp** — un canal
> 1 déborde l'`uint8`. C'est le type des couleurs de pinceau et de pixel.

`NkPixelDepth` (`uint8`) a des valeurs **égales au facteur** : `Depth8 = 1` (RGBA8, 4 o/px),
`Depth16 = 2` (RGBA16, 8 o/px), `Depth32 = 4` (RGBA32F, 16 o/px), d'où `BytesPerPixel() = depth*4`.
Choisir Depth32 pour le HDR/compositing fin, Depth8 pour le travail courant.

`NkCanvasTile` est le **stockage interne** (vous ne le manipulez quasi jamais directement) : tuile de
`kSize = 64`, `pixels` (CPU), `gpuTex`, drapeaux `dirty`/`blank`, méthodes `Allocate(depth)`/`Clear`.
**Dette** : son destructeur fait `delete[] pixels` (heap CRT, incohérent avec NKMemory).

`NkBrushDab` est **un tampon** posé le long d'un trait : `sizePx`, `hardness` [0=flou..1=dur],
`opacity`, `flow`, `angleDeg`, `roundness` [0=plat..1=rond], `scatter`, `color`, et un `stampTex`
(0 = cercle). Son enum imbriqué `NkBrushDab::BlendMode` (`uint8`, **14 valeurs distinctes** de
`NkBlendMode`) : `Normal, Multiply, Screen, Overlay, SoftLight, HardLight, Darken, Lighten,
Difference, Hue, Saturation, Color, Luminosity, Erase`. C'est ce mode-là qui régit le mélange d'un
coup de pinceau et de `Blit`.

### `NkRasterCanvas` — la toile

Non-copiable, destructeur appelant `Destroy()`, tout `noexcept`.

- **Cycle de vie** — `Create(width, height, depth=Depth8)`, `Destroy()`, `IsValid()` (inline).
- **Peinture CPU** — `PaintDab(dab, center)`, `Fill(color, region)`, `Erase(region)`,
  `FloodFill(x, y, color, tolerance=32)` [0..255], `Blit(src, srcRect, dstPos, opacity,
  NkBrushDab::BlendMode)`.
- **Transformations** — `FlipH`, `FlipV`, `Rotate90(clockwise=true)`, `Scale(w, h)`, `Crop(region)`.
- **Pixels** — `GetPixel(x, y)`, `SetPixel(x, y, c)`, `GetTilePixels(tileX, tileY)` (`NkSpan<uint8>`,
  accès **batch**, coords **en tuiles**), `MarkTileDirty(tileX, tileY)`, `MarkAllDirty`.
- **GPU sync** — `FlushDirtyTiles(cmd)` (après peinture, avant rendu).
- **Rendu** — `DrawToRender2D(r2d, dst, opacity=1)`.
- **Propriétés inline** — `GetWidth/GetHeight/GetDepth/BytesPerPixel/TileCountX/TileCountY/
  DirtyTileCount`.
- **I/O** — `SavePNG`, `SaveTIFF` (16-bit si Depth16), `SaveEXR` (Depth32 uniquement), `LoadPNG`,
  `LoadTIFF`.

Domaines : **peinture numérique** (le cœur), **textures** (générer/retoucher une texture procédurale
puis la téléverser), **éditeur** (gomme, pot de peinture `FloodFill`, transformations), **animation**
(une frame = une toile). Le `FloodFill` à tolérance sert au *bucket fill* ; le `Blit` à composer un
tampon ou coller une sélection.

> **Workflow obligatoire** : peindre → `FlushDirtyTiles(cmd)` → `DrawToRender2D`. Et `GetTilePixels`/
> `MarkTileDirty` prennent des coordonnées **tile** (pixel ÷ 64), pas pixel.

> Statut : ces méthodes sont déclarées sans corps → spec, sauf les inline (`IsValid`, accesseurs).

### `NkBrushPreset` et `NkBrushEngine` — le moteur de pinceau

`NkBrushPreset` capture **toute** la configuration d'un pinceau : forme (`size`, `hardness`, `angle`,
`roundness`, `angleJitter`, `sizeJitter`, `stampTex`), espacement (`spacing` = fraction du diamètre,
`scatter`), opacité et dynamique (`opacity`, `flow`, `pressureOpacity`, `pressureSize`,
`velocityOpacity`, `tiltAngle`), grain (`grainTex`, `grainStrength`, `grainScale`) et `blendMode`. Sept
**presets statiques** prêts : `HardRound`, `SoftRound`, `Pencil`, `Ink`, `Watercolor`, `Airbrush`,
`Eraser`.

`NkBrushEngine` accumule les tampons le long du trait. Sa configuration est **inline** (`SetPreset`,
`SetColor`, `SetCanvas`), ses entrées sont `PointerDown(pos, pressure=1, tiltDeg=0, rotation=0)`,
`PointerMove(...)` (mêmes paramètres) et `PointerUp()`. Deux champs publics de **stabilisation** :
`stabilizerStrength` [0=off..1=Lazy Nezumi] et `stabilizerWindow` (défaut 10). `IsDrawing()` est inline.

> **Idiome** : `SetCanvas → SetPreset → SetColor → PointerDown/Move/Up`. Domaines : tablette graphique
> (la pression/tilt module taille et opacité), encrage net (`Ink`), aquarelle (`Watercolor`), aérographe
> (`Airbrush`), gommage (`Eraser`).

### `NkFillRule`, `NkStrokeCap`, `NkStrokeJoin`, `NkPathCmdType` — le vocabulaire du tracé

Quatre enums `uint8` : `NkFillRule` (`NonZero`, `EvenOdd` — comment remplir une forme à trous),
`NkStrokeCap` (`Butt`, `Round`, `Square` — bout de ligne), `NkStrokeJoin` (`Miter`, `Round`, `Bevel`
— angle entre segments), `NkPathCmdType` (`MoveTo=0, LineTo, CubicTo, QuadTo, ArcTo, Close` — les
commandes). Ce sont exactement les notions de SVG/PostScript.

### `NkGradientStop`, `NkPaint`, `NkStrokeStyle`, `NkPathCmd` — remplir et contourer

`NkGradientStop` : `offset` [0..1] + `color` `NkVec4f` (RGBA [0..1]), sans défauts.

`NkPaint` décrit **comment remplir** : son enum imbriqué `Type` (`None=0, Solid, LinearGradient,
RadialGradient, ConicGradient, Pattern, ImageFill`), une `solidColor`, les bornes/rayon de dégradé,
jusqu'à `kMaxStops = 16` arrêts, et un `imageHandle` + échelles pour le *image fill*. Ses **factories
inline** (réellement définies) : `Solid(color)`, `Black()`, `White()`, `Transparent()` (Solid a=0),
`LinearGrad(from, to, c0, c1)` (pose deux stops). Cas d'usage : dégradé de fond, motif répété, couleur
plate d'un aplat, remplissage par image (texture sur une forme).

`NkStrokeStyle` décrit **le contour** : `width`, `lineCap` (défaut `Round`), `lineJoin` (`Round`),
`miterLimit`, jusqu'à `kMaxDash = 8` segments de pointillé (`dash`, `dashCount`, `dashOffset`),
`enabled`. Aucune méthode — donnée pure. Cas d'usage : trait plein, pointillé d'un guide, contour
épais d'un cartoon.

`NkPathCmd` est **une commande** : `type`, six `float32 pts` (assez pour `CubicTo`), drapeaux d'arc
(`largeArc`, `sweepFlag`). Factories inline : `Move(x,y)`, `Line(x,y)`,
`Cubic(cx1,cy1,cx2,cy2,x,y)`, `Quad(cx,cy,x,y)`, `CloseCmd()` (noter **`CloseCmd`**, pas `Close`).

### `NkVectorPath` — le chemin à fond

Copie **autorisée** par défaut (les booléennes statiques s'en servent), constructeur/destructeur
`default`.

- **Construction fluide** (renvoient `NkVectorPath&`) — `MoveTo`, `LineTo`, `CubicTo`, `QuadTo`,
  `ArcTo(rx, ry, xRotDeg, largeArc, sweep, x, y)`, `Close`.
- **Formes haut niveau** (chaînables) — `AddRect(x,y,w,h,radius=0)`, `AddCircle(cx,cy,r)`,
  `AddEllipse(cx,cy,rx,ry)`, `AddStar(cx,cy,outerR,innerR,points)`,
  `AddRoundedRect(x,y,w,h,tl,tr,br,bl)` (un rayon par coin).
- **Transform in-place** (chaînables) — `Transform(mat3)`, `Translate`, `Scale`, `Rotate(deg, cx=0,
  cy=0)`.
- **Booléennes statiques** (renvoient un nouveau chemin) — `Union(a,b)`, `Subtract(a,b)`,
  `Intersect(a,b)`, `Exclude(a,b)`.
- **Métriques const** — `GetBoundingBox()`, `GetLength()`, `PointAtLength(t)` (t∈[0..1]),
  `TangentAtLength(t)`, `Contains(x, y)`.
- **Rendu const** — `DrawFill(r, fill, rule=NonZero, tolerance=0.5)`, `DrawStroke(r, paint, stroke)`,
  `Draw(r, fill, strokePaint, stroke, rule=NonZero)`.
- **SVG** — `ToSVGPath()` (`NkString`), `FromSVGPath(d)` (statique).
- **Accès inline** — `IsEmpty()`, `CmdCount()`, `Clear()`.

Domaines : **illustration** (logo, icône, forme nette), **rendu UI** (boutons arrondis, jauges,
formes vectorielles à toute échelle), **IO** (import/export SVG), **gameplay/éditeur** (zones de
collision dessinées à la main, `Contains` pour le hit-test ; `PointAtLength`/`TangentAtLength` pour
faire avancer un objet **le long** d'un chemin — caméra sur rail, projectile guidé), **animation**
(morphing entre chemins). Les booléennes servent à sculpter (découper un trou, fusionner deux formes).

> **Idiome** : `path.MoveTo(...).LineTo(...).Close()`. Les caches internes (bounds, longueur) sont
> `mutable` et invalidés par `Clear`/modification.

> Statut : tout est **spec** (sans corps) sauf les factories inline et les accesseurs `IsEmpty/
> CmdCount/Clear`.

### `NkSymmetryTool`, `NkPerspectiveGuide`, `NkStabilizer` — les assistants de dessin

`NkSymmetryTool` reflète automatiquement les coups de pinceau. Son `Mode` (`uint8`) : `None, MirrorH,
MirrorV, MirrorBoth, Radial, Kaleidoscope, Mandala, Point`. Champs : `mode`, `center`, `radialCount`
(défaut 6), `axisAngle`, `showGuide`. Méthodes : `GetMirrorPoints(pos, out)`,
`GetMirrorDabs(dab, center, out)` et `DrawGuides(r2d, canvasRect, zoom)`. Cas d'usage : dessiner un
visage symétrique, un motif radial (mandala), un kaléidoscope.

> **Dette** : `GetMirrorDabs` reçoit un `NkVector<std::pair<NkBrushDab, NkVec2f>>&` — usage de
> `std::pair`, en contradiction avec le zéro-STL.

`NkPerspectiveGuide` aide à dessiner en perspective. Son `Mode` : `OnePoint, TwoPoint, ThreePoint,
Isometric` (défaut `TwoPoint`). Trois `NkVanishingPoint points[3]` (chacun : `position`, `color`
bleutée, `enabled`, `lineCount`), plus `visible`, `snapEnabled`, `snapAngle`, `lockHorizon`,
`horizon`. Méthodes : `SnapStrokeAngle(from, to)` (aligne un trait sur la grille de fuite) et
`Draw(...)`. Cas d'usage : décor architectural, scène en perspective deux/trois points, isométrie.

`NkStabilizer` lisse le tracé tremblant. Son `Mode` : `None, Chaikin, LazyNezumi, Average, Kalman`
(défaut `LazyNezumi`). Champs : `strength`, `windowSize` (pour Average), `lazyRadius`. Méthodes :
`Process(rawPos, pressure=1)` (peut **revenir en arrière** en mode LazyNezumi), `Flush(out)`,
`Reset()`, et les inline `IsActive()` (→ `mode != None`) et `LastPos()`. Cas d'usage : encrage propre,
courbes lisses au stylet, lettrage. C'est le pendant document de `NkBrushEngine::stabilizerStrength`.

### `NkAnimationFrame` et `NkAnimationAssist` — l'animation image-par-image

`NkAnimationFrame` est **une image** : `index`, `duration` (défaut 1), un `NkRasterCanvas* canvas`
(**owned**), `visible`. Son destructeur fait `delete canvas` (dette NKMemory). Non-copiable.

`NkAnimationAssist` orchestre l'animation traditionnelle + l'**onion skin** (pelure d'oignon : voir
les frames voisines en transparence). Champs : `fps` (12), `loop`, `onionSkinBefore`/`After`,
`onionOpacity`, `onionColorBefore` (rouge = avant) / `After` (bleu = après), un
`NkVector<NkAnimationFrame*> frames` (**owned**), `currentFrame`, `playing`, `playbackTime`.

- **Lecture inline** — `Play()`, `Stop()` (reset du temps), `Pause()`. Déclaré : `Update(dt)`.
- **Frames** — `AddFrame(w, h)`, `DeleteFrame(idx)`, `DuplicateFrame(idx)`, `MoveFrame(from, to)`,
  `GetCurrent()`, `GetTotalDuration()`.
- **Rendu** — `DrawWithOnionSkin(r2d, dst)`.
- **Export** — `ExportGIF`, `ExportWebP`, `ExportMp4` (« nécessite ffmpeg »), `ExportFrames(dir,
  prefix="frame")`.

Son destructeur **inline** libère les frames (`for (auto* f : frames) delete f;` — dette CRT). Cas
d'usage : animation 2D dessinée à la main, cycles de marche, sprites animés exportés en feuille, GIF
d'aperçu.

### `NkHybridDocument` et `NkExportPreset` — le document raster+vecteur

`NkHybridDocument` est **tout public** (data-oriented). Champs : `name`/`filePath`, `width`/`height`
(2048), `dpi` (150), `layerStack`, les quatre outils (`symmetry`, `perspective`, `stabilizer`,
`animation`), `palettes`, `foreground`/`background`, `undoStack{200}`, métadonnées
`author`/`tags`/`license`. Méthodes : `SaveToFile`/`LoadFromFile` (`.nkart`), `ExportPNG(path,
scale=1)`, `ExportPSD` (« Limité »), `Composite(r2d, cmd, dst)`. C'est le fichier qu'un éditeur de
peinture ouvre et enregistre.

`NkExportPreset` paramètre un export : `name`, un `Target` (`Web, Print, Social, Animation, Vector,
HighRes, Custom`), un `OutputFormat` (`PNG, JPEG, TIFF, SVG, PDF, WebP, GIF, EXR`), dimensions/DPI
cibles, `jpegQuality` (85), drapeaux (`embedColorProfile`, `flattenAllLayers`, `exportAlpha`,
`includeMetadata`), `outputPath`, `filenameTemplate` ("{name}_{date}"). Statics : `Web()`, `Print()`,
`Social()`. Cas d'usage : un bouton « Exporter pour le web » qui applique le bon format/DPI/qualité.

> **`NkUndoStack`** (utilisé `{200}`) n'est **pas défini** dans ces sept headers — dépendance externe
> Noge.

### `NkVectorObjectType`, `NkVectorObject`, `NkVectorLayer`, `NkGuide`, `NkGrid`, `NkSymbol` — la matière vectorielle

`NkVectorObjectType` (`uint8`) : `Path, Text, Image, Group, Symbol`.

`NkVectorObject` est **un objet** sur un artboard : `type`, `name`, drapeaux (`visible`, `locked`,
`selected`), un **transform** (`position`, `rotation` deg, `scale`, `pivot`), une **apparence**
(`fill`, `stroke`, `strokeStyle`, `blendMode`, `opacity`), des données par type (un `path`+`fillRule`
pour Path ; `imageHandle`/`imageSize`/`imagePreserveAspect` pour Image ; `children` **non-owning**
pour Group ; `symbolId`/`symbolSize` pour Symbol) et un masque d'écrêtage optionnel (`clipMask`,
`hasClipMask`). Méthodes : `GetBoundingBox`, `GetTransformMatrix`, les inline `TranslateTo`/`RotateTo`/
`ScaleTo`/`SetPivot`, `Contains`, `Intersects`, et `Draw(r2d, parentOpacity=1)`. Cas d'usage : chaque
forme/texte/image manipulable d'une illustration ; `Contains`/`Intersects` pour la sélection.

`NkVectorLayer` regroupe des objets (**owned** ; destructeur les `delete`) : `name`, `visible`,
`locked`, `blendMode`, `opacity`, `expanded`, `objects`, `parentLayerId`. Méthodes : `AddPath(path,
fill, name)`, `AddGroup(name)`, `Remove`, `MoveUp`, `MoveDown`, `Draw`.

`NkGuide` est une règle d'alignement : `Orientation` (`Horizontal`/`Vertical`), `position`, `color`,
`locked`, `visible`. `NkGrid` une grille : `Type` (`Lines`, `Dots`, `Isometric`), `spacing`,
`divisions`, `color`/`subColor`, `visible`, `snapEnabled`, `snapRadius`. `NkSymbol` un composant
réutilisable : `id`, `name`, `bounds`, `objects` (**owned**, destructeur les `delete`). Cas d'usage :
guides et grille pour aligner ; symbole pour réutiliser un élément (icône, gabarit) — éditer le
symbole met à jour toutes ses instances.

### `NkArtboard` — le plan de travail

`NkArtboard` est une page. Champs : `name`, `width`/`height` (1920×1080), `background`, `clipContent`,
`layers` (**owned**, bottom→top).

- **Calques** — `AddLayer(name, parentId=0)`, `DeleteLayer(idx)`, `MoveLayer(from, to)`,
  `ActiveLayer()`, l'inline `SetActiveLayer(idx)`.
- **Guides/grille** — `guides`, `grid`, l'inline `AddGuide(orientation, pos)`.
- **Sélection** (**non-owning**) — `selection`, `Select(obj, addToSel=false)`, `Deselect`,
  `SelectAll`, `DeselectAll`, `SelectInRect(aabb)`.
- **Hit-test** — `HitTest(pos)` (renvoie l'objet sous le curseur).
- **Rendu** — `Draw(r2d, viewport, zoom=1)`. Destructeur : `delete` des layers.

Cas d'usage : la zone d'édition d'un artboard, avec son fond, ses guides, sa sélection au lasso/rect,
le clic pour sélectionner (`HitTest`).

### `NkVectorDocument` — le document Illustrator-like

Tout public. Champs : `name`/`filePath`, `unitsPerInch` (96), un enum `Units` (`Pixels, Millimeters,
Centimeters, Inches, Points`) + `units`, `artboards` (**owned**), `activeArtboard`, `symbols`,
`colorPalettes`, `undoStack{100}`, métadonnées `author`/`description`/`version`, et un `clipboard`
(**owned**).

- **Artboards** — `AddArtboard(name, w=1920, h=1080)`, `DeleteArtboard(idx)`, l'inline `GetActive()`
  (null-safe).
- **Symboles** — `AddSymbol(name)`, `FindSymbol(id)`.
- **Sérialisation** — `SaveToFile`, `LoadFromFile`, `ExportSVG(path, artboardIdx=0)`, `ExportPDF`,
  `ExportPNG(path, artboardIdx=0, scale=1)`.
- **Presse-papier** — `Copy`, `Cut`, `Paste`, `Duplicate`. Destructeur déclaré.

Cas d'usage : maquette multi-pages (un artboard par écran), affiche, jeu d'icônes (un artboard chacun),
export SVG/PDF pour l'impression ou le web.

> **Piège ownership** : `children` d'un `NkVectorObject` est **non-owning** (les objets appartiennent
> au layer), mais le `clipboard` du document est commenté **owned**, tandis que la `selection` d'un
> artboard est **non-owning**. Placer dans le `clipboard` un objet déjà owned par un layer **sans le
> copier** ouvre la porte au double-delete.

### `NkColorSpace` et `NkColor` — la couleur juste

`NkColorSpace` (`uint8`, 10 valeurs) : `LinearRGB, sRGB, HSL, HSV, LAB, LCH, CMYK, XYZ, OKLab, OKLch`
(`XYZ` et `OKLch` sont présents bien qu'absents du commentaire d'en-tête).

`NkColor` stocke en interne du **Linear RGB** [0..1] + alpha. Constructeurs : `default` et
`(r,g,b,a=1)` — **interprété comme Linear RGB**.

- **Factories** — `FromSRGB`, `FromHSL`, `FromHSV`, `FromLAB`, `FromLCH`, `FromCMYK`, `FromOKLab`,
  `FromHex("#RRGGBB[AA]")`, `FromU8(r,g,b,a=255)`.
- **Conversions const** — `ToLinearRGB()` (inline), `ToSRGB`, `ToHSL`, `ToHSV`, `ToLAB`, `ToLCH`,
  `ToCMYK`, `ToOKLab`, `ToHexString(withAlpha=false)`, `ToU8()` (`NkVec4f` [0..255]). *(Pas de
  `From/ToXYZ` ni `From/ToOKLch` malgré l'enum.)*
- **Accès inline** — `R/G/B/A`, `SetAlpha` (clampé).
- **Ajustements const** (renvoient `NkColor`) — `WithHue/Saturation/Lightness/Value/Alpha`, `Brighten`,
  `Darken`, `Saturate`, `Desaturate`, `Complement`, `Invert`.
- **Métriques perceptuelles const** — `Luminance`, `DeltaE` (CIE76), `DeltaE2000`, `ContrastRatio`,
  `IsAccessible(bg, minRatio=4.5)`, `BestContrast(dark, light)`.
- **Statiques d'interpolation** — `Lerp`, `LerpLAB`, `LerpOKLab`, `Mix`.
- **Blend modes const** (résultat Linear RGB) — `Multiply`, `Screen`, `Overlay`, `SoftLight`,
  `HardLight`.
- **Couleurs nommées** — inline `Black`, `White`, `Transparent` ; déclarées `Red`, `Green`, `Blue`,
  `Yellow`, `Cyan`, `Magenta`.
- **Opérateurs** — `==`/`!=`.

Domaines : **rendu** (mélanger en linéaire est physiquement correct), **UI/accessibilité**
(`ContrastRatio`/`IsAccessible` pour le WCAG, `BestContrast` pour choisir noir ou blanc sur un fond),
**design** (`LerpOKLab` pour des dégradés perceptuellement réguliers, `DeltaE2000` pour mesurer un
écart de couleur), **import** (`FromHex`/`FromU8` depuis un fichier ou l'écran).

> **Piège majeur** : le constructeur `NkColor(r,g,b,a)` est en **Linear RGB**, pas sRGB. Pour des
> valeurs écran, **`FromSRGB`** ou **`FromU8`** ; sinon les couleurs paraîtront trop sombres.

### `NkSwatch`, `NkPalette`, `NkHarmony`, `NkColorPicker` — palettes et harmonies

`NkSwatch` est une couleur nommée : `name` (`kMaxName = 64`), `color`, `selected`. Son constructeur
`(n, c)` utilise **`std::strncpy`** (dette zéro-STL).

`NkPalette` regroupe des swatches : `name`, `swatches`. Inline : `Add(name, color)`, `Count()`.
Déclarées : `Find(name)`, `Get(idx)`. Statics : `Material`, `Tailwind`, `Pastels`, `Monochrome`,
`WebSafe`. I/O : `SaveToFile`/`LoadFromFile` (`.ase`/`.aco`/`.json`). Cas d'usage : la nuancier d'un
projet, l'import/export de palettes standard de l'industrie.

`NkHarmony` génère des **harmonies** (tout statique, renvoient `NkVector<NkColor>`) :
`Complementary` (180°), `Triadic` (120°), `Tetradic` (90°), `Analogous(angle=30)`,
`SplitComplementary(split=30)`, `Monochromatic(count=5)`, `GradientOKLab(from, to, steps=5)`. Cas
d'usage : proposer une palette cohérente à partir d'une couleur de marque (design, UI, génération
procédurale de thèmes).

`NkColorPicker` est l'**état UI** d'un sélecteur : `currentColor`/`previousColor`, `displaySpace`
(défaut HSV), composantes HSV (`h/s/v`), Linear RGB (`r/g/b`), `a`, `hexStr`. Méthodes : `SetColor`,
`SyncFromHSV`/`SyncFromRGB`/`SyncFromHex` (resynchronise les champs après édition d'un curseur ou du
champ hex). Cas d'usage : le widget color-picker d'un éditeur.

### `NkColorManager` — le singleton de couleur

`NkColorManager` est un **singleton** (`Global()`, Meyers, inline ; constructeur privé). Champs :
`foreground`/`background`, `palettes`, un historique borné (`kHistorySize = 32`, `history`,
`historyCount`). Inline : `SwapFgBg()` (**`std::swap`** — dette zéro-STL), `ResetDefault()`,
`AddPalette(name)`. Déclarées : `PushHistory(color)`, `SampleScreen(x, y)` (pipette par readback
NKRHI). Cas d'usage : l'état global des couleurs d'avant-plan/arrière-plan d'une appli de dessin, la
pipette qui échantillonne l'écran, l'historique récent.

### `NkTextAlign`, `NkVerticalAlign`, `NkTextDecoration` — la mise en forme du texte

Trois enums `uint8` : `NkTextAlign` (`Left, Center, Right, Justify`), `NkVerticalAlign` (`Top, Middle,
Bottom`), `NkTextDecoration` (`None, Underline, Strikethrough, Overline`).

### `NkFontStyle`, `NkTextRun`, `NkRichText`, `NkTextOnPath` — la typographie

`NkFontStyle` décrit un style : `family` ("Arial"), `size` (14 pts), `bold`, `italic`, `decoration`,
`letterSpacing` (em), `lineHeight` (1.2 em), `color`. `NkTextRun` est une portion de texte stylée :
`text` + `style`.

`NkRichText` est du **texte multi-style** : `runs`, `align`, `vAlign`, `width` (0 = pas de retour à la
ligne). Méthodes : `AddRun(text, style)`, `Draw(r2d, pos)`, `MeasureSize()`. Cas d'usage : un
paragraphe mêlant gras/italique/couleurs, un label mesuré pour la mise en page.

`NkTextOnPath` pose du texte **le long d'un chemin** : `path`, `text` (un `NkRichText`), `startOffset`
([0..1] ou pixels selon l'unité), `side` (false = dessus, true = dessous). Méthode : `Draw(r2d)`. Cas
d'usage : un titre courbe, un texte qui suit un arc ou un cercle (sceau, badge, illustration
éditoriale).

> **Note de format** : `NkTextPath.h` ouvre `namespace nkentseu {` sans indentation et n'a pas de
> section privée — un style légèrement différent des autres headers, sans incidence sur l'API.

### Synthèse des pièges transverses

- **Statut spec** — toutes les méthodes non-inline (`Composite`, `Draw*`, `Export*`, `Save/Load*`,
  conversions `NkColor`, booléennes de chemin, tessellation, `Sync*` du picker, harmonies…) sont
  **déclarées sans corps** → à considérer non implémentées tant qu'aucun `.cpp` n'apparaît. Garantis :
  les inline (factories `NkColorRGBA`/`NkPaint`/`NkPathCmd`, accesseurs, `Play/Stop/Pause`, `AddGuide`,
  `GetActive`, `AddPalette`, `SwapFgBg`/`ResetDefault`, destructeurs `for…delete`).
- **Ownership brut + `delete` CRT** — `NkLayer`, `NkCanvasTile`, `NkAnimationFrame`, `NkVectorLayer`,
  `NkSymbol`, `NkArtboard`, `NkVectorDocument` libèrent par `delete`/`delete[]` → **viole la règle dure
  NKMemory** ; risque `c0000374` si mélangé avec `NkAlloc`.
- **Usages `std::`** — `std::pair` (`NkSymmetryTool::GetMirrorDabs`), `std::strncpy` (`NkSwatch`),
  `std::swap` (`NkColorManager::SwapFgBg`) : contradictions zéro-STL.
- **Deux `BlendMode`** — `NkBlendMode` global (27 + `COUNT`) pour les calques *vs* `NkBrushDab::BlendMode`
  imbriqué (14) pour les tampons/`Blit`. Toujours qualifier.
- **Dépendances non définies ici** — `NkUndoStack` (`{200}`/`{100}`) et `NkOffscreenTarget` (handle)
  viennent d'ailleurs dans Noge.
- **Enum vs commentaire** — plusieurs enums dépassent leur commentaire (`NkBlendMode` « 24 » → 27,
  `NkLayer::Type::Reference`, `NkColorSpace::XYZ`/`OKLch`).
- **`NkColorRGBA::FromFloat`** ne clampe pas (overflow `uint8` si > 1).
- **`NkColor` brut = Linear RGB**, pas sRGB.
- **`id` vs `index`** dans `NkLayerStack` ; **coords tile vs pixel** dans `NkRasterCanvas`.
- **Includes divergents** — `NkLayerStack/NkRasterCanvas/NkVectorPath` utilisent des chemins courts
  (`Design/Raster/...`, `NKRenderer/Tools/Render2D/...`) tandis que `NkHybridDocument/NkVectorDocument/
  NkTextPath` utilisent `Nkentseu/Design/...` et `NKRenderer/src/Tools/...` — risque de non-compilation
  selon les includedirs Jenga.

---

### Exemple

```cpp
#include "Noge/Doc/NkHybridDocument.h"
using namespace nkentseu;

// 1) Un document de peinture, avec sa pile de calques.
NkHybridDocument doc;
doc.width = 2048; doc.height = 2048;

NkLayer* paint = doc.layerStack.AddRasterLayer("Couleur");
NkLayer* ink   = doc.layerStack.AddVectorLayer("Encrage");
ink->blendMode = NkBlendMode::Multiply;          // calque : NkBlendMode

// 2) Peindre sur le calque raster avec le moteur de pinceau.
NkBrushEngine brush;
brush.SetCanvas(paint->rasterCanvas);
brush.SetPreset(NkBrushPreset::Watercolor());
brush.SetColor(NkColorRGBA::FromFloat(0.2f, 0.5f, 0.9f));  // attention : pas de clamp si >1
brush.PointerDown({400.f, 300.f}, 0.6f);
brush.PointerMove({520.f, 360.f}, 1.0f);
brush.PointerUp();
paint->rasterCanvas->FlushDirtyTiles(cmd);        // remonte les tuiles touchées

// 3) Tracer une forme vectorielle nette sur le calque d'encrage.
NkVectorPath shape;
shape.AddRoundedRect(100, 100, 300, 180, 24, 24, 24, 24);
shape.Draw(r2d, NkPaint::Transparent(), NkPaint::Black(), NkStrokeStyle{});

// 4) Couleur juste : choisir le texte le plus lisible sur un fond.
NkColor bg = NkColor::FromHex("#2D7FF9");          // valeur ÉCRAN → FromHex/FromSRGB
NkColor txt = bg.BestContrast(NkColor::Black(), NkColor::White());

// 5) Compositer le tout (spec — non implémenté tant qu'aucun .cpp).
doc.Composite(r2d, cmd, dstRect);
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
