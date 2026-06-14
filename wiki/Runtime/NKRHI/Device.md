# Le device et sa fabrique

> Couche **Runtime** · NKRHI · La **porte d'entrée** du GPU : la fabrique `NkDeviceFactory`
> qui choisit et crée un backend, l'interface abstraite `NkIDevice` qui crée et détruit toutes
> les ressources, le descripteur `NkContextDesc` / `NkDeviceInitInfo` qui décrit *quoi* créer, et
> le `NkISwapchain` qui présente à l'écran.

Avant de pouvoir dessiner quoi que ce soit, il faut **parler au GPU**. Mais « le GPU » n'est pas
une chose unique : c'est Vulkan sur un PC Windows, Metal sur un Mac, DirectX sur une Xbox, du logiciel
pur quand rien d'autre n'est disponible. Chacune de ces API a son vocabulaire, ses objets, ses pièges.
Le rôle de NKRHI (*Render Hardware Interface*) est de **gommer ces différences** derrière une seule
interface : on programme une fois contre `NkIDevice`, et le même code tourne sur six backends. Tout le
problème — et tout le compromis — tient en une phrase : **on échange un peu de contrôle bas niveau
spécifique à chaque API contre un code de rendu unique, portable et testable sur toutes les plateformes.**

Le device n'est **pas** un contexte graphique brut (le `NkIGraphicsContext` de la couche 0, qui possède
la fenêtre et la surface) : le RHI **réutilise** ce contexte natif au lieu d'en recréer un. Ce n'est pas
non plus un moteur de rendu de haut niveau (ça, c'est NKRenderer ou NKCanvas, qui se construisent
*au-dessus* du RHI) : le device ne connaît ni matériaux, ni lumières, ni caméras — seulement des buffers,
des textures, des pipelines et des command buffers. C'est la **fine couche** qui transforme « crée-moi un
tampon de 4 Ko visible en vertex » en l'appel Vulkan/DX/Metal correspondant.

- **Namespace** : `nkentseu` (aucun sous-namespace ; `NkGraphicsApi` est un alias de `graphics::NkGraphicsApi`)
- **Headers réels** : `NKRHI/Core/NkIDevice.h`, `NkDeviceFactory.h`, `NkDeviceInitInfo.h`,
  `NkContextDesc.h`, `NkContextInfo.h`, `NkGraphicsApi.h`, `NkGpuPolicy.h`, `NkISwapchain.h`,
  `NkTypes.h`

---

## Choisir et créer : `NkDeviceFactory`

On ne construit **jamais** un device avec `new`. Le point d'entrée unique de toute la couche est la
fabrique `NkDeviceFactory`, une classe purement statique qui sait quels backends ont été **compilés**
dans le binaire et lesquels **marchent réellement** sur la machine. Cette distinction est le cœur du
sujet : un backend peut être *présent* (le code DX12 est dans l'exécutable) sans *fonctionner* (la carte
ne le supporte pas, le driver est trop vieux). La fabrique sépare les deux mondes.

Quatre façons de créer, du plus dirigiste au plus automatique. `Create(init)` crée le device pour l'API
fixée dans le bloc d'init. `CreateForApi(api, init)` force une API explicite. `CreateWithFallback(init,
{…})` essaie une liste d'API dans l'ordre que vous donnez et garde la première qui marche.
`CreateAutoDetect(init)` enfin teste les API dans l'**ordre optimal de la plateforme** (Windows :
Vulkan > DX12 > DX11 > OpenGL > Software) et — détail crucial — **réécrit `init.api`** avec l'API
finalement choisie, pour que le reste du code sache sur quoi il tourne.

```cpp
NkDeviceInitInfo init = /* … api, fenêtre, callbacks … */;
NkIDevice* device = NkDeviceFactory::CreateAutoDetect(init);   // modifie init.api
if (!device) { /* aucune API ne marche sur cette machine */ }
// … on rend …
NkDeviceFactory::Destroy(device);   // met le pointeur à nullptr
```

Attention à la nuance temporelle : `IsApiSupported(api)` et `GetSupportedApis()` répondent à
**compile-time** (« ce backend est-il dans le binaire ? ») et **ne garantissent pas** qu'il tourne ;
seul `CreateAutoDetect` teste pour de vrai à **runtime**. Et l'**ownership** est strict : tout device
créé par la fabrique se détruit par `Destroy(device&)`, qui remet le pointeur à `nullptr` — jamais
`delete`.

> **En résumé.** `NkDeviceFactory` est la seule porte d'entrée. `CreateAutoDetect` pour « débrouille-toi »
> (il réécrit `init.api`), `CreateForApi`/`CreateWithFallback` pour piloter le choix. `IsApiSupported` =
> compile-time (présent), `CreateAutoDetect` = runtime (marche vraiment). On détruit toujours via
> `Destroy`, jamais `delete`.

---

## Décrire ce qu'on veut : `NkContextDesc` et `NkDeviceInitInfo`

Créer un device, c'est répondre à une question : **quoi, comment, et où ?** Ces réponses tiennent dans
deux structures emboîtées. `NkContextDesc` dit *comment* configurer le backend (version d'OpenGL,
couches de validation Vulkan, taille des heaps DX12, format du swapchain…). `NkDeviceInitInfo` l'enrobe
avec le *où* (l'API, la surface fenêtre, la taille, les callbacks de présentation et de redimensionnement).

Le principe directeur est qu'on **n'a pas à tout remplir**. Chaque champ a un défaut sensé, et des
**fabriques statiques** couvrent les cas courants : `NkContextDesc::MakeVulkan()`, `MakeDirectX12()`,
`MakeOpenGL(4, 6)`, `MakeSoftware()` produisent un descripteur prêt à l'emploi. À l'intérieur,
`NkContextDesc` contient un sous-descripteur par backend (`NkVulkanDesc`, `NkDirectX11Desc`,
`NkDirectX12Desc`, `NkMetalDesc`, `NkSoftwareDesc`, `NkOpenGLDesc`) plus la politique compute
(`NkComputeActivationDesc`) et la sélection GPU (`NkGpuSelectionDesc`).

```cpp
NkDeviceInitInfo init;
init.context = NkContextDesc::MakeVulkan();   // défauts sains (validation OFF)
init.api     = init.context.api;
init.width   = 1280; init.height = 720;
init.presentCallback = [&]{ window.SwapBuffers(); };
```

Un point d'unification important : le **format du swapchain** est une **source unique cross-API**,
`NkContextDesc::swapchainFormat` (défaut `NK_SWAPCHAIN_BGRA8_UNORM`, sans ré-encodage gamma). Ce n'est
**pas** aux anciens champs par-API (`vulkan.srgbSwapchain`, `opengl.srgbFramebuffer`, `metal.srgb`) qu'il
faut toucher : ils existent encore dans les structs mais **ne sont plus lus** par les devices — piège de
redondance classique. De même, deux pièges de cohérence : `validationLayers` Vulkan est **opt-in**
(`false` par défaut, car l'activer peut charger un mauvais runtime et crasher), et l'API peut être
indiquée à **deux endroits** (`init.api` au top-level *ou* `init.context.api`) qu'il faut garder alignés.

> **En résumé.** `NkContextDesc` = *comment* (un sous-desc par backend + compute + GPU), `NkDeviceInitInfo`
> = *où* (API, surface, taille, callbacks). Préférez les fabriques `MakeXxx()`. Le format de présentation
> passe par le **seul** `swapchainFormat` (les champs sRGB par-API sont morts). Validation Vulkan opt-in.

---

## Piloter le GPU : `NkIDevice`

Une fois le device en main, **tout** passe par lui. C'est une interface abstraite massive (toutes ses
méthodes sont `virtual`) dont chaque backend fournit l'implémentation. Le motif est toujours le même et
profondément régulier : un couple `CreateXxx` / `DestroyXxx` par type de ressource — buffers, textures,
samplers, shaders, pipelines, render passes, framebuffers, descriptor sets, fences, sémaphores, command
buffers. On crée, on obtient un **handle opaque**, on l'utilise, on le détruit *par le device*.

Cette régularité cache une astuce de conception : beaucoup de méthodes ont une **implémentation par
défaut** (bindless, queries GPU, debug names, swapchain multi-fenêtre, semaphores…). Un backend simple
comme le Software n'override que le strict nécessaire et hérite de no-op partout ailleurs. C'est ce qui
permet d'avoir six backends sans dupliquer six fois le même code mort.

```cpp
NkBufferHandle vbo = device->CreateBuffer(vboDesc);
device->WriteBuffer(vbo, vertices, size);      // upload synchrone (bloque)

NkFrameContext frame;
if (device->BeginFrame(frame)) {
    NkICommandBuffer* cmd = device->CreateCommandBuffer();
    // … enregistrer les commandes …
    device->SubmitAndPresent(cmd);
    device->DestroyCommandBuffer(cmd);
    device->EndFrame(frame);
}
device->DestroyBuffer(vbo);                     // jamais delete, toujours DestroyXxx
```

Ce n'est **pas** un objet qu'on manipule à coups de `new`/`delete` ni de NKMemory direct : *chaque*
handle, command buffer, swapchain et sémaphore se détruit par sa méthode `DestroyXxx` du device. Et ce
n'est **pas** thread-hostile : les `CreateXxx`/`DestroyXxx` sont protégés par un mutex interne, `Submit`
est sérialisé par queue, `Map`/`Unmap` sont sûrs — on peut créer des ressources depuis plusieurs threads.

> **En résumé.** `NkIDevice` est l'interface abstraite unique du GPU : un couple `CreateXxx`/`DestroyXxx`
> par ressource, des handles opaques, beaucoup de méthodes à impl par défaut (les backends simples
> n'overrident que l'essentiel). On détruit **tout** via le device, jamais `delete`. Création/destruction
> thread-safe.

---

## Présenter à l'écran : `NkISwapchain`

Le swapchain est la chaîne d'images dans lesquelles on dessine et qu'on **présente** tour à tour à la
fenêtre. En mono-fenêtre, on n'a rien à faire : le device gère un swapchain interne (les getters
`GetSwapchainWidth/Height/Format` sont là, marqués *deprecated* mais conservés). L'interface
`NkISwapchain` n'apparaît que pour le cas **multi-fenêtre** : on en crée *autant qu'on veut* via
`NkIDevice::CreateSwapchain`, chacun lié à sa surface, pour rendre dans plusieurs fenêtres ou viewports.

La boucle est canonique : `AcquireNextImage` (récupérer l'image libre suivante) → rendre dedans →
`Present` (l'afficher), encadrée par `Initialize` et `Shutdown`. Comme le device, un swapchain se détruit
par `NkIDevice::DestroySwapchain`, qui remet le pointeur à `nullptr`.

> **En résumé.** Mono-fenêtre : le device suffit (swapchain interne). Multi-fenêtre : un `NkISwapchain`
> par fenêtre, créé/détruit via le device, piloté par la boucle `AcquireNextImage → render → Present`.

---

## Aperçu de l'API

Tous les éléments publics, par header. La « Référence complète » qui suit détaille chaque catégorie avec
ses usages.

### Handles, formats et énumérations — `NkTypes.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Handle | `NkRhiHandle<Tag>` (`IsValid`, `==`/`!=`, `static Null()`) | Poignée opaque 64-bit ; `id==0` = invalide |
| Tags | `NkTagBuffer/Texture/Sampler/Shader/Pipeline/RenderPass/Framebuffer/Fence/Semaphore/DescSet/BindlessHeap` | Étiquettes de typage (structs vides) |
| Alias handle | `NkBufferHandle`, `NkTextureHandle`, `NkSamplerHandle`, `NkShaderHandle`, `NkPipelineHandle`, `NkRenderPassHandle`, `NkFramebufferHandle`, `NkFenceHandle`, `NkDescSetHandle`, `NkSemaphoreHandle`, `NkBindlessHeapHandle` | Handles typés par ressource |
| Format | `enum NkGPUFormat` (8/16/32-bit, packés, depth/stencil, compressés, `NK_UNDEFINED`, `NK_COUNT`) | Format de pixel/attribut |
| Format (fn) | `NkFormatIsDepth`, `NkFormatHasStencil`, `NkFormatIsSrgb`, `NkFormatBytesPerPixel` | Interrogations sur un format |
| Vertex/Index | `NkVertexFormat` (= `NkGPUFormat`), `NkVertexFormatSize`, `enum NkIndexFormat {NK_UINT16, NK_UINT32}` | Format de sommets et d'indices |
| Usage/Bind | `enum NkResourceUsage`, `enum NkBindFlags` (+ `\|` `&` `~`, `NkHasFlag`) | Heap CPU/GPU et liaisons d'une ressource |
| Buffer/Texture | `enum NkBufferType`, `enum NkTextureType`, `enum NkSampleCount` | Nature d'un buffer/texture, MSAA |
| Shader | `NkShaderStage` (= `NkSLStage`, de NKSL), `NkPushConstantRange` | Étage shader et plage de push-constants |
| État fixe | `enum NkPrimitiveTopology`, `NkFillMode`, `NkCullMode`, `NkFrontFace`, `NkCompareOp`, `NkStencilOp`, `NkBlendFactor`, `NkBlendOp` | États du pipeline graphique fixe |
| Sampler | `enum NkFilter`, `NkMipFilter`, `NkAddressMode`, `NkBorderColor` | Filtrage et bordures d'échantillonnage |
| Render pass | `enum NkLoadOp`, `NkStoreOp` | Charge/sauvegarde des attachements |
| Barrière/queue | `enum NkPipelineStage` (+ `\|`), `NkResourceState`, `NkQueueType` | Étages, états et queues GPU |
| Viewport | `NkViewport` (`flipY`), `NkRect2D`/`NkScissor`, `NkClearColor`, `NkClearDepth`, `NkClearValue` | Zone de rendu, ciseaux, valeurs de clear |
| Résultat | `enum NkRHIResult`, `NkSucceeded`, `NkRHIResultName` | Code de retour et helpers |

### Nom d'API — `NkGraphicsApi.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum (ext.) | `NkGraphicsApi` (= `graphics::NkGraphicsApi`, de NKPlatform) | API graphique cible |
| Fonction | `NkGraphicsApiName(api)` | Chaîne lisible ("OpenGL", "Vulkan", "DirectX 12"…) |

### Infos runtime — `NkContextInfo.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Struct | `NkContextInfo` (`api`, `renderer`, `vendor`, `version`, `vramMB`, `debugMode`, `computeSupported`, `maxTextureSize`, `maxMSAASamples`, `windowWidth/Height`) | Remontée d'infos du device actif |

### Descripteur de création — `NkContextDesc.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Politique GPU | `enum NkGpuPreference`, `NkGpuVendor` | Préférence perf/conso et vendeur |
| Swapchain | `enum NkSwapchainFormat`, `NkSwapchainFormatIsSrgb` | Format de présentation cross-API |
| Sélection | `NkGpuSelectionDesc` | Choix d'adaptateur (index, vendeur, software…) |
| Par backend | `NkVulkanDesc`, `NkDirectX11Desc`, `NkDirectX12Desc`, `NkMetalDesc`, `NkSoftwareDesc`, `NkComputeActivationDesc` | Configuration spécifique à chaque API |
| OpenGL | `enum NkPFDFlags`/`NkPFDPixelType`, `NkWGLFallbackPixelFormat`, `enum NkGLProfile`/`NkGLContextFlags`/`NkGLSwapInterval`, `NkGLXHints`, `NkEGLHints`, `NkOpenGLRuntimeOptions`, `NkOpenGLDesc` | Détails de contexte GL/WGL/GLX/EGL |
| Principal | `NkContextDesc` (`api`, sous-descs, `swapchainFormat`, `IsComputeEnabledForApi`, fabriques `MakeXxx`) | Le descripteur global de création |

### Politique de sélection GPU — `NkGpuPolicy.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Classe util. | `NkGpuPolicy` (`final`, statics) | Utilitaire de sélection d'adaptateur |
| Méthodes | `ApplyPreContext`, `MatchesVendorPciId`, `PreferenceName`, `VendorName` | Hints pré-création, filtre PCI, chaînes |

### Bloc d'initialisation — `NkDeviceInitInfo.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Callbacks | `NkGLGetProcAddressFn`, `NkPresentCallback`, `NkResizeCallback` | Chargement GL, présentation, redimensionnement |
| Struct | `NkDeviceInitInfo` (`api`, `context`, `surface`, `width/height`, `minimized`, callbacks) | Bloc complet de création |
| Fonctions | `NkDeviceInitApi`, `NkDeviceInitWidth`, `NkDeviceInitHeight`, `NkDeviceInitComputeEnabledForApi` | Lecture cohérente des champs |

### Fabrique de device — `NkDeviceFactory.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Création | `Create`, `CreateForApi`, `CreateWithFallback`, `CreateAutoDetect` | Créer un device (fixe / explicite / fallback / auto) |
| Capacités | `GetSupportedApis`, `IsApiSupported` | API compilées dans le binaire (compile-time) |
| Destruction | `Destroy(device&)` | Détruire (met le pointeur à `nullptr`) |

### Interface device — `NkIDevice.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Auxiliaires | `NkDeviceCaps`, `NkMappedMemory`, `NkFrameContext`, `NkIDevice::FrameStats` | Capacités, mémoire mappée, contexte de frame, stats |
| Cycle de vie | `Initialize`, `Shutdown`, `IsValid`, `GetApi`, `IsSwapchainSrgb`, `GetCaps`, `GetContextInfo` | Init/arrêt et interrogation du device |
| Buffers | `CreateBuffer`/`DestroyBuffer`, `WriteBuffer`, `WriteBufferAsync`, `ReadBuffer`, `MapBuffer`/`UnmapBuffer` | Cycle et upload/readback/mapping des buffers |
| Textures | `CreateTexture`/`DestroyTexture`, `WriteTexture`, `WriteTextureRegion`, `GenerateMipmaps` | Cycle et upload des textures, mipmaps |
| Samplers | `CreateSampler`/`DestroySampler` | Cycle des échantillonneurs |
| Shaders | `CreateShader`/`DestroyShader` | Cycle des shaders |
| Pipelines | `CreateGraphicsPipeline`, `CreateComputePipeline`, `DestroyPipeline`, `SavePipelineCache`/`LoadPipelineCache` | Cycle des pipelines, cache disque |
| Passes/FBO | `CreateRenderPass`/`DestroyRenderPass`, `CreateFramebuffer`/`DestroyFramebuffer`, `GetFramebufferRenderPass`, getters swapchain interne (deprecated) | Render passes et framebuffers |
| Descriptors | `CreateDescriptorSetLayout`/`Destroy…`, `AllocateDescriptorSet`/`FreeDescriptorSet`, `UpdateDescriptorSets`, `BindUniformBuffer`, `BindTextureSampler` | Liaison des ressources aux shaders |
| Command buffers | `CreateCommandBuffer`, `DestroyCommandBuffer` | Création/destruction des CB |
| Soumission | `Submit`, `SubmitAndPresent`, `SubmitOnQueue`, `SubmitGraphics`, `HasDedicatedComputeQueue` | Envoi des CB au GPU |
| Synchro | `CreateFence`/`DestroyFence`, `WaitFence`, `IsFenceSignaled`, `ResetFence`, `WaitIdle`, `CreateGpuSemaphore`/`DestroySemaphore`/`DestroyGpuSemaphore` | Fences CPU↔GPU et sémaphores GPU↔GPU |
| Frame | `BeginFrame`/`EndFrame`, `GetFrameIndex`, `GetMaxFramesInFlight`, `GetFrameNumber` | Boucle de frame |
| Resize | `OnResize` | Redimensionnement du swapchain |
| Queries | `BeginTimestampQuery`, `EndTimestampQuery`, `GetTimestampResults`, `GetTimestampPeriodNs` | Timing GPU (no-op par défaut) |
| Natif | `GetNativeDevice`, `GetNativeCommandQueue`, `GetNativePhysicalDevice` | Accès aux objets natifs |
| Stats/debug | `GetLastFrameStats`, `ResetFrameStats`, `SetDebugName` (×3) | Profilage et noms de debug |
| Swapchain | `CreateSwapchain`, `DestroySwapchain` | Swapchains multi-fenêtre |
| Bindless | `CreateBindlessHeap`/`DestroyBindlessHeap`, `WriteBindlessTexture`/`WriteBindlessBuffer`, `BindBindlessHeap` | Tables de descripteurs sans liaison |

### Swapchain multi-fenêtre — `NkISwapchain.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Struct | `NkSwapchainDesc` (`width/height`, `vsync`, `imageCount`, `colorFormat`, `depthFormat`, `hdr`, `debugName`) | Description d'un swapchain |
| Cycle de vie | `Initialize`, `Shutdown`, `IsValid` | Init/arrêt |
| Boucle | `AcquireNextImage`, `Present(waits)`, `Present()`, `Resize` | Acquérir/présenter/redimensionner |
| Getters | `GetCurrentFramebuffer/RenderPass`, `GetColorFormat/DepthFormat`, `GetWidth/Height`, `GetCurrentImageIndex`, `GetImageCount`, `SupportsHDR`, `SupportsTearing` | État courant du swapchain |

---

## Référence complète

Chaque élément est repris ici à fond, avec ses usages dans les différents domaines d'un moteur — rendu,
ECS, physique, animation, gameplay/IA, audio, UI/2D, IO, compute GPU, threading, outils/éditeur.

### Les handles opaques et leurs tags

`NkRhiHandle<Tag>` est une poignée 64-bit, paramétrée par une étiquette de type (`NkTagBuffer`,
`NkTagTexture`…) pour qu'un `NkBufferHandle` ne puisse pas être confondu avec un `NkTextureHandle` à la
compilation. Le contrat est minimal : un `id` entier, `0` signifie *invalide*, `IsValid()` le teste,
`Null()` le fabrique, et `==`/`!=` comparent. Le handle ne porte **aucune** notion de propriété : c'est
le device qui crée et détruit la ressource sous-jacente ; le handle n'est qu'un ticket.

- **Rendu** — on garde des `NkPipelineHandle`/`NkTextureHandle` dans le matériau, sans exposer l'objet GPU réel.
- **ECS** — un composant *MeshRenderer* stocke des handles (vbo, ibo, pipeline) : copiables, comparables, triviaux.
- **Outils/éditeur** — sérialiser un handle n'a pas de sens (il est volatile) ; on resérialise la *description*, pas le handle.
- **Threading** — un handle est une valeur triviale : on le passe entre threads sans verrou (c'est la ressource qui est protégée, pas le ticket).

### `NkGPUFormat` et ses fonctions

L'énumération couvre toute la gamme : entiers/flottants 8/16/32-bit, formats packés vertex
(`NK_A2B10G10R10_UNORM`, `NK_R11G11B10_FLOAT`), depth/stencil (`NK_D24_UNORM_S8_UINT`…), et blocs
compressés (BC1/3/5/7, ETC2, ASTC). `NK_UNDEFINED = 0` est l'absence de format ; `NK_COUNT` la sentinelle.
Quatre helpers l'interrogent : `NkFormatIsDepth` (les quatre D*), `NkFormatHasStencil` (les deux avec _S8),
`NkFormatIsSrgb` (les SRGB), et `NkFormatBytesPerPixel` (octets par pixel).

- **Rendu** — choisir le format d'une render target (HDR `NK_RGBA16_FLOAT`, G-buffer `NK_RGBA8_UNORM`), d'un depth buffer (`NK_D32_FLOAT`).
- **Textures/IO** — un loader d'image mappe son format disque vers un `NkGPUFormat`, choisit BC7 pour les albédos, BC5 pour les normales.
- **Calcul de taille** — `NkFormatBytesPerPixel` aide à dimensionner un upload — **mais piège** : son switch est *partiel*, il renvoie `0` pour les formats non listés (packés, compressés, la plupart des depth). Ne vous y fiez pas pour les formats exotiques.
- **Piège sRGB** — `NkFormatIsSrgb` classe `NK_ETC2_RGB_UNORM` comme sRGB (approximation) : à savoir si vous raisonnez gamma sur mobile.

### Formats de vertex et d'index

Le RHI ne préjuge pas de la structure d'un sommet : `NkVertexFormat` est juste un alias de
`NkGPUFormat`, et `NkVertexFormatSize` délègue à `NkFormatBytesPerPixel` (même limite). `NkIndexFormat`
choisit entre `NK_UINT16` (jusqu'à 65 536 sommets, deux fois moins de bande passante) et `NK_UINT32`
(maillages volumineux).

- **Rendu** — déclarer la disposition d'un vertex (position `NK_RGB32_FLOAT`, UV `NK_RG32_FLOAT`, couleur `NK_RGBA8_UNORM`).
- **2D/UI** — les vertices d'UI tiennent en `NK_UINT16` ; un terrain massif passe en `NK_UINT32`.

### `NkResourceUsage`, `NkBindFlags`, `NkBufferType`, `NkTextureType`, `NkSampleCount`

Ces énumérations décrivent **où vit** une ressource et **comment** le GPU l'utilise. `NkResourceUsage`
(`NK_DEFAULT`, `NK_UPLOAD`, `NK_READBACK`, `NK_IMMUTABLE`) place la mémoire côté GPU rapide, ou
CPU-visible pour écrire/lire. `NkBindFlags` est un **bitmask** (combinable par `|`, testable par
`NkHasFlag`) : un buffer peut être à la fois `NK_VERTEX_BUFFER | NK_TRANSFER_DST`. `NkBufferType` et
`NkTextureType` donnent la nature (vertex/index/uniform/storage ; 1D/2D/3D/cube/array), et
`NkSampleCount` le MSAA (la valeur de l'enum **est** le nombre d'échantillons : `NK_S4 = 4`).

- **Rendu** — un vertex buffer statique en `NK_IMMUTABLE` ; une render target combine `NK_RENDER_TARGET | NK_SHADER_RESOURCE` pour être dessinée puis ré-échantillonnée.
- **Compute GPU** — un storage buffer `NK_STORAGE_BUFFER | NK_UNORDERED_ACCESS` pour un compute shader (particules, simulation).
- **Animation** — un buffer de matrices d'os en `NK_UPLOAD` (réécrit chaque frame).
- **IO/readback** — capturer un screenshot via `NK_READBACK` puis `ReadBuffer`.
- **Anti-aliasing** — un framebuffer `NkSampleCount::NK_S4` pour du MSAA 4×.

### `NkShaderStage` et `NkPushConstantRange`

`NkShaderStage` n'est **pas** défini ici : c'est un alias de `NkSLStage`, possédé par le module **NKSL**.
Ses opérateurs (`|`, `&`, `~`) et `NkSLStageToString` viennent donc de NKSL. La valeur référencée par le
RHI est `NK_ALL_GRAPHICS` (tous les étages graphiques). `NkPushConstantRange` décrit une petite plage de
données poussée directement dans le pipeline (`stages`, `offset`, `size`) — le moyen le plus rapide
d'envoyer quelques octets (une matrice MVP, un index) sans passer par un uniform buffer.

- **Rendu** — pousser la matrice modèle d'un objet en push-constant : pas d'allocation, mise à jour quasi gratuite par draw.
- **2D/UI** — pousser l'offset/échelle d'un quad d'interface.

### États du pipeline fixe : topologie, rasterizer, profondeur, blending

Une grappe d'énumérations décrit la partie **non programmable** du pipeline. `NkPrimitiveTopology`
(triangles list/strip/fan, lignes, points, patches) dit comment assembler les sommets. `NkFillMode`
(plein/fil de fer/point), `NkCullMode` (aucun/face avant/arrière) et `NkFrontFace` (CCW/CW) règlent la
rasterisation. `NkCompareOp` (les huit comparaisons, de `NK_NEVER` à `NK_ALWAYS`) sert au test de
profondeur *et* au stencil ; `NkStencilOp` (keep/zero/replace/incr/decr/invert…) dit quoi faire du
stencil. Enfin `NkBlendFactor` et `NkBlendOp` (`NK_ADD`, `NK_SUB`, `NK_MIN`/`NK_MAX`…) composent la
couleur finale avec ce qui est déjà à l'écran.

- **Rendu** — `NK_CULL_BACK` + `NK_FRONT_FACE_CCW` pour la géométrie solide ; `NK_FILL_WIREFRAME` pour un mode debug ; `NK_COMPARE_LESS` pour le depth test classique.
- **2D/UI** — blending alpha standard : `srcAlpha`/`oneMinusSrcAlpha` + `NK_BLEND_OP_ADD`.
- **Effets** — blending additif (`NK_ONE`/`NK_ONE`, `NK_ADD`) pour les particules lumineuses, le feu, les *glow*.
- **Outils/éditeur** — le stencil isole une sélection (écrire un masque puis n'éclairer que `NK_COMPARE_EQUAL`) pour un contour de surbrillance.
- **Ombres** — un *shadow pass* désactive le cull ou inverse `NkFrontFace` pour réduire l'acné d'auto-ombrage.

### Échantillonnage : `NkFilter`, `NkMipFilter`, `NkAddressMode`, `NkBorderColor`

Comment lire une texture. `NkFilter` (nearest/linear/cubic) règle le filtrage entre texels ;
`NkMipFilter` (none/nearest/linear) le passage entre niveaux de mip (le *trilinear* = linear+linear).
`NkAddressMode` (repeat/mirror/clamp-to-edge/clamp-to-border/mirror-once) décide ce qui se passe **hors
[0,1]**, et `NkBorderColor` la couleur du bord en mode clamp-to-border.

- **Rendu** — linéaire + mipmaps trilinéaire pour les textures de surface ; `clamp-to-edge` pour éviter le suintement aux bords d'un atlas.
- **2D/pixel-art** — `NK_NEAREST` pour garder des pixels nets.
- **Ombres** — `clamp-to-border` + bordure blanche sur une shadow map pour que l'extérieur soit toujours « éclairé ».
- **Terrain/sol** — `NK_REPEAT` pour carreler une texture sur une grande surface.

### Render pass : `NkLoadOp`, `NkStoreOp`

Au début d'une passe, que fait-on des attachements ? `NkLoadOp` : `NK_LOAD` (garder le contenu),
`NK_CLEAR` (effacer), `NK_DONT_CARE` (peu importe, le plus rapide). À la fin, `NkStoreOp` : `NK_STORE`
(conserver le résultat), `NK_DONT_CARE` (jeter), `NK_RESOLVE` (résoudre un MSAA vers une cible simple).
Ces choix sont décisifs en **bande passante**, surtout sur GPU à tuiles (mobile).

- **Rendu** — la passe principale fait `CLEAR` sur la couleur + le depth, puis `STORE` la couleur.
- **Depth pre-pass** — `STORE` le depth (réutilisé), mais `DONT_CARE` la couleur.
- **MSAA** — `STORE` sur la cible résolue via `NK_RESOLVE`, `DONT_CARE` sur l'image multisamplée intermédiaire.

### Barrières et queues : `NkPipelineStage`, `NkResourceState`, `NkQueueType`

Le GPU travaille en parallèle ; il faut parfois lui dire « attends que ceci soit fini avant cela ».
`NkPipelineStage` est un bitmask d'étages (du `TOP_OF_PIPE` au `BOTTOM_OF_PIPE`, en passant par
vertex/fragment/compute/transfer) — seul `|` est défini ici. `NkResourceState` décrit l'état logique
d'une ressource (`NK_RENDER_TARGET`, `NK_SHADER_READ`, `NK_PRESENT`, `NK_TRANSFER_DST`…), ce qui pilote
les transitions de layout DX12/Vulkan. `NkQueueType` enfin nomme les files matérielles : graphics,
compute, transfer, present.

- **Rendu** — transiter une texture de `NK_RENDER_TARGET` (on l'a dessinée) vers `NK_SHADER_READ` (on va l'échantillonner) au bon étage.
- **Compute GPU** — soumettre une simulation sur la queue `NK_COMPUTE` en parallèle du graphics, synchroniser par sémaphore.
- **IO/upload** — un thread de streaming pousse les textures sur la queue `NK_TRANSFER` sans bloquer le rendu.

### Viewport, scissor et valeurs de clear

`NkViewport` définit la zone écran cible (`x`, `y`, `width`, `height`, `minDepth`, `maxDepth`) avec un
champ singulier : `flipY` (défaut `true`) inverse l'axe Y pour Vulkan, afin de retrouver la convention
OpenGL — on le met à `false` pour les passes d'ombre. `NkRect2D` (alias de `math::NkIntRect`) sert de
`NkScissor` (recadrage). Pour le clear : `NkClearColor` (alias `math::NkColorF`), `NkClearDepth`
(`depth` + `stencil`), et l'union `NkClearValue` qui regroupe les deux et s'initialise par défaut en noir
opaque.

- **Rendu** — un viewport par split-screen ; un scissor pour limiter le rendu à une région sale.
- **UI/2D** — le scissor implémente le *clipping* d'un panneau (ne dessiner que l'intérieur d'une fenêtre d'interface).
- **Ombres** — `flipY = false` sur la passe d'ombre (convention shadow map cohérente cross-API).

### `NkRHIResult` et ses helpers

Le code de retour des opérations RHI : `NK_OK`, puis les erreurs (`NK_OUT_OF_MEMORY`, `NK_DEVICE_LOST`,
`NK_INVALID_PARAM`, `NK_NOT_SUPPORTED`, `NK_ALREADY_EXISTS`, `NK_TIMEOUT`, `NK_UNKNOWN`). `NkSucceeded(r)`
teste l'absence d'erreur, `NkRHIResultName(r)` donne une chaîne — **piège** : la table de noms est
incomplète, `NK_ALREADY_EXISTS`/`NK_TIMEOUT`/`NK_UNKNOWN` tombent tous sur `"Unknown"`.

- **Robustesse** — détecter un `NK_DEVICE_LOST` (driver crashé, GPU retiré) pour tenter une recréation propre.
- **Outils** — logger `NkRHIResultName` pour diagnostiquer un échec de création.

### `NkGraphicsApi` et `NkGraphicsApiName`

`NkGraphicsApi` (alias de `graphics::NkGraphicsApi`, défini canoniquement dans
`NKPlatform/NkCGXDetect.h`) énumère toutes les API : `NK_GFX_API_NONE`, OpenGL/OpenGLES, Vulkan,
DX11/DX12, Metal, WebGL/WebGL2/WebGPU, Software, GNM, NVN, plus `NK_GFX_API_MAX` et `NK_GFX_API_AUTO`.
`NkGraphicsApiName` en donne une chaîne lisible ("OpenGL", "Vulkan", "DirectX 12"…). **Piège** : il
existe *aussi* `NkGraphicsApiToString` (côté NKEvent) au libellé légèrement différent ("D3D11", "D3D12",
"OpenGLES") — distinct de `NkGraphicsApiName`, ne pas confondre.

- **Outils/éditeur** — afficher l'API active dans la barre de titre ou un panneau de diagnostic.
- **Tests** — itérer sur `GetSupportedApis()` pour valider une démo sur chaque backend.

### `NkContextInfo`

Structure de **remontée** d'infos, produite par `NkIDevice::GetContextInfo()` : l'API active, les
chaînes `renderer`/`vendor`/`version` du driver, la VRAM (`vramMB`), `debugMode`, `computeSupported`, la
taille de texture max, le MSAA max, et les dimensions courantes de la fenêtre.

- **Outils/éditeur** — un panneau « À propos GPU » qui affiche la carte, le driver, la VRAM.
- **Robustesse** — vérifier `maxTextureSize` avant de créer une texture géante ; `computeSupported` avant de lancer un compute shader.

### Politique GPU : `NkGpuPreference`, `NkGpuVendor`, `NkGpuSelectionDesc`, `NkGpuPolicy`

Sur les machines à deux GPU (intégré + dédié), il faut **choisir**. `NkGpuPreference` exprime l'intention
(`NK_DEFAULT`, `NK_LOW_POWER` pour économiser la batterie, `NK_HIGH_PERFORMANCE` pour le dédié) et
`NkGpuVendor` un constructeur précis (NVIDIA, AMD, Intel, ARM, Qualcomm, Apple, Microsoft). On combine
tout cela dans `NkGpuSelectionDesc` : `preference`, `adapterIndex` (`-1` = auto), `vendorPreference`,
`allowSoftwareAdapter`, `enableOpenGLPlatformHints`. La classe utilitaire `NkGpuPolicy` (purement
statique, `final`) applique ces choix : `ApplyPreContext(desc)` pose des hints process-level **avant** la
création du contexte (variables d'environnement de sélection GPU), `MatchesVendorPciId(id, vendor)` filtre
un adaptateur par son PCI vendor ID (helper DXGI/Vulkan), et `PreferenceName`/`VendorName` donnent des
chaînes lisibles.

- **Gameplay/perf** — forcer `NK_HIGH_PERFORMANCE` pour viser le GPU dédié sur un portable.
- **Mobile/batterie** — `NK_LOW_POWER` pour rester sur l'iGPU dans un menu statique.
- **Outils** — lister et choisir l'adaptateur dans les options du moteur.

### Format de swapchain : `NkSwapchainFormat`

Le format de présentation, **unifié cross-API** : `NK_SWAPCHAIN_BGRA8_UNORM` (défaut, pas de
ré-encodage gamma), `NK_SWAPCHAIN_BGRA8_SRGB` (encode gamma à la présentation), les variantes RGBA8,
et deux formats HDR GPU-only (`NK_SWAPCHAIN_RGB10A2_UNORM` pour HDR10,
`NK_SWAPCHAIN_RGBA16F` pour scRGB float16). `NkSwapchainFormatIsSrgb` teste les deux SRGB. **Câblage
réel** : seuls BGRA8_UNORM/SRGB sont pleinement câblés (VK+GL ; DX en UNORM) ; les autres retombent en
fallback BGRA8_UNORM.

- **Rendu** — UNORM si l'on fait son propre tonemapping (gamma à la main dans le shader), SRGB si l'on laisse le swapchain encoder.
- **HDR** — viser RGBA16F sur un écran HDR (sous réserve de support réel).

### Descripteurs par backend

Chacun configure une API, avec des défauts sains. **`NkVulkanDesc`** : noms d'app/moteur, versions,
`validationLayers=false` (opt-in — l'activer peut charger un mauvais msvcp140 et crasher),
`debugMessenger=false`, `vsync=true`, `srgbSwapchain=true`, `swapchainImages=3`, `msaaSamples=1`,
`enableComputeQueue=true`, extensions instance/device optionnelles. **`NkDirectX11Desc`** : `debugDevice`,
`vsync`, `allowTearing`, MSAA, `swapchainBuffers=2`, `minFeatureLevel=0` (=11_0). **`NkDirectX12Desc`** :
`debugDevice`, `gpuValidation`, `allowTearing=true`, `swapchainBuffers=3`, tailles de heaps
(`rtvHeapSize=1024`, `dsvHeapSize=256`, `srvHeapSize=65536` — large car l'allocateur ne libère pas,
`samplerHeapSize=256`), `enableComputeQueue=true`. **`NkMetalDesc`** : `validation`, `vsync`,
`sampleCount`, `srgb`. **`NkSoftwareDesc`** : `threading`, `threadCount=0` (0 = hardware_concurrency),
`useSSE`, `pixelFormat=0` (RGBA8). **`NkComputeActivationDesc`** : un `enable` global plus un drapeau par
backend pour activer la queue compute API par API.

- **Robustesse** — laisser `validationLayers=false` en production, l'activer en debug via `MakeVulkan(true)`.
- **Perf DX12** — ajuster `srvHeapSize` si l'on crée énormément de textures (pas de libération).
- **Software** — `threadCount=0` pour exploiter tous les cœurs en rastérisation logicielle.

### OpenGL : descripteurs et énumérations de contexte

OpenGL demande une configuration de contexte fine, très spécifique à la plateforme. `NkOpenGLDesc`
centralise tout : version (`majorVersion=4`, `minorVersion=6`), `profile` (`NkGLProfile` :
core/compatibility/ES), `contextFlags` (`NkGLContextFlags` : debug, forward-compat, robust-access,
no-error, combinables par `|`, testables par `HasFlag`), bits de couleur/depth/stencil, MSAA,
`srgbFramebuffer`, `doubleBuffer`, et `swapInterval` (`NkGLSwapInterval` : immediate/vsync/adaptive).
S'y greffent des sous-structures par plateforme : `NkWGLFallbackPixelFormat` (Windows, avec ses
`NkPFDFlags`/`NkPFDPixelType` et ses fabriques `Minimal()`/`Standard()`/`HighPrecision()`/`SingleBuffer()`),
`NkGLXHints` (Linux/X11), `NkEGLHints` (Android/Web/EGL), et `NkOpenGLRuntimeOptions` (chargement des
entry points, validation de version, callback de debug). `NkOpenGLDesc` offre trois fabriques :
`Desktop46()`, `Desktop33()`, `ES32()`.

- **Rendu** — `Desktop46(true)` pour un contexte 4.6 core avec couche de debug pendant le dev.
- **Mobile/Web** — `ES32()` pour OpenGL ES 3.2 (Android) ou WebGL.
- **Portabilité** — le `wglFallback` garantit un format de pixel correct si le chemin moderne échoue sur Windows.

### `NkContextDesc` — le descripteur principal

Il agrège tout : l'`api` cible, un sous-descripteur par backend (`opengl`, `vulkan`, `dx11`, `dx12`,
`metal`, `software`), la `compute`, la `gpu`, et le `swapchainFormat` (la **source unique** du format de
présentation). `IsComputeEnabledForApi(api)` répond `false` si le compute global est désactivé, sinon
mappe l'API vers son drapeau par-backend. Les fabriques `MakeOpenGL`, `MakeOpenGLES`, `MakeVulkan` (le
booléen active validation+debugMessenger), `MakeDirectX11`, `MakeDirectX12`, `MakeMetal`, `MakeSoftware`
produisent un descripteur prêt à l'emploi. **Note importante** : `swapchainFormat` *remplace* les anciens
champs sRGB par-API (`vulkan.srgbSwapchain`, `opengl.srgbFramebuffer`, `metal.srgb`) qui restent dans les
structs mais **ne sont plus lus** — piège de redondance.

- **Boot** — `NkContextDesc::MakeVulkan()` puis ajustement de deux champs, et c'est parti.
- **Compute** — activer `compute.enable` et vérifier `IsComputeEnabledForApi` avant de créer un pipeline compute.

### `NkDeviceInitInfo` et ses fonctions de lecture

Le bloc complet de création : `api` (top-level), `context` (le `NkContextDesc`), `surface` (le
`NkSurfaceDesc` venant de NKWindow, qui porte le handle natif de fenêtre), `width`/`height`, `minimized`,
et deux callbacks — `presentCallback` (`NkPresentCallback`, appelé pour présenter) et `resizeCallback`
(`NkResizeCallback`, renvoyant un `bool` sur redimensionnement). L'alias `NkGLGetProcAddressFn` couvre le
chargement des fonctions GL. Quatre fonctions libres lisent ce bloc de façon **cohérente** :
`NkDeviceInitApi` (prend `init.api`, sinon `init.context.api` — **piège** : deux sources à aligner),
`NkDeviceInitWidth`/`Height` (prennent `width` si >0, sinon `surface.width/height`), et
`NkDeviceInitComputeEnabledForApi` (renvoie `true` si le compute n'est *pas* configuré — compat
ascendante — sinon délègue à `context.IsComputeEnabledForApi`).

- **Boot** — remplir `init`, brancher `presentCallback` sur le swap de la fenêtre, passer à la fabrique.
- **Resize** — le `resizeCallback` est l'occasion de recréer les ressources dépendantes de la taille.

### `NkDeviceFactory` — la fabrique

Détaillée dans le tutoriel ci-dessus : `Create`/`CreateForApi`/`CreateWithFallback`/`CreateAutoDetect`
créent, `GetSupportedApis`/`IsApiSupported` interrogent les **compétences compile-time**, `Destroy`
détruit en remettant le pointeur à `nullptr`. La règle d'or : compile-time (présent dans le binaire) ≠
runtime (marche sur cette machine) — seul `CreateAutoDetect` tranche le second.

- **Portabilité** — `CreateAutoDetect` pour un binaire unique qui s'adapte à chaque PC.
- **Tests/CI** — `CreateForApi` pour forcer un backend précis et valider un rendu identique partout.

### Structs auxiliaires du device : `NkDeviceCaps`, `NkMappedMemory`, `NkFrameContext`, `FrameStats`

`NkDeviceCaps` est la fiche technique du GPU : des limites (tailles de textures, attachements couleur,
plages de buffers, attributs de vertex, tailles de groupes compute, alignements, anisotropie, VRAM) et
une nuée de booléens de fonctionnalités (tessellation, geometry/mesh shaders, draw indirect, bindless,
ray tracing, VRS, compression BC/ETC2/ASTC, queries…), la plupart `false` par défaut sauf
`nonPowerOfTwoTextures` et `mipGenInShader`. `NkMappedMemory` décrit une zone mappée (`ptr`, `size`,
`rowPitch`, `depthPitch`, `IsValid`). `NkFrameContext` porte l'état d'une frame en vol (`frameIndex`,
`frameNumber`, `frameFence`). `NkIDevice::FrameStats` agrège le profilage (draw/dispatch calls, triangles,
vertices, changements de pipeline/descriptor, mémoire GPU, temps GPU/CPU).

- **Robustesse** — interroger `GetCaps()` avant d'utiliser une feature optionnelle (ne pas lancer du ray tracing si `rayTracing == false`).
- **Outils/profilage** — afficher `FrameStats` dans un overlay (draw calls, triangles, temps GPU).

### Cycle de vie du device

`Initialize(init)` crée toutes les ressources internes du backend, `Shutdown()` les libère, `IsValid()`
indique l'état, `GetApi()` renvoie l'API réelle. `GetCaps()` donne la fiche technique, `GetContextInfo()`
la remontée pour l'UI. `IsSwapchainSrgb()` (défaut `false`) dit si le swapchain encode déjà le gamma —
auquel cas le tonemapping doit utiliser gamma 1.0 pour éviter le double encodage.

- **Boot/shutdown** — encadrer toute la session entre `Initialize` et `Shutdown`.
- **Rendu** — adapter le tonemapping au retour de `IsSwapchainSrgb` (correction gamma une seule fois).

### Buffers : création, upload, mapping

`CreateBuffer`/`DestroyBuffer` gèrent le cycle. Pour pousser des données, trois voies : `WriteBuffer`
(upload **synchrone**, bloque jusqu'à la fin), `WriteBufferAsync` (via staging, **non-bloquant**, pour
les gros transferts), `ReadBuffer` (readback synchrone). `MapBuffer`/`UnmapBuffer` exposent directement
la mémoire CPU-visible d'un buffer `NK_UPLOAD`/`NK_READBACK` (renvoie un `NkMappedMemory`).

- **Rendu** — créer un vertex/index buffer une fois, le remplir au boot.
- **Animation** — `MapBuffer` un buffer de matrices d'os, écrire les transformations de la frame, `UnmapBuffer`.
- **Compute GPU** — `WriteBufferAsync` un gros tampon de simulation sans figer le thread principal.
- **IO/readback** — `ReadBuffer` pour récupérer un résultat de compute ou un screenshot côté CPU.

### Textures : création, upload, mipmaps

`CreateTexture`/`DestroyTexture` pour le cycle. `WriteTexture` écrit le mip 0 / layer 0 ;
`WriteTextureRegion` cible une sous-région précise (offset x/y/z, dimensions, `mipLevel`, `arrayLayer`,
`rowPitch`) ; `GenerateMipmaps(tex, filter)` reconstruit la chaîne de mips (synchrone).

- **Rendu** — uploader une albédo, générer ses mips, l'échantillonner.
- **Streaming/IO** — `WriteTextureRegion` pour remplir progressivement un atlas ou un tableau de textures sans tout réécrire.
- **UI/2D** — mettre à jour une sous-zone d'un atlas de glyphes quand un nouveau caractère est rastérisé.

### Samplers, shaders, pipelines

`CreateSampler`/`DestroySampler` matérialisent une configuration d'échantillonnage. `CreateShader`/
`DestroyShader` compilent un étage. `CreateGraphicsPipeline` et `CreateComputePipeline` figent l'état
complet (shaders + état fixe + layouts) en un objet GPU, détruit par `DestroyPipeline`.
`SavePipelineCache`/`LoadPipelineCache` (no-op par défaut) persistent le cache de compilation sur disque
pour accélérer les démarrages suivants.

- **Rendu** — un pipeline par combinaison shader/état ; un sampler partagé entre plusieurs textures.
- **Compute GPU** — `CreateComputePipeline` pour une passe de simulation ou de post-traitement.
- **Boot** — `LoadPipelineCache` au lancement pour éviter de recompiler tous les shaders.

### Render passes et framebuffers

`CreateRenderPass`/`DestroyRenderPass` décrivent la structure d'une passe (attachements + load/store).
`CreateFramebuffer`/`DestroyFramebuffer` lient des textures concrètes à cette structure.
`GetFramebufferRenderPass` retrouve le render pass implicite d'un framebuffer (utile en Vulkan ; `{}` en
GL/Software). Les getters de swapchain interne (`GetSwapchainFramebuffer/RenderPass/Format/DepthFormat/
Width/Height`) sont **deprecated** mais conservés pour la compat mono-fenêtre.

- **Rendu** — un render pass pour le G-buffer, un autre pour la passe d'éclairage, un dernier pour le post-process.
- **Ombres** — un framebuffer dédié à la shadow map (depth only).

### Descriptor sets et helpers de binding

C'est ainsi qu'on **branche** les ressources sur les shaders. `CreateDescriptorSetLayout` décrit la forme
attendue (quels bindings, quels types), `AllocateDescriptorSet(layout)` en alloue une instance,
`UpdateDescriptorSets(writes, count)` la remplit, `FreeDescriptorSet` la rend. Deux **helpers concrets**
(non virtuels) simplifient les cas fréquents : `BindUniformBuffer(set, binding, buf, range)` et
`BindTextureSampler(set, binding, tex, samp)` construisent le `NkDescriptorWrite` et appellent
`UpdateDescriptorSets` pour vous.

- **Rendu** — un set « par-frame » (matrices caméra), un set « par-matériau » (textures), un set « par-objet ».
- **2D/UI** — `BindTextureSampler` pour lier l'atlas de glyphes au shader de texte.

### Command buffers

`CreateCommandBuffer(type)` alloue un tampon d'enregistrement de commandes (graphics par défaut, ou
compute/transfer). C'est un **pointeur brut** dont le device garde l'ownership : on le détruit par
`DestroyCommandBuffer(cb&)`, jamais `delete`. On y enregistre les draws/dispatches avant de le soumettre.

- **Rendu** — un command buffer par frame, ou plusieurs enregistrés en parallèle sur des threads différents.
- **Threading** — la création de CB est thread-safe : chaque thread de rendu enregistre le sien.

### Soumission au GPU

`Submit(cbs, count, signalFence)` envoie un lot de command buffers, signalant optionnellement une fence à
la fin. `SubmitAndPresent(cb)` est le raccourci du cas courant (soumettre puis présenter). Pour le
multi-queue : `SubmitOnQueue(queue, info)` cible une file précise (défaut : ignore les sémaphores et
retombe sur la queue principale), `SubmitGraphics(cmd, waitSem, waitStage, signalSem, fence)` est un
helper concret qui bâtit le `NkSubmitInfo` (sémaphores conditionnés par `IsValid()`) et soumet sur
graphics, et `HasDedicatedComputeQueue()` indique si une queue compute séparée existe.

- **Rendu** — `SubmitAndPresent` en mono-queue, le cas de 99 % des frames.
- **Compute GPU** — `SubmitOnQueue(NK_COMPUTE, …)` pour exécuter une simulation en parallèle, synchronisée par sémaphore.

### Synchronisation : fences et sémaphores

Les **fences** synchronisent CPU↔GPU : `CreateFence(signaled)`/`DestroyFence`, `WaitFence(fence,
timeoutNs)` (attente CPU), `IsFenceSignaled`, `ResetFence`. Les **sémaphores** synchronisent GPU↔GPU :
`CreateGpuSemaphore`/`DestroySemaphore` (et `DestroyGpuSemaphore`, qui remet `id=0`). `WaitIdle()` fait un
flush GPU complet — efficace mais **à utiliser avec parcimonie** (il sérialise tout). (Détail
d'implémentation : le header `#undef` la macro Win32 `CreateGpuSemaphore` pour éviter une collision.)

- **Frame pacing** — une fence par frame en vol pour ne pas écraser des ressources encore en cours d'usage GPU.
- **Multi-queue** — un sémaphore pour que la passe de rendu attende la fin de la simulation compute.
- **Shutdown** — `WaitIdle()` avant de tout détruire, pour garantir qu'aucune commande n'est en vol.

### Gestion de frame

`BeginFrame(frame)`/`EndFrame(frame)` encadrent une frame et remplissent le `NkFrameContext`.
`GetFrameIndex()` donne l'index dans le cycle de frames en vol, `GetMaxFramesInFlight()` la profondeur du
pipeline (double/triple buffering), `GetFrameNumber()` le compteur monotone total.

- **Rendu** — toute la boucle de dessin vit entre `BeginFrame` et `EndFrame`.
- **Ressources par-frame** — utiliser `GetFrameIndex()` pour choisir le bon jeu de ressources cyclées.

### `OnResize`

`OnResize(width, height)` recrée le swapchain et ses dépendances. **Piège connu et documenté** : Windows
émet un événement de resize *à la création même* de la fenêtre — un handler naïf déclenche alors un
`OnResize` inutile **avant** la première frame, ce qui sous DX12 réinitialise une command list non
consommée (crash) et sous Software réinitialise la DIB. Toujours **tracker la taille courante** et ne
déclencher `OnResize` que sur un **changement réel**.

- **Fenêtrage** — réagir au redimensionnement de la fenêtre, au plein écran, au changement de DPI.
- **Robustesse** — garde de taille pour éviter le faux resize de démarrage.

### Queries GPU, accès natif, stats et debug

Les **queries timestamp** (`BeginTimestampQuery`, `EndTimestampQuery`, `GetTimestampResults`,
`GetTimestampPeriodNs`) mesurent le temps GPU — no-op par défaut, à implémenter par backend. L'**accès
natif** (`GetNativeDevice`, `GetNativeCommandQueue`, `GetNativePhysicalDevice`) expose les objets bruts du
backend pour interopérer (ImGui, capture RenderDoc, extensions). Les **stats** (`GetLastFrameStats`,
`ResetFrameStats`) et les **noms de debug** (trois surcharges `SetDebugName` sur buffer/texture/pipeline,
no-op par défaut) servent au profilage et au diagnostic dans les outils GPU.

- **Profilage** — encadrer une passe de queries timestamp pour mesurer son coût GPU.
- **Outils/éditeur** — `SetDebugName` pour que RenderDoc/PIX affichent des noms lisibles ; `GetNativeDevice` pour brancher un overlay tiers.

### Bindless

Les méthodes bindless (`CreateBindlessHeap`/`DestroyBindlessHeap`, `WriteBindlessTexture`/
`WriteBindlessBuffer`, `BindBindlessHeap`) implémentent le modèle « table géante de descripteurs »
indexée dynamiquement par le shader. Non supporté par défaut (impl par défaut vides). `BindBindlessHeap`
se fait une fois par frame et remplace tous les descriptor sets classiques.

- **Rendu** — un seul heap pour toutes les textures de la scène, indexé par matériau (réduit drastiquement les changements d'état).
- **GPU-driven** — la base d'un pipeline *GPU-driven* où le GPU choisit lui-même quelles ressources lire.

### Swapchain multi-fenêtre : `NkSwapchainDesc` et `NkISwapchain`

`NkSwapchainDesc` décrit un swapchain : `width`/`height`, `vsync`, `imageCount` (double/triple buffering),
`colorFormat` (défaut `NK_RGBA8_SRGB`), `depthFormat` (défaut `NK_D32_FLOAT`), `hdr`, `debugName`.
L'interface `NkISwapchain` (créée par `NkIDevice::CreateSwapchain`, détruite par `DestroySwapchain`)
pilote la présentation multi-fenêtre : `Initialize(ctx, desc)`/`Shutdown`/`IsValid`,
`AcquireNextImage(signalSem, fence, timeout)` (récupère l'image suivante), `Present(waits, count)` et la
surcharge `Present()` (affiche), `Resize(w, h)`. Les getters donnent l'état courant
(`GetCurrentFramebuffer`/`RenderPass`, `GetColorFormat`/`DepthFormat`, `GetWidth`/`Height`,
`GetCurrentImageIndex`, `GetImageCount`) et les capacités (`SupportsHDR`, `SupportsTearing`, défaut
`false`). L'idiome : `Initialize → boucle { AcquireNextImage; render; Present } → Shutdown`.

- **Outils/éditeur** — une fenêtre principale + plusieurs viewports détachés, chacun son swapchain.
- **Multi-écran** — un swapchain par moniteur en configuration multi-affichage.
- **2D/UI** — une fenêtre de prévisualisation séparée présentée indépendamment.

### Idiomes et pièges transversaux

- **Ownership unique** — tout (device, handle, command buffer, swapchain, sémaphore) se détruit par sa méthode `DestroyXxx` ; **jamais** `delete` ni NKMemory direct.
- **Compile-time vs runtime** — `IsApiSupported`/`GetSupportedApis` = présent dans le binaire ; `CreateAutoDetect` = marche réellement.
- **Source unique du format** — `swapchainFormat` (les champs sRGB par-API sont morts mais présents).
- **Deux sources d'API** — garder `init.api` et `init.context.api` cohérents.
- **Resize de démarrage** — tracker la taille, ne déclencher `OnResize` que sur changement réel.
- **Réutilisation du contexte natif** — le RHI partage le device natif du `NkIGraphicsContext` (couche 0), il n'en recrée pas.
- **Thread safety** — `CreateXxx`/`DestroyXxx`/`CreateCommandBuffer` protégés par mutex, `Submit` sérialisé par queue, `Map`/`Unmap` sûrs.

---

### Exemple

```cpp
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
using namespace nkentseu;

// 1) Décrire ce qu'on veut, puis laisser la fabrique choisir le meilleur backend.
NkDeviceInitInfo init;
init.context = NkContextDesc::MakeVulkan();      // défauts sains (validation OFF)
init.width   = 1280;  init.height = 720;
init.surface = window.GetSurfaceDesc();          // handle natif de fenêtre (NKWindow)
init.presentCallback = [&]{ window.SwapBuffers(); };

NkIDevice* device = NkDeviceFactory::CreateAutoDetect(init);   // réécrit init.api
if (!device) return -1;                                        // aucune API ne marche

// 2) Créer des ressources (handles opaques, détruits par le device).
NkBufferHandle vbo = device->CreateBuffer(vboDesc);
device->WriteBuffer(vbo, vertices, vboSize);     // upload synchrone

// 3) La boucle de frame.
NkFrameContext frame;
while (running) {
    if (device->BeginFrame(frame)) {
        NkICommandBuffer* cmd = device->CreateCommandBuffer();
        // … BeginRenderPass, SetViewport, BindPipeline, Draw, EndRenderPass …
        device->SubmitAndPresent(cmd);
        device->DestroyCommandBuffer(cmd);
        device->EndFrame(frame);
    }
}

// 4) Arrêt propre : on attend le GPU, on détruit tout via le device, puis la fabrique.
device->WaitIdle();
device->DestroyBuffer(vbo);
NkDeviceFactory::Destroy(device);                // met le pointeur à nullptr
```

---

[← Index NKRHI](README.md) · [Récap NKRHI](../NKRHI.md) · [Couche Runtime](../README.md)
