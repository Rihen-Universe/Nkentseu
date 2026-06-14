# Le rendu de l'interface

> Couche **Runtime** · NKUI · Transformer une frame d'UI en **triangles** : la draw list
> `NkUIDrawList`, l'interface renderer `NkUIRenderer` (et son implémentation logicielle
> `NkUICPURenderer`), la police `NkUIFont` avec son atlas et son pont vers NKFont, et le
> **thème** `NkUITheme`.

Une bibliothèque d'interface en *immediate-mode* ne dessine rien elle-même. Chaque frame, le
code de l'application décrit ce qu'il veut voir — un bouton ici, une fenêtre là, du texte — et
toute cette description doit, au bout du compte, devenir des **triangles** que le GPU sait
afficher. C'est exactement le rôle de cette couche : un **producteur** de géométrie
(`NkUIDrawList`) qui accumule sommets, indices et commandes ; une **interface de
soumission** (`NkUIRenderer`) que chaque backend graphique implémente pour téléverser cette
géométrie ; une **police** (`NkUIFont`) qui transforme du texte en quads texturés ; et un
**thème** (`NkUITheme`) qui centralise toutes les couleurs et tous les espacements. La règle
qui structure tout : **on décrit pendant la frame, on dessine à la fin de la frame.**

Toute la philosophie tient dans la séparation entre *décrire* et *dessiner*. La draw list ne
connaît ni OpenGL, ni Vulkan, ni DirectX : elle remplit des tableaux. Le renderer ne connaît
pas les widgets : il reçoit des tableaux et les pousse sur le GPU. Ce découplage est ce qui
permet à la même UI de tourner sur cinq backends — et même **sans GPU** du tout, via le
renderer logiciel.

- **Namespace** : `nkentseu::nkui`
- **Headers** : `NKUI/NkUIDrawList.h`, `NKUI/NkUIRenderer.h`, `NKUI/NkUIFont.h`,
  `NKUI/NkUIFontBridge.h`, `NKUI/NkUITheme.h`
- **Macros** : `NKUI_API` (export), `NKUI_INLINE` (inline)

---

## La draw list : `NkUIDrawList`

C'est le **cœur** du rendu, l'objet vers lequel tout converge. Une draw list est trois
tableaux qui grandissent ensemble pendant la frame : les **sommets** (`vtx`, chacun une
position, des coordonnées de texture et une couleur compactée), les **indices** (`idx`, qui
relient les sommets en triangles), et les **commandes** (`cmds`, qui découpent la géométrie en
lots homogènes). Quand le code appelle `AddRectFilled` ou `AddText`, la draw list ne dessine
rien : elle **ajoute** des sommets et des indices à ses tableaux, et éventuellement une
commande pour signaler un changement d'état (un nouveau clip, une nouvelle texte).

Le point décisif est la **gestion mémoire**. La draw list est un **buffer arène fixe** :
`Init` réserve une fois pour toutes la capacité maximale (`maxVtx`, `maxIdx`, `maxCmds`), et
`Reset()`, appelé au début de chaque frame, remet les compteurs à zéro **sans libérer** —
aucune allocation, aucune libération pendant le rendu. C'est ce qui donne un coût de rendu
prévisible, frame après frame.

```cpp
NkUIDrawList dl;
dl.Init();                                  // arène : 65536 vtx, 196608 idx, 512 cmds
// ... chaque frame :
dl.Reset();                                 // remet les compteurs à zéro
dl.AddRectFilled(NkRect{10, 10, 200, 40}, NkColor::White, 6.f);
dl.AddText(NkVec2{20, 20}, "Bonjour", NkColor::Black, 14.f);
// dl.vtx / dl.idx / dl.cmds sont maintenant prêts à être soumis.
```

Ce n'est **pas** un *retained-mode scene graph* : la draw list ne garde aucun objet entre les
frames, elle est entièrement reconstruite à chaque `Reset()`. Et ce n'est **pas** un renderer :
elle ne sait pas parler au GPU ; elle ne fait que **produire** les données qu'un `NkUIRenderer`
consommera.

> **En résumé.** `NkUIDrawList` accumule sommets / indices / commandes dans une **arène
> fixe** allouée par `Init` et remise à zéro par `Reset()` chaque frame — zéro malloc en cours
> de rendu. Elle **produit** la géométrie ; elle ne la dessine pas.

### Les primitives, le clip et le path builder

Au-dessus des tableaux bruts, la draw list expose une **batterie de primitives** : triangles
et lignes, rectangles (carrés ou à coins arrondis, avec dégradés multicolores), cercles,
ellipses, arcs, courbes de Bézier, texte (simple ou avec retour à la ligne), images texturées.
Toutes finissent par écrire dans les mêmes trois tableaux.

Deux mécanismes méritent qu'on s'y attarde. D'abord le **clip rect** : `PushClipRect` /
`PopClipRect` empilent des rectangles de découpe (jusqu'à 32 de profondeur) — tout ce qui est
dessiné ensuite est confiné à l'intersection courante. C'est ce qui permet à une zone
scrollable de ne pas déborder. Ensuite le **path builder**, à la manière du SVG : on construit
un tracé point par point (`PathMoveTo`, `PathLineTo`, `PathArcTo`, `PathBezierCubicTo`…) dans
un buffer interne, puis on le **remplit** (`PathFill`) ou on le **trace** (`PathStroke`). C'est
l'outil des formes complexes que les primitives toutes faites ne couvrent pas.

> **En résumé.** Au-dessus des trois tableaux, la draw list offre des **primitives**
> (rect/cercle/arc/Bézier/texte/image), un **clip rect** empilable (max 32) pour confiner le
> dessin, et un **path builder** SVG-like pour les formes libres.

---

## Le renderer : `NkUIRenderer` et `NkUICPURenderer`

La draw list a produit la géométrie ; il faut maintenant l'**afficher**. C'est le rôle de
`NkUIRenderer`, une **interface abstraite** minimale : trois méthodes pures — `BeginFrame`,
`Submit`, `EndFrame` — qui structurent une frame de rendu. Chaque backend graphique
(OpenGL, Vulkan, DirectX, logiciel) hérite de cette interface et implémente ces trois
méthodes ; le reste de NKUI ne connaît que l'interface, jamais le backend concret.

Le pivot est `Submit(const NkUIContext& ctx)` : le renderer reçoit le **contexte** d'UI
(défini ailleurs, dans `NkUIContext.h`), en extrait la draw list, et pousse ses sommets et
indices vers la cible de rendu. Autour, trois méthodes virtuelles avec implémentation par
défaut gèrent les **textures** — `UploadTexture` (téléverser des pixels et obtenir un handle),
`FreeTexture`, `GetFontAtlas` — pour que la police et les images aient un identifiant de
texture exploitable.

```cpp
class MyGLRenderer : public NkUIRenderer {
public:
    void BeginFrame(int32 w, int32 h) noexcept override { /* viewport, projection */ }
    void Submit(const NkUIContext& ctx) noexcept override { /* upload vtx/idx, draw */ }
    void EndFrame() noexcept override { /* flush */ }
};
```

Le seul renderer concret livré dans ces headers est `NkUICPURenderer` : un rasteriseur
**logiciel**, sans GPU, qui dessine la draw list dans un **buffer de pixels RGBA32**. On
l'initialise avec une taille (`Init(w, h)`), on déroule la frame (`BeginFrame` / `Submit` /
`EndFrame`), puis on récupère le résultat via `GetPixels()` — et `SavePNG` peut l'écrire sur
disque (via NKImage). C'est l'outil idéal des **tests automatisés** (comparer une frame d'UI à
une image de référence), du **rendu offline** (capturer une interface sans ouvrir de fenêtre)
et du **débogage** (inspecter ce que la draw list a réellement produit).

Ce n'est **pas** un backend GPU : il est volontairement lent et destiné au hors-ligne. Et
attention au cycle de vie — `GetPixels()` n'est valide **qu'après** `EndFrame()`.

> **En résumé.** `NkUIRenderer` est l'**interface** (3 pures : `BeginFrame`/`Submit`/
> `EndFrame` ; + gestion de textures par défaut) que chaque backend implémente.
> `NkUICPURenderer` en est l'unique implémentation concrète de ces headers : un rasteriseur
> **logiciel** vers un buffer RGBA, parfait pour les tests et le rendu offline.

---

## La police : `NkUIFont`, son atlas et le pont NKFont

Le texte est le cas le plus délicat : chaque caractère est une petite image qu'il faut
rasteriser, ranger dans un **atlas** (une grande texture qui regroupe tous les glyphes), puis
dessiner comme un quad texturé qui échantillonne le bon morceau de l'atlas. NKUI structure tout
cela en quatre acteurs qui s'emboîtent.

`NkUIFontAtlas` est l'atlas lui-même : une texture **Gray8 de 512×512**, gérée par un *shelf
packer* simple qui place les glyphes en rangées. On y **alloue** une case (`Alloc`), on y
**ajoute** un glyphe avec ses métriques (`AddGlyph`), on **retrouve** un glyphe par son
codepoint (`Find`), et on le **téléverse** sur le GPU (`UploadToGPU`). `NkUIFont` est une
**police à une taille donnée** : elle pointe vers un atlas (ou, à défaut, vers une **police
bitmap intégrée** 6×10 couvrant l'ASCII), connaît ses métriques (ascender, descender, hauteur
de ligne), et sait **mesurer** (`MeasureWidth`, `FitChars`) et **rendre** du texte directement
dans une draw list (`RenderText`, `RenderTextWrapped`, `RenderChar`).

`NkUIFontManager` est le **conteneur optionnel** qui orchestre plusieurs polices et atlas : il
ajoute la police bitmap intégrée (`AddBuiltin`), charge des fichiers ou de la mémoire TTF/OTF
(`LoadFromFile`, `LoadFromMemory`), des polices **embarquées** (`LoadEmbedded`) ou un **backend
custom** (`LoadCustom`), et téléverse les atlas modifiés (`UploadDirtyAtlases`). Enfin,
`NkUIFontBridge` est le **pont** qui remplit un atlas depuis une vraie police vectorielle :
soit via le module **NKFont** (le comportement par défaut, `useNKFont=true`), soit via un jeu
de **callbacks** que l'application fournit (`NkUIFontLoaderDesc`).

```cpp
NkUIFontManager fonts;
fonts.Init();
uint32 builtin = fonts.AddBuiltin(14.f);                 // police bitmap intégrée
int32  roboto  = fonts.LoadFromFile("Roboto.ttf", 16.f); // via NKFont (pont automatique)
fonts.UploadDirtyAtlases(/* uploadFunc */);              // pousse les atlas sur le GPU
fonts.Default()->RenderText(dl, NkVec2{20, 20}, "Texte", NkColor::Black);
```

Ce n'est **pas** un moteur de typographie complet : NKUI ne rasterise pas les contours lui-même
(c'est NKFont, ou votre backend custom, qui le fait) ; NKUI **range** les glyphes dans l'atlas
et les **dessine**. Et le pont sépare nettement les deux : un backend custom doit fournir
**cinq** callbacks (`LoadFont`, `GetGlyph`, `GetBBox`, `GetMetrics`, `Destroy`) — `userData`
n'est **pas** requis.

> **En résumé.** Le texte passe par un **atlas** (`NkUIFontAtlas`, Gray8 512×512, shelf
> packer), une **police par taille** (`NkUIFont`, atlas ou bitmap intégré), un **gestionnaire**
> (`NkUIFontManager`) et un **pont** vers NKFont ou un backend custom (`NkUIFontBridge`). NKUI
> range et dessine ; la rasterisation vient d'ailleurs.

---

## Le thème : `NkUITheme`

Une interface cohérente ne code pas ses couleurs en dur dans chaque widget : elle les puise
dans un **thème** central. `NkUITheme` regroupe tout ce qui définit l'apparence — une
**palette de couleurs** sémantique (`NkUIColorPalette`), des **métriques** d'espacement et de
taille (`NkUIMetrics`), des **définitions de polices** (`NkUIFonts`), et des **réglages
d'animation** (`NkUIAnimDef`). Changer de thème, c'est changer cet objet ; tous les widgets en
héritent automatiquement.

La palette est **sémantique**, et c'est l'idée clé : on n'y nomme pas « gris clair » mais
`bgPrimary`, pas « bleu » mais `accent`. Les couleurs disent leur **rôle** (fond, bordure,
texte, accent, état de succès/avertissement/danger) et non leur valeur. C'est ce qui permet de
basculer du mode clair au mode sombre sans toucher au code des widgets : le moteur fournit
d'ailleurs quatre presets prêts à l'emploi — `Default()`, `Dark()`, `Minimal()`,
`HighContrast()`.

```cpp
NkUITheme theme = NkUITheme::Dark();
NkColor bg     = theme.colors.bgPrimary;
float32 pad    = theme.metrics.paddingX;     // 10
float32 radius = theme.metrics.cornerRadius; // 5
```

> **En résumé.** `NkUITheme` centralise l'apparence : palette **sémantique**
> (`NkUIColorPalette`), métriques (`NkUIMetrics`), polices (`NkUIFonts`), animation
> (`NkUIAnimDef`), avec quatre presets (`Default/Dark/Minimal/HighContrast`). On nomme les
> couleurs par leur **rôle**, jamais par leur valeur.

---

## Aperçu de l'API

La liste de tous les éléments publics, regroupés par header. Le détail (formules, cas d'usage)
suit dans la « Référence complète ».

### `NkUIDrawList.h` — draw list & primitives

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Sommet/commande | `NkUIVertex` (`pos`, `uv`, `col`) | Sommet : position, UV ([0,1], (0,0)=solide), couleur RGBA compactée. |
| Sommet/commande | `NkUIDrawCmdType` (enum) | `NK_TRIANGLES`, `NK_TEXTURED_TRIS`, `NK_CLIP_RECT`, `NK_SET_FONT`. |
| Sommet/commande | `NkUIDrawCmd` (`type`, `idxOffset`, `idxCount`, `texId`, `clipRect`) | Un lot homogène d'indices à dessiner. |
| Données | `vtx`/`idx`/`cmds` + compteurs + capacités | Tableaux arène (géométrie + commandes). |
| Données | `clipStack[32]`/`clipDepth`, `opaqueRects[64]`/`opaqueCount` | Pile de clip ; rectangles opaques (occlusion). |
| Données | `fillColor`, `strokeColor`, `strokeWidth`, `cornerRadius`, `fontId` | Style courant. |
| Cycle de vie | `Init(maxVtx, maxIdx, maxCmds)`, `Destroy()`, `Reset()` | Réserver l'arène / libérer / réinit début de frame. |
| Clip | `PushClipRect(r, intersect)`, `PopClipRect()`, `GetClipRect()` | Empiler/dépiler/lire le rectangle de découpe. |
| Primitives | `AddTriangle`/`AddTriangleFilled`, `AddLine`, `AddPolyline`, `AddConvexPolyFilled` | Triangles, lignes, polylignes, polygone convexe plein. |
| Rectangles | `AddRect`, `AddRectFilled`, `AddRectFilledMultiColor`, `AddRectFilledCorners` | Cadre, plein, dégradé 4 coins, coins sélectifs (masque). |
| Cercles/ellipses | `AddCircle`/`AddCircleFilled`, `AddEllipse`/`AddEllipseFilled` | Contour / plein (`segs=0` ⇒ auto). |
| Arcs/Bézier | `AddArc`/`AddArcFilled`, `AddBezierCubic`, `AddBezierQuadratic` | Arcs et courbes de Bézier. |
| Texte | `AddText`, `AddTextWrapped` | Texte (`size=0` ⇒ taille par défaut) / avec retour à la ligne. |
| Images | `AddImage`, `AddImageRounded` | Quad texturé (UV + tint) / à coins arrondis. |
| Path builder | `PathClear`, `PathMoveTo`, `PathLineTo`, `PathArcTo`, `PathBezierCubicTo`, `PathBezierQuadTo`, `PathRect`, `PathFill`, `PathStroke` | Tracé SVG-like puis remplissage / contour. |
| Helpers widgets | `AddShadow`, `AddCheckMark`, `AddArrow`, `AddResizeGrip`, `AddScrollbarThumb`, `AddSpinner`, `AddColorWheel` | Composites prêts pour les widgets. |

### `NkUIRenderer.h` — interface renderer + renderer CPU

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Interface | `NkUIRenderer` (abstrait) | Base de tout backend de rendu UI. |
| Interface (pures) | `BeginFrame(w, h)`, `Submit(ctx)`, `EndFrame()` | Démarrer / soumettre le contexte / terminer la frame. |
| Interface (défaut) | `UploadTexture(pixels, w, h, channels)`, `FreeTexture(id)`, `GetFontAtlas()` | Gestion de textures (implémentations par défaut). |
| Renderer CPU | `NkUICPURenderer` (final) | Rasteriseur logiciel vers buffer RGBA, sans GPU. |
| Renderer CPU | `Init(w, h)`, `Destroy()` | Allouer / libérer le buffer de pixels. |
| Renderer CPU | `GetPixels()`, `GetWidth()`, `GetHeight()`, `GetStride()` | Accès au résultat (valide après `EndFrame`). |
| Renderer CPU | `SavePNG(path)` | Écrire le buffer sur disque (via NKImage). |

### `NkUIFont.h` — police UI, atlas, gestionnaire

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config | `NkUIFontConfig` (`yAxisUp`, `enableAtlas`, `enableBitmapFallback`, `defaultFontSize`) | Réglages de police. |
| Glyphe/métriques | `NkUIGlyph`, `NkUIFontMetrics` | Données d'un glyphe (atlas + UV + avance) / métriques de police. |
| Atlas | `NkUIFontAtlas` (Gray8 512×512, `MAX_GLYPHS=1024`) | Texture d'atlas + shelf packer. |
| Atlas | `Alloc`, `Find`, `AddGlyph`, `Clear`, `UploadToGPU`, `DumpStats` | Placer / retrouver / ajouter / vider / téléverser. |
| Police | `NkUIFont` (`atlas` ou bitmap 6×10) | Police à une taille donnée. |
| Police | `MeasureWidth`, `FitChars`, `RenderText`, `RenderTextWrapped`, `RenderChar`, `GetBFH()` | Mesurer / ajuster / rendre / hauteur bitmap. |
| Gestionnaire | `NkUIFontManager` (`MAX_FONTS=16`, `MAX_ATLAS=4`) | Conteneur de polices/atlas. |
| Gestionnaire | `Init`, `Destroy`, `AddBuiltin`, `AddFromAtlas`, `Get`, `Default`, `UploadDirtyAtlases`, `SetGlobalYAxisUp` | Cycle de vie + accès. |
| Gestionnaire (chargement) | `LoadFromFile`, `LoadFromMemory`, `LoadCustom`, `LoadEmbedded` | Charger TTF/OTF / backend custom / police embarquée. |

### `NkUIFontBridge.h` — pont NKFont / backend custom

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Callbacks | `NkUIFontLoadFn`, `NkUIFontGetGlyphFn`, `NkUIFontGetMetricsFn`, `NkUIFontGetBBoxFn`, `NkUIFontDestroyFn` | Signatures d'un backend de police custom. |
| Descripteur | `NkUIFontLoaderDesc` (5 callbacks + `userData`) + `IsValid()` | Décrit un backend custom (valide si les 5 callbacks non nuls). |
| Pont | `NkUIFontBridge` (`useNKFont`, `customDesc`, `nkfontFace`…) | Remplit un atlas depuis TTF/OTF (NKFont ou custom). |
| Pont | `InitFromFile`, `InitFromMemory`, `InitCustom`, `Destroy` | Initialiser le pont / libérer. |
| Pont (plages) | `RangesASCII`, `RangesLatinExtended`, `RangesDefault` | Plages Unicode prédéfinies. |

### `NkUITheme.h` — thème

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Palette | `NkUIColorPalette` (fond, bordures, texte, accents, widgets, fenêtres/dock, états) | Couleurs **sémantiques**. |
| Métriques | `NkUIMetrics` (padding, espacement, coins, hauteurs, fenêtres, dock, widgets) | Dimensions et espacements. |
| Polices | `NkUIFontDef`, `NkUIFonts` (`body`, `small`, `large`, `heading`, `mono`, `icon`) | Définitions de polices du thème. |
| Animation | `NkUIAnimDef` (durées + easing + `enabled`) | Réglages d'animation des widgets. |
| Thème | `NkUITheme` (`colors`, `metrics`, `fonts`, `anim`) | Agrège le tout. |
| Presets | `Default()`, `Dark()`, `Minimal()`, `HighContrast()` | Thèmes prêts à l'emploi. |
| Énumérations | `NkUIWidgetType`, `NkUIWidgetState` (flags) | Type de widget / état (drapeaux bitwise). |
| Fonctions libres | `operator|`, `HasState` | Combiner / tester des drapeaux d'état. |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses cas d'usage à travers les différents
domaines d'une application — outils, jeux, éditeurs, applications scientifiques, débogage.

### `NkUIVertex`, `NkUIDrawCmdType`, `NkUIDrawCmd` — l'unité de géométrie

Tout ce que dessine NKUI se réduit à des `NkUIVertex` : une **position** (`pos`), des
**coordonnées de texture** (`uv`, dans `[0,1]`) et une **couleur compactée** (`col`, RGBA dans
un seul `uint32`). La convention d'UV est subtile et utile : `uv = (0,0)` désigne le pixel
**solide** de l'atlas — autrement dit, un sommet à `(0,0)` est peint en couleur unie, sans
échantillonner de texture. C'est ce qui permet aux formes pleines et au texte de partager le
même shader.

Les sommets sont reliés en triangles par le tableau d'**indices**, et découpés en lots par les
`NkUIDrawCmd`. Une commande dit « dessine `idxCount` indices à partir de `idxOffset`, avec ce
`texId` et ce `clipRect` ». Le `NkUIDrawCmdType` qualifie le lot :

- `NK_TRIANGLES` — géométrie pleine, sans texture (formes, bordures).
- `NK_TEXTURED_TRIS` — géométrie texturée (texte, images).
- `NK_CLIP_RECT` — changement de rectangle de découpe.
- `NK_SET_FONT` — changement de police active.

Ce découpage en commandes est ce qui permet à un backend de **minimiser les changements
d'état** : il regroupe tout ce qui partage la même texture et le même clip en un seul appel de
dessin.

### `NkUIDrawList` — l'arène de rendu

**La distinction fondamentale : produire, pas dessiner.** La draw list ne fait qu'**écrire**
dans `vtx`, `idx` et `cmds`. C'est un buffer **arène fixe** : `Init(maxVtx, maxIdx, maxCmds)`
réserve une fois (par défaut 65536 sommets, 196608 indices, 512 commandes), et `Reset()` remet
les compteurs à zéro à chaque frame **sans libérer** — d'où l'absence totale de malloc/free
pendant le rendu, et un coût prévisible. Les pointeurs `vtx`/`idx`/`cmds` ne sont valides
**qu'après** `Init`.

**Le style courant.** `fillColor`, `strokeColor`, `strokeWidth`, `cornerRadius`, `fontId`
servent de valeurs par défaut implicites aux helpers composites — on les fixe une fois, les
widgets les consultent.

**L'occlusion.** Le tableau `opaqueRects` (max 64, réinitialisé par `Reset()`) garde trace des
rectangles **opaques** déjà dessinés ; ce qui est entièrement caché derrière peut être sauté.
C'est une optimisation utile dans une UI dense (panneaux qui se recouvrent).

Cas d'usage, par domaine :
- **Outils & éditeurs** — toute l'interface d'un éditeur (panneaux, inspecteurs, timeline)
  passe par une seule draw list reconstruite chaque frame.
- **Jeux** — HUD, menus, overlays de debug : on `Reset()` puis on redécrit, rien à conserver.
- **Débogage** — afficher des statistiques, des graphes, des superpositions par-dessus la scène.
- **Applications scientifiques** — graphes, jauges, annotations dessinées au-dessus d'une vue 3D.

### Le clip rect et l'occlusion

`PushClipRect(r, intersect)` empile un rectangle de découpe ; quand `intersect=true` (le
défaut), il s'**intersecte** avec le clip courant, de sorte qu'un enfant ne déborde jamais de
son parent. `PopClipRect()` revient au précédent, et `GetClipRect()` lit le clip actif (ou un
rectangle quasi infini `{0,0,1e9,1e9}` si la pile est vide). La pile est **plafonnée à 32**
niveaux — largement suffisant pour une hiérarchie d'UI, mais à ne pas dépasser.

Le clip est ce qui rend possibles les **zones scrollables** (le contenu qui dépasse est
coupé), les **fenêtres** (rien ne fuit hors du cadre) et les **listes virtuelles** (on ne
dessine et n'affiche que ce qui tombe dans la vue).

### Les primitives basse couche et les rectangles

Au plus bas niveau, `AddTriangle`/`AddTriangleFilled` posent un triangle contour ou plein,
`AddLine` une ligne d'épaisseur donnée, `AddPolyline` une suite de segments (optionnellement
fermée), et `AddConvexPolyFilled` remplit un polygone **convexe**. Ce sont les briques sur
lesquelles tout le reste est bâti.

Les rectangles sont si fréquents qu'ils ont leur propre famille. La convention des coins
arrondis est `rx[0]=TL, 1=TR, 2=BR, 3=BL`, `0` valant carré :

- `AddRect` — un cadre (épaisseur `thickness`), coins arrondis optionnels.
- `AddRectFilled` — un rectangle plein, coins arrondis optionnels (le pain quotidien des fonds
  de boutons et de panneaux).
- `AddRectFilledMultiColor` — un rectangle plein avec **une couleur par coin**, donc un dégradé
  bilinéaire (fonds dégradés, barres de progression colorées, vignettes).
- `AddRectFilledCorners` — un rectangle à rayon uniforme mais avec un **masque de coins** (bits
  0-3 = TL|TR|BR|BL) : n'arrondir que le haut d'un onglet, que la gauche d'un champ, etc.

### Cercles, ellipses, arcs, Bézier

`AddCircle`/`AddCircleFilled` et `AddEllipse`/`AddEllipseFilled` dessinent contours et pleins ;
`segs=0` laisse la draw list **choisir automatiquement** le nombre de segments selon le rayon
(plus c'est gros, plus c'est lisse). Les **arcs** (`AddArc`/`AddArcFilled`, bornés par deux
angles `a0`/`a1`) servent aux jauges, aux *spinners*, aux camemberts. Les **courbes de Bézier**
cubiques (`AddBezierCubic`, quatre points de contrôle) et quadratiques (`AddBezierQuadratic`,
trois points) tracent des connexions lisses — idéales pour les **graphes de nœuds** (les fils
courbes entre prises) et les courbes d'animation.

### Texte et images

`AddText(pos, text, col, size, fontId)` dessine une chaîne (avec `size=0` ⇒ la taille par
défaut de la police, `fontId` choisissant la police) ; `AddTextWrapped(bounds, …)` la replie
dans un rectangle. Côté images, `AddImage(texId, dst, uvMin, uvMax, tint)` pose une texture
dans un rectangle avec une sous-région UV et une teinte (icônes, vignettes, atlas), et
`AddImageRounded` fait de même avec des coins arrondis (avatars, miniatures arrondies).

### Le path builder

Pour les formes que les primitives toutes faites ne couvrent pas, le path builder construit un
tracé à la manière du SVG, dans un buffer interne (jusqu'à 4096 points) : `PathClear` repart de
zéro, puis `PathMoveTo` / `PathLineTo` / `PathArcTo` / `PathBezierCubicTo` / `PathBezierQuadTo`
/ `PathRect` ajoutent des segments, et enfin `PathFill(col)` remplit le tracé ou
`PathStroke(col, thickness, closed)` le trace. C'est l'outil des **formes libres** : icônes
vectorielles, courbes de réponse, contours arbitraires, infographies.

### Les helpers composites

Pour ne pas réécrire les mêmes dessins dans chaque widget, la draw list fournit des composites
prêts à l'emploi, utilisés par la couche widgets :

- `AddShadow` — une ombre portée (rayon, couleur, décalage) sous un rectangle (cartes,
  fenêtres, menus flottants).
- `AddCheckMark` — la coche d'une case cochée.
- `AddArrow` — une flèche directionnelle (`dir`) : combos, arbres, en-têtes de tri.
- `AddResizeGrip` — la poignée de redimensionnement d'une fenêtre (coin bas-droit).
- `AddScrollbarThumb` — le curseur d'une barre de défilement (vertical ou horizontal).
- `AddSpinner` — un indicateur d'activité tournant (chargement).
- `AddColorWheel` — une roue chromatique teinte/saturation/valeur (sélecteur de couleur).

### `NkUIRenderer` — l'interface de soumission

Trois méthodes **pures** définissent une frame : `BeginFrame(w, h)` prépare la cible (viewport,
projection), `Submit(ctx)` consomme le `NkUIContext` (et la draw list qu'il contient) pour
téléverser et dessiner la géométrie, `EndFrame()` finalise. Un backend GPU custom n'a qu'à
hériter et implémenter ces trois méthodes.

Trois méthodes virtuelles avec **implémentation par défaut** gèrent les textures, parce que le
texte et les images en ont besoin : `UploadTexture(pixels, w, h, channels)` renvoie un handle
(0 par défaut), `FreeTexture(id)` le libère (no-op par défaut), `GetFontAtlas()` renvoie le
handle de l'atlas de police (0 par défaut). Un backend qui veut afficher du texte **doit**
redéfinir au moins `UploadTexture` et `GetFontAtlas`.

> Note : `NkUIContext` n'est **pas** défini dans ces headers ; il vient de `NkUIContext.h` et
> n'est manipulé que comme entrée de `Submit`.

### `NkUICPURenderer` — le rendu logiciel

C'est l'unique renderer **concret** livré ici : un rasteriseur **logiciel** qui dessine la draw
list dans un buffer de pixels **RGBA32**, sans toucher au GPU. On l'initialise par
`Init(w, h)`, on déroule la frame normalement (`BeginFrame` → `Submit` → `EndFrame`), puis on
lit le résultat :

- `GetPixels()` — le buffer RGBA (valide **uniquement après** `EndFrame()`).
- `GetWidth()` / `GetHeight()` — les dimensions.
- `GetStride()` — l'enjambée d'une ligne, soit largeur × 4 (format RGBA).
- `SavePNG(path)` — écrit directement le buffer en PNG (nécessite NKImage).

Ses usages sont **hors-ligne** par nature :
- **Tests automatisés** — rendre une frame d'UI et la comparer pixel à pixel à une référence
  (tests de non-régression visuelle).
- **Rendu offline** — générer l'aperçu d'une interface, une vignette, une documentation, sans
  ouvrir de fenêtre ni de contexte GPU.
- **Débogage** — capturer exactement ce que la draw list a produit, indépendamment du backend.

Il n'est **pas** prévu pour le temps réel ; pour cela, il faut un backend GPU implémentant
`NkUIRenderer`.

### `NkUIFontConfig`, `NkUIGlyph`, `NkUIFontMetrics` — les données de police

`NkUIFontConfig` règle le comportement : `yAxisUp` choisit l'orientation de l'axe Y
(`false` = Y vers le bas, convention OpenGL/écran ; `true` = Y vers le haut, convention
mathématique) — un détail crucial pour que le texte ne soit pas à l'envers selon le backend ;
`enableAtlas` et `enableBitmapFallback` activent l'atlas et le repli bitmap ; `defaultFontSize`
fixe la taille par défaut (14). `NkUIGlyph` décrit un caractère placé : son codepoint, sa
**position dans l'atlas** (`x0,y0,x1,y1`), ses **UV** (`u0,v0,u1,v1`), et ses métriques de pose
(`advanceX`, `bearingX`, `bearingY`). `NkUIFontMetrics` porte les métriques globales d'une
police : `ascender`, `descender`, `lineGap`, `lineHeight`, `spaceWidth`, `tabWidth`.

### `NkUIFontAtlas` — l'atlas de glyphes

L'atlas est une texture **Gray8 de 512×512** (`ATLAS_W`/`ATLAS_H`), pouvant contenir jusqu'à
`MAX_GLYPHS=1024` glyphes, peuplée par un **shelf packer** simple (`shelfX`/`shelfY`/`shelfH`
suivent la position courante de remplissage). On `Alloc(w, h, outX, outY)` une case pour un
glyphe, on l'enregistre via `AddGlyph(...)`, on le retrouve par codepoint avec `Find(cp)`, on
remet l'atlas à zéro avec `Clear()`, et on le téléverse sur le GPU via
`UploadToGPU(uploadFunc)` — où `uploadFunc` est un **pointeur de callback** (typé erased,
`nullptr` ⇒ pas d'upload). Le drapeau `dirty` signale qu'il faut re-téléverser. `DumpStats()`
aide au débogage du remplissage.

### `NkUIFont` — la police à une taille

`NkUIFont` représente une police **à une taille donnée**. Elle s'appuie soit sur un `atlas`
(rasterisation vectorielle), soit, si `atlas == nullptr`, sur une **police bitmap intégrée**
6×10 couvrant l'ASCII 32-127 (un repli toujours disponible, sans aucun fichier). Elle expose le
nécessaire pour le texte :

- `MeasureWidth(text, maxLen)` — largeur d'une chaîne (mise en page, centrage, troncature).
- `FitChars(text, maxWidth)` — combien de caractères tiennent dans une largeur (ellipsis,
  césure).
- `RenderText(dl, pos, text, col, maxWidth, ellipsis)` — dessine du texte dans la draw list,
  avec largeur max et points de suspension optionnels.
- `RenderTextWrapped(dl, bounds, text, col, lineSpacing)` — dessine avec retour à la ligne dans
  un rectangle.
- `RenderChar(dl, pos, codepoint, col)` — un seul caractère.
- `GetBFH()` — la hauteur de la police bitmap intégrée (10).

### `NkUIFontManager` — orchestrer les polices

Conteneur **optionnel** (on peut s'en passer pour un cas simple), il gère jusqu'à
`MAX_FONTS=16` polices et `MAX_ATLAS=4` atlas. `Init` / `Destroy` encadrent son cycle de vie.
On ajoute des polices de plusieurs façons :

- `AddBuiltin(size)` — la police bitmap intégrée, toujours disponible.
- `AddFromAtlas(name, size, atlas, metrics)` — une police construite sur un atlas déjà rempli.
- `LoadFromFile(path, sizePx, name, ranges)` — charger un TTF/OTF depuis un fichier (via
  NKFont), renvoie l'index ou -1.
- `LoadFromMemory(data, dataSize, …)` — idem depuis un buffer mémoire.
- `LoadEmbedded(id, sizePx, …)` — une police **embarquée** dans le binaire (`NkEmbeddedFontId`).
- `LoadCustom(path, sizePx, desc, …)` — via un **backend custom** (`NkUIFontLoaderDesc`).

Pour l'accès, `Get(idx)` renvoie une police (avec **clamp** : hors borne ⇒ `fonts[0]`, ou
`nullptr` si vide) et `Default()` la première. `UploadDirtyAtlases(uploadFunc)` pousse sur le
GPU les atlas marqués sales, et `SetGlobalYAxisUp(yUp)` fixe l'orientation Y pour tous. Quand
`ranges == nullptr`, le chargement couvre ASCII + Latin-1.

L'idiome manuel (sans passer par `LoadFromFile`) est : créer un atlas → `Clear()` → rasteriser
les glyphes via NKFont (`Alloc` + copie des pixels + `AddGlyph`) → `UploadToGPU` → remplir le
`NkUIFont`.

### `NkUIFontBridge` et `NkUIFontLoaderDesc` — le pont

Le pont est ce qui **remplit un atlas** depuis une vraie police vectorielle. Par défaut
(`useNKFont=true`), il s'appuie sur le module **NKFont** : `InitFromFile` /
`InitFromMemory` chargent un TTF/OTF, rasterisent les glyphes des plages demandées dans l'atlas
et génèrent le `NkUIFont` associé. `Destroy()` libère le tout.

Pour brancher un **moteur de police tiers**, on passe par `InitCustom` et un
`NkUIFontLoaderDesc` : une structure de **cinq callbacks** — `LoadFont` (ouvrir la police),
`GetGlyph` (rasteriser un glyphe dans l'atlas), `GetBBox` (taille d'un glyphe), `GetMetrics`
(métriques globales), `Destroy` (fermer) — plus un `userData` optionnel. Sa méthode `IsValid()`
renvoie vrai **si et seulement si les cinq callbacks** sont non nuls ; `userData` n'est **pas**
exigé. Enfin, trois helpers fournissent des plages Unicode prêtes : `RangesASCII()`,
`RangesLatinExtended()` et `RangesDefault()` (ASCII + supplément Latin-1).

### `NkUIColorPalette` — les couleurs sémantiques

La palette nomme les couleurs par leur **rôle**, jamais par leur valeur. Les groupes :

- **Fond** : `bgPrimary`, `bgSecondary`, `bgTertiary`, `bgWindow`, `bgPopup`, `bgHeader` —
  les différents niveaux de fond.
- **Bordures** : `border`, `borderFocus` (bleu accent), `borderHover`.
- **Texte** : `textPrimary`, `textSecondary`, `textDisabled`, `textOnAccent` (texte posé sur
  une couleur d'accent).
- **Accents** : `accent`, `accentHover`, `accentActive`, `accentDisabled` — la couleur vive de
  marque et ses variantes d'interaction.
- **Widgets** : couleurs des boutons (`buttonBg`/`buttonHover`/`buttonActive`/`buttonText`),
  cases (`checkBg`, `checkMark`), curseurs (`sliderTrack`, `sliderThumb`), champs de saisie
  (`inputBg`, `inputText`, `inputPlaceholder`, `inputCursor`, `inputSelection`) et barres de
  défilement (`scrollBg`, `scrollThumb`, `scrollThumbHov`).
- **Fenêtres / Dock** : barre de titre (`titleBarBg`/`titleBarActive`/`titleBarText`/
  `titleBarBtn`/`titleBarBtnHov`), `separator`, zones d'ancrage (`dockZone`,
  `dockZoneBorder`) et onglets (`tabBg`/`tabActive`/`tabText`/`tabActiveText`).
- **États spéciaux** : `success` (vert), `warning` (orange), `danger` (rouge), `info` (bleu),
  `tooltip` / `tooltipText`, et `overlay` (un voile semi-transparent).

C'est cette indirection sémantique qui permet de basculer clair/sombre sans toucher au code.

### `NkUIMetrics` — les dimensions

Toutes les dimensions de l'UI sont rassemblées ici, en `float32` : le **padding** intérieur
(`paddingX=10`, `paddingY=6`), l'**espacement** entre éléments (`spacingX`, `spacingY`,
`itemSpacing`, `sectionSpacing`), les **rayons de coins** (`cornerRadius=5` et ses variantes
`Lg`/`Sm`), les **bordures** (`borderWidth`, `borderWidthFocus`), les **hauteurs**
d'éléments (`itemHeight=28` et variantes), les réglages de **fenêtres** (`titleBarHeight=32`,
`scrollbarWidth`, `resizeBorder`, marges et tailles minimales), de **dock** (`dockZoneSize`,
`dockTabHeight`, `dockSplitW`) et de **widgets** spécifiques (`checkboxSize`, `radioSize`,
`sliderThumbW`, `colorPickerW`, `comboArrowW`, `treeIndent`, `tooltipDelay=0.5s`, paddings de
tooltip). Centraliser ces valeurs garantit une **densité visuelle cohérente** sur toute
l'interface.

### `NkUIFontDef`, `NkUIFonts`, `NkUIAnimDef` — polices et animation du thème

`NkUIFontDef` décrit une police logique du thème : `family` (« sans-serif » par défaut), `size`,
`bold`, `italic`, `lineHeight`, `letterSpacing`. `NkUIFonts` regroupe les rôles typographiques :
`body` (corps), `small`, `large`, `heading` (gras, taille 16), `mono` (monospace) et `icon`.
`NkUIAnimDef` règle les **animations** des widgets : durées (`hoverDuration`, `pressDuration`,
`openDuration`, `closeDuration`, `scrollDuration`), courbes d'easing (`hoverEasing`,
`openEasing`, où 0=linéaire, 1=ease-in, 2=ease-out, 3=ease-in-out) et un interrupteur global
`enabled`.

### `NkUITheme` — agréger et les presets

`NkUITheme` rassemble le tout : un `name`, un drapeau `darkMode`, et les quatre blocs `colors`,
`metrics`, `fonts`, `anim`. Quatre presets statiques renvoient un thème complet par valeur :
`Default()` (clair standard), `Dark()` (sombre), `Minimal()` (épuré) et `HighContrast()`
(accessibilité). Charger un thème, c'est affecter cet objet ; les widgets s'y conforment.

### `NkUIWidgetType` et `NkUIWidgetState` — type et état

`NkUIWidgetType` énumère tous les types de widgets que la couche connaît — des boutons
(`NK_BUTTON`, `NK_BUTTON_SMALL`, `NK_BUTTON_LARGE`), cases et radios (`NK_CHECKBOX`, `NK_RADIO`,
`NK_TOGGLE`), curseurs (`NK_SLIDER_FLOAT`, `NK_SLIDER_INT`, `NK_SLIDER_FLOAT2`), champs de
saisie (`NK_INPUT_TEXT`, `NK_INPUT_INT`, `NK_INPUT_FLOAT`, `NK_INPUT_MULTILINE`), combos et
listes (`NK_COMBO`, `NK_COMBO_MULTI`, `NK_LIST_BOX`), arbres (`NK_TREE_NODE`, `NK_TREE_LEAF`),
tableaux (`NK_TABLE`, `NK_TABLE_ROW`, `NK_TABLE_CELL`), barres de progression
(`NK_PROGRESS_BAR`, `NK_PROGRESS_CIRCLE`), séparateurs et espaceurs, libellés
(`NK_LABEL` et variantes), images et canevas, sélecteurs de couleur, barres de défilement,
barre de titre et ses boutons, onglets, menus, tooltip, modale, jusqu'à `NK_CUSTOM` — bornés
par `NK_NONE=0` au début et `NK_COUNT` à la fin.

`NkUIWidgetState` est un jeu de **drapeaux bitwise** décrivant l'état d'un widget :
`NK_NORMAL=0`, `NK_HOVERED` (survolé), `NK_ACTIVE` (pressé), `NK_FOCUSED` (a le focus clavier),
`NK_DISABLED` (désactivé), `NK_SELECTED` (sélectionné), `NK_CHECKED` (coché). On les combine
avec `operator|(a, b)` et on teste un drapeau avec `HasState(s, flag)` (vrai si le bit est posé)
— l'idiome pour, par exemple, savoir si un bouton est à la fois survolé **et** pressé.

---

### Exemple

```cpp
#include "NKUI/NkUIDrawList.h"
#include "NKUI/NkUIFont.h"
#include "NKUI/NkUIRenderer.h"
#include "NKUI/NkUITheme.h"
using namespace nkentseu::nkui;

// 1. Thème : on puise toutes les couleurs et dimensions dans un preset.
NkUITheme theme = NkUITheme::Dark();

// 2. Polices : police intégrée + un TTF chargé via le pont NKFont.
NkUIFontManager fonts;
fonts.Init();
fonts.AddBuiltin(14.f);
fonts.LoadFromFile("Roboto.ttf", 16.f);

// 3. Draw list : arène réservée une fois.
NkUIDrawList dl;
dl.Init();

// 4. Renderer logiciel (tests / offline). En temps réel, un backend GPU à la place.
NkUICPURenderer renderer;
renderer.Init(640, 480);

// --- chaque frame ---
dl.Reset();                                                 // remet l'arène à zéro
dl.PushClipRect(NkRect{0, 0, 640, 480});                    // confine le dessin
dl.AddRectFilled(NkRect{20, 20, 300, 120},                  // une carte
                 theme.colors.bgWindow, theme.metrics.cornerRadius);
dl.AddShadow(NkRect{20, 20, 300, 120}, theme.metrics.cornerRadius,
             theme.colors.overlay);                          // ombre portée
fonts.Default()->RenderText(dl, NkVec2{32, 32}, "Bonjour Nkentseu",
                            theme.colors.textPrimary);
dl.PopClipRect();
fonts.UploadDirtyAtlases();                                  // pousse l'atlas

renderer.BeginFrame(640, 480);
// renderer.Submit(ctx);                                     // consomme le NkUIContext
renderer.EndFrame();
renderer.SavePNG("frame.png");                              // résultat valide après EndFrame
```

---

[← Index NKUI](README.md) · [Récap NKUI](../NKUI.md) · [Couche Runtime](../README.md)
