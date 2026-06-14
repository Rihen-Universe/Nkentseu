# Les outils de l'éditeur

> Couche **Runtime** · NKUI · Les grands widgets « éditeur » construits au-dessus de l'UI immédiate :
> le navigateur de fichiers `NkUIFileBrowser`, le navigateur d'assets `NkUIContentBrowser`, l'arbre
> générique `NkUITree`, le manipulateur 3D `NkUIGizmo` (avec sa grille) et le viewport 3D embarqué
> `NkUIViewport3D`.

Un moteur de jeu ne s'arrête pas aux boutons et aux cases à cocher. Dès qu'on bâtit un **éditeur** —
le sien (Nogee) ou un panneau d'outils interne — il faut des widgets d'un autre calibre : ouvrir un
fichier sur le disque, parcourir une bibliothèque d'assets façon Unreal, déplier une hiérarchie de
scène, **attraper un objet et le bouger** dans une vue 3D. NKUI regroupe ces gros morceaux dans la
famille **Tools**. Tous suivent la même philosophie que le reste de NKUI : **mode immédiat**,
**zéro-STL** et **zéro-heap** — pas de `new`, pas de `std::string`, pas de `std::vector`. Tout vit
dans des tableaux fixes bornés par des constantes `MAX_*` et des `char[N]`.

Le fil conducteur de toute la page tient en une seule forme. Chaque widget expose **une seule**
fonction publique :

```cpp
auto result = NkUIXxx::Draw(ctx, dl, font, id, rect, config, state, /* provider… */);
```

`Draw` dessine le widget **et** traite ses entrées en un seul appel, puis renvoie un `*Result` qui
vous dit ce qui s'est passé cette frame (un fichier choisi, un objet déplacé, un nœud renommé). À
vous de **posséder** la `Config` (réglages, in/out) et le `State` (état persistant entre les frames,
in/out) : le widget ne garde rien de son côté. C'est exactement le contrat de l'immédiat — l'UI est
une **fonction de votre état**, pas un objet qui survit en cachette.

- **Namespace** : `nkentseu::nkui`
- **Header parapluie** : `#include "NKUI/Tools/NkUITools.h"` — attention, il n'amène **que** Gizmo,
  Tree et FileSystem. Le viewport 3D s'inclut à part :
  `#include "NKUI/Tools/Viewport/NkUIViewport3D.h"`.

---

## Le navigateur de fichiers : `NkUIFileBrowser`

Ouvrir un projet, enregistrer une scène, choisir une texture : tôt ou tard l'éditeur doit montrer le
**disque**. `NkUIFileBrowser` est une boîte de dialogue complète — barre d'outils, fil d'Ariane,
panneau de favoris, vue liste ou icônes, renommage, suppression, création de dossier, glisser-déposer
— le tout en mode immédiat dans un simple `NkRect`.

La grande idée, c'est que le widget **ne touche jamais le disque lui-même**. Il ne sait pas lister un
dossier ou lire un fichier ; il **demande** à un *provider* — une petite structure de pointeurs de
fonction C (`NkUIFSProvider`) — de le faire. C'est ce qui le rend portable et testable : sur poste de
travail vous branchez `NkUIFSProvider::NativeProvider()` (le système de fichiers réel), en test ou sur
une plateforme sans disque vous branchez `NkUIFSProvider::Null()` ou votre propre provider (un système
de fichiers virtuel, une archive, un dépôt distant). Le widget reste identique.

```cpp
NkUIFileBrowserConfig cfg;                 // possédé par l'app, persistant
cfg.mode = NK_FBM_OPEN;                     // ouvrir un fichier
NkUIFileBrowser::AddDefaultBookmarks(cfg);  // accueil, racine, etc.

NkUIFileBrowserState state;                 // possédé par l'app, persistant
auto fs = NkUIFSProvider::NativeProvider();

// chaque frame :
auto r = NkUIFileBrowser::Draw(ctx, dl, font, id, rect, cfg, state, fs);
if (r.event == NK_FB_FILE_SELECTED)
    OpenProject(r.path);
```

Le mode (`NkUIFBMode`) change tout le comportement : `NK_FBM_OPEN` choisit un fichier existant,
`NK_FBM_SAVE` affiche un champ de nom et confirme un enregistrement, `NK_FBM_SELECT_DIR` ne retient
qu'un dossier, `NK_FBM_MULTI` autorise une sélection multiple. Le `NkUIFileBrowserResult` vous rend
un `event` typé (`NK_FB_FILE_SELECTED`, `NK_FB_SAVE_CONFIRMED`, `NK_FB_DELETE_PERMANENT`, …) plus le
ou les chemins concernés.

Ce n'est **pas** un explorateur Windows ni un widget Qt : aucune fenêtre native, aucune dépendance
système, aucune allocation. Les entrées d'un dossier tiennent dans `entries[MAX_ENTRIES]` (2048), la
sélection dans `selected[MAX_SELECTED]` (64), l'historique de navigation dans un tableau fixe.

> **En résumé.** `NkUIFileBrowser` = dialogue fichier complet en mode immédiat, **branché sur un
> provider** (`NkUIFSProvider`) au lieu de toucher le disque directement. Possédez `Config` + `State`,
> appelez `Draw` chaque frame, lisez le `Result`. `NativeProvider()` pour le vrai disque, `Null()`
> pour les tests.

---

## Le navigateur d'assets : `NkUIContentBrowser`

Quand ce n'est plus *des fichiers* mais *des assets* qu'on parcourt — textures, maillages, sons,
matériaux, scènes, scripts — on ne veut plus une liste de noms, mais une **grille de vignettes** avec
un **arbre de dossiers** à gauche, exactement comme le *Content Browser* d'Unreal. C'est le rôle de
`NkUIContentBrowser`. Il s'appuie sur le **même provider** `NkUIFSProvider` que le navigateur de
fichiers, mais présente chaque entrée comme un `NkUIAssetEntry` typé (`NkUIAssetType` : texture,
mesh, audio, matériau, scène, script, font, animation, prefab, dossier).

La différence visible, c'est la vignette. Chaque asset porte un `thumbnailTexId` — un **handle GPU**
(0 = aucune vignette) — que vous renseignez vous-même : le content browser sait *placer* et *dessiner*
la vignette, mais c'est à vous de la **rendre** (générer un aperçu de texture, un rendu de mesh) et de
lui passer l'identifiant. La taille des vignettes est réglable au vol (`thumbnailSize`, bornée par
`minThumbnailSize`/`maxThumbnailSize`), et un `filterMask` (un bit par `NkUIAssetType`) masque les
familles non voulues.

```cpp
NkUIContentBrowserConfig cfg;     // treeWidth, thumbnailSize, filterMask…
NkUIContentBrowserState  state;

auto r = NkUIContentBrowser::Draw(ctx, dl, font, id, rect, cfg, state, fs);
if (r.event == NK_CB_ASSET_DOUBLE_CLICKED)
    OpenAsset(r.path);
```

L'arbre de dossiers est intégré : `state` contient ses propres `treeNodes[MAX_TREE_NODES]` (256) — un
arbre **plat indexé par parent**, pas une hiérarchie de pointeurs (toujours le zéro-heap). Les
événements (`NkUIContentBrowserEvent`) couvrent la vie d'un asset : sélection simple ou multiple,
double-clic, glisser, changement de dossier, renommage, suppression. Deux utilitaires aident à
classer : `GuessAssetType(filename)` devine le type d'après l'extension, `AssetTypeLabel(type)` en
donne le libellé lisible.

> **En résumé.** `NkUIContentBrowser` = grille de vignettes + arbre de dossiers façon Unreal, sur le
> même `NkUIFSProvider`. Les entrées sont des `NkUIAssetEntry` typés ; **vous** fournissez le
> `thumbnailTexId` (handle GPU). L'arbre est un tableau plat indexé par parent, jamais de heap.

---

## L'arbre générique : `NkUITree`

Le navigateur d'assets a son arbre intégré, mais on a souvent besoin d'un arbre **tout seul** : la
hiérarchie d'une scène, un outliner d'os de squelette, une arborescence de calques, un menu de
réglages. `NkUITree` est ce widget d'arbre **générique** — et son design mérite qu'on s'y arrête,
parce qu'il est l'inverse exact des deux précédents.

Le navigateur de fichiers **possède** ses données (il les copie dans `state.entries`). L'arbre, lui,
ne possède **rien** : il est *callback-first*. Vos données restent **chez vous**, sous la forme que
vous voulez (un graphe d'entités, une table ECS, un fichier). Vous identifiez chaque nœud par un
simple entier opaque, `NkUITreeNodeID` (un `uint64`, `NKUI_TREE_NODE_NONE = 0` = aucun), et vous
fournissez un jeu de **callbacks** (`NkUITreeCallbacks`) que l'arbre interroge pendant le dessin :

- **Requis** : `getChildCount` (combien d'enfants directs ? 0 si feuille), `getChild` (le i-ème
  enfant), `getLabel` (écrire le libellé dans un buffer).
- **Optionnels** (nullables) : `getIcon` (une icône Unicode/ASCII), `canDrag`/`canDrop` (autoriser le
  glisser-déposer entre nœuds), `onRename` (valider un renommage), `onMove` (un nœud reparenté par
  DnD), `onSelect` (la sélection a changé).

```cpp
NkUITreeCallbacks cb{};
cb.getChildCount = [](NkUITreeNodeID n, void* u) -> int32 { /* … */ };
cb.getChild      = [](NkUITreeNodeID n, int32 i, void* u) -> NkUITreeNodeID { /* … */ };
cb.getLabel      = [](NkUITreeNodeID n, char* out, int32 max, void* u) { /* … */ };
cb.userData      = &maScene;

NkUITreeConfig  cfg;     // indentation, hauteur de ligne, couleurs…
NkUITreeState   state;   // nœuds dépliés + sélection (sur la pile)

auto r = NkUITree::Draw(ctx, dl, font, id, rect, racine, cb, cfg, state);
if (r.event == NK_TREE_NODE_ACTIVATED)   // double-clic
    Focus(r.node);
```

Le `NkUITreeState` est entièrement **sur la pile** : il mémorise quels nœuds sont ouverts
(`openNodes[MAX_OPEN]`, 256), lesquels sont sélectionnés (`selected[MAX_SELECTED]`, 64), l'état de
renommage et de glisser-déposer. Il offre quelques **méthodes membres** (non statiques) pour piloter
cet état à la main : `IsOpen`/`SetOpen`, `IsSelected`/`SetSelected`, `ClearSelection`. Et pour
afficher un nœud profond, `NkUITree::ExpandToNode(root, target, cb, state)` déplie tout le chemin
jusqu'à lui.

Ce n'est **pas** un conteneur d'arbre : il ne stocke aucune donnée d'arbre, il **lit la vôtre** à
travers les callbacks. Et `DrawMultiRoot` gère plusieurs racines (une forêt) si votre hiérarchie n'a
pas de sommet unique.

> **En résumé.** `NkUITree` = arbre **générique callback-first** : vos données restent chez vous,
> identifiées par un `NkUITreeNodeID`. Trois callbacks requis (`getChildCount`/`getChild`/`getLabel`),
> le reste optionnel. État sur la pile, méthodes `IsOpen`/`IsSelected`…, `ExpandToNode` pour révéler un
> nœud profond, `DrawMultiRoot` pour une forêt.

---

## Le manipulateur 3D : `NkUIGizmo`

Voilà le morceau de bravoure. Dans tout éditeur 3D, on attrape un objet par ses **poignées** — les
trois flèches colorées (translation), les anneaux (rotation), les cubes (mise à l'échelle) — pour le
déplacer à la souris. `NkUIGizmo` dessine ce manipulateur **et** convertit le glissé souris en
transformation, en mode immédiat, sans dépendre d'un moteur 3D : il travaille en **espace écran**.

Le point clé à comprendre, c'est que le gizmo ne fait **aucune projection 3D lui-même**. C'est *vous*
qui lui dites où l'objet apparaît à l'écran et dans quel sens partent ses axes, via le
`NkUIGizmo3DDesc` : `originPx` (la position écran de l'objet), `axisXDirPx`/`axisYDirPx`/`axisZDirPx`
(les trois directions d'axes déjà projetées en pixels) et `unitsPerPixel` (combien d'unités-monde fait
un pixel de glissé). Le gizmo dessine les poignées le long de ces directions, détecte celle qu'on
attrape, et accumule le déplacement.

```cpp
NkUIGizmoConfig cfg;          // mode, espace, masque d'axes, snap, tailles px…
cfg.mode  = NK_TRANSLATE;
cfg.space = NK_WORLD;

NkUIGizmo3DDesc desc;
desc.viewport      = viewportRect;
desc.originPx      = ProjectToScreen(obj.position);
desc.axisXDirPx    = ProjectDir(worldX);   // déjà projeté par votre caméra
desc.unitsPerPixel = 0.014f;

NkUIGizmoState state;          // gèle les axes au début du glissé
auto r = NkUIGizmo::Manipulate3D(ctx, dl, desc, cfg, state, obj.transform);
if (r.changed) {
    // obj.transform a déjà été mis à jour en place ;
    // r.deltaPosition / r.totalPosition décrivent le mouvement de cette frame.
}
```

`Manipulate3D` modifie `transform` **en place** (in/out) et renvoie un `NkUIGizmoResult` avec les
deltas de la frame (`deltaPosition`/`deltaRotationDeg`/`deltaScale`) et les totaux depuis le début du
glissé. Il existe une variante 2D, `Manipulate2D`, qui n'a besoin que d'une origine
(`NkUIGizmo2DDesc`).

Deux familles d'identifiants d'axes cohabitent, et il ne faut **pas** les confondre. Côté
**configuration**, `NkUIGizmoAxisMask` est un masque de bits (`NK_GIZMO_AXIS_X=1`, `Y=2`, `Z=4`,
`ALL=7`) qui dit *quels axes sont autorisés* — c'est ce qu'on met dans `cfg.axisMask`. Côté **axe
actif** (celui qu'on tient en ce moment), le moteur utilise des constantes `uint8` distinctes
incluant les **plans** : `NKGIZMO_AX_X/Y/Z`, `NKGIZMO_AX_XY=3`, `NKGIZMO_AX_XZ=5`, `NKGIZMO_AX_YZ=6`,
et `NKGIZMO_AX_UNI=255` (échelle uniforme / translation libre).

Le gizmo se double d'une **grille de référence**, `NkUIGizmo::DrawGrid2D` / `DrawGrid3D`, configurée
par `NkUIGridConfig` : sol quadrillé style Blender, axes colorés, fondu à l'horizon quand
`infinite=true`. C'est l'arrière-plan naturel d'une scène 3D.

> **En résumé.** `NkUIGizmo` = manipulateur translate/rotate/scale en **espace écran** : *vous*
> fournissez l'origine et les directions d'axes projetées (`NkUIGizmo3DDesc`), il dessine les poignées
> et écrit la transformation **en place**. `axisMask` (masque autorisé) ≠ `NKGIZMO_AX_*` (axe/plan
> actif, dont `255` = uniforme). `DrawGrid2D`/`DrawGrid3D` posent le sol de référence.

---

## Le viewport 3D embarqué : `NkUIViewport3D`

Tout ce qui précède se combine dans le plus gros widget de la famille : `NkUIViewport3D`, un viewport
3D complet façon Unreal, dans un simple `NkRect`. Un seul appel à `Draw` dessine la **barre d'outils**,
la **vue 3D** (avec caméra orbitale, grille, objets), l'**outliner** (la liste des objets), le panneau
de **détails** (la transformation sélectionnée) et la **barre d'état** (FPS, sélection). Il réutilise
`NkUIGizmo` pour manipuler l'objet sélectionné et `NkUIGridConfig` pour son sol.

```cpp
NkVP3DConfig cfg;            // panneaux, overlays, sensibilités, couleurs UE…
NkVP3DState  state;
NkUIViewport3D::SetupDemoScene(state);   // peuple une scène d'exemple

// chaque frame (dt sert à l'anim de focus et au FPS) :
auto r = NkUIViewport3D::Draw(ctx, dl, font, id, rect, cfg, state, dt);
if (r.event == NK_VP3D_OBJECT_SELECTED)
    Inspect(r.objIdx);
```

Le viewport **possède sa scène** : `NkVP3DState` contient jusqu'à `MAX_OBJECTS` (64) `NkVP3DObject`,
chacun avec sa forme (`NkVP3DObjectShape` : cube, sphère, cylindre, plan, cône, tore, lumières,
caméra, vide), sa couleur, et — c'est le couplage clé — **son propre `NkUIGizmoTransform` et son
`NkUIGizmoState`**. La caméra (`NkVP3DCamera`) est orbitale (yaw/pitch/distance autour d'un pivot),
avec un mode perspective ou six vues orthographiques. On peuple la scène par `AddObject(...)` (qui rend
l'index du nouvel objet) et on recadre la caméra sur la sélection par `FocusSelected(...)`.

L'en-tête du fichier décrit des raccourcis clavier (G/R/S pour les modes, F pour focus, Numpad pour
les vues, Ctrl+Z, Del) : c'est de la **documentation d'intention**, le câblage réel se fait dans votre
code via les événements renvoyés (`NkVP3DEvent` : objet sélectionné/désélectionné/transformé/supprimé,
caméra déplacée).

> **En résumé.** `NkUIViewport3D` = viewport 3D Unreal-like complet (toolbar + vue + outliner +
> détails + barre d'état) en un `Draw`. Il **possède sa scène** (`NkVP3DObject[64]`, chacun embarquant
> un gizmo) et réutilise `NkUIGizmo`/`NkUIGridConfig`. `AddObject`/`FocusSelected`/`SetupDemoScene` la
> pilotent ; `dt` nourrit l'anim de focus et le FPS.

---

## Aperçu de l'API

Tous les widgets exposent une fonction `Draw` unique (mode immédiat) renvoyant un `*Result`. Vous
possédez `Config` (réglages, in/out) et `State` (état persistant, in/out). Tout est `static noexcept`,
zéro-heap. Header parapluie `NkUITools.h` (sauf le viewport 3D, à inclure à part).

### Navigateur de fichiers — `NkUIFileBrowser`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Dessin | `Draw(ctx, dl, font, id, rect, config&, state&, provider)` | Dessine + traite le dialogue, renvoie `NkUIFileBrowserResult`. |
| Navigation | `Navigate(state&, path)` · `Refresh(state&, provider, config)` | Aller à un chemin · relister le dossier courant. |
| Requête | `GetSelectedPath(state)` · `MatchesFilter(entry, filter)` | Chemin sélectionné · l'entrée passe-t-elle le filtre ? |
| Favoris | `AddBookmark(cfg&, label, path, special=false)` · `RemoveBookmark(cfg&, idx)` · `AddDefaultBookmarks(cfg&)` | Ajouter / retirer / poser les favoris par défaut. |
| Mode | `NkUIFBMode` : `OPEN` `SAVE` `SELECT_DIR` `MULTI` | Comportement du dialogue. |
| Vue | `NkUIFBViewMode` : `LIST` `ICONS_SMALL` `ICONS_LARGE` `TILES` | Disposition des entrées. |
| Tri | `NkUIFBSortCol` : `NAME` `SIZE` `DATE` `TYPE` | Colonne de tri. |
| Type | `NkUIFileType` : `UNKNOWN` `FILE` `DIRECTORY` `SYMLINK` | Nature d'une entrée. |
| Événement | `NkUIFileBrowserEvent` : `FILE_SELECTED` `SAVE_CONFIRMED` `DIR_CHANGED` `DELETE_PERMANENT` … | Ce qui s'est passé cette frame. |
| Données | `NkUIFileEntry` · `NkUIFileBrowserResult` · `NkUIFBBookmark` · `NkUIFileBrowserConfig` · `NkUIFileBrowserState` | Entrée / résultat / favori / réglages / état. |

### Provider de système de fichiers — `NkUIFSProvider`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NativeProvider()` · `Null()` | Disque réel · provider neutre (no-op). |
| Callbacks | `list` `read` `write` `mkdir_` `move` `delete_` `stat` `userData` | Pointeurs de fonction C (`mkdir_`/`delete_` avec underscore). |
| Typedefs | `NkUIFSListFn` `…ReadFn` `…WriteFn` `…MkdirFn` `…MoveFn` `…DeleteFn` `…StatFn` | Signatures C des callbacks. |

### Navigateur d'assets — `NkUIContentBrowser`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Dessin | `Draw(ctx, dl, font, id, rect, config&, state&, provider)` | Grille de vignettes + arbre, renvoie `NkUIContentBrowserResult`. |
| Données | `LoadFolder(state&, path, provider)` | Charger un dossier dans l'état. |
| Classement | `GuessAssetType(filename)` · `AssetTypeLabel(type)` | Deviner le type · libellé lisible. |
| Type | `NkUIAssetType` : `TEXTURE` `MESH` `AUDIO` `MATERIAL` `SCENE` `SCRIPT` `FONT` `ANIMATION` `PREFAB` `FOLDER` `UNKNOWN` | Famille d'un asset. |
| Événement | `NkUIContentBrowserEvent` : `ASSET_SELECTED` `ASSET_DOUBLE_CLICKED` `ASSET_DRAGGED` `FOLDER_CHANGED` … | Vie d'un asset. |
| Données | `NkUIAssetEntry` · `NkUIContentBrowserConfig` · `NkUIContentBrowserState` (+ `TreeNode`) · `NkUIContentBrowserResult` | Asset / réglages / état (arbre inclus) / résultat. |

### Arbre générique — `NkUITree`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Dessin | `Draw(…, root, cb, cfg, state&)` · `DrawMultiRoot(…, roots, rootCount, cb, cfg, state&)` | Une racine · plusieurs racines (forêt). |
| Navigation | `ExpandToNode(root, target, cb, state&)` | Déplier le chemin jusqu'à un nœud. |
| Identité | `NkUITreeNodeID` (`uint64`) · `NKUI_TREE_NODE_NONE` | Identifiant opaque de nœud · « aucun ». |
| Callbacks | `getChildCount` `getChild` `getLabel` (requis) · `getIcon` `canDrag` `canDrop` `onRename` `onMove` `onSelect` (optionnels) · `userData` | Le widget lit **vos** données. |
| Méthodes d'état | `IsOpen`/`SetOpen` · `IsSelected`/`SetSelected` · `ClearSelection` | Piloter ouverture & sélection (membres de `NkUITreeState`). |
| Événement | `NkUITreeEvent` : `NODE_SELECTED` `NODE_ACTIVATED` `NODE_RENAMED` `NODE_MOVED` `EXPAND_CHANGED` | Action de cette frame. |
| Données | `NkUITreeCallbacks` · `NkUITreeConfig` · `NkUITreeState` · `NkUITreeResult` | Callbacks / réglages / état / résultat. |

### Gizmo & grille — `NkUIGizmo`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Manipulation | `Manipulate3D(ctx, dl, desc, cfg, state&, transform&)` · `Manipulate2D(…)` | Poignées 3D / 2D, écrit `transform` en place. |
| Grille | `DrawGrid2D(cfg, dl, viewport, originPx)` · `DrawGrid3D(cfg, dl, desc, gizmoCfg)` | Grille de référence 2D / 3D. |
| Mode | `NkUIGizmoMode` : `TRANSLATE` `ROTATE` `SCALE` | Type de manipulation. |
| Espace | `NkUIGizmoSpace` : `LOCAL` `WORLD` `NORMAL` | Repère de la manipulation. |
| Masque d'axes | `NkUIGizmoAxisMask` : `NONE=0` `X=1` `Y=2` `Z=4` `ALL=7` | Axes **autorisés** (`cfg.axisMask`). |
| Axe actif | `NKGIZMO_AX_X/Y/Z` · `_XY=3` `_XZ=5` `_YZ=6` · `_UNI=255` | Axe/plan **tenu** (uint8, inclut plans & uniforme). |
| Math | `NkUIGizmoVec3` · `NkUIGizmoTransform` | Vecteur & transform (position/rotationDeg/scale). |
| Données | `NkUIGizmoConfig` · `NkUIGizmoSnap` · `NkUIGridConfig` · `NkUIGizmo3DDesc` · `NkUIGizmo2DDesc` · `NkUIGizmoState` · `NkUIGizmoResult` | Réglages / snap / grille / descripteurs / état / résultat. |

### Viewport 3D — `NkUIViewport3D`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Dessin | `Draw(ctx, dl, font, id, rect, config&, state&, dt)` | Viewport complet, renvoie `NkVP3DResult`. |
| Scène | `AddObject(state&, name, shape, color={…})` · `FocusSelected(state&, cfg)` · `SetupDemoScene(state&)` | Ajouter un objet (→ index) · recadrer · peupler une démo. |
| Caméra | `NkVP3DCameraMode` : `PERSPECTIVE` + `ORTHO_FRONT/BACK/LEFT/RIGHT/TOP/BOTTOM` | Mode de caméra. |
| Rendu | `NkVP3DShadingMode` : `WIREFRAME` `SOLID` `UNLIT` | Mode d'affichage. |
| Forme | `NkVP3DObjectShape` : `CUBE` `SPHERE` `CYLINDER` `PLANE` `CONE` `TORUS` `LIGHT_POINT` `LIGHT_DIR` `CAMERA` `EMPTY` | Type d'objet. |
| Événement | `NkVP3DEvent` : `OBJECT_SELECTED` `OBJECT_TRANSFORMED` `OBJECT_DELETED` `CAMERA_MOVED` … | Action de cette frame. |
| Données | `NkVP3DObject` · `NkVP3DCamera` · `NkVP3DConfig` · `NkVP3DState` · `NkVP3DResult` | Objet / caméra / réglages / état / résultat. |

---

## Référence complète

Chaque widget est repris à fond, avec ses cas d'usage par domaine. Le fil rouge ne change jamais :
**possédez `Config` et `State`, appelez `Draw` chaque frame, lisez le `Result`** — pas de heap, des
tableaux fixes `MAX_*` et des `char[N]` partout.

### Le contrat commun : Draw / Config / State / Result

Avant les détails, le patron qui se répète cinq fois. Un widget Tools n'est **pas** un objet : c'est
une fonction `static Draw(...)` qui prend votre `Config` (in/out, vos réglages) et votre `State`
(in/out, ce qui doit survivre d'une frame à l'autre : sélection, défilement, nœuds dépliés, glissé en
cours) et renvoie un `*Result` décrivant l'action de la frame. Conséquences pratiques, valables pour
tous :

- **Persistance = à vous.** `Config` et `State` vivent dans votre code (membre de panneau, struct
  d'éditeur). Si vous les recréez chaque frame, vous perdez sélection et défilement.
- **Zéro-heap, bornes dures.** Tout est borné : `MAX_ENTRIES=2048`, `MAX_SELECTED=64`,
  `MAX_OBJECTS=64`, `MAX_BOOKMARKS=20`, `MAX_OPEN=256`… Au-delà, le surplus est ignoré, pas alloué.
  C'est le prix du zéro-STL — dimensionnez vos dossiers en conséquence.
- **Chaînes fixes.** Aucun `std::string` : noms et chemins sont des `char[N]` (`name[256]`,
  `path[512]`, …). Copiez-y vos chaînes, ne stockez pas de pointeurs qui pourraient pendre.

### `NkUIFileBrowser` à fond

Le dialogue se règle par `NkUIFileBrowserConfig` et mémorise tout dans `NkUIFileBrowserState`. Le
`mode` (`NkUIFBMode`) gouverne l'intention : `NK_FBM_OPEN` (un fichier existant), `NK_FBM_SAVE`
(affiche le champ `saveFilename` et confirme), `NK_FBM_SELECT_DIR` (un dossier), `NK_FBM_MULTI`
(sélection multiple, jusqu'à `MAX_SELECTED`). La présentation se choisit par `viewMode`
(`NkUIFBViewMode` : liste, petites/grandes icônes, tuiles) et le tri par `sortCol` (`NkUIFBSortCol` :
nom, taille, date, type). Des dizaines de bools fins activent ou coupent chaque capacité
(`allowRename`, `allowDelete`, `allowPermanentDelete`, `allowCreateDir`, `allowDnD`, `showHidden`,
`showBookmarks`…), et un filtre d'extension (`filterExt`/`filterLabel`) restreint l'affichage.

Le **provider** `NkUIFSProvider` est l'abstraction centrale. Ses sept callbacks (`list`, `read`,
`write`, `mkdir_`, `move`, `delete_`, `stat`) plus un `userData` opaque décrivent *tout* ce que le
widget peut faire au stockage — et `mkdir_`/`delete_` portent un underscore final (mots réservés). Le
widget appelle `list` pour remplir `state.entries`, `stat` pour les détails d'une entrée, `move` pour
renommer, etc. Selon le contexte :

- **Éditeur de bureau** — `NkUIFSProvider::NativeProvider()` : le disque réel, prêt à l'emploi.
- **Tests / CI** — `Null()` : un provider neutre qui ne touche rien, pour tester l'UI sans
  effets de bord.
- **Système de fichiers virtuel** — vos propres callbacks : une archive `.pak`, un dépôt distant, une
  base d'assets. Le widget ne voit pas la différence.
- **Plateformes sans disque classique** (console, web, mobile sandboxé) — un provider adapté au
  stockage de la plateforme.

Le `NkUIFileBrowserResult` rapporte l'`event` (`NkUIFileBrowserEvent`) et le ou les chemins
(`path`, `paths[MAX_SELECTED]`, `numPaths`, plus `target` pour un renommage). La gamme d'événements est
large parce que le widget gère tout le cycle de vie : sélection (`FILE_SELECTED`, `FILES_SELECTED`,
`DIR_SELECTED`), enregistrement (`SAVE_CONFIRMED`), navigation (`DIR_CHANGED`), édition
(`RENAME_COMMITTED`, `CREATE_DIR`, `CREATE_FILE`), suppression (`DELETE_REQUESTED`,
`DELETE_PERMANENT`), glisser-déposer (`DND_DROP`) et annulation (`CANCELLED`). Les utilitaires
complètent le pilotage : `Navigate` saute à un chemin, `Refresh` reliste, `GetSelectedPath` lit la
sélection, `MatchesFilter` teste une entrée, et le trio `AddBookmark`/`RemoveBookmark`/
`AddDefaultBookmarks` gère les favoris (jusqu'à `MAX_BOOKMARKS`).

### `NkUIContentBrowser` à fond

Là où le file browser raisonne en *fichiers*, le content browser raisonne en *assets*. Chaque entrée
est un `NkUIAssetEntry` : un nom, un chemin, un `NkUIAssetType` (texture, mesh, audio, matériau, scène,
script, font, animation, prefab, dossier), une taille, des drapeaux (`isFolder`,
`hasUnsavedChanges`) et surtout un `thumbnailTexId` — un **handle GPU** vers la vignette (0 = aucune).
Le widget place et dessine la vignette, mais c'est à vous de la **produire** et de fournir
l'identifiant : aperçu de texture, rendu miniature de mesh, icône d'aperçu de matériau.

`NkUIContentBrowserConfig` règle l'aspect : taille de vignette ajustable au vol (`thumbnailSize`,
bornée par `min/maxThumbnailSize`), largeur de l'arbre (`treeWidth`), affichage du type/de la taille,
et un `filterMask` (un bit par `NkUIAssetType`) pour ne montrer qu'une famille. `NkUIContentBrowserState`
contient les assets (`assets[MAX_ASSETS]`, 2048), la sélection, la recherche, **et l'arbre de
dossiers** : un tableau plat `treeNodes[MAX_TREE_NODES]` (256) où chaque `TreeNode` (struct publique
imbriquée) connaît son `parent` par index — une hiérarchie sans pointeurs, fidèle au zéro-heap.

Cas d'usage par domaine :

- **Édition de niveaux** — parcourir meshes, matériaux et prefabs, en glisser un dans la scène
  (`NK_CB_ASSET_DRAGGED`), double-cliquer pour ouvrir (`NK_CB_ASSET_DOUBLE_CLICKED`).
- **Pipeline d'art** — une grille de textures avec aperçus, le drapeau `hasUnsavedChanges` signalant
  un asset modifié non sauvegardé.
- **Audio / animation** — filtrer par `filterMask` pour n'afficher que les sons ou les clips.

Deux aides au classement : `GuessAssetType(filename)` déduit le type de l'extension (pour bâtir vos
entrées), `AssetTypeLabel(type)` en donne le libellé d'affichage. `LoadFolder` recharge le contenu
d'un dossier via le provider.

### `NkUITree` à fond

L'arbre est le widget le plus pur de la famille parce qu'il est **sans état de données** : il ne stocke
pas l'arbre, il le **lit chez vous** à travers `NkUITreeCallbacks`. Le modèle mental : votre monde
expose des nœuds identifiés par des `NkUITreeNodeID` (`uint64` opaques, `0 = NKUI_TREE_NODE_NONE`), et
l'arbre vous interroge pendant le dessin. Les trois callbacks **requis** suffisent à un arbre en
lecture : `getChildCount` (nombre d'enfants, 0 = feuille), `getChild` (le i-ème enfant), `getLabel`
(écrire le texte dans le buffer fourni). Les **optionnels** débloquent les fonctions riches :
`getIcon` (icône Unicode/ASCII, "" si aucune), `canDrag`/`canDrop` (autoriser le glisser-déposer entre
nœuds), `onRename` (valider une saisie), `onMove` (un nœud reparenté), `onSelect` (sélection changée).
Tous reçoivent votre `userData`.

Le `NkUITreeState` est entièrement sur la pile : `openNodes[MAX_OPEN]` (les dépliés, 256),
`selected[MAX_SELECTED]` (64), plus l'état de renommage et de DnD. Il expose des **méthodes membres**
(non statiques) pour le manipuler hors dessin : `IsOpen`/`SetOpen` (plier/déplier par programme),
`IsSelected`/`SetSelected(id, sel, clearOthers=true)` (sélection mono ou multi),
`ClearSelection`. Le `NkUITreeConfig` règle le visuel (indentation `indentPx`, hauteur de ligne,
connecteurs, multi-sélection, couleurs des états sélectionné/survolé/cible-DnD).

Le `Draw` renvoie un `NkUITreeResult` : l'`event` (`NkUITreeEvent` : sélection simple, activation par
double-clic, renommage, déplacement DnD avec `newParent`, changement de pliage avec `expanded`) et le
`node` concerné. `DrawMultiRoot` gère une **forêt** (plusieurs racines, ex. plusieurs scènes ouvertes),
et `ExpandToNode(root, target, cb, state)` déplie tout le chemin pour rendre un nœud profond visible
(ex. « localiser dans la hiérarchie » après une recherche).

Cas d'usage : hiérarchie de scène (outliner), arborescence d'os d'un squelette, calques d'un éditeur
2D, arbre de réglages, explorateur d'un graphe de blueprint. Partout où la donnée vit ailleurs et où
l'on veut juste **la montrer en arbre** sans la dupliquer.

### `NkUIGizmo` à fond

Le gizmo est un manipulateur **en espace écran**, et c'est sa force : il ne dépend d'aucun moteur 3D,
d'aucune matrice — *vous* faites la projection, lui s'occupe des poignées et de la conversion
glissé→transformation. Le `NkUIGizmoConfig` décrit l'outil : `mode` (`NkUIGizmoMode` :
translate/rotate/scale), `space` (`NkUIGizmoSpace` : local/world/normal), `axisMask` (quels axes sont
autorisés), un `NkUIGizmoSnap` (pas d'aimantation : `translateStep`, `rotateStepDeg`, `scaleStep`),
des tailles en pixels (`axisLength` — qui ne vit **que** ici —, `axisThickness`, `handleRadius`,
`planeSize`), des bascules de visibilité (centre, étiquettes, plans, grille, angle de rotation) et les
couleurs des trois axes plus le surbrillance.

Le `NkUIGizmo3DDesc` est le pont avec votre caméra : `originPx` (où l'objet est à l'écran),
`axisXDirPx`/`axisYDirPx`/`axisZDirPx` (les directions d'axes **déjà projetées** en pixels) et
`unitsPerPixel` (l'échelle glissé→monde). `Manipulate3D` dessine, attrape l'axe au survol, et accumule
le déplacement en modifiant `transform` (in/out). Pendant le glissé, `NkUIGizmoState` **gèle** les axes
et l'origine (`frozenAxisX/Y/Z`, `frozenOrigin`) pour que le mouvement reste stable même si la caméra
bouge, et tient les accumulateurs (dont les accumulateurs de snap). Le `NkUIGizmoResult` rend les
deltas de la frame (`deltaPosition`/`deltaRotationDeg`/`deltaScale`) **et** les totaux depuis le début
(`totalPosition`…), plus l'`activeAxis`.

Le double système d'axes est le piège classique : `NkUIGizmoAxisMask` (`X=1`, `Y=2`, `Z=4`, `ALL=7`)
est un masque de **permission** posé dans la config ; les constantes `NKGIZMO_AX_*` désignent l'axe ou
le **plan actif** tenu en ce moment (`_XY=3`, `_XZ=5`, `_YZ=6`, `_UNI=255` pour l'uniforme/libre). On
les compare par **égalité** sur ces `uint8` explicites — pas avec `>=`.

La grille accompagne le gizmo : `NkUIGridConfig` (axes, plans `XZ`/`XY`/`YZ`, taille de cellule en
px, nombre de lignes, mode `infinite` avec fondu façon Blender, couleurs) alimente `DrawGrid2D` (plan
XY simple) ou `DrawGrid3D` (grille projetée via le `NkUIGizmo3DDesc`). `Manipulate2D` +
`NkUIGizmo2DDesc` couvrent le cas 2D (éditeur de sprites, niveau 2D), où seule l'origine est requise.

Cas d'usage : déplacer/tourner/scaler un objet de scène (le cœur d'un éditeur), poser une lumière ou
une caméra, ajuster un point de pivot, manipuler des poignées 2D (boîte englobante d'un sprite, point
de contrôle de courbe).

### `NkUIViewport3D` à fond

Le viewport est l'assemblage final : un `Draw` peint la barre d'outils, la vue 3D, l'outliner, les
détails et la barre d'état, gère la caméra orbitale et délègue la manipulation au gizmo. Contrairement
à l'arbre (qui ne possède rien), le viewport **possède sa scène** : `NkVP3DState` contient
`objects[MAX_OBJECTS]` (64) `NkVP3DObject`. Chaque objet a une forme (`NkVP3DObjectShape` : cube,
sphère, cylindre, plan, cône, tore, lumière point/directionnelle, caméra, vide), une couleur, des
drapeaux (`visible`, `locked`, `selected`), et — couplage déterminant — **son propre
`NkUIGizmoTransform` et son `NkUIGizmoState`**, plus un `prevTransform` pour l'annulation.

La caméra `NkVP3DCamera` est **orbitale** : `yaw`/`pitch`/`distance` autour d'un pivot, `fovDeg` en
perspective ou six vues orthographiques (`NkVP3DCameraMode`). Le `NkVP3DConfig` règle l'éditeur dans le
détail : largeurs de panneaux (outliner, toolbar, barre d'état), une `NkUIGridConfig` pour le sol,
les overlays (stats, cube d'orientation, mini-axes, info caméra/sélection), les sensibilités de
navigation (orbite, pan, zoom, padding de focus), les couleurs façon Unreal (sélection orange, accent
bleu UE) et les pas de snap. Le `state` porte aussi le mode d'ombrage (`NkVP3DShadingMode` :
wireframe/solid/unlit), l'état de navigation (orbite/pan en cours), l'undo de transformation et le
suivi de FPS.

Le `Draw` prend un `dt` (delta-temps) qui nourrit l'**animation de focus** (le recadrage doux de la
caméra) et le **calcul de FPS**. Il renvoie un `NkVP3DResult` : l'`event` (`NkVP3DEvent` : objet
sélectionné/désélectionné/transformé/supprimé, caméra déplacée), l'`objIdx` concerné et la
`newTransform`. Pour piloter la scène : `AddObject(state, name, shape, color)` ajoute un objet et rend
son index, `FocusSelected(state, cfg)` recadre la caméra sur la sélection (ou tout), et
`SetupDemoScene(state)` peuple une scène d'exemple pour démarrer vite.

Les raccourcis décrits dans l'en-tête du fichier (G/R/S, F, Numpad 1/3/7, Ctrl+Z, Del) sont une
**intention de conception** : reliez-les vous-même aux actions via les événements renvoyés. Cas
d'usage : un mini-éditeur de scène intégré à un jeu, un outil de mise en place de niveaux, un
prévisualisateur d'assets 3D, un bac à sable de prototypage rapide.

---

### Exemple

```cpp
#include "NKUI/Tools/NkUITools.h"               // Gizmo + Tree + FileSystem
#include "NKUI/Tools/Viewport/NkUIViewport3D.h" // le viewport s'inclut à part
using namespace nkentseu::nkui;

// --- File browser : ouvrir un projet (provider = disque réel) ---
static NkUIFileBrowserConfig fbCfg = [] {
    NkUIFileBrowserConfig c; c.mode = NkUIFBMode::NK_FBM_OPEN;
    NkUIFileBrowser::AddDefaultBookmarks(c); return c;
}();
static NkUIFileBrowserState  fbState;
auto fs = NkUIFSProvider::NativeProvider();

auto fb = NkUIFileBrowser::Draw(ctx, dl, font, id1, rectA, fbCfg, fbState, fs);
if (fb.event == NkUIFileBrowserEvent::NK_FB_FILE_SELECTED) OpenProject(fb.path);

// --- Tree : montrer la hiérarchie de scène, callback-first ---
static NkUITreeCallbacks cb = [] {
    NkUITreeCallbacks c{};
    c.getChildCount = [](NkUITreeNodeID n, void* u){ return SceneChildCount(n); };
    c.getChild      = [](NkUITreeNodeID n, int32 i, void* u){ return SceneChild(n, i); };
    c.getLabel      = [](NkUITreeNodeID n, char* o, int32 m, void* u){ SceneName(n, o, m); };
    return c;
}();
static NkUITreeConfig treeCfg; static NkUITreeState treeState;
auto tr = NkUITree::Draw(ctx, dl, font, id2, rectB, sceneRoot, cb, treeCfg, treeState);
if (tr.event == NkUITreeEvent::NK_TREE_NODE_ACTIVATED) FocusEntity(tr.node);

// --- Viewport 3D : scène de démo + gizmo intégré ---
static NkVP3DConfig vpCfg;
static NkVP3DState  vpState = [] { NkVP3DState s; NkUIViewport3D::SetupDemoScene(s); return s; }();
auto vp = NkUIViewport3D::Draw(ctx, dl, font, id3, rectC, vpCfg, vpState, dt);
if (vp.event == NkVP3DEvent::NK_VP3D_OBJECT_TRANSFORMED) MarkDirty(vp.objIdx);
```

---

[← Index NKUI](README.md) · [Récap NKUI](../NKUI.md) · [Couche Runtime](../README.md)
