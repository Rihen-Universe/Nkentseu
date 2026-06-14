# Le contexte graphique

> Couche **Runtime** · NKCanvas · La porte d'entrée vers le GPU : choisir une API
> (`NkGraphicsApi`), décrire ce qu'on veut (`NkContextDesc`), fabriquer un contexte
> (`NkContextFactory` → `NkIGraphicsContext`), et — si besoin — du calcul pur sur GPU
> (`NkIComputeContext`).

Avant de dessiner le moindre triangle, il faut un **contexte graphique** : l'objet qui
détient le *device* GPU, la *surface* attachée à la fenêtre, et le *swapchain* (la chaîne de
back-buffers qu'on présente à l'écran). C'est lui qui sait parler à OpenGL, Vulkan, DirectX,
Metal ou au rasteriseur logiciel. Tout le problème de cette couche est de **cacher cette
diversité derrière une seule interface** : votre code de rendu manipule un `NkIGraphicsContext*`
sans jamais savoir quel backend tourne en dessous, et un seul descripteur (`NkContextDesc`)
décrit ce qu'on veut pour les six familles d'API à la fois.

Ce n'est **pas** un renderer 2D ni un RHI : le contexte ne dessine rien, ne connaît ni sommets
ni matériaux. Il ouvre la session GPU, gère le cycle de vie de la frame (`BeginFrame` /
`EndFrame` / `Present`), survit aux redimensionnements (`OnResize`), et donne accès aux handles
natifs quand on en a vraiment besoin. Le dessin lui-même est l'affaire de
[NKRenderer2D](README.md) (qui s'appuie sur ce contexte).

- **Namespace** : `nkentseu`
- **Headers** : `NKCanvas/Core/NkGraphicsApi.h`, `NkContextDesc.h`, `NkIGraphicsContext.h`,
  `NkContextInfo.h`, `NkOpenGLDesc.h`, `NkWGLPixelFormat.h`, `NkGpuPolicy.h`,
  `NkNativeContextAccess.h`, `NKCanvas/Factory/NkContextFactory.h`,
  `NKCanvas/Compute/NkIComputeContext.h`
- **Note importante** : aucune des fonctions/méthodes décrites ici n'est marquée `noexcept` ni
  `[[nodiscard]]` dans les headers. Seul le déclaré est documenté — les commentaires d'usage des
  README peuvent être aspirationnels.

---

## Choisir une API : `NkGraphicsApi`

Tout commence par le choix du backend, et ce choix est un **enum** : `NkGraphicsApi`. C'est le
type pivot de toute la couche — il apparaît dans le descripteur, dans la fabrique, dans les
infos runtime et dans l'accès natif. Ses valeurs couvrent l'éventail complet du temps réel :
`NK_GFX_API_OPENGL`, `NK_GFX_API_OPENGLES`, `NK_GFX_API_VULKAN`, `NK_GFX_API_DX11`,
`NK_GFX_API_DX12`, `NK_GFX_API_METAL`, `NK_GFX_API_WEBGL`/`WEBGL2`/`WEBGPU`,
`NK_GFX_API_SOFTWARE`, plus les consoles `GNM`/`NVN`. `NK_GFX_API_NONE` (= 0) est le « rien
choisi », et `NK_GFX_API_AUTO` demande au moteur de sélectionner selon la plateforme.

L'enum n'est pas défini par NKCanvas mais par **NKPlatform** (`NkCGXDetect.h`), avec un alias
public exposé via NKEvent — NKCanvas le récupère simplement en incluant
`NKCanvas/Core/NkGraphicsApi.h`. Ce header ajoute deux fonctions utilitaires : `NkGraphicsApiName`
donne un nom lisible (`"Vulkan"`, `"DirectX 12"`, `"Software"`…) pour les logs et l'UI, et
`NkGraphicsApiIsAvailable` répond *à la compilation* si l'API existe sur la plateforme courante
(Windows : GL/Vulkan/DX11/DX12/Software ; macOS : GL/Vulkan/Metal/Software ; etc.). Les deux
sont `O(1)`.

Ce n'est **pas** la même chose que `NkContextFactory::IsApiSupported` : `IsApiSupported`
interroge la fabrique sur le support réel à l'exécution, alors que `NkGraphicsApiIsAvailable`
est une garde de compilation pure (gardée pour Vulkan par `NKENTSEU_ENABLE_VULKAN_BACKEND`).

> **En résumé.** `NkGraphicsApi` est l'enum qui désigne le backend GPU, partagé par toute la
> couche. `NkGraphicsApiName` pour l'afficher, `NkGraphicsApiIsAvailable` pour savoir s'il est
> compilable sur la plateforme. `NK_GFX_API_AUTO` laisse le moteur choisir.

---

## Décrire ce qu'on veut : `NkContextDesc`

On ne crée pas un contexte « brut » : on en décrit la configuration via `NkContextDesc`, un
**descripteur unifié**. Sa particularité est d'agréger *par valeur* un sous-descripteur pour
chaque famille d'API — `NkOpenGLDesc opengl`, `NkVulkanDesc vulkan`, `NkDirectX11Desc dx11`,
`NkDirectX12Desc dx12`, `NkMetalDesc metal`, `NkSoftwareDesc software` — plus la sélection GPU
(`NkGpuSelectionDesc gpu`) et l'activation du compute (`NkComputeActivationDesc compute`). Le
champ `api` dit lequel sera réellement utilisé ; les autres sous-descripteurs restent à leurs
défauts sans coût.

L'idée est qu'on remplit *un seul objet* qui sait tout configurer, et qu'on n'a pas à jongler
avec six types de création différents. Pour les cas courants, des **fabriques statiques**
remplissent tout pour vous : `MakeOpenGL(4, 6)`, `MakeVulkan(/*validation*/true)`,
`MakeDirectX12(/*debug*/false)`, `MakeSoftware(/*threaded*/true)`, etc. Chacune positionne `api`
et le sous-descripteur correspondant.

```cpp
NkContextDesc desc = NkContextDesc::MakeVulkan();     // Vulkan, validation OFF par défaut
desc.vulkan.swapchainImages = 3;
desc.gpu.preference = NkGpuPreference::NK_HIGH_PERFORMANCE;  // GPU dédié
```

Chaque sous-descripteur a ses propres réglages — taille du swapchain, MSAA, niveau de débogage,
tearing, tailles de heaps DX12, threading du rasteriseur logiciel — tous avec des défauts sûrs
(par exemple `NkVulkanDesc::validationLayers = false`, car la couche de validation peut crasher
sur certaines machines). La sélection GPU (`NkGpuSelectionDesc`) exprime une *préférence* :
basse consommation contre haute performance, un vendeur précis (`NkGpuVendor::NK_NVIDIA`…), un
index d'adaptateur explicite, ou l'autorisation du fallback logiciel.

Ce n'est **pas** une description du *rendu* (pas de format de texture, pas de pipeline) : c'est
purement la configuration de la **session GPU**. Le reste relève des couches au-dessus.

> **En résumé.** `NkContextDesc` est le descripteur unique qui agrège la config des six familles
> d'API + sélection GPU + compute. Utilisez les fabriques `MakeOpenGL` / `MakeVulkan` /
> `MakeDirectX11/12` / `MakeMetal` / `MakeSoftware` puis ajustez le sous-descripteur ciblé.

---

## Fabriquer et posséder : `NkContextFactory` et `NkIGraphicsContext`

Le contexte se crée **toujours** par la fabrique, jamais par `new` : `NkContextFactory::Create`
prend une fenêtre et un descripteur, instancie le backend, crée device + surface + swapchain, et
renvoie un `NkIGraphicsContext*` dont **vous êtes propriétaire**. La règle non négociable de tout
le module : ce pointeur a été alloué par **NKMemory**, donc on le libère par
`NkContextFactory::Destroy` — **jamais** `delete`, sous peine de heap corruption (`c0000374` sous
Windows). `Create` ↔ `Destroy`, toujours en paire.

`NkIGraphicsContext` est l'**interface abstraite** (convention `NKI`) que chaque backend
implémente. Elle décrit le cycle de vie complet — `Initialize` / `Shutdown` / `IsValid` —, la
boucle de frame — `BeginFrame` / `EndFrame` / `Present` —, le redimensionnement (`OnResize`), la
synchronisation verticale (`SetVSync` / `GetVSync`), l'introspection (`GetApi`, `GetInfo`,
`GetDesc`, `SupportsCompute`) et l'accès natif brut (`GetNativeContextData`). C'est cette
interface que tout votre code de rendu manipule, sans jamais savoir quel GPU tourne dessous.

```cpp
NkIGraphicsContext* ctx = NkContextFactory::Create(window, NkContextDesc::MakeOpenGL());
// ... boucle de rendu ...
NkContextFactory::Destroy(ctx);   // JAMAIS delete ctx;
```

Quand on veut de la robustesse, `CreateWithFallback` essaie une liste d'API dans l'ordre jusqu'au
premier succès — typiquement `{DX12, DX11, Metal, Vulkan, OpenGL, Software}` : si le DX12 échoue
(driver, matériel), on retombe automatiquement sur DX11, et au pire sur le rasteriseur logiciel
qui marche partout. C'est le pattern à privilégier pour une application qui doit tourner sur des
machines variées.

Ce n'est **pas** un singleton : la fabrique supporte **plusieurs contextes** par application et
par fenêtre (multi-fenêtre, viewports d'éditeur). À chaque `Create` correspond son `Destroy`.

> **En résumé.** `NkContextFactory::Create` fabrique, vous possédez, `NkContextFactory::Destroy`
> libère (NKMemory — **jamais** `delete`). `NkIGraphicsContext` est l'interface backend-agnostique
> du cycle de vie et de la frame. `CreateWithFallback` pour la robustesse multi-machine.

---

## Le calcul pur sur GPU : `NkIComputeContext`

Toutes les API modernes savent faire du **GPGPU** — du calcul massivement parallèle sans rien
afficher : simulation de particules, physique, post-traitement d'image, réduction de données.
NKCanvas l'expose via une interface dédiée, `NkIComputeContext`, créée par la fabrique soit en
*standalone* (`CreateCompute`, sans surface ni swapchain — du calcul pur), soit *dérivée* d'un
contexte graphique existant (`ComputeFromGraphics`, qui partage device et queue, sans device
supplémentaire). Dans les deux cas, on libère par `DestroyCompute`.

Le compute n'est **pas** activé par défaut : il faut `desc.compute.enable = true` plus le flag du
backend ciblé. `NkContextDesc::IsComputeEnabledForApi` permet de tester *avant* de créer si le
compute est bien armé pour une API donnée. L'interface elle-même est un mini-RHI de calcul :
créer/écrire/lire des buffers (`CreateBuffer` / `WriteBuffer` / `ReadBuffer`), compiler des
kernels (`CreateShaderFromSource` / `FromFile`), monter des pipelines, lier et dispatcher
(`BindBuffer` / `BindPipeline` / `Dispatch`), puis synchroniser (`WaitIdle`, `MemoryBarrier`).

> **En résumé.** `NkIComputeContext` = calcul GPU pur (pas de surface). `CreateCompute` (autonome)
> ou `ComputeFromGraphics` (partagé), libéré par `DestroyCompute`. Nécessite
> `compute.enable = true` + le flag du backend ; testez avec `IsComputeEnabledForApi`.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, par famille. Chacun est détaillé dans la « Référence
complète » qui suit.

### API & disponibilité — `NkGraphicsApi.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkGraphicsApi` (alias de `graphics::NkGraphicsApi`) | Désigne le backend GPU (`NONE`/`OPENGL`/`OPENGLES`/`VULKAN`/`DX11`/`DX12`/`METAL`/`WEBGL`/`WEBGL2`/`WEBGPU`/`SOFTWARE`/`GNM`/`NVN`/`MAX`/`AUTO`). |
| Fonction | `NkGraphicsApiName(api)` | Nom lisible (`"Vulkan"`, `"DirectX 12"`…), `O(1)`. |
| Fonction | `NkGraphicsApiIsAvailable(api)` | Disponibilité *compile-time* selon la plateforme, `O(1)`. |
| Macro | `NKENTSEU_ENABLE_VULKAN_BACKEND` | Définie à `1` si absente (garde Vulkan). |

### Descripteurs de création — `NkContextDesc.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkGpuPreference` | `NK_DEFAULT` / `NK_LOW_POWER` / `NK_HIGH_PERFORMANCE`. |
| Enum | `NkGpuVendor` | `NK_ANY`/`NK_NVIDIA`/`NK_AMD`/`NK_INTEL`/`NK_ARM`/`NK_QUALCOMM`/`NK_APPLE`/`NK_MICROSOFT`. |
| Struct | `NkGpuSelectionDesc` | Préférence, index d'adaptateur, vendeur, fallback logiciel, hints GL. |
| Struct | `NkVulkanDesc` | Noms/versions, validation, vsync, images de swapchain, MSAA, extensions, queue compute. |
| Struct | `NkDirectX11Desc` | Debug, vsync, tearing, MSAA, buffers, feature level. |
| Struct | `NkDirectX12Desc` | Debug, validation GPU, tearing, buffers, tailles de heaps (RTV/DSV/SRV/Sampler), queue compute. |
| Struct | `NkMetalDesc` | Validation, vsync, sampleCount, sRGB. |
| Struct | `NkSoftwareDesc` | Threading, nombre de threads, SSE, format de pixel. |
| Struct | `NkComputeActivationDesc` | `enable` + flags par backend (`opengl`/`vulkan`/`directx11`/`directx12`/`metal`/`software`). |
| Struct | `NkContextDesc` | Descripteur unifié : `api` + tous les sous-desc agrégés. |
| Méthode | `NkContextDesc::IsComputeEnabledForApi(api)` | Le compute est-il armé pour cette API ? `O(1)`. |
| Fabrique | `MakeOpenGL` / `MakeOpenGLES` / `MakeVulkan` / `MakeDirectX11` / `MakeDirectX12` / `MakeMetal` / `MakeSoftware` | Construisent un `NkContextDesc` prêt pour une API. |

### Contexte graphique — `NkIGraphicsContext.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Interface | `NkIGraphicsContext` | Interface abstraite (`NKI`) du contexte de rendu. |
| Cycle de vie | `Initialize`, `Shutdown`, `IsValid` | Création/destruction du device/surface/swapchain. |
| Frame | `BeginFrame`, `EndFrame`, `Present` | Début/fin de frame ; présentation à l'écran. |
| Surface | `OnResize`, `SetVSync`, `GetVSync` | Recréation swapchain ; synchro verticale. |
| Introspection | `GetApi`, `GetInfo`, `GetDesc`, `SupportsCompute` | API, infos runtime, descripteur, support compute. |
| Natif | `GetNativeContextData` | Pointeur natif opaque (caster via `NkNativeContext`). |
| Clear | `SetClearColor(r,g,b,a)` | Couleur d'effacement (no-op par défaut ; requis Vulkan/Software). |
| Courant | `MakeCurrent`, `ReleaseCurrent` | Rendre courant pour le thread (no-op hors GL). |
| Callbacks | `AddCleanUpCallback`, `AddRecreateCallback`, `RemoveCleanUpCallback`, `RemoveRecreateCallback` | Hooks avant/après recréation du swapchain. |
| Typedefs | `NkSwapchainCallbackHandle`, `NK_INVALID_CALLBACK_HANDLE`, `NkSwapchainCleanFn`, `NkSwapchainRecreateFn`, `NkGraphicsContextPtr` | Handle de callback, sentinelle, fonctions, pointeur propriétaire. |

### Infos runtime — `NkContextInfo.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Struct | `NkContextInfo` | `api`, `renderer`, `vendor`, `version`, `vramMB`, `debugMode`, `computeSupported`, `maxTextureSize`, `maxMSAASamples`, `windowWidth`, `windowHeight`. |

### Configuration OpenGL — `NkOpenGLDesc.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkGLProfile` | `CORE` / `COMPATIBILITY` / `ES` (+ alias `Core`/`Compatibility`/`ES`). |
| Enum | `NkGLContextFlags` (bitmask) | `NONE`/`DEBUG`/`FORWARD_COMPAT`/`ROBUST_ACCESS`/`NO_ERROR` (+ alias). |
| Enum | `NkGLSwapInterval` | `IMMEDIATE`(0) / `VSYNC`(1) / `ADAPTIVE_VSYNC`(-1). |
| Opérateur | `operator\|(NkGLContextFlags,…)`, `HasFlag(set, flag)` | Combinaison / test de bits. |
| Struct | `NkGLXHints` | Hints GLX (Linux) : stéréo, FBO flottant, fenêtre transparente, bits de couleur. |
| Struct | `NkEGLHints` | Hints EGL : bits de couleur, bind-to-texture, pbuffer, conformité. |
| Struct | `NkOpenGLRuntimeOptions` | Auto-load des entry points, validation de version, debug callback. |
| Struct | `NkOpenGLDesc` | Version, profil, flags, bits de couleur/depth/stencil, MSAA, sRGB, swap interval, hints. |
| Fabrique | `Desktop46`, `Desktop33`, `ES32` | Configs GL 4.6 / 3.3 / ES 3.2 prêtes. |

### Format de pixel Windows — `NkWGLPixelFormat.h` (Windows-only)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkPFDFlags` (bitmask) | Flags Win32 PFD (`DrawToWindow`/`DoubleBuffer`/`Stereo`/… + `Default`/`DefaultSingle`). |
| Enum | `NkPFDPixelType` | `RGBA` / `ColorIndex`. |
| Opérateur | `operator\|(NkPFDFlags,…)` | Combinaison de bits. |
| Struct | `NkWGLFallbackPixelFormat` | Bits couleur/alpha/depth/stencil/accum, version, type, flags. |
| Fabrique | `Minimal`, `Standard`, `HighPrecision`, `SingleBuffer` | Formats de secours WGL prêts. |

### Politique GPU — `NkGpuPolicy.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Classe | `NkGpuPolicy` (static-only) | Utilitaires de sélection GPU cross-platform. |
| Méthode | `ApplyPreContext(desc)` | Applique des hints process *avant* création (best-effort GL). |
| Méthode | `MatchesVendorPciId(pciId, vendor)` | Filtre un adaptateur par PCI ID (DXGI/Vulkan). |
| Méthode | `PreferenceName(pref)`, `VendorName(vendor)` | Noms lisibles. |

### Accès natif — `NkNativeContextAccess.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Template | `NkGetNativeAs<T>(ctx, expected)` | Cast sûr du handle natif si `GetApi() == expected`, sinon `nullptr`. |
| Struct (statics) | `NkNativeContext` | Accesseurs typés par backend des handles GPU natifs. |
| OpenGL | `OpenGL`, `GetOpenGLProcAddressLoader`, `GetOpenGLProcAddress`, `GetWGLContext/DC`, `GetGLXContext/Display`, `GetEGLContext/Display` | Handles GL/GLES/WebGL + loaders d'adresses de procédures. |
| Vulkan | `Vulkan`, `GetVkDevice/Instance/PhysicalDevice`, `GetVkGraphicsQueue/ComputeQueue`, `GetVkCurrentCommandBuffer/RenderPass/CurrentFramebuffer`, `GetVkComputePool` | Handles Vulkan de la frame courante. |
| DX11 | `DX11`, `GetDX11Device/Context/Swapchain/RTV/DSV` | Handles Direct3D 11. |
| DX12 | `DX12`, `GetDX12Device/CommandQueue/ComputeQueue/CommandList/ComputeCmdList`, `GetDX12CurrentBackBuffer/CurrentRTV/DSV` | Handles Direct3D 12. |
| Metal | `Metal`, `GetMetalDevice/CommandEncoder/CommandBuffer/Layer` | Handles Metal (`void*`, caster côté Obj-C). |
| Software | `Software`, `GetSoftwareBackBuffer/FrontBuffer` | Framebuffers CPU du rasteriseur. |

### Contexte compute — `NkIComputeContext.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Struct | `NkComputeBufferDesc` | Taille, lisible/écrivable CPU, atomics, données initiales. |
| Struct | `NkComputeBuffer`, `NkComputeShader`, `NkComputePipeline` | Handles opaques + `valid`. |
| Interface | `NkIComputeContext` | Interface GPGPU (toutes méthodes virtuelles pures). |
| Cycle de vie | `IsValid`, `Shutdown` | État / arrêt. |
| Buffers | `CreateBuffer`, `DestroyBuffer`, `WriteBuffer`, `ReadBuffer` | Allouer/libérer + upload/readback. |
| Shaders | `CreateShaderFromSource`, `CreateShaderFromFile`, `DestroyShader` | Compiler des kernels. |
| Pipelines | `CreatePipeline`, `DestroyPipeline` | Monter/défaire un pipeline compute. |
| Dispatch | `BindBuffer`, `BindPipeline`, `Dispatch` | Lier ressources puis lancer le calcul. |
| Synchro | `WaitIdle`, `MemoryBarrier` | GPU idle ; barrière GPU→GPU. |
| Capacités | `GetApi`, `GetMaxGroupSizeX/Y/Z`, `GetSharedMemoryBytes`, `SupportsAtomics`, `SupportsFloat64` | Introspection du backend compute. |
| Typedef | `NkComputeContextPtr` | Pointeur propriétaire (`NkUniquePtr`). |

### Fabrique — `NkContextFactory.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Classe | `NkContextFactory` (static-only) | Fabrique de contextes (multi-contexte par app/fenêtre). |
| Graphique | `Create(window, desc)` | Crée un contexte complet (vous êtes propriétaire). |
| Graphique | `Destroy(ctx)` | Libère via NKMemory (no-op si `nullptr`) — **jamais** `delete`. |
| Graphique | `IsApiSupported(api)` | Disponibilité runtime de l'API. |
| Graphique | `CreateWithFallback(window, order, count)` | Essaie une liste d'API jusqu'au premier succès. |
| Compute | `CreateCompute(api, desc)` | Contexte compute autonome (sans surface). |
| Compute | `ComputeFromGraphics(gfx)` | Contexte compute dérivé d'un contexte graphique. |
| Compute | `DestroyCompute(ctx)` | Libère le contexte compute (no-op si `nullptr`). |

### Umbrella — `NKRenderer2D.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| En-tête parapluie | `NKRenderer2D.h` | Inclut tout le système renderer 2D (aucun type propre). |

### Shim — `NkSurfaceDesc.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| En-tête de compat | `NkSurfaceDesc.h` | Réexporte `NKWindow/Core/NkSurface.h` (aucun symbole ajouté). |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines du temps
réel — rendu, ECS/scène, physique, animation, gameplay/IA, audio, UI/2D, IO, GPU compute,
threading, outils/éditeur. La prose est à puces, pas en tableaux.

### `NkGraphicsApi` — l'enum pivot

C'est le type qui circule partout : le descripteur dit quelle API, la fabrique en crée une, les
infos en rapportent une, l'accès natif en exige une. Ses valeurs sont **séquentielles** à partir
de `NK_GFX_API_NONE = 0` ; `NK_GFX_API_MAX` est une sentinelle qui vaut le nombre total d'API
(pratique pour dimensionner un tableau indexé par API), et `NK_GFX_API_AUTO` demande la sélection
automatique selon la plateforme.

- **Outils / éditeur** — un combo box « backend de rendu » dans les préférences se peuple en
  parcourant les valeurs et en filtrant par `NkGraphicsApiIsAvailable`, affichées via
  `NkGraphicsApiName`.
- **Rendu** — le code de plus haut niveau adapte parfois un détail au backend (flip Y de la
  shadow map en DX, sRGB du swapchain en Vulkan) en testant `ctx->GetApi()`.
- **IO / config** — sérialiser le choix de l'utilisateur revient à stocker la valeur de l'enum ;
  `NK_GFX_API_AUTO` est la valeur par défaut « laisse le moteur décider ».
- **Threading** — l'enum aide à savoir si `MakeCurrent`/`ReleaseCurrent` ont un sens (GL est
  lié au thread courant ; les autres non).

### `NkGraphicsApiName`, `NkGraphicsApiIsAvailable` — nommer et filtrer

`NkGraphicsApiName` renvoie un nom lisible et stable (`"OpenGL"`, `"DirectX 11"`, `"Software"`,
`"None"` par défaut) — c'est l'outil des **logs** et de l'**UI**. `NkGraphicsApiIsAvailable` est
une garde *de compilation* : elle reflète quels backends sont compilés pour la plateforme
courante (Windows, macOS, iOS, Android, Emscripten, autre), Vulkan étant en plus conditionné par
`NKENTSEU_ENABLE_VULKAN_BACKEND`. Les deux sont `O(1)`.

- À ne pas confondre avec `NkContextFactory::IsApiSupported`, qui interroge le **runtime** (driver
  présent, matériel capable) : `IsAvailable` dit « ce code existe sur cette plateforme »,
  `IsApiSupported` dit « cette machine peut vraiment l'ouvrir ».

### `NkGpuPreference`, `NkGpuVendor`, `NkGpuSelectionDesc` — choisir le bon GPU

Sur les machines à plusieurs GPU (laptop avec iGPU + dGPU, station multi-cartes), il faut souvent
*orienter* le choix. `NkGpuPreference` exprime l'intention : `NK_LOW_POWER` (économiser la
batterie, prendre l'iGPU) contre `NK_HIGH_PERFORMANCE` (forcer le GPU dédié), `NK_DEFAULT`
laissant le système trancher. `NkGpuVendor` permet d'exiger un fabricant précis
(`NK_NVIDIA`/`NK_AMD`/`NK_INTEL`/`NK_ARM`/`NK_QUALCOMM`/`NK_APPLE`/`NK_MICROSOFT`), `NK_ANY` étant
neutre.

`NkGpuSelectionDesc` rassemble tout cela : `preference`, `adapterIndex` (−1 = auto, ou un index
explicite), `vendorPreference`, `allowSoftwareAdapter` (autoriser le fallback logiciel, `true` par
défaut) et `enableOpenGLPlatformHints` (hints best-effort côté GL).

- **Rendu** — une appli AAA force `NK_HIGH_PERFORMANCE` ; un outil léger ou un mode batterie
  prend `NK_LOW_POWER`.
- **Outils / éditeur** — exposer la sélection d'adaptateur par index permet à l'utilisateur de
  choisir sa carte sur une station multi-GPU.
- **Tests / CI** — `allowSoftwareAdapter = true` garantit qu'un build tourne même sur une machine
  sans GPU (WARP, rasteriseur logiciel).

### `NkVulkanDesc`, `NkDirectX11Desc`, `NkDirectX12Desc`, `NkMetalDesc`, `NkSoftwareDesc` — les sous-descripteurs

Chaque famille a son struct de réglages, agrégé par valeur dans `NkContextDesc`. Tous ont des
défauts sûrs ; on n'en touche que ce qu'on veut changer.

- **`NkVulkanDesc`** — noms d'app/moteur, version d'API (`0` = auto VK 1.3), `validationLayers` et
  `debugMessenger` (**`false` par défaut** : la couche de validation peut crasher en chargeant un
  mauvais runtime), `vsync`, `swapchainImages` (3), `msaaSamples`, `preferredAdapterIndex`,
  `enableComputeQueue`, et des extensions instance/device supplémentaires.
- **`NkDirectX11Desc`** — `debugDevice`, `vsync`, `allowTearing`, `msaaSamples`/`msaaQuality`,
  `swapchainBuffers` (2), `preferredAdapter`, `minFeatureLevel` (`0` = 11_0).
- **`NkDirectX12Desc`** — `debugDevice`, `gpuValidation`, `vsync`, `allowTearing` (`true`),
  `swapchainBuffers` (3), et surtout les **tailles de heaps** (`rtvHeapSize` 256, `dsvHeapSize`
  64, `srvHeapSize` 1024, `samplerHeapSize` 64) qu'il faut dimensionner selon le nombre de
  ressources, plus `preferredAdapter` et `enableComputeQueue`.
- **`NkMetalDesc`** — `validation`, `vsync`, `sampleCount`, `srgb`.
- **`NkSoftwareDesc`** — `threading` (rasterisation multi-thread), `threadCount` (`0` =
  `hardware_concurrency`), `useSSE`, `pixelFormat` (`0` = RGBA8). C'est le backend de secours
  universel, sans GPU.

Cas transverses : sur **DX12**, sous-dimensionner `srvHeapSize` provoque des échecs de binding
quand on charge beaucoup de textures (cf. les sessions de bring-up) ; sur **Vulkan**, laisser la
validation à `false` évite les crashs sur certaines machines tout en gardant l'opt-in pour le
débogage ; le backend **Software** sert autant aux tests CI qu'au fallback de production.

### `NkComputeActivationDesc`, `NkContextDesc::IsComputeEnabledForApi` — armer le compute

Le calcul GPU n'est jamais activé implicitement. `NkComputeActivationDesc` porte un `enable`
global (`false` par défaut) plus un flag par backend (`opengl`/`vulkan`/`directx11`/`directx12`/
`metal`/`software`, tous `true`). Pour qu'un contexte expose le compute, il faut donc
`compute.enable = true` **et** le flag du backend ciblé. `IsComputeEnabledForApi(api)` fait
exactement ce test : `false` si `!compute.enable`, sinon il mappe l'API vers son flag (`OPENGL`/
`OPENGLES` → `opengl`, etc.) — `O(1)`.

- **Physique / particules** — activer le compute pour décharger une simulation sur le GPU.
- **Rendu** — passes de post-traitement en compute (réduction, histogramme, bloom).
- **Gameplay / IA** — pathfinding ou raycasts massivement parallèles.
- Toujours **tester `IsComputeEnabledForApi` avant** d'appeler `CreateCompute`/`ComputeFromGraphics`,
  pour échouer proprement plutôt qu'à la création.

### `NkContextDesc` et ses fabriques — le descripteur unifié

Le cœur du module côté configuration. On remplit un seul objet : `api` désigne le backend réel,
les sous-descripteurs (`opengl`, `vulkan`, `dx11`, `dx12`, `metal`, `software`, `compute`, `gpu`)
restent à leurs défauts tant qu'on n'y touche pas. Les fabriques statiques court-circuitent le
remplissage manuel :

- `MakeOpenGL(maj=4, min=6, dbg=false)` — GL 4.6 Core (via `NkOpenGLDesc::Desktop46`), version
  surchargeable.
- `MakeOpenGLES(maj=3, min=2)` — GL ES 3.2 (via `ES32`).
- `MakeVulkan(val=false)` — Vulkan, validation/debug positionnés ensemble.
- `MakeDirectX11(dbg=false)` / `MakeDirectX12(dbg=false)` — DX avec device de débogage optionnel.
- `MakeMetal()` — Metal par défaut.
- `MakeSoftware(threaded=true)` — rasteriseur logiciel, multi-thread par défaut.

Usage typique : on appelle une fabrique, puis on ajuste finement le sous-descripteur ciblé
(`desc.dx12.srvHeapSize = 65536;`, `desc.gpu.preference = …;`, `desc.compute.enable = true;`).

### `NkIGraphicsContext` — l'interface du contexte

L'abstraction centrale, implémentée par chaque backend. On ne la construit ni ne la détruit
directement : elle naît de `NkContextFactory::Create` et meurt par `Destroy`. Ses méthodes se
groupent par rôle :

- **Cycle de vie** — `Initialize(window, desc)` crée device/surface/swapchain ; `Shutdown` libère ;
  `IsValid` indique l'état.
- **Boucle de frame** — `BeginFrame` (sur Vulkan/Software, le *clear* a lieu ici via le loadOp ou
  l'effacement du back-buffer CPU), `EndFrame`, puis `Present` qui affiche.
- **Surface** — `OnResize(w, h)` recrée la swapchain. **Piège** : à ne déclencher que si la taille
  a *réellement* changé — Windows envoie un `WM_SIZE` à la création de la fenêtre, et un handler
  naïf provoque un `OnResize` inutile avant la première frame (crash DX12 sur cmdList non
  consommée, réinit de DIB sur Software).
- **Synchro** — `SetVSync`/`GetVSync`.
- **Introspection** — `GetApi`, `GetInfo`, `GetDesc`, `SupportsCompute`.
- **Natif** — `GetNativeContextData` renvoie un `void*` opaque ; ne le castez pas à la main,
  passez par `NkNativeContext` (cf. plus bas).

Méthodes à implémentation par défaut :

- `SetClearColor(r, g, b, a)` (composantes 0..1) — **no-op par défaut** car GL/DX effacent via le
  renderer ; mais **Vulkan et Software en ont besoin AVANT `BeginFrame`** (sinon fond en dur).
  `NkRenderWindow::Clear` l'appelle. C'est l'un des pièges les plus fréquents du module.
- `MakeCurrent`/`ReleaseCurrent` — rendent le contexte courant pour le thread appelant ; no-op
  hors OpenGL (les autres API ne sont pas liées au thread). Important en **threading** quand on
  rend depuis un thread dédié.
- **Callbacks de swapchain** — `AddCleanUpCallback` (appelé *avant* destruction du swapchain :
  détruire pipelines/framebuffers liés à l'ancien) et `AddRecreateCallback` (appelé *après*
  recréation : reconstruire). Ils retournent un `NkSwapchainCallbackHandle` (ou
  `NK_INVALID_CALLBACK_HANDLE`) qu'on passe à `Remove*`. Ils reproduisent le pattern Vulkan
  m_CleanList/m_RecreateList — indispensable pour tout système qui détient des objets dépendants
  du swapchain (passes de rendu, descripteurs, FBO).

Les typedefs associés complètent l'interface : `NkSwapchainCleanFn`/`NkSwapchainRecreateFn` sont
des `NkFunction<void()>` (lambdas avec captures OK), et `NkGraphicsContextPtr` est un
`NkUniquePtr<NkIGraphicsContext>` quand on veut une propriété RAII plutôt que le couple
`Create`/`Destroy` manuel.

### `NkContextInfo` — la carte d'identité runtime

Renvoyé par `GetInfo()`, c'est un struct pur de données décrivant le contexte vivant : `api`,
`renderer` (nom du GPU), `vendor`, `version`, `vramMB`, `debugMode`, `computeSupported`,
`maxTextureSize`, `maxMSAASamples`, et `windowWidth`/`windowHeight`.

- **Outils / éditeur** — un panneau « infos système » affiche directement ces champs (GPU, VRAM,
  version du driver).
- **Rendu** — `maxTextureSize`/`maxMSAASamples` bornent ce qu'on peut demander ; `computeSupported`
  conditionne les passes GPGPU.
- **Piège majeur** — un backend qui **oublie** de renseigner `windowWidth`/`windowHeight` fait
  tomber le renderer 2D consommateur sur un fallback 800×600, d'où une projection trop petite et
  « la moitié droite du monde invisible ». Tout backend *doit* les remplir.

### `NkOpenGLDesc` et ses types — la configuration OpenGL fine

OpenGL demande plus de réglages que les autres (création de contexte historique). `NkGLProfile`
choisit Core / Compatibility / ES. `NkGLContextFlags` est un bitmask
(`DEBUG`/`FORWARD_COMPAT`/`ROBUST_ACCESS`/`NO_ERROR`) combinable par `operator|` et testable par
`HasFlag`. `NkGLSwapInterval` règle la présentation : `IMMEDIATE` (pas de vsync, tearing),
`VSYNC`, ou `ADAPTIVE_VSYNC` (−1, vsync qui se désactive sous le rafraîchissement).

Les sous-structs portent les détails par plateforme : `NkGLXHints` (Linux/GLX : stéréo, FBO
flottant, fenêtre transparente, bits de couleur), `NkEGLHints` (bits, bind-to-texture, pbuffer,
conformité), et `NkOpenGLRuntimeOptions` (auto-load des entry points — ignoré si `NK_NO_GLAD2` —,
validation de version, installation du debug callback). `NkOpenGLDesc` assemble tout : version,
profil, flags, bits couleur/depth/stencil, MSAA, sRGB, double buffer, swap interval, hints, et le
format de secours WGL.

Trois fabriques couvrent l'essentiel : `Desktop46(dbg)` (GL 4.6 Core ForwardCompat, debug
optionnel), `Desktop33(dbg)` (GL 3.3, matériel plus ancien), `ES32()` (GL ES 3.2 pour mobile, sans
debug callback).

- **Rendu** — `srgbFramebuffer` et le swap interval pilotent le rendu de couleur et le tearing.
- **Outils** — `Debug` + `installDebugCallback` activent les messages du driver pendant le
  développement.
- **Mobile / Web** — `ES32` est la base GLES ; les hints EGL servent Android et l'embarqué.

### `NkWGLPixelFormat` — le format de pixel Windows (Windows-only)

Sous Windows, créer un contexte GL passe par un *pixel format* Win32. `NkPFDFlags` mappe les
drapeaux PFD (`DrawToWindow`/`DoubleBuffer`/`Stereo`/`SwapExchange`/`SwapCopy`…), combinables par
`operator|`, avec les agrégats `Default` et `DefaultSingle`. `NkPFDPixelType` choisit `RGBA` ou
`ColorIndex` (legacy). `NkWGLFallbackPixelFormat` décrit un format de secours (bits couleur/
alpha/depth/stencil/accum, version, type, flags) utilisé quand le chemin moderne échoue ; ses
fabriques `Minimal` (24/0/16/0), `Standard` (défauts), `HighPrecision` (couleur 32, depth 32) et
`SingleBuffer` (sans double buffer) couvrent les cas usuels. Hors Windows, le struct se réduit aux
mêmes fabriques renvoyant `{}` (compat de compilation, code cross-platform inchangé).

### `NkGpuPolicy` — appliquer la politique GPU

Classe utilitaire *static-only* qui met en œuvre la sélection GPU au niveau process et adaptateur.
`ApplyPreContext(desc)` applique des hints *avant* la création du contexte (best-effort, surtout
côté OpenGL où le choix du GPU se fait par variables/symboles du driver). `MatchesVendorPciId(pciId,
vendor)` filtre un adaptateur DXGI/Vulkan par son PCI vendor ID (sélectionner NVIDIA vs AMD à
l'énumération). `PreferenceName`/`VendorName` donnent des noms lisibles pour les logs et l'UI.

- **Rendu / éditeur** — câbler la préférence utilisateur jusqu'au choix réel de l'adaptateur.
- **Multi-GPU** — `MatchesVendorPciId` permet d'itérer les adaptateurs et de retenir celui du bon
  vendeur.

### `NkNativeContext` et `NkGetNativeAs` — descendre au niveau natif

Tôt ou tard, on doit récupérer un handle GPU brut : passer un device Vulkan à une bibliothèque
tierce, interfacer un overlay, brancher un débogueur. `GetNativeContextData()` renvoie un `void*`
opaque ; le caster à la main est fragile. `NkNativeContext` est l'**API publique cross-API** qui
fait le cast *sûr* : chaque accesseur est `static`, vérifie `GetApi()`, et renvoie `nullptr` /
handle nul si l'API ne correspond pas.

Le helper générique `NkGetNativeAs<T>(ctx, expected)` résume le motif : `nullptr` si `ctx` est nul
ou si `ctx->GetApi() != expected`, sinon `static_cast<T*>(ctx->GetNativeContextData())` — `O(1)`.

Les accesseurs spécialisés couvrent tous les backends :

- **OpenGL/GLES/WebGL** — `OpenGL(ctx)` (accepte les trois), plus les loaders d'adresses de
  procédures (`GetOpenGLProcAddressLoader`, `GetOpenGLProcAddress`, avec fallback Emscripten pour
  WebGL) et les handles natifs par plateforme : `GetWGLContext`/`GetWGLDC` (Windows),
  `GetGLXContext`/`GetGLXDisplay` (XLib/XCB), `GetEGLContext`/`GetEGLDisplay` (Wayland/Android).
- **Vulkan** (gardé par `NKENTSEU_HAS_VULKAN_HEADERS`) — `Vulkan(ctx)` plus `GetVkDevice`,
  `GetVkInstance`, `GetVkPhysicalDevice`, `GetVkGraphicsQueue`/`GetVkComputeQueue`,
  `GetVkCurrentCommandBuffer` (frame courante), `GetVkRenderPass`, `GetVkCurrentFramebuffer` (image
  courante), `GetVkComputePool` — chacun renvoyant `VK_NULL_HANDLE` hors Vulkan.
- **DirectX 11** (Windows) — `DX11(ctx)`, `GetDX11Device` (`ID3D11Device1*`), `GetDX11Context`
  (`ID3D11DeviceContext1*`), `GetDX11Swapchain` (`IDXGISwapChain1*`), `GetDX11RTV`/`GetDX11DSV`.
- **DirectX 12** (Windows) — `DX12(ctx)`, `GetDX12Device` (`ID3D12Device5*`),
  `GetDX12CommandQueue`/`GetDX12ComputeQueue`, `GetDX12CommandList` (`ID3D12GraphicsCommandList4*`),
  `GetDX12ComputeCmdList`, `GetDX12CurrentBackBuffer`, `GetDX12CurrentRTV`
  (`D3D12_CPU_DESCRIPTOR_HANDLE`), `GetDX12DSV`.
- **Metal** (macOS/iOS) — `Metal(ctx)`, `GetMetalDevice`/`GetMetalCommandEncoder`/
  `GetMetalCommandBuffer`/`GetMetalLayer` (renvoient `void*`, à caster côté Obj-C).
- **Software** — `Software(ctx)`, `GetSoftwareBackBuffer`/`GetSoftwareFrontBuffer` (framebuffers
  CPU, via `dynamic_cast`, `nullptr` hors Software).

Cas d'usage : **interop GPU** (passer le device à une lib externe), **outils** (capture
RenderDoc/PIX), **rendu avancé** (injecter une passe custom dans la command list courante), **UI**
(brancher un backend d'overlay sur le command buffer Vulkan/DX). Toujours préférer ces accesseurs
typés au cast `void*` manuel : la vérification d'API évite les *undefined behaviors*.

### `NkComputeBufferDesc`, `NkComputeBuffer`, `NkComputeShader`, `NkComputePipeline` — les ressources compute

Le compute manipule des handles opaques (pas de templates — compat C linkage). `NkComputeBufferDesc`
décrit un buffer : `sizeBytes`, `cpuReadable` (readback GPU→CPU), `cpuWritable` (upload CPU→GPU,
`true` par défaut), `atomics` (UAV/SSBO atomiques), `initialData`. `NkComputeBuffer`,
`NkComputeShader` et `NkComputePipeline` sont des poignées (`void* handle` + `valid`) qu'on teste
avant usage.

### `NkIComputeContext` — l'interface GPGPU

Le mini-RHI de calcul, toutes méthodes virtuelles pures (sauf le destructeur). On l'obtient par la
fabrique et on l'utilise en quatre temps :

- **Cycle de vie** — `IsValid`, `Shutdown`.
- **Buffers** — `CreateBuffer(desc)` / `DestroyBuffer(buf)` ; `WriteBuffer` (upload) /
  `ReadBuffer` (readback), avec offset.
- **Shaders** — `CreateShaderFromSource(source, entry="main")` ou `CreateShaderFromFile(path,
  entry)` ; la source est du GLSL/HLSL/MSL ou un chemin `.spv` selon le backend, `entry` étant le
  nom du kernel. `DestroyShader` libère.
- **Pipelines** — `CreatePipeline(shader)` / `DestroyPipeline`.
- **Dispatch** — `BindBuffer(slot, buf)`, `BindPipeline(pipeline)`, puis `Dispatch(groupX,
  groupY=1, groupZ=1)` lance la grille de groupes de threads.
- **Synchronisation** — `WaitIdle()` bloque jusqu'à GPU au repos (avant un readback) ;
  `MemoryBarrier()` ordonne deux dispatches qui se passent des données.
- **Capacités** — `GetApi`, `GetMaxGroupSizeX/Y/Z` (limites de taille de groupe),
  `GetSharedMemoryBytes` (mémoire partagée par groupe), `SupportsAtomics`, `SupportsFloat64`.

Domaines : **physique/particules** (intégration de milliers de corps en parallèle), **rendu**
(culling GPU, génération de mipmaps, post-traitement), **IA/gameplay** (pathfinding de masse),
**audio** (convolution, FFT), **outils** (traitement d'image hors-écran). Détail technique : sous
Windows, le header `#undef MemoryBarrier` pour éviter le conflit avec la macro Win32 du même nom.
Tout `Create*` ou handle a son `Destroy*` symétrique. Le typedef `NkComputeContextPtr`
(`NkUniquePtr<NkIComputeContext>`) offre une propriété RAII.

### `NkContextFactory` — la fabrique

La porte unique de création/destruction, *static-only*, supportant plusieurs contextes par
application et par fenêtre.

- **Graphique** — `Create(window, desc)` instancie le contexte complet (device + surface +
  swapchain) ; **vous êtes propriétaire**. `Destroy(ctx)` appelle `Shutdown()` puis libère via
  NKMemory (no-op si `nullptr`). **Jamais `delete`** — c'est la règle dure du module (heap
  corruption sinon). `IsApiSupported(api)` interroge le support *runtime*. `CreateWithFallback(window,
  order, count)` essaie une liste d'API dans l'ordre jusqu'au premier succès — ordre conseillé
  `{DX12, DX11, Metal, Vulkan, OpenGL, Software}`, le Software garantissant un succès final.
- **Compute** — `CreateCompute(api, desc)` crée un contexte compute autonome (sans surface,
  GPGPU pur), requérant `desc.compute.enable` + le backend activé. `ComputeFromGraphics(gfx)`
  dérive un contexte compute d'un contexte graphique existant (partage device/queue, pas de device
  en plus), requérant `gfx->GetDesc().compute.enable == true`. `DestroyCompute(ctx)` libère (no-op
  si `nullptr`).

Cas d'usage : **multi-fenêtre** (un contexte par fenêtre d'éditeur), **viewports** (plusieurs vues
d'une scène), **fallback robuste** sur machines hétérogènes, **compute partagé** (réutiliser le
device de rendu pour la simulation sans coût supplémentaire).

### `NKRenderer2D.h` — l'umbrella du renderer 2D

En-tête **parapluie** : il ne déclare aucun type propre, il inclut tout le système renderer 2D
(`NkRenderer2DTypes.h`, `NkIRenderer2D.h`, `NkRenderer2DFactory.h`, le module externe
`NKImage/NKImage.h`, `NkTexture.h`, `NkFont.h`, `NkSprite.h` qui contient aussi `NkText`). Les
types eux-mêmes vivent dans `nkentseu::renderer` et sont documentés ailleurs — ici, on retient
seulement que `#include "NKCanvas/Core/NKRenderer2D.h"` suffit à tirer tout le 2D d'un coup.

### `NkSurfaceDesc.h` — le shim de compatibilité

Un simple `#include "NKWindow/Core/NkSurface.h"` : il **n'ajoute aucun symbole**. Les types de
surface canoniques vivent dans NKWindow ; ce shim maintient la compatibilité des anciens includes
NKRenderer qui référençaient `NkSurfaceDesc.h`. Documenter `NkSurface` relève du module NKWindow.

### Récap des pièges transverses

- **Create ↔ Destroy** — contextes graphiques *et* compute alloués par NKMemory : libérer
  **uniquement** par `NkContextFactory::Destroy`/`DestroyCompute`, **jamais** `delete` ni heap CRT.
- **`SetClearColor` avant `BeginFrame`** — indispensable sur Vulkan et Software (sinon fond en
  dur).
- **`OnResize`** — ne le déclencher que sur changement *réel* de taille (piège du `WM_SIZE`
  initial Windows).
- **`GetInfo().windowWidth/Height`** — doivent être renseignés par le backend, sinon le renderer
  2D tombe sur un fallback 800×600.
- **Compute** — nécessite `compute.enable = true` + le flag du backend ; tester avec
  `IsComputeEnabledForApi`.
- **Aucune** méthode n'est `noexcept`/`[[nodiscard]]` dans ces headers.

---

### Exemple

```cpp
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Factory/NkContextFactory.h"
#include "NKCanvas/Core/NkNativeContextAccess.h"
using namespace nkentseu;

// 1) Décrire ce qu'on veut : Vulkan, GPU dédié, compute armé.
NkContextDesc desc = NkContextDesc::MakeVulkan();
desc.gpu.preference = NkGpuPreference::NK_HIGH_PERFORMANCE;
desc.compute.enable = true;                       // pour du GPGPU

// 2) Fabriquer (avec repli si Vulkan échoue), on devient propriétaire.
const NkGraphicsApi order[] = {
    NK_GFX_API_VULKAN, NK_GFX_API_DX12, NK_GFX_API_OPENGL, NK_GFX_API_SOFTWARE
};
NkIGraphicsContext* ctx = NkContextFactory::CreateWithFallback(window, order, 4);

// 3) Boucle de frame.
ctx->SetClearColor(0.1f, 0.1f, 0.12f, 1.f);       // AVANT BeginFrame (Vulkan/Software)
while (running) {
    if (resized) ctx->OnResize(w, h);             // seulement si la taille a changé
    ctx->BeginFrame();
    // ... dessin via le renderer 2D, qui s'appuie sur ce contexte ...
    ctx->EndFrame();
    ctx->Present();
}

// 4) Accès natif typé si besoin (interop, débogueur).
NkContextInfo info = ctx->GetInfo();
NkLogInfo("GPU: %s (%s) — VRAM %u Mo", info.renderer, info.vendor, info.vramMB);

// 5) Compute partagé, dérivé du contexte graphique.
if (desc.IsComputeEnabledForApi(ctx->GetApi())) {
    NkIComputeContext* cc = NkContextFactory::ComputeFromGraphics(ctx);
    // ... CreateBuffer / CreateShaderFromFile / Dispatch / WaitIdle ...
    NkContextFactory::DestroyCompute(cc);
}

// 6) Libérer — JAMAIS delete.
NkContextFactory::Destroy(ctx);
```

---

[← Index NKCanvas](README.md) · [Récap NKCanvas](../NKCanvas.md) · [Couche Runtime](../README.md)
