# NKCanvas — Roadmap

> **Renommé NKContext → NKCanvas le 2026-05-28.** Objectif : couche graphique
> conviviale **SFML-like** (simple d'usage, ou plus avancée que SFML mais
> accessible), par opposition à NKRHI (bas niveau) et NKRenderer (3D avancé).
> Les symboles `NkContext*` / `NkIGraphicsContext` sont conservés (concept
> « contexte graphique GPU » légitime — ce ne sont pas des canvas). Le kind
> jenga est désormais `canvas` (macro `NKENTSEU_CANVAS`).

État actuel (mai 2026) : Abstraction GPU multi-backend (OpenGL/GLES/WebGL,
Vulkan, DX11, DX12, Metal, Software) + chemin compute parallèle +
Renderer2D backend-agnostic (SFML-like) + ressources GPU (NkTexture, NkSprite,
NkText) + fontes via wrapper du module NKFont. Répartition : Backend/, Compute/,
Core/, Factory/, Renderer/.

## Évolutions 2026-05-28

- **Renommage NKContext → NKCanvas** (module, dossier, jenga, includes, kind).
- **renderer::NkFont** réécrit en **wrapper GPU du module externe NKFont**
  (FreeType supprimé) : une page (atlas module rasterisé + NkTexture GPU) par
  taille de caractère. C'est le `sf::Font` de NKCanvas ; le moteur de
  rasterisation est le module NKFont.
- **renderer::NkImage supprimé** : NkTexture consomme désormais le module
  externe **NKImage** (`nkentseu::NkImage`) directement.
- **Dossier STB supprimé** (~2 Mo, 24 headers) : NKCanvas n'utilise plus
  stb_image/stb_truetype — tout passe par NKImage et NKFont maison.

## Évolutions 2026-05-30 — Refonte SFML-like (8/10 étapes livrées)

Couche **complète style SFML** posée par-dessus l'infrastructure existante.
Build NKCanvas vert à chaque étape. Voir aussi [USAGE.md](USAGE.md) pour les
exemples d'utilisation.

### A.1 — Types fondamentaux (livré)
- [`NkRenderer2DTypes.h`](src/NKCanvas/Renderer/Core/NkRenderer2DTypes.h) :
  ajout `NkPrimitiveType` (POINTS, LINES, LINE_STRIP, TRIANGLES,
  TRIANGLE_STRIP, TRIANGLE_FAN) + alias `NkVertex = NkVertex2D`.
- [`NkTransform.h`](src/NKCanvas/Renderer/Core/NkTransform.h) : matrice 2D
  affine wrappant `math::NkMat4f` (interop NKRenderer 3D/NKCamera) avec
  ops 2D spécialisées (Combine 12 mul, Rotate cols 0/1, TransformPoint/Rect).
  `GetMatrix() → const float32*` pour upload GPU direct, `GetMatrix4() →
  const NkMat4f&` pour passer aux systèmes 3D.
- [`NkTransformable.h`](src/NKCanvas/Renderer/Core/NkTransformable.h) : base
  SFML-like position/rotation/scale/origin avec cache de transform (dirty
  flag, recalcul lazy).

### A.2 — VertexArray + RenderStates (livré)
- [`NkVertexArray.h`](src/NKCanvas/Renderer/Core/NkVertexArray.h) : conteneur
  `NkVector<NkVertex>` + `NkPrimitiveType`, GetBounds, Append/Resize/Clear.
- [`NkRenderStates.h`](src/NKCanvas/Renderer/Core/NkRenderStates.h) :
  POD {transform, blendMode, texture, shader} + ctors mince + `Default()`.

### A.3 — NkDrawable (livré)
- [`NkDrawable.h`](src/NKCanvas/Renderer/Core/NkDrawable.h) : nouvelle
  interface `Draw(NkRenderTarget&, const NkRenderStates&)`. NkIDrawable2D
  legacy reste en place (dual heritage temporaire).

### A.4 — Shapes (livré) — SFML-like
- [`NkShape.h/cpp`](src/NKCanvas/Renderer/Shapes/NkShape.h) : base
  abstraite NkTransformable + NkDrawable. Fill via TRIANGLE_FAN (n vertices),
  outline via LINE_STRIP (n+1 vertices, ferme la boucle).
- [`NkRectangleShape.h`](src/NKCanvas/Renderer/Shapes/NkRectangleShape.h)
- [`NkCircleShape.h`](src/NKCanvas/Renderer/Shapes/NkCircleShape.h)
- [`NkConvexShape.h`](src/NKCanvas/Renderer/Shapes/NkConvexShape.h)
- [`NkLineShape.h`](src/NKCanvas/Renderer/Shapes/NkLineShape.h)
- Support `LINE_STRIP`, `TRIANGLE_STRIP`, `TRIANGLE_FAN` ajouté dans
  `NkRenderWindow::Draw(raw vertices)` (expansion vers TRIANGLES list).

### A.5 — RenderTarget + RenderWindow + RenderTexture (livré)
- [`NkRenderTarget.h/cpp`](src/NKCanvas/Renderer/Targets/NkRenderTarget.h) :
  abstract base, Draw(drawable)/Draw(vertexArray)/Draw(raw)/Draw(NkIDrawable2D
  compat), Clear/Display/View/Viewport/MapPixelCoords.
- [`NkRenderWindow.h/cpp`](src/NKCanvas/Renderer/Targets/NkRenderWindow.h) :
  concret. Wrappe NkWindow + NkIGraphicsContext (factory) + NkIRenderer2D
  (factory). Frame management (Begin/End auto). **`OnResize()` et
  `OnDpiChange()`** pour la recréation swapchain cross-API.
- [`NkRenderTexture.h`](src/NKCanvas/Renderer/Targets/NkRenderTexture.h) :
  STUB (API posée, `Create()` retourne false). Impl réelle à venir.

### A.6 — NkRenderer2D facade concrete (livré)
- [`NkRenderer2D.h`](src/NKCanvas/Renderer/Core/NkRenderer2D.h) : facade
  user-facing wrappant NkIRenderer2D. Aucune virtualisation supplémentaire,
  forwarding inline. Accessible via `target.GetRenderer2D()`.

### A.7 — NkTextureSetBackend wiring 5 backends (livré)
**Bug critique listé depuis 2026-05-26 enfin résolu.** Chaque backend
appelle `NkTextureSetBackend()` à la fin de son Initialize avec ses 5
callbacks (Create/Update/Destroy/SetFilter/SetWrap).
- [`NkTextureBackend.h`](src/NKCanvas/Renderer/Resources/NkTextureBackend.h)
  exposé publiquement (était privé dans NkTexture.cpp).
- **OpenGL** : helpers statiques déjà présents, juste câblage en fin d'Init.
- **Software** : callbacks no-op + ID monotone (le rasterizer sample
  directement via NkTexture::GetCPUPixels).
- **Vulkan** : registry globale (gVkTexRegistry) capturant device/queue/cmd
  pool ; chaque entry = VkImage + VkImageView + VkDeviceMemory ; upload via
  staging buffer + transition layouts. SetFilter/Wrap = no-op (sampler
  immutable dans descriptor set layout — TODO pour sampler-per-texture).
- **DX11** : registry globale (gDX11Registry), chaque entry = ID3D11Texture2D
  + SRV + sampler dédié. SetFilter/Wrap rebuildent le sampler. Binding
  par-batch dans SubmitBatches.
- **DX12** : registry globale (gDX12Registry), upload synchrone via fence
  dédiée. Default heap + upload heap, transitions COPY_DEST↔PIXEL_SHADER_RESOURCE.
  SetFilter/Wrap = no-op (root signature avec sampler unique — TODO sampler
  heap multi-slots).

### A.8 — Migration NkSprite/NkText vers NkDrawable (livré)
- Dual heritage : `class NkSprite : public NkIDrawable2D, public NkDrawable`.
  Idem NkText.
- Ancien `Draw(NkIRenderer2D&)` conservé pour compat backends.
- Nouveau `Draw(NkRenderTarget&, NkRenderStates)` ajouté ; delegate pour
  l'instant à l'ancien path (composition `states.transform` sera intégrée à
  la refonte Pong A.9 quand un besoin concret se présente).

### Matrice backend × plateforme (état actuel)

| Backend | Win clang-mingw | Linux X11/XCB/Wayland | macOS | Android | HarmonyOS | iOS | Web | Xbox/UWP |
|---------|---|---|---|---|---|---|---|---|
| Software | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| OpenGL/GLES | ✅ | ✅ | ✅ | ✅ ES3 | ✅ ES3 | – | ✅ WebGL | – |
| Vulkan | ✅ (VULKAN_SDK) | ✅ (VULKAN_SDK) | – (MoltenVK possible) | ✅ (si dispo) | ✅ (si dispo) | – | – | – |
| DX11 | ✅ linké (2026-05-30) | – | – | – | – | – | – | – |
| DX12 | ✅ linké (2026-05-30) | – | – | – | – | – | – | ✅ |
| Metal Renderer2D | ❌ pas implémenté | – | ⚠️ context seul | – | – | ⚠️ context seul | – | – |

DX11/DX12 désormais linkés par défaut sur Windows desktop clang-mingw : libs `libd3d11.a`,
`libd3d12.a`, `libdxgi.a`, `libdxguid.a` sont dans `C:/msys64/ucrt64/lib/` (msys2 ucrt64
toolchain). Les .exe consommateurs de NKCanvas n'ont donc plus besoin de re-déclarer ces
libs côté app.

## Extensions 2026-05-30 — NkShader / NkMaterial / NkRenderTexture / Pong refonte

Architecture **autonome de NKRHI** (pas de couplage NkSL/shaderc/spirv-cross). Trois nouvelles
dispatch tables backend, parallèles à `NkTextureBackend` :

- **`NkShader`** (`Renderer/Resources/NkShader.h` + `NkShaderBackend.h`) : shader programmable
  user-écrit. L'utilisateur fournit le source dans le langage du backend cible (GLSL / HLSL /
  MSL / SPIR-V). Uniforms par nom : `SetFloat/Vec2/Vec3/Vec4/Mat4/Color/Texture`. Permet
  notamment la **modification UV / couleurs dans le fragment shader** (scroll, ondulation,
  color grading, distortion, etc.).
- **`NkMaterial`** (`Renderer/Resources/NkMaterial.h`) : wrap d'un `NkShader` + uniforms
  préconfigurés + states GPU. Pour effets réutilisables : toon 2D, lumière 2D, feu (additive
  + noise UV), eau (sin wave UV), distortion, color grading, glow. `material.States()`
  applique tous les uniforms et retourne un `NkRenderStates` prêt pour `target.Draw(drawable, states)`.
- **`NkRenderTexture`** (`Renderer/Targets/NkRenderTexture.h` + `NkRenderTextureBackend.h`) :
  rendu offscreen vers texture (FBO OpenGL real), pour post-process / mini-map / blit UI.

### NkShape — UV modifiable + bugfix triangulation

- **`mTextureRect`** est maintenant câblé (était documenté mais ignoré). `SetTextureRect()`
  remappe les UV vers une sous-région de texture.
- Méthode virtuelle **`GetPointUV(index, bounds)`** : sous-classes peuvent override pour
  mappings custom (polaire, cylindrique, etc.).
- **Bug fix « rectangles creux »** : NkShape émet maintenant en **NK_TRIANGLES directs** au
  lieu de NK_TRIANGLE_FAN. Raison : la conversion FAN→TRIANGLES dans `NkRenderWindow::Draw`
  utilise un buffer scratch local au switch ; si le backend batch lazyement (NkBatchRenderer2D),
  le scratch est détruit avant flush → artefacts. Triangulation côté NkShape garantit la
  durée de vie du buffer pendant tout l'appel `target.Draw`.

### A.9 — Refonte Pong sur NKCanvas (livré, 2026-05-30)

`Applications/Pong/src/Pong/Apps.cpp` réécrit en démo NKCanvas SFML-like. **Sélection backend
dynamique** via fichier `pong.config` :

```
backend=opengl | vulkan | dx11 | dx12 | software | auto
```

- `auto` → `PongConfig::PickBestForPlatform()` + fallback chain (DX12→DX11→Vulkan→OpenGL→Software
  sur Windows, etc.).
- **`PongConfig`** utilise **`NkFile` (NKFileSystem)** pour la portabilité (Android AAssetManager
  reroute, Web virtual FS, Harmony resource pack).
- **Event pump correct** via `NkEvents().PollEvent()` → résout le freeze "Not Responding".
- **Input clavier** : W/S (paddle gauche), ↑/↓ (paddle droit), ESC (quit). Fallback IA auto si
  aucune touche pressée.
- **Resize + DPI change** → `target.OnResize()` / `target.OnDpiChange()` via events.

L'ancien Pong Ultra Arena (PongApp + AI + obstacles + particles + power-ups) reste sur disque
dans `Game/` / `Render/` mais hors-build le temps de la migration progressive.

### NK_GFX_API_AUTO ajouté à `NkGraphicsApi`

Nouvelle valeur dans l'enum `NkGraphicsApi` (`NKPlatform/NkCGXDetect.h`) pour exprimer
« sélection automatique selon plateforme et disponibilité backend ».

### Bugs runtime détectés 2026-05-30 (à debug sur hardware par le user)

Compile clean sur les 5 backends, mais le user a testé Pong runtime et observé :

| Backend | Symptôme runtime | Hypothèse |
|---|---|---|
| OpenGL | Rectangles « creux » (corrigé par triangulation directe NkShape côté A.9) | Buffer scratch lifetime — **FIXÉ** |
| Vulkan | Crash | Init device/swapchain à investiguer (logs validation layer) |
| DX11 | Affiche rien | Probable : sampler state ou viewport non initialisé, à inspecter |
| DX12 | Crash | Init PSO ou descriptor heap à investiguer |
| Software | Écran noir | Probable : framebuffer non blité en fenêtre, ou format pixel mismatch |
| Tous | Shutdown lent | Destructor `NkRenderWindow` attend probablement une opération GPU bloquante |

Ces bugs **nécessitent validation runtime sur hardware** (logs Vulkan validation, RenderDoc /
PIX pour DX, breakpoints debug) — non faisables depuis l'environnement de développement
distant qui n'a pas de GPU. À traiter en sessions dédiées avec accès au runtime.

## À venir (futur proche)

- **A.9 — Refonte Pong sur NKCanvas** : Pong utilise actuellement son propre
  GLContext/GLRenderer2D/Texture2D maison ; le migrer sur NKCanvas pour en
  faire la démo vitrine SFML-like. (Idem démos NkCameraDemos.) Résout aussi
  bugs Pong (NkSafeArea include obsolète, Texture2D.cpp ancien API NkImage).
- **A.10 — Doc/ROADMAP/mémoire finale**.
- **`NkRenderTexture` cross-API** : implémentation réelle (FBO OpenGL,
  VkFramebuffer Vulkan, RTV DX11/12, MTLTexture Metal, framebuffer offscreen
  Software). Posera l'infra pour post-process/mini-map/blit UI.
- **Outline épais** (`mOutlineThickness > 1`) : expansion en quads avec
  joints miter/bevel — actuellement DrawLine thickness=1 hardcodé.
- **Sampler per-texture sur DX12 et Vulkan** : descriptor heap sampler
  multi-slots OU sampler cache (filter+wrap → VkSampler).
- **Composition `states.transform`** : NkSprite/NkText/NkShape passent
  actuellement `states.transform` au backend via path legacy. Construire
  les vertices client-side avec le transform composé permettra de pleinement
  exploiter le pattern parent→enfants (utile pour widgets imbriqués).
- **Metal Renderer2D** : currently `Backend/Metal/` n'a que les
  ComputeContext + Context placeholder. Manque NkMetalRenderer2D complet
  (rendu sur CAMetalLayer + MTLRenderCommandEncoder).
- **`NkShader` cross-API (shaders custom user-écrits)** : actuellement
  `NkRenderStates::shader` est un forward-decl placeholder ; chaque backend
  utilise un shader hardcodé qui fait `texture(uTex, uv) * color`. Pour
  permettre à l'utilisateur de modifier les UV (scroll, distortion), les
  couleurs (color grading), ou faire du post-process custom, il faut :
  - Une classe `NkShader` qui charge du **NkSL** (langage shader Nkentseu
    existant côté NKRHI, à exposer ici) → transpilé en GLSL/HLSL/MSL via
    `NkVulkanShaderCompiler` (shaderc + spirv-cross, déjà dans NKRHI).
  - Une API uniformes : `SetFloat/Vec/Matrix/Texture(name, value)`.
  - Un binding shader-courant dans chaque backend Renderer2D (Vulkan/DX12
    impliquent un cache pipeline car ils sont objet-state-monolithique).
  - Intégration dans le batcher : flush quand `states.shader` change.
  Cible : `NkRenderStates st; st.shader = &monShaderDistorsion; target.Draw(va, st);`.
- **Intégration NKUI** : voir comment NKCanvas s'articule avec le module
  NKUI. Probable que NKUI rende ses widgets via NkRenderTarget/NkRenderer2D
  (DrawNinePatch, DrawText, scissor stack pour clipping). À planifier post-A.10.
- **Hot-reload shaders 2D**.
- **Tests cross-backend** : suite end-to-end qui itère sur les 5 backends
  et valide draws identiques (à pixel-diff près).

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| `NkIGraphicsContext` interface + `NkContextDesc`/`NkContextInfo` | Livré | — | — |
| `NkContextFactory` (Create + CreateWithFallback) | Livré | — | — |
| Backend OpenGL/GLES/WebGL (WGL + GLX + EGL + CGL) | Livré | — | — |
| Backend Vulkan (instance + device + swapchain + sync) | Livré | — | — |
| Backend DX11 (device + swapchain + RTV/DSV) | Livré | — | — |
| Backend DX12 (device + cmdqueue + descriptor heaps) | Livré | — | — |
| Backend Metal (MTLDevice + CAMetalLayer) | Partiel | M | Moyenne |
| Backend Software (GDI / XShm / SHM / Canvas) | Livré | — | — |
| `NkIComputeContext` interface + 6 backends compute | Livré | — | — |
| `NkSurfaceDesc` (consommé depuis NKWindow) | Livré | — | — |
| `NkSurfaceHints` (OpenGL/GLX pixel format prep) | Livré | — | — |
| `NkGpuPolicy` (sélection GPU NVIDIA/AMD, hint exports) | Livré | — | — |
| `NkIRenderer2D` API SFML-like + 5 backends | Livré | — | — |
| `NkBatchRenderer2D` (vertex batching) | Livré | — | — |
| Resources : `NkImage`, `NkTexture`, `NkSprite`, `NkFont` | Livré | — | — |
| `NkVulkanShaderCompiler` (GLSL→SPIR-V via shaderc/glslang) | Livré | — | — |
| Multi-contexte + contexte partagé (asset thread) | Livré | — | — |
| Recreate surface (Android background/foreground) | Livré | — | — |
| Callbacks swapchain (Clean/Recreate) | Livré | — | — |
| STB vendored (24 headers, image/font/textedit/...) | Livré | — | — |
| **`NkPrimitiveType` + `NkVertex` alias** (A.1, 2026-05-30) | ✅ Livré | — | — |
| **`NkTransform` (wraps `math::NkMat4f`)** (A.1) | ✅ Livré | — | — |
| **`NkTransformable` (base SFML-like)** (A.1) | ✅ Livré | — | — |
| **`NkVertexArray` + `NkRenderStates`** (A.2) | ✅ Livré | — | — |
| **`NkDrawable` (nouveau pattern target.Draw)** (A.3) | ✅ Livré | — | — |
| **`NkShape` + Rectangle/Circle/Convex/Line** (A.4) | ✅ Livré | — | — |
| **`NkRenderTarget` + `NkRenderWindow` concret** (A.5) | ✅ Livré | — | — |
| **`OnResize` + `OnDpiChange` cross-API swapchain recreate** (A.5) | ✅ Livré | — | — |
| **`NkRenderTexture` stub (API posée)** (A.5) | 🔶 Partiel | M | Moyenne |
| **`NkRenderer2D` facade concrete** (A.6) | ✅ Livré | — | — |
| **`NkTextureSetBackend` wiring 5 backends** (A.7) | ✅ Livré | — | — |
| **Migration `NkSprite`/`NkText` vers `NkDrawable`** (A.8) | ✅ Livré (dual) | — | — |
| `LINE_STRIP` / `TRIANGLE_STRIP` / `TRIANGLE_FAN` dans Draw(raw) (A.4) | ✅ Livré | — | — |
| **Refonte Pong sur NKCanvas (vitrine SFML)** (A.9) | ⏳ À venir | L | Haute |
| **`NkRenderTexture` impl cross-API réelle** (post-A.9) | ⏳ À venir | L | Moyenne |
| **Outline épais (joints miter/bevel)** | ⏳ À venir | M | Basse |
| **Sampler per-texture DX12 + Vulkan** | ⏳ À venir | M | Basse |
| **Composition `states.transform` Sprite/Text/Shape** | ⏳ À venir | M | Moyenne |
| **Intégration NKUI (widgets via NkRenderTarget)** | ⏳ À venir | L | Moyenne |
| Renderer2D Metal | ❌ TODO | L | Basse |
| Hot-reload shaders 2D | ❌ TODO | M | Moyenne |
| Tests cross-backend | ❌ TODO | L | Haute |

Légende : Livré, Partiel, En cours, TODO, Abandonné.

---

## Livré

### Phase A — Core interfaces

#### `NkIGraphicsContext` ([Core/NkIGraphicsContext.h](src/NKContext/Core/NkIGraphicsContext.h))
- Interface abstraite unifiée : `Initialize(window, desc)`, `Shutdown`,
  `IsValid`, `BeginFrame`/`EndFrame`/`Present`, `MakeCurrent`/`ReleaseCurrent`
  (multi-thread GL), `OnResize`, `SetVSync`/`GetVSync`, `GetApi`,
  `GetInfo`, `GetDesc`, `GetNativeContextData` (caster via
  `NkNativeContext::XXX()`), `SupportsCompute`.
- **Callbacks swapchain** (inspirés de l'ancien VulkanContext) :
  - `AddCleanUpCallback(NkSwapchainCleanFn)` — appelé AVANT destruction
    (détruire pipelines).
  - `AddRecreateCallback(NkSwapchainRecreateFn)` — appelé APRÈS recréation
    (reconstruire pipelines).
  - Retournent un `NkSwapchainCallbackHandle` pour `Remove*` ultérieur.
  - `NkFunction<void()>` zero-STL.

#### `NkContextDesc` ([Core/NkContextDesc.h](src/NKContext/Core/NkContextDesc.h))
- Descripteur unifié contenant les sous-descs par API :
  - `NkVulkanDesc` : appName, validationLayers, msaaSamples, swapchainImages,
    `preferredAdapterIndex`, `enableComputeQueue`, extra instance/device
    extensions.
  - `NkDirectX11Desc` : debugDevice, allowTearing, msaa, swapchainBuffers,
    minFeatureLevel.
  - `NkDirectX12Desc` : debugDevice, gpuValidation, RTV/DSV/SRV/sampler
    heap sizes, `enableComputeQueue`.
  - `NkMetalDesc` : validation, sampleCount, vsync.
  - `NkOpenGLDesc` ([NkOpenGLDesc.h](src/NKContext/Core/NkOpenGLDesc.h)) :
    version min/max, profile (Core/Compat/ES), debugContext, msaa,
    stereoscopic, sharedContext.
- `NkGpuSelectionDesc` : preference (LowPower/HighPerf), adapterIndex,
  vendorPreference (NVIDIA/AMD/Intel/ARM/Qualcomm/Apple/Microsoft),
  allowSoftwareAdapter.

#### `NkGpuPolicy` ([Core/NkGpuPolicy.cpp](src/NKContext/Core/NkGpuPolicy.cpp))
- `ApplyPreContext(desc)` exporte les hints système :
  - `NvOptimusEnablement = 0x01` (NVIDIA Optimus laptop dual-GPU).
  - `AmdPowerXpressRequestHighPerformance = 1` (AMD switchable graphics).
  - Choix DXGI adapter / VkPhysicalDevice avant création.

#### `NkContextInfo` ([Core/NkContextInfo.h](src/NKContext/Core/NkContextInfo.h))
- Renseignements post-init : vendor, renderer name, driver version,
  api version, max texture size, max anisotropy, supported extensions.

### Phase B — Backends graphics

#### Backend OpenGL/GLES/WebGL ([Backend/OpenGL/](src/NKContext/Backend/OpenGL/))
- 4 plateformes context creation :
  - **Win32 WGL** : `wglCreateContextAttribsARB` 3.3+ Core, debug context,
    `wglShareLists` pour shared contexts.
  - **Linux GLX (XLib + XCB)** : `glXChooseFBConfig` (via `NkSurfaceHints`),
    `glXCreateContextAttribsARB`. XCB utilise `XGetXCBConnection()`.
  - **EGL (Wayland + Android)** : `eglChooseConfig`, `eglCreateContext`,
    `eglCreateWindowSurface`.
  - **macOS CGL/NSOpenGLContext** ([NkOpenGLContextMacOS.mm](src/NKContext/Backend/OpenGL/NkOpenGLContextMacOS.mm))
    : NSOpenGLPixelFormat + NSOpenGLContext, legacy 4.1 max
    (Apple a déprécié GL).
  - **WebGL (Emscripten)** : `emscripten_webgl_create_context` WebGL2.
- **Loader optionnel** : compile avec GLAD2 si headers présents
  (`__has_include(<glad/...>)`), sinon `NK_NO_GLAD2` et le runtime peut
  utiliser un loader externe. Doc : *« NK_NO_GLAD2 recommandé pour
  NKContext »*.
- `RecreateSurface(window)` : recrée `eglSurface` sans détruire le
  context (Android background → foreground cycle).
- `CreateSharedContext(window)` : contexte fils partageant textures/buffers
  pour asset loader thread.
- Compute : `NkOpenGLComputeContext` via `glDispatchCompute` (GL 4.3+).

#### Backend Vulkan ([Backend/Vulkan/](src/NKContext/Backend/Vulkan/))
- `NkVulkanContext` : instance + debug messenger (validation layers
  optionnels) + physical device selection (preference adapter / vendor) +
  logical device avec compute queue dédiée si dispo.
- Swapchain : 2/3 images, format selection (BGRA8 SRGB par défaut),
  present mode (FIFO/MAILBOX/IMMEDIATE selon vsync).
- Framebuffers + render pass color+depth, command buffers per-frame
  (`mData.currentFrame`), `vkAcquireNextImageKHR` / `vkQueueSubmit` /
  `vkQueuePresentKHR`.
- Direct accessors typés : `GetDevice()`, `GetRenderPass()`,
  `GetCurrentCommandBuffer()`, `GetCurrentFramebuffer()`, `WaitIdle()`.
- Callbacks swapchain implémentés (`mCleanCallbacks`, `mRecreateCallbacks`).
- Compute : `NkVulkanComputeContext` queue COMPUTE_BIT séparée si dispo.

#### Backend DX11 ([Backend/DirectX/](src/NKContext/Backend/DirectX/))
- `NkDX11Context` : `D3D11CreateDeviceAndSwapChain` (legacy) puis
  IDXGIFactory2 + CreateSwapChainForHwnd (moderne).
- `D3D_FEATURE_LEVEL_11_0/11_1` selon hardware.
- RTV/DSV recréés sur resize.
- `HandleDeviceLost` : retry adapter sélection après TDR.
- `ID3D11Device1*` + `ID3D11DeviceContext1*` accessors typés.
- Compute : `NkDX11ComputeContext` via `CSSetShader` + UAV/SRV bindings.

#### Backend DX12 ([Backend/DirectX/](src/NKContext/Backend/DirectX/))
- `NkDX12Context` : `D3D12CreateDevice` + `ID3D12CommandQueue` direct +
  swap chain triple-buffer + descriptor heaps RTV/DSV/CBV-SRV-UAV/Sampler
  sizés selon `NkDirectX12Desc`.
- Fence + frameIndex tracking, `WaitForGpu()` au shutdown.
- AllowTearing pour 120/144Hz VRR.
- Compute : `NkDX12ComputeContext` queue COMPUTE séparée optionnelle.

#### Backend Metal ([Backend/Metal/](src/NKContext/Backend/Metal/))
- `NkMetalContext` Objective-C++ (.mm). `MTLCreateSystemDefaultDevice`
  + `CAMetalLayer` configuré depuis le `NSView`/`UIView` de la fenêtre.
- `id<MTLCommandQueue>` + drawable acquisition par frame.
- VSync via `CAMetalLayer.displaySyncEnabled`.
- Compute : `NkMetalComputeContext` via `MTLComputeCommandEncoder`.
- Partiel : pas de Renderer2D Metal (TODO), pas de tests sur macOS récent.

#### Backend Software ([Backend/Software/](src/NKContext/Backend/Software/))
- `NkSoftwareContext` : pixel buffer en RAM, présenté selon plateforme :
  - Windows : GDI DIBSection + `StretchDIBits`.
  - Linux XLib : XShm + `XPutImage`.
  - Linux XCB : `xcb_image` + `xcb_put_image`.
  - Linux Wayland : SHM buffer mmappé partagé via `wl_shm_pool`.
  - Android : `ANativeWindow_lock`.
  - Emscripten : `<canvas>` 2D context via `emscripten_set_canvas_size` +
    `ImageData`.
- `OnResize` : `mCachedSurface` stocke la NkSurfaceDesc pour réinit après
  resize.
- `NkSWPixel.h` : layout natif (BGRA sur Windows GDI, RGBA ailleurs) +
  SIMD SSE2/SSSE3/NEON pour conversion/blits rapides.
- Compute : `NkSoftwareComputeContext` fallback CPU pour les shaders
  compute (sert d'oracle pour tests cross-backend).

### Phase C — `NkContextFactory` ([Factory/NkContextFactory.h](src/NKContext/Factory/NkContextFactory.h))
- `Create(window, desc)` switch sur `desc.api` → backend correspondant.
- `IsApiSupported(api)` délègue à `NkGraphicsApiIsAvailable` (compile-time
  par plateforme).
- `CreateWithFallback(window, prefOrder[], count)` : essaie les APIs dans
  l'ordre, log d'erreur clair sur chaque échec. Ordre conseillé :
  {DX12, DX11, Metal, Vulkan, OpenGL, Software}.
- `CreateCompute(api, desc)` : compute standalone sans surface
  (GPGPU pur, ML, simulation).
- `ComputeFromGraphics(gfx)` : compute partageant device+queue avec un
  contexte graphique existant (requiert `desc.compute.enable`).
- Pré-init via `NkGpuPolicy::ApplyPreContext` (NVIDIA Optimus / AMD switch).

### Phase D — Multi-contexte
- Plusieurs contextes par application : entièrement supporté (rendu
  principal + chargement + preview).
- Plusieurs contextes par fenêtre :
  - OpenGL : `wglCreateContextAttribsARB(..., hShareContext)` →
    `NkOpenGLContext::CreateSharedContext`.
  - Vulkan : une surface par fenêtre, plusieurs command buffers OK.
  - DX11/12 : un swap chain par HWND, plusieurs device contexts OK.
  - Metal : un CAMetalLayer par view, plusieurs command queues OK.

### Phase E — `NkIComputeContext` ([Compute/NkIComputeContext.h](src/NKContext/Compute/NkIComputeContext.h))
- Interface unifiée GPGPU avec handles opaques `NkComputeBuffer`,
  `NkComputeShader`, `NkComputePipeline`.
- Méthodes : CreateBuffer/Destroy/Write/Read, CreateShaderFromSource/File,
  CreatePipeline, BindBuffer (UAV/SSBO/UAV), Dispatch(x,y,z),
  Barrier (memory + execution).
- Shader sources :
  - OpenGL : GLSL `#version 430 compute`.
  - Vulkan : SPIR-V (chemin `.spv` ou GLSL compilé via NkVulkanShaderCompiler).
  - DX11/12 : HLSL CS_5_0.
  - Metal : MSL kernel.
  - Software : pipeline CPU-only.
- Atomics, cpuReadable/cpuWritable flags, initialData support.
- Utilisé par NKRenderer (NkML, NkAnimationSystem morph, voxel AO,
  auto-exposure V1 future).

### Phase F — Renderer2D

#### `NkIRenderer2D` ([Renderer/Core/NkIRenderer2D.h](src/NKContext/Renderer/Core/NkIRenderer2D.h))
- API style **SFML** : `Begin/End/Flush`, `Clear(color)`,
  `SetView/Viewport/BlendMode`, `Draw(sprite|text|drawable)`,
  primitives `DrawPoint/Line/Rect/FilledRect/Circle/Triangle/...`,
  `DrawVertices` batch custom.
- Stats : `GetStats` (drawCalls, triangles, vertices, batches).
- Conversion coords : `MapPixelToCoords`, `MapCoordsToPixel`.
- `NkIDrawable2D` interface : tout objet implémentant `Draw(renderer)`.

#### `NkBatchRenderer2D` ([Renderer/Batch/NkBatchRenderer2D.h](src/NKContext/Renderer/Batch/NkBatchRenderer2D.h))
- Classe de base partagée entre tous les backends 2D.
- Accumule vertices + indices en CPU, groupe par (texture, blendMode).
- `kMaxVertices = 65536`, `kMaxIndices ~98304`.
- Flush auto sur state change ou capacité atteinte.
- Backends étendent `UploadBatch/SubmitBatch/BeginBatch/EndBatch`.

#### Backends Renderer2D (5/6 implémentés)
- `NkSoftwareRenderer2D` : rasterizer pur CPU (scanline + bresenham +
  bilinear sampling).
- `NkOpenGLRenderer2D` : VBO+VAO+IBO, MVP UBO, sampler2D textured/colored.
- `NkVulkanRenderer2D` : descriptor sets dyn, pipeline cache, SPIR-V
  embedded ([NkRenderer2DVkSpv.inl](src/NKContext/Backend/Vulkan/NkRenderer2DVkSpv.inl)).
- `NkDX11Renderer2D` : InputLayout + VS/PS shaders runtime.
- `NkDX12Renderer2D` : root signature + bindless texture array.
- Metal : header `NkMetalRenderer2D` absent (TODO ci-dessous).

#### `NkRenderer2DFactory` ([Renderer/Core/NkRenderer2DFactory.h](src/NKContext/Renderer/Core/NkRenderer2DFactory.h))
- `Create(graphicsContext)` retourne l'implémentation correspondant à
  l'API du contexte. Renvoie `NkRenderer2DPtr` (NkUniquePtr).

#### `NkVulkanShaderCompiler` ([Backend/Vulkan/NkVulkanShaderCompiler.h](src/NKContext/Backend/Vulkan/NkVulkanShaderCompiler.h))
- Compilation runtime GLSL → SPIR-V via :
  1. `shaderc` (Vulkan SDK officielle) si `NK_VK2D_USE_SHADERC`.
  2. `glslang` C++ API en fallback.
  3. Aucun compilateur → retourne `{}` et erreur claire (shaders
     précompilés requis).
- Permet hot-edit des shaders 2D pendant le dev.

### Phase G — Resources (CPU + GPU wrappers)

#### `NkImage` ([Renderer/Resources/NkImage.h](src/NKContext/Renderer/Resources/NkImage.h))
- Pixel buffer CPU style `sf::Image`. RGBA8 uniquement.
- Load : `LoadFromFile` (PNG/JPG/BMP/TGA/GIF/PSD via `stb_image`),
  `LoadFromMemory`, `LoadFromPixels`.
- Save : `SaveToFile` PNG (via `stb_image_write`).
- Ops : `GetPixel/SetPixel`, `FlipVertically` (Y-up pour OpenGL),
  `Copy(source, destX, destY, srcRect, applyAlpha)`,
  `CreateMaskFromColor(color, alpha)`.

#### `NkTexture` ([Renderer/Resources/NkTexture.h](src/NKContext/Renderer/Resources/NkTexture.h))
- Wrapper GPU non-copiable, movable.
- Create blank dims, `LoadFromFile` (NkImage → upload GPU),
  `LoadFromImage`, `LoadFromPixels`.
- `NkTextureFilter` (NEAREST/LINEAR), `NkTextureWrap`
  (CLAMP/REPEAT/MIRROR_REPEAT).
- `SetSmooth(bool)`, `SetRepeated(bool)`, `Update(image)`.

#### `NkSprite` ([Renderer/Resources/NkSprite.h](src/NKContext/Renderer/Resources/NkSprite.h))
- Transform 2D (position, rotation, scale, origin) + texture + textureRect
  + color tint.
- Drawable : implémente `NkIDrawable2D::Draw(renderer)`.

#### `NkFont` ([Renderer/Resources/NkFont.h](src/NKContext/Renderer/Resources/NkFont.h))
- TTF/OTF via **FreeType 2** (et non stb_truetype malgré qu'il soit
  disponible — choix pour subpixel rendering + hinting).
- Atlas LRU dynamique par taille de point.
- Glyphs : textureRect, bounds (bearing + size), advance.
- `NkTextStyle` : REGULAR / BOLD / ITALIC / UNDERLINED / STRIKE_THROUGH.

### Phase H — STB vendored ([STB/](src/NKContext/STB/))
24 headers public domain checkés dans le repo :
- `stb_image.h` (2.30 — JPG/PNG/TGA/BMP/PSD/GIF/HDR/PIC).
- `stb_image_write.h` (1.16 — PNG/TGA/BMP).
- `stb_image_resize2.h` (2.16 — qualité supérieure).
- `stb_truetype.h` (1.26 — fallback / preview rapide).
- `stb_rect_pack.h` (atlas packing).
- `stb_sprintf.h` (formatage standalone).
- `stb_dxt.h` (compression BC1-3, utile pour NKRenderer Phase H).
- `stb_ds.h` (typesafe containers — pas utilisé directement, NK* préférés).
- `stb_textedit.h` (édition texte, utilisé par NKUI).
- `stb_perlin.h`, `stb_voxel_render.h`, `stb_hexwave.h`,
  `stb_easy_font.h`, `stb_include.h`, `stb_c_lexer.h`, `stb_divide.h`,
  `stb_leakcheck.h`, `stb_connected_components.h`, `stb_vorbis.c`
  (utilisé par NKAudio), `stb_herringbone_wang_tile.h`,
  `stb_tilemap_editor.h`.
- LICENSE + SECURITY.md + README.md (auto-générés, ne pas modifier à
  la main).

### Phase I — Surface / Native interop

#### `NkSurfaceDesc` ([Core/NkSurfaceDesc.h](src/NKContext/Core/NkSurfaceDesc.h))
- Re-export du `NkSurfaceDesc` de NKWindow pour découplage. Consommé via
  `window.GetSurfaceDesc()`.

#### `NkNativeContextAccess` ([Core/NkNativeContextAccess.h](src/NKContext/Core/NkNativeContextAccess.h))
- Helpers de cast `void* GetNativeContextData()` → types natifs :
  `NkNativeContext::VkInstance(ctx)`, `NkNativeContext::VkDevice(ctx)`,
  `NkNativeContext::D3D11Device(ctx)`, `NkNativeContext::HGLRC(ctx)`,
  etc. Permet à NKRHI / NKRenderer d'utiliser le device sans inclure
  les headers natifs côté `NkIGraphicsContext`.

#### `NkWGLPixelFormat` ([Core/NkWGLPixelFormat.h](src/NKContext/Core/NkWGLPixelFormat.h))
- Helpers internes Win32 : `ChoosePixelFormatARB` + DescribePixelFormat
  pour OpenGL Core context creation.

---

## En cours / TODO immédiat

### Renderer2D Metal
- `NkMetalRenderer2D` à écrire :
  - MTLBuffer dyn pour vertices + indices.
  - MTLRenderPipelineState par blend mode.
  - Shaders MSL embedded ou compilés runtime via `MTLCompileOptions`.
  - Estimé : 1-2 jours (suivre pattern Vulkan/DX12).

### Hot-reload shaders 2D
- Système actuel utilise les SPIR-V embedded (`NkRenderer2DVkSpv.inl`).
  Pour itération rapide, ajouter un mode dev qui :
  - Watch `Resources/Renderer2D/*.glsl` via filesystem.
  - Recompile via `NkVulkanShaderCompiler` à la volée.
  - Recrée pipeline + descripteurs.
- Estimé : 2-3h.

### Renderer2D : DrawText avec atlas dynamique
- Actuellement `NkFont` expose `NkGlyph` mais le draw text passe par
  `NkText` qui n'est pas encore wired sur tous les backends. À auditer
  et finaliser sur DX11/DX12/Software.

### Tests cross-backend
- Pas de dossier `tests/` actuel dans NKContext.
- Tests à écrire :
  - Context create/destroy x 6 backends, sans leak Vulkan
    (`vkValidationLayer LeakError`), sans leak DX (`ReportLiveObjects`).
  - BeginFrame/EndFrame/Present 1000x sans crash.
  - Compute : Buffer write/dispatch/read avec oracle CPU
    (NkSoftwareComputeContext) pour comparer.
  - Renderer2D : test golden image pour primitives (rect, circle,
    rotated sprite, text).

### Validation des backends sur kit réel
- Vulkan + OpenGL validés sur Windows. À tester :
  - DX11/12 sur Windows (compile mais pas exécuté dans demos NKRenderer).
  - Metal sur macOS 14/15 (compile mais pas exécuté).
  - Vulkan via MoltenVK sur macOS (compile).
  - Android EGL/Vulkan sur device réel (smartphone récent).
  - Emscripten WebGL2 + WebGPU futur dans navigateur récent.

### `NkSoftwareContext` performance
- Path GDI Win32 est correct mais `StretchDIBits` reste lent en HiDPI
  (1440p+). Envisager Direct2D présenter pour software fallback.
- Path XShm vs MIT-SHM extension : vérifier que XShm est dispo (sinon
  fallback XPutImage non-shm très lent).

---

## À venir / À ajouter (futur proche)

### WebGPU backend
- `NkWebGPUContext` (Emscripten + Dawn natif desktop).
- Remplace WebGL pour navigateurs modernes (Chrome/Edge/Safari 17+).
- Mêmes concepts que Vulkan/DX12 mais API plus simple.
- Estimé : 2-3 semaines.

### Bindless / descriptor indexing
- Actuellement chaque renderer 2D crée un descriptor set par texture
  → limite à ~256 textures uniques par batch (Vulkan).
- Avec descriptor indexing (Vulkan 1.2+ / DX12 SM6.6), passer à
  texture arrays indexés par per-vertex sampler ID.

### Vidéo decode (NVDEC / VAAPI)
- Pour streaming texture vidéo dans le sprite renderer (sprites animés
  via fichiers MP4/WebM).
- Via cuvid (NVIDIA), VAAPI (Linux Intel/AMD), VideoToolbox (Apple),
  MediaFoundation (Windows).

### Compute kernel cache
- Compilation shader compute (HLSL DXC, GLSL → SPIR-V) à chaque appel
  `CreateShaderFromFile`. Ajouter cache disque + hash source.

### NkSurfaceDesc : refactor opaque
- Voir TODO côté NKWindow — proposer un `NkSurfaceHandle` opaque +
  accessors typés pour réduire les includes natifs dans `NkSurfaceDesc`.

### Stats GPU détaillées
- `NkContextInfo` actuel donne vendor/renderer/version. Ajouter :
  - Memory budget / current usage (DXGI, VK_EXT_memory_budget).
  - PCIe link speed, driver version structurée, supported features list.
  - Affichage dans NKUI debug overlay.

### Capture frame (RenderDoc/PIX-friendly)
- API `NkICaptureSession` : start/end capture, annotations
  `BeginEvent("PBR Pass")` / `EndEvent`.
- Câblage via :
  - Vulkan : `vkCmdBeginDebugUtilsLabelEXT`.
  - DX11 : `ID3DUserDefinedAnnotation::BeginEvent`.
  - DX12 : PIX runtime markers.
  - OpenGL : `KHR_debug` push/pop groups.
  - Metal : `pushDebugGroup`.

### Loaders d'image manquants
- EXR (OpenEXR vendored ou tinyexr).
- KTX2 + Basis Universal (compression GPU portable).
- AVIF (libavif).
- WebP (libwebp).
- Mipmap auto-generation depuis NkImage.

---

## Bugs / quirks connus
- **`#include <wingdi.h>` dans NkWin32Window.h** : Win32 type leak en
  cascade dans tout code qui inclut `NkWindow.h` côté Windows. À
  encapsuler.
- **`NkSoftwareContext` OnResize** : si appelé pendant que le pixel
  buffer est en cours de présentation, race condition possible
  (mCachedSurface read pendant que Resize écrit). Single-thread assumé.
- **GLAD2 vs runtime-loaded GL** : `NK_NO_GLAD2` doit être propagé
  côté NKRHI/NKRenderer pour éviter double resolution. Documenté mais
  pas vérifié par assertion.
- **Vulkan validation layers** : sur Windows, les `MessageHandler` font
  ~400 fps → 100 fps en Debug build avec validation activée (cf. bug
  similaire noté dans NKRenderer roadmap).
- **`<algorithm>` STL** : utilisé dans NkUWPWindow.cpp, NkXboxWindow.cpp,
  NkCocoaWindow.mm — à migrer vers NKMath helpers.
- **`NkTextureSetBackend` jamais appelé (BUG MAJEUR pré-existant)** :
  identifié 2026-05-27. La fonction `NkTextureSetBackend()` définie dans
  [NkTexture.cpp](src/NKContext/Renderer/Resources/NkTexture.cpp) **n'est
  appelée par AUCUN backend Renderer2D** (DX11, OpenGL, Vulkan, DX12,
  Software, Metal). Conséquence : `NkTexture::LoadFromImage` / `Create` /
  `Update` retournent toujours `false` avec warning `[NkTexture] No backend
  registered, texture will be invalid for rendering`. Toute app qui crée
  des textures dynamiques via NkTexture+NkSprite ne peut pas les afficher.
  Même `Sandbox/Context/Renderer2D/Renderer2dExample.cpp:196` log ce
  warning et continue avec sprite invisible. Pong contourne en utilisant
  son propre `GLContext + GLRenderer2D + Texture2D` (glXxx direct via glad).
  Les démos NkCameraDemos suivent la même approche (OpenGL pur).
  **Fix requis** : câbler `NkTextureSetBackend({Create, Update, Destroy,
  SetFilter, SetWrap})` dans `NkDX11Renderer2D::Initialize` et équivalents
  OpenGL/Vulkan/DX12/Metal. ~1-2h par backend.

---

## Dépendances
- **Couches en dessous (utilisées)** :
  - NKPlatform (détection compile-time, `NkCGXDetect` pour pixel formats)
  - NKCore (types, atomics)
  - NKContainers (`NkString`, `NkVector`, `NkUniquePtr`, `NkFunction`)
  - NKMath (`NkVec2/3/4`, `NkColor`, `NkRect`, `NkMat4`, transforms)
  - NKMemory (allocateurs, `NkUniquePtr`)
  - NKLogger (logs backend init/erreurs)
  - NKEvent (consomme `NkGraphicsApi` enum + émet `NkGraphicsEvent`
    sur device lost / swapchain recreate)
  - NKWindow (consomme `NkSurfaceDesc` via `window.GetSurfaceDesc()`)
  - NKFileSystem (load shaders depuis disk)
  - **Externes** : Vulkan SDK (validation + shaderc/glslang optionnel),
    DirectX SDK (incluse dans Windows SDK), Metal (SDK macOS/iOS),
    FreeType 2 (NkFont).
- **Modules au-dessus qui en dépendent** :
  - NKRHI (couche d'abstraction supérieure qui utilise les contextes
    bruts via `GetNativeContextData`)
  - NKRenderer (consomme NKRHI + Renderer2D pour overlays)
  - NKUI (consomme Renderer2D pour widgets, NkFont pour text)
  - Nkentseu/Core (Application init crée le contexte via factory)
  - Noge, PV3DE
