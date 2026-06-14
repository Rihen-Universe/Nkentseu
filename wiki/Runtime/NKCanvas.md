# NKCanvas

> Couche **Runtime** · La couche de rendu **2D SFML-like** du moteur, posée sur le RHI : contexte
> graphique, renderer 2D immédiat/batché, transform & transformable, formes, sprites/textures,
> shaders/matériaux, et cibles de rendu (fenêtre / texture offscreen) — portable sur 5 backends GPU.

Dès qu'une application doit **afficher quelque chose en deux dimensions** — un jeu, un HUD, un
éditeur, une interface — elle passe par NKCanvas. C'est l'ex-`NKContext`, refondu en une couche
calquée sur **SFML** : on ouvre une session GPU (le *contexte*), on **transforme** des objets, on les
rend sur une **cible** en composant un **état**, et un **batcher** regroupe tout en un minimum de
draw calls. Le même code de dessin tourne identique sur OpenGL, Vulkan, DirectX 11, DirectX 12 et le
rasteriseur logiciel.

Le principe directeur est l'**autonomie vis-à-vis de NKRHI** : chaque backend renderer installe à son
initialisation des **tables de pointeurs de fonction** (texture, shader, render-texture), et les
ressources passent par ces tables sans jamais savoir quel GPU les sert. L'autre fil conducteur est la
**règle dure NKMemory** : tout ce qui se crée par une fabrique ou un `Create` se libère par son
`Destroy` symétrique — **jamais** `delete` (heap corruption `c0000374` sinon).

- **Namespace principal** : `nkentseu::renderer` (le **contexte graphique** vit dans `nkentseu`, et
  `NkGraphicsApi` est un alias de `nkentseu::graphics`)
- **Includes** : pas de parapluie unique top-level. On inclut le header de la fonctionnalité voulue —
  `NKCanvas/Core/NKRenderer2D.h` (umbrella du renderer 2D), `NKCanvas/Factory/NkContextFactory.h`,
  `NKCanvas/Renderer/Core/NkRenderer2D.h`, `NKCanvas/Renderer/Resources/NkSprite.h`,
  `NKCanvas/Renderer/Shapes/NkRectangleShape.h`, `NKCanvas/Renderer/Targets/NkRenderWindow.h`…

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Ouvrir une session GPU, choisir une API, faire du calcul GPU pur | [Le contexte graphique](NKCanvas/Context.md) |
| Dessiner en 2D : renderer, frame, vue, clip, transform, vertex array | [Dessiner en 2D](NKCanvas/Drawing.md) |
| Poser des formes prêtes à l'emploi (rectangle, cercle, polygone, ligne) | [Les formes dessinables](NKCanvas/Shapes.md) |
| Afficher images, texte, effets shader/matériau (ressources GPU) | [Les ressources GPU](NKCanvas/Resources.md) |
| Choisir *où* rendre : fenêtre, texture offscreen, pont UI | [Les cibles de rendu](NKCanvas/Targets.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de toute
l'API, puis une **référence complète** où chaque élément est expliqué avec ses cas d'usage concrets
(rendu, gameplay, animation, UI/2D, physique, outils/éditeur).

---

## Aperçu des familles

- **Contexte graphique** (`nkentseu`) — `NkIGraphicsContext`, l'interface backend-agnostique qui
  détient device/surface/swapchain et la boucle de frame (`BeginFrame`/`EndFrame`/`Present`). Un
  descripteur unifié `NkContextDesc` (fabriques `MakeOpenGL`/`MakeVulkan`/…), l'enum pivot
  `NkGraphicsApi`, la fabrique `NkContextFactory` (`Create`/`Destroy`/`CreateWithFallback`), l'accès
  natif typé `NkNativeContext`, et le calcul GPU pur `NkIComputeContext`.
- **Renderer 2D** (`renderer`) — la façade `NkRenderer2D` (immediate-mode, `Begin`/`Draw…`/`End`),
  son interface `NkIRenderer2D` et sa fabrique `NkRenderer2DFactory`. Les types partagés
  (`NkView2D`, `NkBlendMode`, `NkPrimitiveType`, `NkRenderStats2D`…), la matrice `NkTransform`, la
  base `NkTransformable`, le `NkVertexArray`, les interfaces `NkDrawable`/`NkIDrawable2D`, l'état
  `NkRenderStates`, et le `NkBatchRenderer2D` (batching CPU, base des 5 backends).
- **Formes** (`renderer`) — la base abstraite `NkShape` (transform + style fill/outline/texture +
  triangulation fan), et ses dérivées `NkRectangleShape`, `NkCircleShape`, `NkConvexShape`,
  `NkLineShape`.
- **Ressources GPU** (`renderer`) — la texture `NkTexture`, le sprite `NkSprite`, le texte `NkText`,
  le shader `NkShader`, le matériau `NkMaterial`, la fonte `renderer::NkFont` (wrapper du module
  NKFont), plus les tables d'aiguillage backend `NkTextureBackend` / `NkShaderBackend`.
- **Cibles de rendu** (`renderer`) — la surface abstraite `NkRenderTarget` (rituel
  `Clear`/`Draw`/`Display`), la fenêtre `NkRenderWindow`, la texture offscreen `NkRenderTexture`,
  leur table de dispatch `NkRenderTextureBackend`, et le pont `NkUICanvasBackend` (NKUI → NKCanvas).

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `Core/NkGraphicsApi.h` | Enum `NkGraphicsApi` + `NkGraphicsApiName`/`IsAvailable`. | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NkContextDesc.h` | `NkContextDesc` + sous-desc par API + fabriques `Make…`. | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NkIGraphicsContext.h` | Interface `NkIGraphicsContext` (cycle de vie, frame). | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NkContextInfo.h` | `NkContextInfo` (carte d'identité runtime). | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NkOpenGLDesc.h` | Configuration OpenGL fine (`NkOpenGLDesc`, profils, flags). | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NkWGLPixelFormat.h` | Format de pixel Win32 (Windows-only). | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NkGpuPolicy.h` | `NkGpuPolicy` (sélection GPU cross-platform). | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NkNativeContextAccess.h` | `NkNativeContext` / `NkGetNativeAs` (handles natifs). | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NkSurfaceDesc.h` | Shim de compat (réexporte `NKWindow/Core/NkSurface.h`). | [Le contexte graphique](NKCanvas/Context.md) |
| `Core/NKRenderer2D.h` | Umbrella : inclut tout le système renderer 2D. | [Le contexte graphique](NKCanvas/Context.md) |
| `Factory/NkContextFactory.h` | `NkContextFactory` (`Create`/`Destroy`/fallback/compute). | [Le contexte graphique](NKCanvas/Context.md) |
| `Compute/NkIComputeContext.h` | Interface GPGPU `NkIComputeContext` + ressources. | [Le contexte graphique](NKCanvas/Context.md) |
| `Renderer/Core/NkRenderer2DTypes.h` | Types partagés (vue, blend, primitive, stats…). | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Core/NkRenderer2D.h` | Façade `NkRenderer2D` + interface `NkIRenderer2D`. | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Core/NkRenderer2DFactory.h` | `NkRenderer2DFactory`. | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Core/NkTransform.h` | Matrice affine 2D `NkTransform`. | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Core/NkTransformable.h` | Base `NkTransformable` (pos/rot/scale/origine). | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Core/NkVertexArray.h` | Tableau de sommets `NkVertexArray`. | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Core/NkDrawable.h` | Interfaces `NkDrawable` / `NkIDrawable2D`. | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Core/NkRenderStates.h` | États d'un draw `NkRenderStates`. | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Batch/NkBatchRenderer2D.h` | Batcher partagé (base des backends). | [Dessiner en 2D](NKCanvas/Drawing.md) |
| `Renderer/Shapes/NkShape.h` | Base abstraite des formes. | [Les formes dessinables](NKCanvas/Shapes.md) |
| `Renderer/Shapes/NkRectangleShape.h` | Rectangle. | [Les formes dessinables](NKCanvas/Shapes.md) |
| `Renderer/Shapes/NkCircleShape.h` | Cercle (polygone régulier). | [Les formes dessinables](NKCanvas/Shapes.md) |
| `Renderer/Shapes/NkConvexShape.h` | Polygone convexe libre. | [Les formes dessinables](NKCanvas/Shapes.md) |
| `Renderer/Shapes/NkLineShape.h` | Segment épais (quad). | [Les formes dessinables](NKCanvas/Shapes.md) |
| `Renderer/Resources/NkTexture.h` | Texture GPU `NkTexture` + backend texture. | [Les ressources GPU](NKCanvas/Resources.md) |
| `Renderer/Resources/NkSprite.h` | `NkSprite` + `NkText` (header pratique). | [Les ressources GPU](NKCanvas/Resources.md) |
| `Renderer/Resources/NkFont.h` | `renderer::NkFont` (wrapper module NKFont). | [Les ressources GPU](NKCanvas/Resources.md) |
| `Renderer/Resources/NkShader.h` | Shader user-écrit `NkShader` + backend shader. | [Les ressources GPU](NKCanvas/Resources.md) |
| `Renderer/Resources/NkMaterial.h` | Matériau 2D copiable `NkMaterial`. | [Les ressources GPU](NKCanvas/Resources.md) |
| `Renderer/Targets/NkRenderTarget.h` | Surface abstraite `NkRenderTarget`. | [Les cibles de rendu](NKCanvas/Targets.md) |
| `Renderer/Targets/NkRenderWindow.h` | Cible écran `NkRenderWindow`. | [Les cibles de rendu](NKCanvas/Targets.md) |
| `Renderer/Targets/NkRenderTexture.h` | Cible offscreen `NkRenderTexture`. | [Les cibles de rendu](NKCanvas/Targets.md) |
| `Renderer/Targets/NkRenderTextureBackend.h` | Table de dispatch GPU du offscreen. | [Les cibles de rendu](NKCanvas/Targets.md) |
| `UI/NkUICanvasBackend.h` | Pont NKUI → NKCanvas. | [Les cibles de rendu](NKCanvas/Targets.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
