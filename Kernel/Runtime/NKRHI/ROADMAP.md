# NKRHI — Roadmap

État actuel (mai 2026) : NKRHI expose une interface `NkIDevice`
([src/NKRHI/Core/NkIDevice.h](src/NKRHI/Core/NkIDevice.h)) couvrant 6 backends
sélectionnables par `NkDeviceFactory`
([src/NKRHI/Core/NkDeviceFactory.cpp](src/NKRHI/Core/NkDeviceFactory.cpp)).
**Validé bout-en-bout sur 5 backends (2026-05-31)** : la démo bas-niveau
`NkRHIDemoFull` ([Applications/Sandbox/src/DemoNkentseu/Base03/NkRHIDemoFull.cpp](../../../Applications/Sandbox/src/DemoNkentseu/Base03/NkRHIDemoFull.cpp))
rend une scène complète (géométrie + Phong + shadow mapping) sur **Vulkan,
OpenGL, DirectX 11, DirectX 12 et Software**, sélectionnables par
`-bgl/-bvk/-bdx11/-bdx12/-bsw`. Avant cette session seuls Vulkan
([src/NKRHI/Vulkan/NkVulkanDevice.cpp](src/NKRHI/Vulkan/NkVulkanDevice.cpp),
2158 lignes) et OpenGL 4.3+
([src/NKRHI/Opengl/NkOpenglDevice.cpp](src/NKRHI/Opengl/NkOpenglDevice.cpp),
1348 lignes) étaient validés (par les démos NKRenderer). DX11 (1156 l.) et
DX12 (1638 l.) sont désormais validés par démo (rendu + ombres corrects).
Metal (798 l.) compile mais non testable sur Windows. Software
([src/NKRHI/Software/NkSoftwareDevice.cpp](src/NKRHI/Software/NkSoftwareDevice.cpp),
1419 l.) est un rasterizer CPU complet avec pixel shaders émulés via lambdas
(`NkSWShaderBridge`) — validé (Phong + ombres) mais via un `fragFn` C++ écrit
à la main dans la démo (pas encore branché sur NkSL→C++ ni sur le compute). Compute cross-API VK+GL opérationnel
(`NkComputeContext` + `NkMLContext` MatMul/Conv2D/Attention/AdamW). Pipeline
shader complet : NkSL → glslang → SPIR-V → SPIRV-Cross →
GLSL/HLSL/MSL/C++ via [src/NKRHI/ShaderConvert/NkShaderConvert.h](src/NKRHI/ShaderConvert/NkShaderConvert.h)
et [src/NKRHI/SL/NkSLCompiler.h](src/NKRHI/SL/NkSLCompiler.h), avec cache
binaire disque (.nksc, FNV-1a 64-bit).

---

## Synthèse

| Backend / Feature                       | Statut    | Effort | Priorité |
|-----------------------------------------|-----------|--------|----------|
| Backend Vulkan 1.2+                     | Livré     | —      | —        |
| Backend OpenGL 4.3+ (DSA, compute)      | Livré     | —      | —        |
| Backend Software (rasterizer CPU)       | Livré     | —      | —        |
| Backend DirectX 11.1                    | Livré     | —      | —        |
| Backend DirectX 12                      | Livré     | —      | —        |
| Backend Metal (macOS/iOS)               | Partiel   | L      | P2       |
| Backend WebGL / WebGPU                  | TODO      | XL     | P3       |
| `NkIDevice` (handles, frame, submit)    | Livré     | —      | —        |
| `NkICommandBuffer` (graphics+compute)   | Livré     | —      | —        |
| Swapchain multi-fenêtre (`NkISwapchain`)| Partiel   | S      | P2       |
| `NkComputeContext` (cache pipelines)    | Livré     | —      | —        |
| `NkMLContext` (MatMul, Attention, Adam) | Livré     | —      | —        |
| `NkGpuPolicy` (sélection vendor)        | Livré     | —      | —        |
| Compute cross-API VK+GL                 | Livré     | —      | —        |
| Compute DX11/DX12/Metal                 | Partiel   | M      | P1       |
| Async compute (queue dédiée)            | Partiel   | M      | P2       |
| `DispatchIndirect`                      | Partiel   | S      | P2       |
| NkSL → SPIRV via glslang                | Livré     | —      | —        |
| SPIRV → GLSL / HLSL / MSL (SPIRV-Cross) | Livré     | —      | —        |
| NkSL → C++ lambdas (Software)           | Livré     | —      | —        |
| `NkShaderCache` (.nksc binaire)         | Livré     | —      | —        |
| Bindless descriptor heap                | TODO      | L      | P1       |
| `ClearRect` API (caching VSM per-tile)  | TODO      | S      | P1       |
| Dynamic UBO offsets (10k+ draws)        | TODO      | M      | P1       |
| Mesh shaders / Task shaders             | TODO      | L      | P3       |
| `DrawIndirectCount`                     | TODO      | S      | P2       |
| Raytracing hardware (DXR / VK KHR)      | TODO      | XL     | P3       |
| Pipeline cache disque (`Save/Load`)     | Partiel   | S      | P2       |
| Timestamp queries                       | Partiel   | S      | P2       |
| `NkGrid3D` / `NkGizmo3D` tools          | Livré     | —      | —        |

Légende : Livré · Partiel · En cours · TODO · Abandonné
Effort : S (≤1j) · M (2-5j) · L (1-2 sem) · XL (>2 sem)

---

## Livré

### Backend Vulkan 1.2+
- `NkVulkanDevice` complet avec queues graphics/compute séparées
- Allocateur mémoire interne (`NkVkAllocation` : VkDeviceMemory + offset)
- Render passes explicites + framebuffers + sous-passes
- `GetFramebufferRenderPass()` pour les sous-systèmes GL-style
  ([NkVulkanDevice.h:74-82](src/NKRHI/Vulkan/NkVulkanDevice.h))
- Synchronisation complète : fences, sémaphores GPU-GPU, `WaitIdle`
- Frame data (triple buffering) : `NkVkFrameData` par frame in-flight
- Push constants typés via `NkICommandBuffer::PushConstants<T>()`
- Compute pipelines + `Dispatch` + `UAVBarrier` + queue compute dédiée
- Plateformes : Windows, Linux, Android (`VK_USE_PLATFORM_ANDROID_KHR`)
- Validé par 10 démos NKRenderer (PBR, IBL, planar reflection, VSM
  multi-lights, bloom Dual-Kawase, voxel AO, color grading LUT, etc.)

### Backend OpenGL 4.3+
- `NkOpenGLDevice` avec DSA (Direct State Access)
- Loader glad2 embarqué (`#include <glad/gl.h>`)
- Compute shaders + SSBO + `glMemoryBarrier`
- VAOs cachés par pipeline + `NkOpenglApplyRenderState` centralisé
- Descriptor sets émulés sur bindings GL aplatis (`NkOpenglApplyDescSet`)
- WGL context sharing pour multi-thread upload (Windows)
- Validé sur les 10 démos NKRenderer cross-checked avec Vulkan

### Backend Software (rasterizer CPU)
- Triangle setup + interpolation barycentrique + z-buffer
- Pixel shaders émulés via `NkFunction` (`NkSWShaderBridge`)
- Mipmaps + sampling (`NkSWSampler::Sample`)
- Présentation native par plateforme : Win32 (`StretchDIBits`), Xlib/XCB,
  Wayland (shm), Android (`ANativeWindow`), Emscripten (HTML5 canvas)
- Pas de MSAA, pas de compute, mais utile pour fallback/headless/tests
- Fast path 2D (`NkSWFastPath.h`) pour blits accélérés

### Compute infrastructure (cross-API VK+GL)
- `NkComputeContext` ([NkComputeContext.h](src/NKRHI/Core/NkComputeContext.h)) :
  - Cache pipelines par clé string (`GetOrCreatePipeline`, `GetOrCompileGLSL`)
  - Dispatch helpers : `Dispatch1D/2D/3D` avec calcul auto des groupes
  - Lazy binding flush avant chaque dispatch
  - Barrières UAV buffer + texture, transitions de layout
  - `SubmitAsync` sur queue compute dédiée (VK/DX12) ou principale (GL/DX11)
  - `NkComputeBuilder` API fluide pour chaîner Bind/Push/Dispatch
- `NkMLContext` ([NkML.h](src/NKRHI/Core/NkML.h)) — couche ML/DL :
  - GEMM (`MatMul`, `MatMulAdd`, `BatchMatMul`) avec tuiles 16×16 partagées
  - `Conv2D` (im2col GPU), `MaxPool2D`, `AvgPool2D`, `GlobalAvgPool`
  - Activations : ReLU, LeakyReLU, GELU exact, Sigmoid, Tanh, Softmax,
    LogSoftmax, Swish/SiLU
  - Normalisations : LayerNorm, BatchNorm, RMSNorm
  - `ScaledDotProductAttention` pour Transformers
  - Optimiseurs : `AdamWStep`, SGD momentum
  - Loss : MSE, MAE, CrossEntropy, BCE, CosineSimilarity
  - Backward : ReLU, MatMul, LayerNorm, SoftmaxCrossEntropy
  - Tout en GLSL compute (compatible OpenGL 4.3+ / Vulkan)
- Confirmé par `NkRenderer/ROADMAP.md` (audit 2026-05-23) :
  compute support OK cross-API VK+GL, déjà utilisé par NkML, morph
  animation, `NkComputeContext`

### Shader cross-compile (NkShaderConverter + NkSL)
- [NkShaderConvert.h](src/NKRHI/ShaderConvert/NkShaderConvert.h) :
  - `GlslToSpirv` (glslang) — requiert `NK_RHI_GLSLANG_ENABLED`
  - `SpirvToGlsl` / `SpirvToHlsl` / `SpirvToMsl` (SPIRV-Cross) —
    requiert `NK_RHI_SPIRVCROSS_ENABLED`
  - `LoadFile` / `LoadAsSpirv` avec auto-détection extension
    (.glsl/.spirv/.spv/.hlsl/.msl)
  - Helpers chaînés `GlslToHlsl/GlslToMsl/GlslToGlsl`
- `NkShaderFileResolver` : convention `shader.vert.glsl` ↔
  `shader.vert.spirv` ↔ `shader.vert.hlsl` ↔ `shader.vert.msl`
- `NkShaderCache` (.nksc, magic `NKSC`) :
  - Clé FNV-1a 64-bit sur (source + stage + format-cible)
  - `PurgeUnused` / `PurgeUnusedThisSession` / `PurgeOlderThan` pour GC
  - Évite recompilation au prochain lancement
- `NkSL` (Nken Shading Language) — DSL textuel cross-target :
  - 8 cibles : `NK_GLSL`, `NK_GLSL_VULKAN`, `NK_SPIRV`, `NK_HLSL_DX11`,
    `NK_HLSL_DX12`, `NK_MSL`, `NK_MSL_SPIRV_CROSS`, `NK_CPLUSPLUS`
  - Lexer/Parser/Semantic/Reflector/CodeGen séparés
    ([SL/](src/NKRHI/SL/))
  - 7 generators : `NkSLCodeGenGLSL`, `GenGLSLVulkan`, `GenHLSL`,
    `GenHLSL_DX12`, `GenMSL`, `GenMSLSpirvCross`, `GenCPP`
  - Validé par NKRenderer phase F : `NkShaderConverter VK→GL/HLSL/MSL via SPIRV-Cross`

### Capabilities & dispatch policy
- `NkDeviceCaps` ([NkIDevice.h:38-94](src/NKRHI/Core/NkIDevice.h)) :
  ~40 flags (mesh shaders, RT, bindless, VRS, float16, atomicInt64,
  BC/ETC2/ASTC compression, MSAA 2/4/8/16x…)
- `NkDeviceFactory` :
  - `Create(init)` selon `init.api`
  - `CreateWithFallback({apis})` itère jusqu'au premier qui marche
  - `CreateAutoDetect(init)` ordre prioritaire par plateforme
    (Windows : VK → DX12 → DX11 → GL → SW ; macOS : Metal → GL ;
    Android/Linux : VK → GL)
- `NkGpuPolicy` ([NkGpuPolicy.h](src/NKRHI/Core/NkGpuPolicy.h)) :
  - `ApplyPreContext` (NvOptimusEnablement / AmdPowerXpressRequest)
  - `MatchesVendorPciId` pour filtrage DXGI/Vulkan adapter

### Outils intégrés
- `NkGrid3D` ([Tools/Grid3D/](src/NKRHI/Tools/Grid3D/)) — grille
  3D world-space avec subdivisions, fade, axes XZ colorés
- `NkGizmo3D` ([Tools/NkGizmo/](src/NKRHI/Tools/NkGizmo/)) — gizmos
  translate/rotate/scale réutilisables depuis l'éditeur Noge

---

## En cours / TODO immédiat

### Backend DirectX 11 — validé par démo (2026-05-31)
- `NkDirectX11Device` ([src/NKRHI/DirectX11/](src/NKRHI/DirectX11/)) rend
  `NkRHIDemoFull` (`-bdx11`) : géométrie + Phong + shadow map OK.
- Reste : cross-check pixel-parfait vs VK/GL sur Demo1..Demo10 NKRenderer ;
  **acné d'auto-ombrage** résiduelle sur le cube (bias HLSL à monter) ;
  brancher DX11 sur NKRenderer (cf. NKRenderer ROADMAP).
- Compute DX11 limité (cf. NkRenderer ROADMAP) — vérifier
  `DispatchCompute`, SRV/UAV slots, `ID3D11DeviceContext::CSSetShader`
- HLSL fxc SM5 vs DXC SM5 : choisir un chemin officiel

### Backend DirectX 12 — validé par démo (2026-05-31)
- `NkDirectX12Device` ([src/NKRHI/DirectX12/](src/NKRHI/DirectX12/)) avec
  `NkDX12DescHeap` CPU+GPU rend `NkRHIDemoFull` (`-bdx12`) : rendu + ombres OK
  (mêmes résultats que DX11, acné d'auto-ombrage identique).
- Reste : root signature auto-générée depuis `NkDescriptorSetLayoutDesc` ;
  validation des barrières `D3D12_RESOURCE_STATES` vs `NkResourceState` ;
  brancher DX12 sur NKRenderer.
- Async compute queue théorique — pas validé bout en bout
- DXC SM6+ : intégrer dans `NkShaderConverter::GlslToHlsl(sm=60)`
- Pipeline cache disque : implémenter `ID3D12PipelineLibrary`

### Backend Metal — validation
- `NkMetalDevice` ([Metal/NkMetalDevice.mm](src/NKRHI/Metal/NkMetalDevice.mm),
  798 l.) compile en .mm mais runtime macOS pas testé
- Manque : test sur macOS Sonoma / iPadOS via Demo3D minimal
- Metal n'a pas d'objet RasterizerState — stocker manuellement
  (cf. `NkMetalPipeline.frontFaceCCW/cullMode/depthBias…`)
- Argument buffers pour bindless — à implémenter
- MSL via `NkShaderConverter::SpirvToMsl` (SPIRV-Cross) validé pour la
  conversion mais pas pour le run-time Metal

### Async compute & semaphores
- `HasDedicatedComputeQueue()` retourne `true` sur VK/DX12 mais peu utilisé
- `SubmitOnQueue(NK_COMPUTE, info)` à valider avec un cas réel :
  NkML running en parallèle du rendu graphics
- Sémaphores GPU-GPU (`NkSemaphoreHandle`) : binaires (VK_SEMAPHORE_TYPE_BINARY).
  Timeline semaphores (VK 1.2 timeline / DX12 fences) — TODO

### `ClearRect` API (demandée par NkVirtualShadowMaps v2)
- Actuellement clear plein écran via `BeginRenderPass` avec
  `NkClearValue`. Besoin de clear partiel rectangulaire pour caching
  per-tile dans l'atlas shadow VSM
- Vulkan : `vkCmdClearAttachments` avec `VkClearRect`
- OpenGL : `glScissor` + `glClear` ou `glClearTexSubImage` (4.4+)
- DX11/12 : `ClearRenderTargetView` n'accepte pas de rect → besoin scissor
- API proposée : `NkICommandBuffer::ClearAttachmentRect(idx, NkRect2D, NkClearValue)`

### Dynamic UBO offsets (demandé par NKRenderer pour 10k+ draws)
- Actuellement `BindUniformBuffer` lie tout le buffer
- VK : `vkCmdBindDescriptorSets` avec `pDynamicOffsets[]`
- GL : `glBindBufferRange(GL_UNIFORM_BUFFER, ...)` per-draw
- DX12 : root CBV avec offset GPU-VA per-draw
- API existante esquissée : `NkICommandBuffer::BindDescriptorSet(set, idx, dynOff[], count)`
  ([NkComputeContext.h:181-183](src/NKRHI/Core/NkComputeContext.h)) — à
  généraliser pour graphics

### Pipeline cache disque
- Interface présente : `SavePipelineCache(path)` / `LoadPipelineCache(path)`
  retourne `false` par défaut sur tous les backends
- Vulkan : `VkPipelineCache` → blob sur disque
- DX12 : `ID3D12PipelineLibrary` (déjà mentionné côté backend ci-dessus)
- GL : `glGetProgramBinary` / `glProgramBinary` (extension ARB_get_program_binary)
- Gain attendu : -300ms à -1s sur boot

---

## À venir (futur proche)

### Bindless descriptor heap
- API esquissée : `CreateBindlessHeap` / `WriteBindlessTexture` /
  `WriteBindlessBuffer` / `BindBindlessHeap`
  ([NkIDevice.h:423-442](src/NKRHI/Core/NkIDevice.h)) — retournent
  `{}` par défaut, non implémentées
- Requis pour : indirect rendering, megatexture, NKRenderer Phase S
- VK : `VK_EXT_descriptor_indexing` + `VkDescriptorBindingFlags` runtime array
- DX12 : `ResourceDescriptorHeap[]` SM6.6 + bindless root signature
- GL : `GL_ARB_bindless_texture` (NVIDIA mostly)
- Metal : argument buffers tier 2

### Backend WebGL / WebGPU
- Cible Emscripten déjà présente côté Software backend
- WebGL 2.0 : sous-ensemble OpenGL ES 3.0 → adapter le backend GL
  (pas de DSA, pas de SSBO, pas de compute → trade-off lourd)
- WebGPU (recommandé) : ajouter `NkWebGPUDevice` en s'inspirant de Vulkan
- Cibler PV3DE web demo + Noge editor web preview

### Mesh shaders / Task shaders
- `NkDeviceCaps::meshShaders` déjà déclaré
- Requis pour NKRenderer Phase S (GPU-driven, virtual textures)
- VK : `VK_EXT_mesh_shader`
- DX12 : SM6.5 amplification/mesh shaders

### Raytracing hardware
- `NkDeviceCaps::rayTracing` déjà déclaré
- VK : `VK_KHR_ray_tracing_pipeline` + `VK_KHR_acceleration_structure`
- DX12 : DXR 1.1 (`ID3D12Device5::CreateStateObject`)
- API à concevoir : `NkAccelerationStructureHandle`, `NkRayTracingPipelineDesc`,
  `NkICommandBuffer::TraceRays`
- Use case NKRenderer Phase R : RT shadows / reflections / GI

### `DrawIndirectCount` + GPU culling
- `NkDeviceCaps::drawIndirectCount` (VK 1.2 / DX12) déjà déclaré
- API : `NkICommandBuffer::DrawIndirectCount(argsBuf, countBuf, maxDraws)`
- Permet GPU culling complet (cull en compute, indirect draw count = nb visibles)

### Variable rate shading + Conservative rasterization
- Caps déjà déclarés (`variableRateShading`, `conservativeRasterization`)
- Implémentation backend par backend selon support hardware
- Use case : optimisation perf VSM + AO

### `NkISwapchain` multi-fenêtre
- Interface esquissée ([NkISwapchain.h](src/NKRHI/Core/NkISwapchain.h))
  + `NkIDevice::CreateSwapchain/DestroySwapchain` retournent `nullptr` par
  défaut
- Actuellement chaque device gère 1 swapchain interne
  (`GetSwapchainFramebuffer/Width/Height/Format` déprécié mais maintenu)
- Cible : éditeur Noge avec docking et plusieurs vues 3D simultanées

### Timestamp queries production-ready
- API présente (`BeginTimestampQuery/EndTimestampQuery/GetTimestampResults`)
  mais implémentations par défaut retournent `false`
- Implémenter sur VK (`vkCmdWriteTimestamp` + query pool), DX12
  (`ID3D12QueryHeap`), GL (`GL_TIMESTAMP` queries)
- Use case : profiler frame Noge + stats NKRenderer

### Texture compression upload (BC1-7 / ASTC / ETC2)
- Caps déclarés (`textureCompressionBC/ETC2/ASTC`)
- `NkGPUFormat` doit exposer les formats compressés (vérifier
  [NkTypes.h](src/NKRHI/Core/NkTypes.h))
- `WriteTexture` doit accepter des blocs compressés (alignement 4×4 / N×N)
- Requis par NKRenderer Phase H texture pipeline

### NkSL — extensions de langage
- Lié à NKRenderer Phase G material system
- Manque : geometry shaders end-to-end (déclaré dans caps mais non testé NkSL)
- Manque : ray tracing stages (RGEN/MISS/CHIT) — pipeline complet à concevoir

### Software backend — extensions
- Multi-threading rasterizer (tile-based parallel — `std::thread` déjà inclus)
- MSAA software (4x ordered grid minimum)
- Compute software (single-threaded ou tile-based) pour tester `NkComputeContext`
  sans GPU

### Conformité NKMemory — retirer les allocations brutes (À VÉRIFIER / À FAIRE)
> Audit 2026-06-04. La convention projet (CLAUDE.md §1) impose les allocateurs
> NKMemory (`nkentseu::memory::NkAlloc` / `NkFree`, ou `nkMalloc`/`nkFree`) et
> **interdit** `new`/`delete`/`malloc`/`free` bruts (risque heap corruption
> Windows c0000374 si on mélange allocateur custom et heap CRT).
>
> NKRHI **ne respecte pas encore** cette règle : ~33 sites de `new`/`delete`
> bruts repérés. À convertir :
> - `Core/NkDeviceFactory.cpp` : `new NkXxxDevice()` / `delete dev` (6 backends +
>   Create/Destroy/Fallback/AutoDetect) → `NkAlloc`/placement-new + `NkFree`.
> - `DirectX12/NkDirectX12Device.cpp:1421` + `DirectX11`/`Opengl` : `new`/`delete`
>   des `NkXxxCommandBuffer` (Create/DestroyCommandBuffer).
> - `SL/NkSLCompiler.cpp`, `SL/NkSLIntegration.cpp:31` (`new NkSLCompiler`),
>   `SL/NkSLCodeGenAdvanced.cpp` : `new`/`delete` des AST/compiler.
>
> Tâche : remplacer par le pattern NKMemory (allocate + placement-new, destructeur
> explicite + `NkFree`), comme dans NKImage/NKContainers. Vérifier qu'aucun objet
> alloué NKMemory n'est libéré via `delete`/`free` (et inversement).

---

## Bugs / quirks connus

- **OpenGL : pas de concept de `RenderPass` natif**.
  `GetFramebufferRenderPass()` retourne `{}` côté GL (par design).
  Les systèmes ne doivent pas en dépendre obligatoirement
  ([NkIDevice.h:235](src/NKRHI/Core/NkIDevice.h)).
- **Software : pas de compute**. `CreateComputePipeline` / `Dispatch`
  ne sont pas opérationnels — toute la couche `NkComputeContext` /
  `NkMLContext` ne fonctionne pas dessus.
- **OpenGL : `BeginRenderPass` ne clear pas tout seul** en usage RHI direct
  (sans RenderGraph). `GL_BeginRenderPass` ne clear que si `SetClearColor`/
  `SetClearDepth` ont armé `mClearColorPending`/`mClearDepthPending` ; DX/VK/SW
  clear inconditionnellement. Sans appel, le depth buffer GL n'est jamais remis
  à 1.0 → depth test rejette tout → écran noir. Une app RHI directe DOIT appeler
  `SetClearColor/SetClearDepth` avant chaque `BeginRenderPass`.
- **Vulkan : swapchain sRGB par défaut** (`NkVulkanDesc::srgbSwapchain=true`).
  Un shader qui écrit du linéaire « brut » (sans tonemapping/gamma) paraît
  délavé/pâle vs OpenGL/DX (UNORM). Mettre `srgbSwapchain=false` pour un rendu
  identique cross-backend sans gestion gamma ; un vrai renderer (ACES) garde sRGB.
- **Vulkan : validation OFF par défaut** (`validationLayers=false` depuis
  2026-05-31) — l'activer peut crasher `vkCreateInstance` si un `msvcp140.dll`
  incompatible est sur le PATH (ex. Huawei DevEco Studio). Opt-in explicite.
- **Shadow mapping cross-backend** : la convention d'échantillonnage de la
  shadow map diffère par API (DX flip Y dans le HLSL `-y*0.5+0.5` ; GL/VK pas de
  flip ; Software flip car `NDCToScreen` écrit Y-inversé). Résidus à 2026-05-31 :
  acné d'auto-ombrage sur DX (bias à monter), ombre GL pas parfaite.
- **Vulkan : FPS chute en mode Debug** (500→100 fps en ~2s observé
  2026-05-16, validation layers + UBO writes + descriptor updates intensifs).
  À retester en Release. Cf. NkRenderer ROADMAP.
- **DX12 : `ComPtr` partout** — risque de cycles si refs croisées dans
  `NkDX12Buffer/Texture/Pipeline`. Auditer le `Shutdown`.
- **Metal : warning macro `CreateGpuSemaphore`** dans certains SDK Apple →
  workaround `#ifdef CreateGpuSemaphore #undef ... #endif` présent
  ([NkIDevice.h:376-378](src/NKRHI/Core/NkIDevice.h)).
- **NkSL → C++ lambdas** : `NkSWShaderBridge` v5 utilise convention de
  layout par location (POSITION=0, UV/COLOR=1, NORMAL=3) — limite la
  flexibilité, pas d'introspection runtime des noms.
- **Pipeline cache disque** non opérationnel — boot froid recompile tous
  les shaders à chaque session (mitigé par `NkShaderCache` côté
  conversion, mais pas côté driver).

---

## Dépendances

- **Couches en dessous (utilisées)** :
  - `NKCore` — types primitifs (`uint32`, `float32`, …), `NkAtomic`, `NkTraits`
  - `NKMath` — `NkVec*`, `NkMat4`, `NkColor` (rasterizer software + tools)
  - `NKContainers` — `NkVector`, `NkUnorderedMap`, `NkString`, `NkFunction`
  - `NKThreading` — `NkMutex`, `NkScopedLockMutex` (thread-safety device)
  - `NKLogger` — `logger_src.Infof` (factory + tous backends)
  - `NKPlatform` — `NKENTSEU_PLATFORM_*` macros, détection OS
  - `NKContext` — `NkIGraphicsContext` (couche 0 qui crée le device natif)
  - Externes embarqués : `glslang` (sous-module), `SPIRV-Cross` (sous-module),
    `glad2` (loader OpenGL)
  - Externes système (conditionnels) : Vulkan SDK, D3D11/12 SDK Windows,
    Metal SDK macOS/iOS

- **Modules au-dessus qui en dépendent** :
  - `NKRenderer` — consommateur principal, 10 démos cross-API
  - `NKUI` — rendu widgets (utilise `NkICommandBuffer` + `NkIDevice`)
  - `NKAnimation` — morph targets en compute (utilise `NkComputeContext`)
  - `NKImage` ↔ NKRHI — pipeline texture (loaders → upload GPU)
  - `Engine` / `Noge` (PV3DE, Noge éditeur) — consommateurs indirects
    via NKRenderer
