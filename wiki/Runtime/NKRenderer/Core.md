# Le cœur du renderer

> Couche **Runtime** · NKRenderer · Le **point d'entrée** du moteur de rendu : la façade
> `NkRenderer`, sa configuration `NkRendererConfig`, le code d'erreur `NkRResult`, les types
> fondamentaux partagés, le frame graph `NkRenderGraph` et le render-to-texture `NkRenderTarget`.

Un moteur de rendu moderne, c'est des dizaines de sous-systèmes — ombres, post-process, particules,
texte, 2D, 3D, animation… — qui doivent s'orchestrer dans le bon ordre, chaque frame, sur cinq API
graphiques différentes. La question n'est jamais « comment dessine-t-on un triangle » (NKRHI s'en
charge plus bas), mais « **comment assemble-t-on tout ça sans que l'application ait à connaître les
détails** ». La réponse de NKRenderer tient en un objet : `NkRenderer`, une **façade** qui cache la
machinerie derrière une poignée de méthodes (`BeginFrame`, `EndFrame`, `Present`) et qui vous donne
accès aux sous-systèmes quand vous en avez besoin. Cette page décrit ce cœur : la façade, ce qu'on
lui passe à la création, ce qu'elle renvoie quand ça échoue, et les deux briques de bas niveau —
le frame graph et le render target — sur lesquelles tout repose.

Tout le module vit dans le namespace `nkentseu::renderer`. Une particularité à connaître d'emblée :
en tête de `NkRendererTypes.h`, le module fait `using namespace math;` — c'est pourquoi les types
géométriques (`NkVec3f`, `NkMat4f`, `NkColorF`) sont en réalité ceux de `nkentseu::math`, repris ici
sans préfixe. De même, les **handles RHI** (`NkTextureHandle`, `NkBufferHandle`…) viennent de NKRHI,
au scope `nkentseu::` directement, pas `renderer`.

- **Namespace** : `nkentseu::renderer`
- **Headers** : `NKRenderer/NkRenderer.h`, `NKRenderer/Core/NkRendererConfig.h`,
  `NKRenderer/Core/NkRendererResult.h`, `NKRenderer/Core/NkRendererTypes.h`,
  `NKRenderer/Core/NkRenderGraph.h`, `NKRenderer/Core/NkRenderTarget.h`

---

## La façade : `NkRenderer`

C'est l'objet **central**, celui par lequel tout passe. `NkRenderer` est une **interface pure** :
toutes ses méthodes sont `virtual` et l'implémentation concrète (`NkRendererImpl`) est cachée. On ne
l'instancie jamais avec `new` — on appelle la fabrique statique `NkRenderer::Create`, qui choisit
l'impl, branche le device RHI et construit tous les sous-systèmes demandés par la config. À la fin,
`NkRenderer::Destroy(renderer)` détruit l'objet **et met le pointeur à null** (il est passé par
référence). C'est la paire de Create/Destroy imposée par la règle NKMemory : jamais de `new`/`delete`
à la main.

```cpp
NkRendererConfig cfg = NkRendererConfig::ForGame(NK_GFX_API_VULKAN, 1920, 1080);
NkRenderer* renderer = NkRenderer::Create(device, cfg);   // device = NkIDevice* déjà créé
renderer->Initialize();
// ... boucle ...
NkRenderer::Destroy(renderer);   // détruit + met renderer = nullptr
```

Le **cycle de vie** est explicite : `Initialize()` après `Create` (c'est lui qui alloue réellement
les ressources GPU et clampe `framesInFlight` à `[1,3]`), `Shutdown()` pour tout libérer, `IsValid()`
pour savoir si l'init a réussi. Ce n'est **pas** un objet RAII silencieux : `Create` ne fait que
construire, c'est `Initialize` qui peut échouer et qu'il faut vérifier.

Chaque frame suit le rythme **`BeginFrame` → (dessin) → `EndFrame` → `Present`**. `BeginFrame`
renvoie un booléen : `false` signifie qu'on doit sauter la frame (swapchain à recréer, par exemple).
Quand la fenêtre change de taille, on relaie l'événement via `OnResize(width, height)` — typiquement
depuis le handler de `NkGraphicsContextResizeEvent`. La swapchain elle-même n'est **pas** gérée par
le renderer mais par le `NkIDevice` (elle lui est passée à sa propre fabrique) ; le renderer ne fait
que lire ses dimensions.

> **En résumé.** `NkRenderer` est la façade abstraite du moteur. On la crée avec
> `Create(device, cfg)`, on la détruit avec `Destroy(renderer)` (qui annule le pointeur). Cycle :
> `Initialize` une fois, puis `BeginFrame`/`EndFrame`/`Present` chaque frame, `OnResize` au
> redimensionnement. Ce n'est pas du RAII : vérifiez `Initialize()` et `BeginFrame()`.

### Accéder aux sous-systèmes

La façade ne dessine rien toute seule : elle **donne accès** aux sous-systèmes via une famille de
`Get*` (`GetRender2D`, `GetRender3D`, `GetShadow`, `GetPostProcess`, `GetTextRenderer`, `GetVFX`,
`GetAnimation`, `GetTextures`, `GetShaders`, `GetMaterials`, `GetMeshSystem`, `GetRenderGraph`…).
Tous renvoient un pointeur **non possédé** : l'objet appartient au renderer, on ne le libère
**jamais** soi-même. C'est le canal par lequel on soumet un mesh à dessiner, on charge une texture
ou on règle le bloom.

Deux sous-systèmes méritent une mention : `GetPlanarReflection()` rend des réflexions planaires
**automatiquement** — il suffit d'enregistrer un plan réfléchissant, le renderer fait la passe miroir
avant la passe Geometry et met à jour le material cible (pas de double soumission des drawcalls). Et
`GetVoxelAO()` (occlusion ambiante par voxels) : on enregistre les occluders via `RegisterOccluder()`,
on appelle `Build()` une fois, et le shader PBR échantillonne la grille tout seul.

> **En résumé.** Les `Get*` exposent les sous-systèmes, tous **possédés par le renderer** (à ne pas
> libérer). Réflexion planaire et Voxel AO sont « automatiques » : on enregistre, le renderer
> orchestre.

### Activer et désactiver des sous-systèmes à chaud

Tous les sous-systèmes ne sont pas forcément actifs : la config décide lesquels exister, mais on peut
en allouer ou en libérer **en cours de route**. `EnableSubsystem(flags)` initialise ceux qui manquent
puis reconstruit le render graph (renvoie `true` si au moins un a été (re)créé). `DisableSubsystem`
fait l'inverse — et attention aux **dépendances inverses** : couper `RENDER2D` ferme aussi
`TEXT`/`UI`/`OVERLAY` s'ils étaient actifs. `IsSubsystemActive(flags)` ne renvoie `true` que si
**tous** les flags fournis sont actifs ; `GetActiveSubsystems()` donne le bitfield complet.

> **En résumé.** Toggle dynamique : `EnableSubsystem`/`DisableSubsystem` (re)construisent le graph,
> `IsSubsystemActive` exige que **tous** les flags soient actifs, `GetActiveSubsystems` lit l'état.
> Couper RENDER2D entraîne TEXT/UI/OVERLAY.

### Targets offscreen, config dynamique, stats, bas niveau

On peut aussi rendre **hors écran** : `CreateOffscreen(desc)` renvoie un `NkOffscreenTarget*`
(renderer-owned) qu'on relâche par `DestroyOffscreen(t)` — encore une paire qui annule le pointeur.
Côté réglages **à chaud** : `SetVSync`, `SetWireframe`, `SetPostConfig(pp)` (remplace toute la config
de post-process). Pour le profilage, `GetStats()` donne un `NkRendererStats` (drawcalls, triangles,
temps GPU/CPU…) et `ResetStats()` le remet à zéro. Enfin, pour les cas avancés, un accès **bas
niveau** est exposé : `GetDevice()`, `GetCmd()` (le command buffer courant), `GetFrameIndex()`,
`GetWidth()`/`GetHeight()`, `GetConfig()`.

---

## L'implémentation : `NkRendererImpl`

`NkRendererImpl` (`NKRenderer/Core/NkRendererImpl.h`) est la classe `final` qui réalise concrètement
`NkRenderer`. Vous n'avez en principe **jamais** à la nommer : `NkRenderer::Create` l'instancie pour
vous. Elle détient **tous** les sous-systèmes via `memory::NkUniquePtr` (cohérent avec la règle dure
NKMemory), les initialise dans leur ordre de déclaration, et garde `mResources`/`mShaders` toujours
actifs (les autres sont opt-in). Le constructeur prend le même couple `(NkIDevice*, NkRendererConfig)`
que la fabrique.

> **En résumé.** `NkRendererImpl` est l'impl interne — on passe **toujours** par `Create`/`Destroy`,
> jamais par `new`/`delete` directement. Elle possède tout par `NkUniquePtr`.

---

## La configuration : `NkRendererConfig`

Avant même de créer le renderer, il faut décrire **ce qu'on veut** : quelle API, quelle résolution,
quels sous-systèmes, quelle qualité. C'est le rôle de `NkRendererConfig`, un gros POD à valeurs par
défaut raisonnables. On peut le remplir champ par champ, mais le plus simple est de partir d'un
**preset** statique et de l'ajuster.

```cpp
auto cfg = NkRendererConfig::ForGame();          // 1920x1080, OpenGL, sous-systèmes jeu (2D/3D/texte/ombres/post/VFX/anim/overlay)
cfg.api = NK_GFX_API_VULKAN;
cfg.Disable(NK_SS_VFX);                           // pas de particules
cfg.shadow.resolution = 4096;                     // ombres plus fines
```

Les **presets** couvrent les profils courants : `ForGame` (1080p, qualité haute), `ForFilm`
(Vulkan 4K, rendu cinéma), `ForArchviz` (Vulkan 1440p), `ForMobile` (OpenGL ES 720p), `For2D`
(pour un jeu sans 3D), `ForEditor` (1440p), `ForMinimal` (800×600, **aucun** sous-système), et
`ForOffscreen` (= `ForFilm` + offscreen, vsync coupée). Chacun renvoie un `NkRendererConfig` par
valeur, qu'on retouche ensuite.

> **En résumé.** Partez d'un preset (`ForGame`, `ForFilm`, `ForMobile`, `For2D`, `ForEditor`,
> `ForMinimal`, `ForOffscreen`, `ForArchviz`) puis ajustez les champs. Les helpers `Has`/`Enable`/
> `Disable` manipulent le masque de sous-systèmes.

### Le masque de sous-systèmes

Le champ `subsystems` est un **bitmask** de `NkSubsystemFlags` : c'est lui qui décide quels modules
le renderer va allouer. On combine les flags avec `|`, on teste avec `NkHasFlag`, et le moteur fournit
des **bundles** prêts à l'emploi (`NK_SS_2D_ESSENTIALS`, `NK_SS_3D_BASE`, `NK_SS_DEBUG`, `NK_SS_ALL`).
Attention aux **dépendances** déclarées dans l'enum : `TEXT` suppose `RENDER2D`, `UI` suppose
`RENDER2D`+`TEXT`, `SHADOW` suppose `RENDER3D`, `OVERLAY` suppose `RENDER2D`+`TEXT`.

```cpp
cfg.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW | NK_SS_POST_PROCESS | NK_SS_2D_ESSENTIALS;
```

> **En résumé.** `subsystems` est un bitmask `NkSubsystemFlags` (opt-in), combiné par `|`, testé par
> `NkHasFlag`, avec des bundles (`NK_SS_3D_BASE`…). Respectez les dépendances (TEXT⇐RENDER2D, etc.).

---

## L'erreur : `NkRResult`

Quand une opération échoue, NKRenderer ne lance pas d'exception : il renvoie un **code**. `NkRResult`
est un `enum class : int32` inspiré de `VkResult`/`HRESULT`, et son **signe encode la gravité** :
`>= 0` = succès ou info (`NK_OK`, `NK_NOT_READY`, `NK_PARTIAL`, `NK_FALLBACK_USED`), négatif jusqu'à
`-99` = erreur **récupérable** (handle invalide, fichier introuvable, shader qui ne compile pas…),
`<= -100` = erreur **fatale** (mémoire épuisée, device perdu/retiré). C'est la convention de tout le
module.

Deux idiomes coexistent : les fonctions **sans valeur de retour** renvoient un `NkRResult` ; celles
qui renvoient un **handle** signalent l'échec par un handle dont `IsValid()` vaut `false`, le code
exact étant alors récupérable via `NkRGetLastError()` (stocké en TLS, par thread). Les helpers
`NkROk(r)` (`r >= 0`) et `NkRFatal(r)` (`r <= -100`) lisent le signe, et `NkRString(r)` donne un
libellé affichable.

```cpp
NkRResult r = graph.Compile();
if (!NkROk(r)) {
    NK_LOG_ERROR("compile graph: {}", NkRString(r));
    if (NkRFatal(r)) return;     // device perdu, on arrête
}
```

Deux macros simplifient le flot : `NK_R_TRY(expr)` évalue une expression `NkRResult` et `return`
son résultat si elle n'est pas OK ; `NK_R_REQUIRE(cond, code, msg)` pose le last-error et `return
false` si la condition est fausse.

> **En résumé.** `NkRResult` encode la gravité par le signe (`>=0` OK, `<-100` fatal). Les ops sans
> retour renvoient le code ; les fabriques de handles renvoient un handle invalide + `NkRGetLastError()`.
> Vérifiez avec `NkROk`/`NkRFatal`, automatisez avec `NK_R_TRY`/`NK_R_REQUIRE`.

---

## Aperçu de l'API

### `NkRenderer` — la façade

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Fabrique | `Create(device, cfg)`, `Destroy(renderer&)` | Instancie / détruit (annule le pointeur). |
| Cycle de vie | `Initialize`, `Shutdown`, `IsValid` | Démarrage / arrêt / état. |
| Frame | `BeginFrame`, `EndFrame`, `Present` | Rythme d'une frame. |
| Resize | `OnResize(w, h)` | À relayer depuis l'événement resize. |
| Sous-systèmes | `GetRender2D/3D`, `GetShadow`, `GetPostProcess`, `GetTextRenderer`, `GetTextures`, `GetShaders`, `GetMaterials`, `GetMeshSystem`, `GetVFX`, `GetAnimation`, `GetOverlay`, `GetSimulation`, `GetRenderGraph`, `GetPlanarReflection`, `GetVoxelAO`, `GetMaterialCollection` | Accès (pointeurs **non possédés**). |
| Offscreen | `CreateOffscreen(desc)`, `DestroyOffscreen(t&)` | Render-to-texture (annule le pointeur). |
| Config dynamique | `SetVSync`, `SetPostConfig`, `SetWireframe` | Réglages à chaud. |
| Toggle | `EnableSubsystem`, `DisableSubsystem`, `IsSubsystemActive`, `GetActiveSubsystems` | (Dé)activation des sous-systèmes. |
| Stats | `GetStats`, `ResetStats` | Profilage. |
| Bas niveau | `GetDevice`, `GetCmd`, `GetFrameIndex`, `GetWidth`, `GetHeight`, `GetConfig` | Accès RHI brut. |

### `NkRendererConfig` et configs associées

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config principale | `api`, `width`, `height`, `subsystems`, `pipeline`, `quality`, `hdr`, `vsync`, `msaaSamples`, `maxLights/Particles/Meshes`, `framesInFlight` | Paramètres globaux du renderer. |
| Helpers config | `Has(f)`, `Enable(f)`, `Disable(f)` | Manipuler le masque de sous-systèmes. |
| Presets | `ForGame`, `ForFilm`, `ForArchviz`, `ForMobile`, `For2D`, `ForEditor`, `ForMinimal`, `ForOffscreen` | Configs prêtes à l'emploi. |
| Sous-config ombres | `NkShadowConfig` (`cascadeCount`, `resolution`, `pcss`, `softShadows`…) | Réglage des ombres. |
| Sous-config post | `NkPostConfig` (tonemap/ACES, bloom, SSAO, DOF, FXAA/TAA, vignette…) | Pipeline de post-process. |
| Sous-config IBL | `NkIBLConfig` (`useHDR`, `hdrPath`, `skyTop/horizon/ground`, `drawSkybox`…) | Éclairage image / ciel. |
| Sous-config cluster | `NkClusterConfig` (`tilesX/Y`, `sliceCount`, `maxLightsPerCluster`) | Découpe Forward+. |
| Échelle | `NkUnitSystem` (`metersPerUnit`, `MetersToUnits`, `UnitsToMeters`) + `NkUnits()`/`NkSetUnits()` | Unité spatiale globale. |
| Enums | `NkSubsystemFlags`, `NkRenderQuality`, `NkPipelineMode` | Flags + qualité + mode pipeline. |
| Opérateurs flags | `operator|`, `operator&`, `NkHasFlag` | Algèbre de bitmask. |

### `NkRResult` — gestion d'erreur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Codes | `NkRResult` (`NK_OK`, `NK_ERR_*`…) | Code de résultat signé. |
| Tests | `NkROk(r)`, `NkRFatal(r)`, `NkRString(r)` | Succès ? fatal ? libellé. |
| Last-error | `NkRGetLastError`, `NkRGetLastErrorMessage`, `NkRSetLastError`, `NkRClearLastError` | État d'erreur (TLS). |
| Macros | `NK_R_TRY(expr)`, `NK_R_REQUIRE(cond, code, msg)` | Propagation rapide. |

### `NkRendererTypes.h` — types fondamentaux

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Handles | `NkRendHandle<Tag>` + alias `NkTexHandle`, `NkMeshHandle`, `NkShaderHandle`, `NkMatHandle`, `NkMatInstHandle`, `NkFontHandle`, `NkTargetHandle`, `NkEnvMapHandle`, `NkIBLHandle`, `NkSkeletonHandle`, `NkAnimClipHandle` | Références nommées sur ressources. |
| Rendu | `NkBlendMode`, `NkRenderQueue`, `NkViewMode` | Mélange / file de tri / mode de vue. |
| Sommets | `NkVertex2D`, `NkVertex3D`, `NkVertexSkinned`, `NkVertexDebug`, `NkVertexParticle` | Layouts de sommets POD. |
| Géométrie | `NkAABB`, `NkSphere`, `NkPlane`, `NkFrustum` | Volumes englobants et culling. |
| Lumières CPU | `NkLightType`, `NkLightDesc`, `NkLight2DDesc`, `NkShadowCaster2D`, `NkShadowCasterAABB2D` | Descripteurs de lumières. |
| Lumières GPU | `NkLightGPU`, `NkClusterAABB`, `NkClusterLightList` | Layout std140 pour l'upload. |
| Caméra | `NkCameraUBO`, `NkCamera3DData`, `NkCamera2DData` | UBO caméra + descripteurs CPU. |
| Draw calls | `NkDrawCall3D`, `NkDrawCallInstanced`, `NkDrawCallSkinned`, `NkDrawIndirectCmd` | Soumissions 3D. |
| Stats | `NkRendererStats` | Compteurs de frame. |

### `NkRenderGraph` — frame graph

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkRenderGraph(device)` (non copiable) | Crée le graph. |
| Ressources | `ImportTexture`, `ImportBuffer`, `CreateTransient`, `FindByName`, `GetResourceTexture`, `GetPassRenderPass` | Déclarer/retrouver les ressources. |
| Passes | `AddPass`, `AddComputePass`, `AddPostProcessPass` (→ `NkPassBuilder&`) | Déclarer les passes (fluent). |
| Compilation | `Compile`, `Execute(cmd)`, `Reset` | Compiler / exécuter / recycler. |
| Debug | `DumpDOT`, `DumpTimings`, `IsCompiled`, `GetPassCount`, `GetActivePassCount`, `GetResourceCount` | Inspection. |
| Builder | `NkPassBuilder` (`Reads`, `Writes`, `SetColor`, `ClearColor`, `SetDepth`, `Execute`, `ClearWith`, `SetAlwaysExecute`…) | API fluente d'une passe. |
| Types | `NkPassType`, `NkPassCallback`, `NkGraphResId`, `NkPassColorAttachment`, `NkPassDepthAttachment` | Briques du graph. |

### `NkRenderTarget` — render-to-texture

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init(dev, texLib, desc)`, `Shutdown`, `IsValid` | Allouer / libérer / état. |
| Rendu | `BeginRender(cmd, clear, clearDepth)`, `EndRender(cmd)`, `FlushScene(cmd, r3d)` | Ouvrir/fermer la passe, vider la scène 3D. |
| Accès | `GetColorHandle`, `GetDepthHandle`, `GetRenderPass`, `GetWidth`, `GetHeight`, `Resize` | Lire les attachments / redimensionner. |
| Réflexion | `ReflectCamera(cam, plane)`, `ObliqueProjection(proj, view, plane)` (statiques) | Caméra miroir + near clip oblique. |
| Desc | `NkRenderTargetDesc` (`width`, `height`, `hdr`, `depth`, `colorFmt`…) | Description du target. |

---

## Référence complète

### `NkRenderer` à fond

La façade est **abstraite par conception** : l'application ne dépend que de `NkRenderer.h`, jamais
de l'impl ni des backends. Cela permet d'écrire un même code de jeu qui tourne sur OpenGL en debug et
Vulkan en release, simplement en changeant `cfg.api`.

- **Rendu** — la boucle canonique est `BeginFrame` (skip si `false`), on remplit les sous-systèmes
  (3D via `GetRender3D()->Submit(...)`, 2D via `GetRender2D()`, etc.), puis `EndFrame` et `Present`.
  C'est le renderer qui décide, en interne, dans quel ordre les passes s'enchaînent (via son render
  graph) ; l'application ne fait que **soumettre**.
- **ECS / scène** — un système de rendu d'ECS parcourt les entités visibles et, pour chacune, appelle
  un `Get*` du renderer pour soumettre un `NkDrawCall3D`. La façade découple la **logique de scène**
  (côté ECS) de l'**orchestration GPU** (côté renderer).
- **Gameplay / IA / outils** — `GetStats()` alimente un overlay de debug (FPS, drawcalls, triangles) ;
  `SetWireframe(true)` bascule en fil de fer pour inspecter la géométrie ; `EnableSubsystem`/
  `DisableSubsystem` permettent à un éditeur d'allumer/éteindre les particules ou les ombres à la
  volée pour comparer.
- **UI / 2D** — un jeu purement 2D part de `For2D` (qui n'active que `RENDER2D`+`TEXT`) et n'utilise
  que `GetRender2D()`/`GetTextRenderer()` ; la machinerie 3D n'est jamais allouée.
- **IO / outils éditeur** — `CreateOffscreen` produit une texture de la scène pour un viewport
  d'éditeur ou une capture d'écran ; `OnResize` recâble tout quand le panneau change de taille.

Le **piège** central est l'ownership : tout ce que renvoie un `Get*` appartient au renderer. Ne
jamais le `delete`, ne jamais en garder une copie de pointeur au-delà de la vie du renderer. Et
toujours fermer par la paire `Destroy` (façade) / `DestroyOffscreen` (target).

### `NkRendererConfig`, presets et sous-configs à fond

La config est le **contrat de création**. Quelques champs structurants :

- `pipeline` (`NkPipelineMode`) choisit l'architecture : `NK_FORWARD` (simple, peu de lumières),
  `NK_DEFERRED` (beaucoup de lumières, géométrie lourde), `NK_FORWARD_PLUS` (le défaut desktop : tri
  par clusters, bon compromis), `NK_TILED_DEFERRED`. Le `NkClusterConfig` (`tilesX/Y`, `sliceCount`,
  `maxLightsPerCluster`) ne sert qu'aux modes à clusters.
- `quality` (`NkRenderQuality`, de `NK_MOBILE` à `NK_CINEMATIC`) est un curseur global ; les presets
  le règlent pour vous (`ForFilm` vise le cinéma, `ForMobile` le mobile).
- `framesInFlight` est **clampé à `[1,3]` à l'init** : 1 = pas de ring buffer, 2 = double buffering,
  3 = triple. Plus de frames en vol = plus de latence absorbée mais plus de mémoire et de latence
  d'entrée.

Les **sous-configs** sont des POD imbriqués, modifiables après coup :

- `NkShadowConfig` — `cascadeCount` (CSM), `resolution`, `pcss`/`softShadows` (qualité du flou),
  `normalBias`/`depthBias` (lutte contre l'acné), `maxDistance`. Son champ `enabled` est un **toggle
  runtime des passes**, distinct de l'allocation du sous-système via `NK_SS_SHADOW` — on peut allouer
  les ombres mais les couper temporairement.
- `NkPostConfig` — le plus riche : tonemap (ACES, exposition, gamma), bloom (seuil, force, passes),
  SSAO/HBAO, DOF, motion blur, FXAA/TAA, color grading, LUT 3D, auto-exposure, SSR, vignette, grain.
  On le pousse à chaud via `SetPostConfig`. C'est la palette d'un coloriste : on n'active que ce dont
  on a besoin (chaque effet a son booléen).
- `NkIBLConfig` — l'éclairage par image : soit un HDR (`useHDR=true`, `hdrPath`), soit un **ciel
  procédural** (gradient `skyTop`/`horizon`/`ground`), avec `drawSkybox` pour le rendre.

Le **système d'unités** (`NkUnitSystem`) mérite un mot : par défaut 1 unité = 1 mètre, mais on peut
travailler en mm (CAO) ou km (espace) en changeant `metersPerUnit`. Les accesseurs globaux `NkUnits()`
(lecture), `NkUnitsMutable()` et `NkSetUnits(u)` sont des singletons thread-safe. **Réglez l'unité
avant de créer la scène** : re-scaler après coup est un TODO V1.

- **Outils / éditeur** — un éditeur archi-viz appelle `NkSetUnits` avec `metersPerUnit = 0.001f` pour
  saisir des dimensions en millimètres tout en gardant un rendu physiquement correct.
- **Mobile** — `ForMobile` coupe la plupart des post-effets et réduit la résolution d'ombres ; on
  part de là et on ré-active sélectivement.

### `NkRResult` à fond

Le code de résultat est le **langage d'erreur** commun à tout NKRenderer. Sa force est l'encodage par
signe : un simple test de signe suffit à savoir s'il faut **réessayer** (récupérable) ou **abandonner**
(fatal).

- **Rendu / GPU** — `BeginFrame` peut indiquer `NK_NOT_READY` (frame à sauter), un `Compile` de graph
  renvoie `NK_ERR_VALIDATION_FAILED` (cycle de dépendances) ; un device perdu remonte
  `NK_ERR_DEVICE_LOST`/`NK_ERR_DEVICE_REMOVED`, fatals, qui imposent de tout recréer.
- **IO / chargement** — charger un shader peut donner `NK_ERR_COMPILE_FAILED`/`NK_ERR_LINK_FAILED`,
  une texture `NK_ERR_IO`/`NK_ERR_BAD_FORMAT`, une ressource absente `NK_ERR_NOT_FOUND`. Tous
  récupérables : on log et on substitue un fallback.
- **Fallback gracieux** — `NK_FALLBACK_USED` (≥0, donc « OK ») signale qu'une ressource manquante a
  été remplacée par un défaut (texture rose, mesh cube) sans interrompre le rendu.
- **Outils** — `NkRGetLastError()`/`NkRGetLastErrorMessage()` (en TLS, donc par thread) servent à
  remonter le détail d'un handle invalide dans un panneau de log d'éditeur.

Les macros `NK_R_TRY`/`NK_R_REQUIRE` factorisent le boilerplate de propagation dans les fonctions
internes du renderer ; elles supposent un retour `NkRResult` (resp. `bool`).

### Les types fondamentaux à fond

`NkRendererTypes.h` est la **boîte à outils partagée** par tous les sous-systèmes. On y trouve les
types qu'on échange à la frontière de l'API.

**Handles (`NkRendHandle<Tag>` et alias).** Ce sont des **références opaques** sur des ressources :
un `uint64` non nul, avec `IsValid()`, `==`/`!=` et `Null()`. Le paramètre `Tag` (struct vide
`TagRTexture`, `TagRMesh`…) rend chaque alias **incompatible** avec les autres à la compilation : on
ne peut pas passer un `NkMeshHandle` là où un `NkTexHandle` est attendu. Ce sont des wrappers
nommés/ref-counted **au-dessus** des handles RHI bruts — on travaille toujours avec eux côté renderer.

**Blend / file / vue.** `NkBlendMode` (`NK_OPAQUE`, `NK_ALPHA`, `NK_ADDITIVE`, `NK_MULTIPLY`,
`NK_PREMULT`, `NK_SCREEN`) décrit comment un fragment se mélange au fond — `NK_ADDITIVE` pour le feu
et les particules lumineuses, `NK_PREMULT` pour de l'UI proprement composée. `NkRenderQueue` donne
l'ordre de tri (`NK_BACKGROUND`=0 → `NK_OVERLAY`=250 : les opaques avant les transparents).
`NkViewMode` (`NK_SOLID`, `NK_WIREFRAME`, `NK_NORMALS`, `NK_UV`, `NK_DEPTH`, `NK_AO`, `NK_UNLIT`) est
l'outil d'**inspection** d'un éditeur : visualiser les normales, les UV, la profondeur.

**Layouts de sommets.** Des POD à disposition mémoire fixe (le `static_assert sizeof==20` sur
`NkVertex2D` le garantit), prêts à uploader. `NkVertex3D` porte pos/normale/tangente + 2 jeux d'UV +
couleur (le standard PBR) ; `NkVertexSkinned` l'étend avec 4 indices et poids d'os pour l'animation
squelettique ; `NkVertexDebug` (pos + couleur) pour les lignes de debug ; `NkVertexParticle` (pos,
uv, couleur, taille, rotation) pour les systèmes de particules.

**Géométrie et culling.** `NkAABB` est la **boîte englobante alignée aux axes**, omniprésente : on
l'`Expand`/`Merge` pour construire l'englobant d'un mesh, `Transformed(mat)` la transporte (en
recalculant à partir des 8 coins), `Contains`/`Center`/`Extents` la requêtent. `NkSphere` (centre +
rayon) sert au culling rapide et aux tests de proximité. `NkPlane` (normale + distance, avec
`Distance(p)` signée) décrit un sol, un mur de découpe, un plan de réflexion. `NkFrustum` (6 plans)
est le **cœur du frustum culling** : `FromViewProj(vp)` l'extrait d'une matrice (méthode
Gribb-Hartmann), puis `TestAABB`/`TestSphere`/`TestPoint` éliminent en `O(6)` ce qui est hors champ.

- **Rendu / culling** — chaque frame, on extrait le frustum de la caméra et on teste les AABB des
  objets ; ceux qui échouent ne sont jamais soumis (gain massif sur les grandes scènes).
- **Physique / collision** — AABB et sphères servent de **proxies** de broad-phase ; `NkPlane` modélise
  le sol pour un test de contact rapide.
- **Gameplay / IA** — `NkSphere`/`Distance` testent la portée d'une compétence, le champ de détection
  d'un ennemi.

**Lumières.** Côté CPU, `NkLightDesc` décrit une lumière 3D (`NkLightType` : directionnelle, point,
spot, area), avec couleur/intensité/portée, angles de cône (spot), dimensions (area), un index de
cookie (texture projetée, atlas 8 slots, `-1`=aucun), et `castShadow`/`shadowStatic` (cache la shadow
tile si lumière + casters sont statiques). Pour la 2D, `NkLight2DDesc` (position, couleur, rayon,
cône, layer mask) et ses occluders `NkShadowCaster2D` (cercle) / `NkShadowCasterAABB2D` (boîte). Côté
GPU, `NkLightGPU` (96 octets), `NkClusterAABB` et `NkClusterLightList` sont les **layouts std140**
uploadés tels quels — leur taille fixe est la condition pour les passer en bloc à un UBO/SSBO.

**Caméra.** `NkCameraUBO` (256 octets, std140) regroupe toutes les matrices (view, proj, viewProj et
leurs inverses), position, viewport, paramètres de profondeur et les 6 plans du frustum — c'est le
bloc uploadé une fois par frame, lu par tous les shaders. Les descripteurs **CPU** `NkCamera3DData`
(position/cible/up, fov, near/far, ortho) et `NkCamera2DData` (centre, zoom, rotation, taille)
décrivent une caméra de façon lisible côté gameplay, avant d'être transformés en UBO.

**Draw calls.** Ce sont les **unités de soumission** au renderer. `NkDrawCall3D` est le cas général :
un mesh + un material (avec un `materialSlots` par sous-mesh, `material` servant de fallback), une
matrice de transformation, des overrides (tint, alpha, metallic/roughness/aoStrength), son AABB pour
le culling, et des flags (`castShadow`/`receiveShadow`/`visible`, `sortKey`, `lightLayerMask`).
`NkDrawCallInstanced` porte un vecteur de transforms/tints pour dessiner N copies en un appel (forêt,
foule). `NkDrawCallSkinned` ajoute le `boneMatrices` pour un personnage animé. `NkDrawIndirectCmd`
(indexCount/instanceCount/firstIndex/baseVertex/baseInstance) est calqué sur
`GL_DRAW_ELEMENTS_INDIRECT_COMMAND` — pour le GPU-driven rendering où c'est le GPU qui remplit les
commandes.

- **Animation** — un système d'animation calcule les `boneMatrices` (palette de skinning) et les pose
  dans un `NkDrawCallSkinned` ; `NkVertexSkinned` porte les indices/poids côté géométrie.
- **Outils** — `NkVertexDebug` + `NK_WIREFRAME` alimentent un overlay de gizmos et de boîtes
  englobantes dans l'éditeur.

**Stats.** `NkRendererStats` agrège les compteurs d'une frame (drawcalls, triangles, binds, switches,
culled, lights actives) et les temps (`gpuTimeMs`, `cpuTimeMs`, `cullTimeMs`, `shadowTimeMs`…) ;
`Reset()` les remet à zéro (`*this = {}`). C'est la matière première d'un profileur intégré.

> À noter : `NkSceneContext` n'est **pas** dans ce header (il vit dans `Core/NkCamera.h`, pour éviter
> une dépendance circulaire).

### `NkRenderGraph` à fond

Le frame graph est la **partition d'orchestre** du renderer : on y déclare *quelles passes* existent
et *quelles ressources* elles lisent/écrivent, puis le graph calcule l'ordre, les barrières et
l'allocation. C'est ce qui permet d'activer/désactiver un sous-système et de voir le graph se
reconstruire tout seul. Il est **non copiable** (il détient des ressources GPU) et possédé par l'impl.

Le **workflow par frame** est immuable : déclarer les ressources → déclarer les passes (en fluent) →
`Compile()` → `Execute(cmd)` → `Reset()`.

- **Ressources.** On *importe* une ressource externe (`ImportTexture`/`ImportBuffer`, avec son état
  initial) ou on en crée une **transiente** (`CreateTransient`, allouée puis recyclée par le graph).
  Chacune reçoit un `NkGraphResId` (un `uint32`, `NK_INVALID_RES_ID`=0 quand absent). On les retrouve
  par `FindByName`, on lit la texture résolue par `GetResourceTexture(id)` (handle invalide si l'id
  est inconnu, est un buffer, ou n'est pas encore alloué).
- **Passes.** `AddPass(name, type)`, `AddComputePass(name)`, `AddPostProcessPass(name)` renvoient un
  `NkPassBuilder&` qu'on configure en chaîne : `.Reads(...)`, `.Writes(...)`/`.SetColor(...)`/
  `.ClearColor(...)`, `.SetDepth(...)`, `.Execute(callback)`. Le `type` (`NkPassType` : `NK_SHADOW`,
  `NK_GEOMETRY`, `NK_TRANSPARENT`, `NK_LIGHTING`, `NK_COMPUTE`, `NK_POST_PROCESS`, `NK_UI_OVERLAY`,
  `NK_PRESENT`, `NK_CUSTOM`) renseigne le graph sur la nature de la passe. La callback
  (`NkPassCallback = NkFunction<void(NkICommandBuffer*)>`) contient le vrai code de dessin.
- **Compile / Execute.** `Compile()` fait un **tri topologique** (Kahn) avec détection de cycle
  (renvoie `NK_ERR_VALIDATION_FAILED` si le graph boucle), élimine les passes mortes (*culling*),
  alloue les transients et planifie les barrières. `Execute(cmd)` applique les barrières, lance chaque
  callback dans l'ordre et mesure les timings. `Reset()` libère les transients et prépare la frame
  suivante.
- **Builder fluent.** `NkPassBuilder` offre des raccourcis : `ClearWith(color)` force le slot 0 en
  load=CLEAR, `SetAlwaysExecute(true)` **empêche le culling** de tuer une passe qui n'a pas
  d'attachment graph visible (typiquement une passe Shadow ou Compute qui écrit dans un FBO interne).
- **Debug.** `DumpDOT()` exporte le graph en Graphviz (à visualiser), `DumpTimings()` en texte ;
  `GetPassCount`/`GetActivePassCount` (après culling)/`GetResourceCount`/`IsCompiled` renseignent
  l'inspecteur.

Le **piège** : une passe dont les sorties ne sont **consommées par personne** est cullée — c'est
voulu, mais surprenant pour une passe « à effet de bord » (qui écrit dans un FBO qu'elle gère
elle-même). Dans ce cas, `SetAlwaysExecute(true)`. Autre point : les caches FB/RP sont indexés par
**nom de passe** (supposé stable d'une frame à l'autre), et libérés par `Reset()`.

- **Rendu** — c'est l'usage premier : Shadow → Geometry → Lighting → Transparent → Post → UI →
  Present, chaque passe déclarant ce qu'elle lit (la shadow map en entrée du lighting) et écrit.
- **GPU / compute** — `AddComputePass` + `WritesStorage` pour une passe de culling GPU ou de
  simulation de particules, dont le résultat (un buffer) est lu par une passe de rendu suivante.
- **Outils** — `DumpDOT` produit un schéma du pipeline pour le documenter ou déboguer un ordre de
  passes inattendu.

### `NkRenderTarget` à fond

`NkRenderTarget` est le **render-to-texture de haut niveau** : un wrapper qui possède son
`NkOffscreenTarget` (par valeur) et expose une API simple pour dessiner dans une texture plutôt qu'à
l'écran. Contrairement à la façade, il n'a **pas** de fabrique statique : on l'`Init(dev, texLib,
desc)` puis `Shutdown()`.

Le `NkRenderTargetDesc` décrit la cible : `width`/`height`, `hdr` (RGBA16F si vrai, sinon RGBA8),
`depth` (avec ou sans tampon de profondeur), un `name`, et un `colorFmt` qui **prend le pas** sur
`hdr` quand on veut un format précis (R8_UNORM pour un masque, par exemple).

Le **rythme de rendu** : `BeginRender(cmd, clearColor, clearDepth)` ouvre le render pass et **renvoie
le `NkRenderPassHandle`** (nécessaire pour les pipelines, important côté Vulkan), puis on dessine,
puis `EndRender(cmd)`. Pour vider une scène 3D, `FlushScene(cmd, r3d)` appelle
`r3d->Flush(cmd, GetRenderPass())` — à condition d'avoir d'abord fait `r3d->BeginScene()`+`Submit()`
puis `BeginRender()`. `Resize(w, h)` réalloue les attachments, et `GetColorHandle`/`GetDepthHandle`/
`GetRenderPass` exposent les ressources produites.

- **Outils / éditeur** — le cas d'usage roi : rendre la scène dans une texture affichée dans un
  panneau de viewport ; `GetColorHandle()` se branche directement comme image d'UI.
- **Rendu** — miroirs, portails, mini-cartes, captures d'écran : tout passe par un RT.
- **Réflexion planaire** — les statiques `ReflectCamera(cam, plane)` (caméra miroir d'un plan
  `{nx,ny,nz,d}` normalisé) et `ObliqueProjection(proj, view, worldPlane)` (qui aligne le near clip
  sur le plan) servent à rendre une eau ou un sol réfléchissant. Attention : la caméra miroir
  **inverse le winding** des triangles — il faut donc passer en `NkCullMode::NK_FRONT` (ou désactiver
  le culling) dans ce RT.

> Le type `NkCamera3D` manipulé par `ReflectCamera` (≠ `NkCamera3DData`) vient de `Core/NkCamera.h`,
> hors de cette page.

### Ownership et règles transverses

Tout le cœur de NKRenderer obéit à la **règle dure NKMemory** (Create/Destroy par paires, jamais de
`new`/`delete` brut) :

- `NkRenderer` : `Create`/`Destroy` (Destroy annule le pointeur). Les sous-systèmes des `Get*` sont
  **possédés par le renderer** — on ne les libère pas.
- Offscreen : paire `CreateOffscreen`/`DestroyOffscreen` (renderer-owned, pointeur remis à null).
- `NkRendererImpl` détient tout via `NkUniquePtr` — ne jamais le `delete` à la main.
- `NkRenderGraph` est non copiable et possédé par l'impl ; `NkRenderTarget` possède son offscreen
  par valeur et s'ouvre/ferme par `Init`/`Shutdown`.
- Convention d'erreur : `NkRResult` pour les ops sans valeur, handle `IsValid()` + `NkRGetLastError()`
  pour les fabriques.

---

### Exemple

```cpp
#include "NKRenderer/NkRenderer.h"
using namespace nkentseu::renderer;

// 1) Config : on part d'un preset jeu, on passe en Vulkan, on coupe les particules.
NkRendererConfig cfg = NkRendererConfig::ForGame(NK_GFX_API_VULKAN, 1920, 1080);
cfg.Disable(NK_SS_VFX);
cfg.shadow.resolution = 4096;

// 2) Création + init (device = NkIDevice* déjà obtenu de NKRHI).
NkRenderer* renderer = NkRenderer::Create(device, cfg);
if (!renderer->Initialize()) {
    NK_LOG_ERROR("renderer init: {}", NkRString(NkRGetLastError()));
    return;
}

// 3) Boucle de rendu.
while (running) {
    if (!renderer->BeginFrame()) continue;        // frame à sauter

    NkRender3D* r3d = renderer->GetRender3D();     // pointeur NON possédé
    r3d->BeginScene(camera);
    r3d->Submit(drawCall);                          // NkDrawCall3D
    r3d->EndScene();

    renderer->EndFrame();
    renderer->Present();
}

// 4) Resize relayé depuis l'événement fenêtre.
//    renderer->OnResize(ev.width, ev.height);

// 5) Render-to-texture pour un viewport d'éditeur.
NkRenderTarget rt;
rt.Init(renderer->GetDevice(), renderer->GetTextures(), { 512, 512, /*hdr*/true });
rt.BeginRender(renderer->GetCmd(), { 0.f, 0.f, 0.f, 1.f });
rt.FlushScene(renderer->GetCmd(), renderer->GetRender3D());
rt.EndRender(renderer->GetCmd());
NkTexHandle viewportImage = rt.GetColorHandle();
// ... rt.Shutdown(); en fin de vie

// 6) Fermeture : annule le pointeur.
NkRenderer::Destroy(renderer);
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
