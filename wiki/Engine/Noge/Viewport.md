# Le viewport éditeur

> Couche **Engine** · Noge · Tout ce qui fait vivre une **vue d'édition 3D** : la caméra qu'on
> pilote à la souris (`NkViewportCamera`), les poignées de transformation (`NkGizmo`), le picking
> GPU par couleur (`NkSelectionBuffer`), le découpage en plusieurs vues (`NkMultiViewport`) — et,
> à part, l'outillage de **sélection raster** de la peinture numérique (`NkSelectionSystem.h`).

Un éditeur, ce n'est pas un jeu. Dans un jeu, la caméra est *un personnage de la scène* : une entité
ECS avec ses composants, qui produit l'image finale. Dans un éditeur, il faut une caméra **par
au-dessus de la scène** — un point de vue de travail qu'on fait tourner, glisser, zoomer à la souris,
sans qu'il pollue les données du niveau. Il faut aussi *attraper* les objets à l'écran (cliquer sur
un cube, en encadrer plusieurs au lasso), les *bouger* avec des poignées visibles, et souvent
*découper* l'écran en plusieurs angles à la fois comme dans Blender. C'est exactement ce que couvre
cette famille de headers.

Le piège à garder en tête dès l'abord : **la caméra de viewport n'est pas un `NkCameraComponent`**.
Elle ne crée aucune entité, elle vit à côté de l'ECS et crache directement ses matrices view/proj.
De même, deux familles de « sélection » cohabitent ici et **ne doivent pas être confondues** : la
sélection d'**objets 3D** dans le viewport (picking par color-ID), et la sélection de **pixels** dans
une image (les marquees et le lasso de la peinture, dans `NkSelectionSystem.h`). Ce sont deux mondes.

- **Namespace** : `nkentseu` (les quatre headers). `using namespace math;` partout ;
  `using namespace renderer;` en plus dans `NkSelectionBuffer.h`. Types ECS sous `nkentseu::ecs::`,
  types renderer sous `nkentseu::renderer::`.
- **Headers** : `Noge/Viewport/NkGizmo.h`, `Noge/Viewport/NkSelectionBuffer.h`,
  `Noge/Viewport/NkViewportCamera.h`, `Noge/Selection/NkSelectionSystem.h`.

> **Statut.** Tous ces fichiers sont des **headers de spécification**. Aucun `.cpp` n'est visible :
> seuls `NkRay::At`/`IntersectPlane` et les accesseurs triviaux ont un corps. Toute méthode déclarée
> hors-ligne est à considérer **non encore implémentée** tant que le `.cpp` n'existe pas. Cette page
> documente le contrat tel qu'il est posé dans les en-têtes.

---

## La caméra de viewport : `NkViewportCamera`

C'est la pièce centrale. Une caméra **autonome**, pilotée par l'input, qui ne touche pas à l'ECS et
produit elle-même ses matrices `GetViewMatrix()` / `GetProjectionMatrix(aspect)`. Son modèle interne
est **orbital** : elle vise un point pivot (`mTarget`), à une certaine distance (`mDistance`), sous
deux angles — azimut (`mAzimuth`, autour de l'axe vertical) et élévation (`mElevation`, hauteur). Les
valeurs par défaut (`distance = 8`, `azimuth = 45°`, `elevation = 25°`) donnent d'emblée une vue de
trois-quarts confortable, celle qu'on attend en ouvrant une scène.

On la conduit par des **deltas de souris** convertis en mouvement : `Orbit(dx, dy)` la fait tourner
autour du pivot, `Pan(dx, dy)` glisse le pivot dans le plan de l'écran, `Zoom(delta)` rapproche
(positif = vers l'avant). Pour un mode « caméra libre » à la *fly-through*, `Fly(forward, right, up,
dt)` prend des axes normalisés `[-1..1]` et un delta-temps, et `FlyLook(dx, dy)` oriente le regard.
Le mode courant est décrit par `Mode` (`Orbit`, `Pan`, `Fly`, `Walk`).

```cpp
NkViewportCamera cam;
// boucle éditeur : on traduit la souris en navigation
if (mmb && shift)  cam.Pan(mouseDx, mouseDy);
else if (mmb)      cam.Orbit(mouseDx, mouseDy);
cam.Zoom(wheelDelta);

NkMat4f view = cam.GetViewMatrix();
NkMat4f proj = cam.GetProjectionMatrix(viewport.aspect);
```

Ce n'est **pas** une caméra de rendu final. Elle n'a pas de composant, pas de transform ECS, rien à
sérialiser dans la scène : c'est un outil de point de vue. Le rendu de la partie *jeu* reste l'affaire
de `NkCameraComponent`.

> **En résumé.** `NkViewportCamera` = caméra d'édition orbitale autonome (target + distance + azimut +
> élévation), conduite par `Orbit`/`Pan`/`Zoom`/`Fly`, qui livre directement view/proj. Distincte du
> `NkCameraComponent` de l'ECS. Spec : tout est hors-ligne sauf les getters.

### Vues prédéfinies et focus

Un éditeur sérieux propose les **vues orthographiques** d'atelier : face, dessus, côté… `SetOrthoView`
bascule sur l'une des valeurs de `OrthoView` (`None`, `Front`, `Back`, `Left`, `Right`, `Top`,
`Bottom`, `Isometric` — l'iso à 45°/35.26°). `IsOrtho()` / `IsPerspective()` disent dans quel régime
on est (le critère interne est `mOrthoView != OrthoView::None`), et `TogglePerspective()` fait
l'aller-retour. Côté cadrage, `FrameAABB(bbox, padding)` recule et oriente la caméra pour qu'une boîte
englobante tienne dans l'image — c'est le geste « Frame Selection » (touche **F**) — et
`FramePoint(point, distance)` recentre sur un point.

> **En résumé.** Vues ortho d'atelier via `SetOrthoView(OrthoView::…)` + `TogglePerspective`, et
> cadrage automatique via `FrameAABB` (touche F) / `FramePoint`.

### Du clic à la scène : `NkRay` et le picking

Cliquer dans le viewport, c'est lancer un **rayon** dans la scène. `ScreenToRay(px, py, w, h)` renvoie
un `NkRay` (origine + direction normalisée) partant de la caméra à travers le pixel cliqué ;
`WorldToScreen(world, w, h, aspect)` fait le chemin inverse et donne des coordonnées écran `[0..1]`
(pour épingler un label, dessiner un gizmo au bon endroit). Le `NkRay` lui-même porte les seuls
helpers **réellement implémentés** de tout ce lot : `At(t)` (le point à la distance `t`) et
`IntersectPlane(normal, d)` (le `t` d'intersection avec un plan, ou `-1` si parallèle ou derrière).
`IntersectAABB` est déclaré mais reste spec.

> **En résumé.** `ScreenToRay` transforme un clic en `NkRay` pour interroger la scène ;
> `WorldToScreen` projette un point monde à l'écran. `NkRay::At`/`IntersectPlane` sont les seules
> méthodes du lot avec un vrai corps.

### Plusieurs vues d'un coup : `NkMultiViewport`

Pour travailler à la Blender — quatre angles simultanés — `NkMultiViewport` gère jusqu'à
`kMaxViewports = 4` caméras. `SetLayout(NkViewportLayout::Type::…)` choisit la disposition (`Single`,
`Split2H`, `Split2V`, `Split4`), `GetCamera(i)` rend la caméra d'un panneau, `SetActive(i)` désigne
le panneau actif, et `Maximize(i)` / `Unmaximize()` plein-écranent temporairement une vue.

> **En résumé.** `NkMultiViewport` orchestre 1 à 4 `NkViewportCamera` selon un `NkViewportLayout`,
> avec une vue active et un mode plein-écran (`Maximize`).

---

## Les poignées de transformation : `NkGizmo`

Une fois un objet sélectionné, on veut le **déplacer, tourner, redimensionner** à la souris via des
poignées visibles — les flèches et anneaux colorés des éditeurs 3D. C'est `NkGizmo`. On l'appelle
`Update(...)` chaque frame avec l'état de la souris et la transform de l'objet ; il renvoie `true`
tant qu'un drag est en cours. Pendant ce drag, on lit le delta correspondant au mode courant
(`GetTranslateDelta` / `GetRotateDelta` / `GetScaleDelta`) et on l'applique à l'objet.

Le `Mode` (`Translate`, `Rotate`, `Scale`) choisit la nature de la manipulation, le `Space` (`World`,
`Local`) le repère dans lequel elle agit, et l'`Axis` actif (`None`, `X`, `Y`, `Z`, `XY`, `XZ`, `YZ`,
`XYZ`, `Screen`) — interrogeable par `GetActiveAxis()` — dit sur quel handle on a accroché. Le
*snapping* optionnel (`snapping`, `snapTrans`, `snapRot` en degrés, `snapScale`) verrouille les
incréments sur une grille.

```cpp
NkGizmo gizmo;
gizmo.mode = NkGizmo::Mode::Translate;
// chaque frame :
if (gizmo.Update(cam, obj.transform, mousePos, vpSize, mouseDown, mouseDrag)) {
    obj.position += gizmo.GetTranslateDelta();   // delta valide seulement pendant le drag
}
gizmo.Draw(r3d, obj.transform, cam);
```

Ce n'est **pas** un objet à construire : `NkGizmo` n'a pas de constructeur explicite, on agrège ses
champs publics par défaut. Et attention — les deltas n'ont de sens **que pendant un drag** ; hors
drag, ne les lisez pas.

> **En résumé.** `NkGizmo` = les poignées translate/rotate/scale. `Update` chaque frame → `true` si
> drag → lire le delta du `mode` courant. `Mode`/`Space`/`Axis` paramètrent la manipulation,
> `snapping` la verrouille sur une grille. Spec : `Update`/`Draw`/les getters de delta sont hors-ligne.

---

## Le picking GPU : `NkSelectionBuffer`

Comment savoir, au pixel près, sur **quel** objet on a cliqué dans une scène dense ? La technique
robuste est le **color-ID picking** : on rend une passe spéciale où chaque entité est peinte d'une
couleur unique encodant son `NkEntityId`, puis on lit la couleur du pixel sous le curseur et on la
re-décode en identifiant. `NkSelectionBuffer` fait exactement cela. On l'`Init(device, w, h)`, puis
chaque frame on appelle `RenderIDPass(cmd, world, cam, aspect)` **avant** le rendu normal ; au clic,
`Pick(px, py)` rend un `NkPickResult` (l'entité, un sous-index face/vertex éventuel, la profondeur
`[0..1]`, et une position monde reconstruite). Pour une sélection rectangulaire,
`BoxSelect(x0, y0, x1, y1, out)` remplit un `NkVector<NkEntityId>`.

La correspondance ID ↔ couleur est exposée en statiques : `EntityToColor(id)` et `ColorToEntity(color)`
— utile si on veut peindre soi-même ou déboguer la passe.

```cpp
NkSelectionBuffer picker;
picker.Init(device, w, h);
// chaque frame, AVANT le rendu visible :
picker.RenderIDPass(cmd, world, cam, aspect);
// au clic :
NkPickResult hit = picker.Pick(mx, my);
if (hit.entity != ecs::NkEntityId::Invalid()) select(hit.entity);
```

Subtilité de conception : `Pick` et `BoxSelect` sont **`const`** mais déclenchent un *readback*
paresseux (`mReadback` et `mDirty` sont `mutable`). Conséquence pratique : le `RenderIDPass`
**non-const** de la frame doit **précéder** tout `Pick`, sinon on lit une frame périmée.

> **En résumé.** `NkSelectionBuffer` = picking par color-ID : `Init` → `RenderIDPass` (avant le rendu
> normal) → `Pick`/`BoxSelect`. `Pick` est const mais fait un readback paresseux, donc toujours après
> le `RenderIDPass` de la frame. Statiques `EntityToColor`/`ColorToEntity`. Spec : hors les accesseurs.

---

## L'autre sélection : raster (`NkSelectionSystem.h`)

Ce header vit sous `Selection/` mais relève d'un **tout autre domaine** : la **peinture numérique**.
Il sélectionne des **pixels** dans une image (`NkRasterCanvas`), pas des objets 3D — c'est l'univers
des marquees, du lasso, de la baguette magique, des filtres. Ne le confondez jamais avec le picking du
viewport. Le cœur en est `NkSelectionMask`, un masque alpha 8 bits par pixel (`0` = non sélectionné,
`255` = entièrement, `[1..254]` = partiel, pour le feather et l'anti-aliasing), stocké en tuiles comme
le canvas. Autour gravitent un outil interactif (`NkSelectionTool`), des filtres non destructifs
(`NkRasterFilter`) et l'import/export d'images (`NkRasterIO`).

> **En résumé.** `NkSelectionSystem.h` = sélection de **pixels** (peinture), pas d'objets 3D. Centre :
> `NkSelectionMask` (masque alpha 8 bits, tile-based). Distinct du viewport malgré le dossier voisin.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, par header. Chacun est détaillé dans la « Référence
complète » qui suit. Sauf mention contraire, tout est spec (déclaré, pas encore implémenté).

### `NkGizmo` — poignées de transformation

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `Mode` {`Translate`,`Rotate`,`Scale`} · `Space` {`World`,`Local`} · `Axis` {`None`,`X`,`Y`,`Z`,`XY`,`XZ`,`YZ`,`XYZ`,`Screen`} | Type de manip / repère / handle accroché |
| Config | `mode`, `space`, `size`, `snapping`, `snapTrans`, `snapRot`, `snapScale` | Champs publics : mode, repère, taille px, snap (translation/rotation°/échelle) |
| Boucle | `Update(cam, objectTf, mousePos, vpSize, mouseDown, mouseDragging)` | Met à jour, renvoie `true` si en drag |
| Deltas | `GetTranslateDelta`, `GetRotateDelta`, `GetScaleDelta` | Delta du drag courant (selon `mode`) |
| État | `IsDragging`*, `GetActiveAxis`* | En drag ? / axe actif (*inline*) |
| Rendu | `Draw(r3d, objectTf, cam)` | Dessine les poignées via `NkRender3D` |

### `NkViewportCamera` (+ `NkRay`) — caméra éditeur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkRay` | `origin`, `direction` · `At(t)`* · `IntersectPlane(n,d)`* · `IntersectAABB(aabb, tNear)` | Rayon ; point à `t` / inter. plan (*implémentés*) / inter. AABB (spec) |
| Enums | `Mode` {`Orbit`,`Pan`,`Fly`,`Walk`} · `OrthoView` {`None`,`Front`,`Back`,`Left`,`Right`,`Top`,`Bottom`,`Isometric`} | Mode de navigation / vue prédéfinie |
| Config | `fovDeg`, `nearClip`, `farClip`, `orthoSize`, `orbitSpeed`, `panSpeed`, `zoomSpeed`, `flySpeed`, `invertY` | Champs publics de réglage |
| Cycle | `NkViewportCamera()`, `Reset()` | Construction / remise à l'état initial |
| Navigation | `Orbit`, `Pan`, `Zoom`, `Fly`, `FlyLook` | Conduite par deltas souris / clavier |
| Vues | `SetOrthoView`, `TogglePerspective`, `IsOrtho`*, `IsPerspective`*, `GetOrthoView`* | Ortho ↔ perspective (*getters inline*) |
| Focus | `FrameAABB(bbox, padding)`, `FramePoint(point, distance)` | Cadrer une boîte (touche F) / un point |
| Matrices | `GetViewMatrix`, `GetProjectionMatrix(aspect)`, `GetViewProjMatrix(aspect)` | View / proj / view×proj |
| Picking | `ScreenToRay(px,py,w,h)`, `WorldToScreen(world,w,h,aspect)` | Clic → rayon / point monde → écran `[0..1]` |
| État | `GetPosition`*, `GetTarget`*, `GetUp`*, `GetDistance`*, `GetAzimuth`*, `GetElevation`*, `SetTarget`*, `SetDistance`* | Lecture/écriture de l'état orbital (*inline*) |

### `NkMultiViewport` (+ `NkViewportLayout`) — multi-vues

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkViewportLayout` | `Type` {`Single`,`Split2H`,`Split2V`,`Split4`} · `type` | Disposition des panneaux |
| Constante | `kMaxViewports = 4` | Nombre max de vues |
| Layout | `NkMultiViewport()`, `SetLayout(type)`, `GetCamera(i)`* | Construction / disposition / caméra d'un panneau |
| Plein écran | `Maximize(i)`, `Unmaximize()`, `IsMaximized`* | Maximiser/restaurer un panneau |
| Actif | `ActiveViewport`*, `SetActive(i)`* | Lire / définir le panneau actif (*inline*) |

### `NkSelectionBuffer` (+ `NkPickResult`) — picking GPU

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkPickResult` | `entity`, `subIdx`, `depth`, `worldPos` | Entité touchée + sous-index + profondeur + position monde |
| Cycle | `NkSelectionBuffer()`, `~NkSelectionBuffer()`*, `Init(device,w,h)`, `Shutdown()`, `Resize(w,h)` | Construction / destruction (*inline → Shutdown*) / init / resize |
| Passe | `RenderIDPass(cmd, world, cam, aspect)` | Rend chaque entité en couleur-ID (**avant** le rendu normal) |
| Lecture | `Pick(px,py)`, `BoxSelect(x0,y0,x1,y1,out)` | Lire un pixel / un rectangle (const, readback paresseux) |
| Statiques | `EntityToColor(id)`, `ColorToEntity(color)` | Conversion ID ↔ couleur |

### `NkSelectionMask` (raster) — masque de pixels

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle | `Create(w,h)`, `Destroy()` | Allouer / libérer le masque |
| Sélection | `SelectAll`, `SelectNone`, `Invert`, `SelectRect`, `SelectEllipse`, `SelectPolygon`, `SelectByColor` | Sélections géométriques + flood-fill par couleur |
| Booléens | `BoolOp` {`Replace`,`Add`,`Subtract`,`Intersect`} · `Combine`, `AddSelection`, `SubtractSelection`, `IntersectSelection` | Combiner deux masques |
| Modif | `Feather`, `Grow`, `Shrink`, `Smooth`, `Border` | Adoucir / agrandir / réduire / lisser / contour |
| Accès | `GetMask`, `SetMask`, `IsEmpty`*, `IsValid`*, `GetWidth`*, `GetHeight`*, `GetBounds`, `MarchingSquares` | Lecture/écriture pixel + dimensions + bbox + vectorisation |
| Overlay | `DrawOverlay(r2d, animTime, dst)` | Affiche les « marching ants » |

### Outils, filtres et IO raster

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Outil | `NkSelectionToolType` {`RectMarquee`,`EllipseMarquee`,`Lasso`,`PolyLasso`,`MagicWand`,`ColorRange`} · `NkSelectionTool` (`type`, `boolOp`, `feather`, `tolerance`, `antiAlias`, `contiguous`, `allLayers`) · `PointerDown/Move/Up` | Outil de sélection actif + handlers pointeur |
| Filtre | `NkFilterType` (couleur/flou/netteté/texture/déformation) · `NkRasterFilter` (`type`, `enabled`, `intensity`, `params`, `curveR/G/B`) · `Apply`, `Preview` | Filtre non destructif (masque + région optionnels) |
| IO — import | `NkRasterImportOptions` (`convertToLinear`, `premultiplyAlpha`, `targetWidth/Height`, `depth`) | Options d'import |
| IO — export | `NkRasterExportOptions` (`Format` {`PNG`,`JPEG`,`TIFF`,`EXR`,`WebP`,`BMP`}, `jpegQuality`, `exrHDR`, `tiffFloat`, `scale`, `flattenLayers`, `exportAlpha`) | Options d'export |
| IO — API | `NkRasterIO::Import`, `Export`, `ImportFromMemory`, `DetectFormat`, `GetLastError` (toutes statiques) | Lire/écrire des images |

(*) accesseur ou destructeur **inline** (le seul code réellement présent, avec `NkRay::At`/`IntersectPlane`).

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les domaines du moteur. Rappel : hors
les inlines signalés, **tout est déclaré sans corps** — traitez-le comme un contrat à venir.

### `NkViewportCamera` à fond

**Le modèle orbital.** L'état n'est pas une position libre mais un triplet *pivot + distance +
angles* : `mTarget` (le point regardé, `{0,0,0}` au départ), `mDistance` (`8` par défaut), `mAzimuth`
(`45°`) et `mElevation` (`25°`). La position réelle de l'œil (`mPosition`) en découle, recalculée par
le helper interne `UpdatePosition()` dès qu'on touche au pivot ou à la distance — c'est précisément ce
que font les deux setters inline `SetTarget(t)` et `SetDistance(d)` (ce dernier *clampe* à un minimum
de `0.01` pour ne jamais passer derrière le pivot). Les getters `GetPosition/GetTarget/GetUp/
GetDistance/GetAzimuth/GetElevation` exposent cet état en lecture.

**La navigation.** Tout passe par des deltas :
- **Édition / scène** — `Orbit(dx, dy)` est le geste de tourner autour de l'objet qu'on inspecte ;
  `Pan(dx, dy)` recentre le pivot quand on veut décaler le cadrage sans tourner ; `Zoom(delta)`
  (positif vers l'avant) rapproche pour les détails. `orbitSpeed`, `panSpeed`, `zoomSpeed` règlent la
  sensibilité, `invertY` accommode les préférences.
- **Survol libre** — `Fly(forward, right, up, dt)` déplace l'œil à `flySpeed` unités/seconde selon des
  axes `[-1..1]` (typiquement les touches ZQSD), `FlyLook(dx, dy)` oriente le regard (souris). C'est le
  mode pour parcourir un grand niveau ou cadrer une cinématique. Le `Mode` (`Orbit`/`Pan`/`Fly`/`Walk`)
  mémorise le régime actif — noter que `Walk` figure dans l'enum mais n'est pas décrit par l'en-tête.

**Vues et focus.** `SetOrthoView(OrthoView::Top)` etc. basculent en projection orthographique le long
d'un axe d'atelier ; `Isometric` donne la vue iso classique (45° / 35.26°). `orthoSize` fixe la
demi-hauteur en unités monde dans ce régime. `IsOrtho()`/`IsPerspective()` testent
`mOrthoView != OrthoView::None`, `GetOrthoView()` le lit, `TogglePerspective()` revient à la
perspective (`fovDeg`, `nearClip`, `farClip`). `FrameAABB(bbox, padding=1.2)` est le « Frame
Selection » : on calcule l'AABB des objets choisis et la caméra recule juste ce qu'il faut pour les
encadrer (touche **F**) ; `FramePoint(point, distance)` recentre sur un point isolé.

**Les matrices et le picking.** `GetViewMatrix()` / `GetProjectionMatrix(aspect)` /
`GetViewProjMatrix(aspect)` sont ce que consomme le renderer pour dessiner le viewport. `ScreenToRay`
et `WorldToScreen` font le pont écran↔monde : le premier transforme un clic en `NkRay` (pour le
picking analytique ou pour le `NkGizmo`), le second projette un point monde en coordonnées écran
`[0..1]` (pour ancrer une étiquette, positionner une poignée, dessiner une jauge au-dessus d'une
entité). `Reset()` ramène tout aux valeurs de départ.

Cas d'usage par domaine :
- **Éditeur / outils** — la vue de travail elle-même : orbiter, cadrer, basculer en top/front pour
  aligner des objets, multi-angle via `NkMultiViewport`.
- **Rendu** — fournit les matrices view/proj de la passe de viewport sans intervenir dans la passe de
  jeu.
- **Gameplay / IA (debug)** — survoler une scène en `Fly` pour observer des agents, projeter leur
  état à l'écran via `WorldToScreen`.
- **Animation / scène** — `FrameAABB` sur un rig pour le centrer, ortho `Side` pour caler une pose de
  profil.

### `NkRay` à fond

`NkRay` porte une `origin` et une `direction` (normalisée, `{0,0,-1}` par défaut). C'est le seul type
du lot avec de **vraies implémentations** : `At(t)` renvoie `origin + direction*t` (le point à la
distance `t` le long du rayon), et `IntersectPlane(planeNormal, planeD)` calcule le `t` d'intersection
avec un plan — renvoyant `-1.f` si le rayon est **parallèle** (`|denom| < 1e-6`) **ou** si
l'intersection tombe **derrière** l'origine. Attention donc : un `-1` ne distingue pas « pas de plan »
de « plan dans le dos ». `IntersectAABB(aabb, tNear)` (intersection avec une boîte) est déclarée mais
reste spec.

- **Picking / sélection** — déposer un objet sur le sol, c'est `ScreenToRay` puis `IntersectPlane`
  avec le plan du sol, puis `At(t)` pour le point exact.
- **Gameplay** — un tir, une ligne de visée, un *raycast* d'IA partent tous d'un `NkRay`.
- **Édition** — le `NkGizmo` projette les déplacements le long de l'axe choisi en intersectant le
  rayon de souris avec un plan de drag.

### `NkMultiViewport` et `NkViewportLayout` à fond

`NkViewportLayout` n'est qu'un `Type` (`Single`, `Split2H` deux vues empilées, `Split2V` côte à côte,
`Split4` la grille à quatre). `NkMultiViewport` détient un tableau fixe de `kMaxViewports = 4`
caméras : `GetCamera(i)` en renvoie une (avec un `NKECS_ASSERT(i < kMaxViewports)` en debug),
`SetLayout(type)` réagence, `SetActive(i)` désigne le panneau qui reçoit l'input — mais, contrairement
à `GetCamera`, `SetActive` **ne valide pas** l'index (ni clamp ni assert), à l'appelant d'être
prudent. `ActiveViewport()` lit l'actif, `Maximize(i)` plein-écrane une vue (`IsMaximized()` vrai
quand `mMaximizedVp >= 0`), `Unmaximize()` restaure la disposition.

- **Éditeur** — le classique quad-view modélisation (top/front/side/perspective), avec une vue active
  qu'on maximise d'un raccourci pour travailler de près.

### `NkGizmo` à fond

`NkGizmo` n'a pas de constructeur : on agrège ses **champs publics** (`mode`, `space`, `size` en
pixels normalisés, et le bloc de snap `snapping`/`snapTrans`/`snapRot` en degrés/`snapScale`). Le
contrat de la boucle est simple : `Update(cam, objectTf, mousePos, vpSize, mouseDown, mouseDragging)`
chaque frame ; il fait le `HitTest` du rayon de souris contre les handles, met à jour l'axe actif et
l'état de drag, et renvoie `true` si l'on est en train de manipuler. Pendant ce drag — et **seulement**
pendant — on lit le delta correspondant au `mode` : `GetTranslateDelta()` (un `NkVec3f` de
déplacement), `GetRotateDelta()` (un `NkQuatf`), `GetScaleDelta()` (un `NkVec3f` de facteurs).
`IsDragging()` et `GetActiveAxis()` (inline) renseignent l'UI. `Draw(r3d, objectTf, cam)` dessine les
poignées — non pas avec un mesh dédié, mais via `NkRender3D::DrawLine`/`DrawAxis` d'après l'en-tête.

Le `Space` (`World`/`Local`) décide si les axes des handles suivent le monde ou l'orientation propre de
l'objet ; l'`Axis` actif peut être simple (`X`/`Y`/`Z`), planaire (`XY`/`XZ`/`YZ`), uniforme (`XYZ`)
ou écran (`Screen`). Le snapping verrouille les incréments sur une grille — indispensable pour aligner
proprement.

- **Édition / scène** — déplacer, tourner, mettre à l'échelle l'objet sélectionné ; le standard de
  tout éditeur 3D.
- **Animation** — manipuler un os ou une cible IK à la souris en posant des clés.
- **Physique / level design** — positionner des volumes de collision, des points d'apparition, le long
  d'un axe contraint.

### `NkSelectionBuffer` et `NkPickResult` à fond

Le `NkPickResult` agrège ce qu'un clic révèle : `entity` (l'`NkEntityId`, `Invalid()` si on a cliqué
dans le vide), `subIdx` (un sous-index face/vertex/edge quand c'est pertinent), `depth` (profondeur
normalisée `[0..1]`) et `worldPos` (position monde **reconstruite** à partir de la profondeur).

Le buffer s'`Init(device, w, h)` (le destructeur inline appelle `Shutdown()`), se `Resize(w, h)` quand
le viewport change. Le cœur est `RenderIDPass(cmd, world, cam, aspect)` : il rend la scène en
remplaçant les shaders par un shader « flat color ID » (chaque entité = sa couleur unique via
`EntityToColor`), dans une cible couleur `R8G8B8A8` doublée d'une cible profondeur. Cette passe doit
tourner **avant** le rendu visible de la frame. Ensuite, `Pick(px, py)` lit le pixel et décode
(`ColorToEntity`) ; `BoxSelect(x0, y0, x1, y1, out)` parcourt un rectangle et remplit un
`NkVector<NkEntityId>` (sélection au marquee). Les deux sont `const` mais s'appuient sur un readback
**paresseux** (`mReadback`/`mDirty` sont `mutable`) : d'où la règle d'ordre — `RenderIDPass`
(non-const) **puis** `Pick`/`BoxSelect`.

- **Éditeur** — sélection à la souris fiable au pixel près, même sur des objets qui se chevauchent ;
  box-select pour attraper un groupe.
- **Gameplay** — un picking d'unités à la RTS peut réutiliser exactement ce schéma.
- **Rendu (debug)** — visualiser le buffer d'ID pour vérifier la couverture.

### `NkSelectionMask` à fond (raster)

On entre ici dans la **peinture**. `NkSelectionMask` est un masque alpha 8 bits/pixel, tuilé comme le
`NkRasterCanvas`, où `0` = non sélectionné, `255` = plein, et l'entre-deux gère le **feather**
(adoucissement) et l'anti-aliasing. On l'`Create(w, h)` / `Destroy()`.

**Construire la sélection.** `SelectAll`/`SelectNone`/`Invert` couvrent les gestes globaux.
`SelectRect(rect, feather)`, `SelectEllipse(bounds, feather)` et `SelectPolygon(points, feather)`
posent des formes (le `feather` adoucit le bord). `SelectByColor(canvas, x, y, tolerance, contiguous)`
est la **baguette magique** : un flood-fill qui agrège les pixels de couleur proche (tolérance
`[0..255]`), limité aux pixels connectés si `contiguous`.

**Combiner.** Le `BoolOp` (`Replace`/`Add`/`Subtract`/`Intersect`) gouverne comment une nouvelle
sélection fusionne avec l'existante. `Combine(other, op)` est la forme générale ;
`AddSelection`/`SubtractSelection`/`IntersectSelection` en sont les raccourcis — exactement les
modificateurs Shift/Alt des éditeurs d'image.

**Affiner.** `Feather(radius)` floute le bord (gaussien), `Grow(px)`/`Shrink(px)` dilatent/érodent,
`Smooth(radius)` lisse, `Border(width)` ne garde que la frange — pour cerner un contour.

**Lire et vectoriser.** `GetMask(x,y)`/`SetMask(x,y,v)` accèdent au pixel ; `IsEmpty`/`IsValid`/
`GetWidth`/`GetHeight` (inline) renseignent ; `GetBounds()` donne la bbox des pixels non nuls ;
`MarchingSquares(threshold)` convertit le masque en **path vectoriel** (`NkVector<NkVec2f>`) — pour
exporter un tracé ou générer un contour net. `DrawOverlay(r2d, animTime, dst)` affiche les fameux
« marching ants » animés.

- **Peinture / IO** — isoler une zone à filtrer, à copier, à effacer ; la baguette magique pour
  détourer un sujet ; `MarchingSquares` pour transformer une sélection en forme.
- **UI / éditeur** — l'overlay animé qui matérialise la sélection courante à l'écran.

### Outils, filtres et IO raster à fond

**L'outil actif.** `NkSelectionToolType` énumère les outils — `RectMarquee`, `EllipseMarquee`, `Lasso`
(main levée), `PolyLasso` (polygonal), `MagicWand`, `ColorRange`. `NkSelectionTool` porte la config
(`type`, `boolOp`, `feather`, `tolerance`, `antiAlias`, `contiguous`, `allLayers`) **et** l'état du
tracé en cours (`isDrawing`, `startPos`, `lassoPoints`). Ses trois handlers `PointerDown`/`PointerMove`/
`PointerUp` traduisent la souris en construction de masque : on appuie, on trace, on relâche.

**Les filtres non destructifs.** `NkFilterType` couvre cinq familles — couleur
(`BrightnessContrast`, `LevelsRGB`, `CurvesRGB`, `HueSaturation`, `ColorBalance`, `Exposure`,
`Vibrance`, `Invert`, `Posterize`, `Threshold`, `Desaturate`), flou (`GaussianBlur`, `BoxBlur`,
`MotionBlur`, `RadialBlur`), netteté (`Sharpen`, `UnsharpMask`, `HighPass`), texture (`Noise`,
`Emboss`, `EdgeDetect`) et déformation (`Distort`, `Warp`, `Liquify`). `NkRasterFilter` réunit le
`type`, un `enabled`, une `intensity` `[0..1]`, une **union `params`** dont la variante dépend du
filtre (`bc{brightness,contrast}`, `hsl{hue,saturation,lightness}`, `blur{radius}`,
`usm{amount,radius,threshold}`, `noise{amount}`, `liquify{strength,radius}`), plus trois courbes de
`kCurvePoints = 5` points (`curveR/curveG/curveB`) pour `CurvesRGB`. `Apply(canvas, mask, region)`
modifie les pixels en se limitant éventuellement à la sélection et/ou à une région ; `Preview(src, dst,
region)` applique sur une copie (aperçu non destructif).

**L'import/export.** `NkRasterImportOptions` règle la lecture (`convertToLinear` sRGB→linéaire,
`premultiplyAlpha`, redimension `targetWidth/Height`, `depth`) ; `NkRasterExportOptions` l'écriture
(`Format` parmi `PNG`/`JPEG`/`TIFF`/`EXR`/`WebP`/`BMP`, `jpegQuality`, `exrHDR` Float32, `tiffFloat`,
`scale`, `flattenLayers`, `exportAlpha`). `NkRasterIO` est une **façade entièrement statique** :
`Import(path, out, opts)` (PNG/JPEG/TIFF/EXR/BMP/WebP/PSD simplifié), `Export(canvas, path, opts)`,
`ImportFromMemory(data, size, out, opts)`, `DetectFormat(path)` (par extension ou *magic number*), et
`GetLastError()` pour le diagnostic. Toutes sont `[[nodiscard]]` : **ne jamais ignorer** le bool de
retour.

- **Peinture / IO** — la chaîne complète d'édition d'image : importer, sélectionner, filtrer dans la
  sélection, exporter au format voulu.
- **Rendu (texture authoring)** — préparer une texture (niveaux, netteté, conversion linéaire) avant
  de la pousser dans le moteur.

### Idiomes et pièges transverses

- **Enum class partout** — toujours qualifier : `NkGizmo::Mode::Translate`,
  `NkViewportCamera::OrthoView::Isometric`, `NkSelectionMask::BoolOp::Add`,
  `NkSelectionToolType::MagicWand`, `NkRasterExportOptions::Format::EXR`. Aucune conversion implicite
  vers `int`.
- **Tout `noexcept`** ; les requêtes et getters sont `[[nodiscard]]`, ainsi que `NkRasterIO::Import/
  Export/...` — un retour ignoré est une erreur silencieuse.
- **Ordre du picking** — `RenderIDPass` (non-const) **avant** `Pick`/`BoxSelect` (const à readback
  paresseux), sinon lecture périmée.
- **Validation d'index inégale** — `NkMultiViewport::GetCamera` assert en debug, mais `SetActive` ne
  valide rien.
- **`IntersectPlane` ambigu** — `-1.f` couvre à la fois « parallèle » et « derrière l'origine ».
- **Deux domaines, un dossier voisin** — viewport 3D (`NkGizmo`, `NkSelectionBuffer`,
  `NkViewportCamera`, `NkMultiViewport`) vs sélection raster 2D (tout `NkSelectionSystem.h`). Ne pas
  confondre les deux « sélections ».
- **Statut spec** — hors `NkRay::At`/`IntersectPlane` et les accesseurs inline, rien n'est implémenté
  tant que le `.cpp` correspondant n'existe pas.

---

### Exemple

```cpp
#include "Noge/Viewport/NkViewportCamera.h"
#include "Noge/Viewport/NkGizmo.h"
#include "Noge/Viewport/NkSelectionBuffer.h"
using namespace nkentseu;
using namespace nkentseu::math;

NkViewportCamera cam;                 // caméra orbitale autonome (pas d'entité ECS)
NkSelectionBuffer picker;
picker.Init(device, vp.w, vp.h);
NkGizmo gizmo;
gizmo.mode = NkGizmo::Mode::Translate;

// --- chaque frame d'éditeur ---
if (mmb) cam.Orbit(mouseDx, mouseDy);
cam.Zoom(wheelDelta);

picker.RenderIDPass(cmd, world, cam, vp.aspect);   // AVANT le rendu visible

if (clicked) {
    NkPickResult hit = picker.Pick(mx, my);        // const, mais readback paresseux
    if (hit.entity != ecs::NkEntityId::Invalid())
        selection = hit.entity;
}

NkMat4f objTf = world.TransformOf(selection);
if (gizmo.Update(cam, objTf, mousePos, { vp.w, vp.h }, mouseDown, mouseDrag)) {
    if (gizmo.mode == NkGizmo::Mode::Translate)
        world.Translate(selection, gizmo.GetTranslateDelta());  // valide pendant le drag
}
gizmo.Draw(r3d, objTf, cam);

// Cadrer la sélection (touche F)
if (pressedF) cam.FrameAABB(world.BoundsOf(selection));
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
