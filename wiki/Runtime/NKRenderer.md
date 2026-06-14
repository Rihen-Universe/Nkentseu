# NKRenderer

> Couche **Runtime** · Le moteur de rendu 3D UE5-like (~80% MVP) : façade et render graph,
> scène/caméra, ressources/mesh/streaming, matériaux PBR et shaders, éclairage et ombres
> (CSM/VSM/planar/voxel AO), post-process, voxel, sculpt, et systèmes (animation/IK/VFX/simulation/IA).

Quand une application veut **afficher une scène 3D complète** — meshes éclairés en PBR, ombres
projetées, post-traitement cinématique, particules, texte, debug — c'est NKRenderer qui orchestre
tout. Il se pose au-dessus de NKRHI (l'abstraction GPU bas niveau, 6 backends) et offre une façade
de haut niveau pensée comme UE5 : on crée un `NkRenderer`, on remplit un `NkSceneContext`, on soumet
des `NkDrawCall3D`, et le frame graph interne planifie les passes (Shadow → Geometry → Lighting →
Post → Overlay → Present). Le tout est **opt-in par sous-système** : on n'alloue que ce qu'on utilise.

L'architecture est modulaire : un **cœur** (façade + config + render graph + render target), une
couche **scène** (scene graph, caméras, interfaces drawable/transformable), un gestionnaire de
**ressources** (textures, mesh, streaming), un système de **matériaux + shaders**, des **outils**
d'éclairage/ombres/culling/IBL, une pile de **post-process**, des moteurs **voxel** et **sculpt**
(squelettes), et des **systèmes** d'animation/IK/VFX/simulation/IA.

- **Namespace** : `nkentseu::renderer` (les handles RHI bruts restent au scope `nkentseu`)
- **Header parapluie** : `#include "NKRenderer/NkRenderer.h"`

> Le renderer s'instancie via `NkRenderer::Create(device, config)` / `NkRenderer::Destroy(r)`. Tous
> les sous-systèmes sont possédés par le renderer (accès via `Get*()`, jamais à libérer soi-même).
> Règle dure NKMemory : on ne fait jamais `new`/`delete` sur ces objets.

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Créer le renderer, configurer les sous-systèmes, comprendre le frame graph, rendre hors-écran | [Le cœur](NKRenderer/Core.md) |
| Placer une caméra, bâtir un scene graph, contrôler une caméra orbit | [La scène](NKRenderer/Scene.md) |
| Charger des textures, créer/importer des meshes, streamer un monde ouvert | [Les ressources](NKRenderer/Resources.md) |
| Définir des matériaux PBR/toon, compiler des shaders, gérer les includes | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| Éclairer, projeter des ombres (CSM/VSM), culler, faire de l'IBL et des réflexions | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| Bloom, tonemapping, denoise, dessiner en 2D/3D, du texte, un overlay HUD | [Post-process & outils de rendu](NKRenderer/PostProcess.md) |
| Sculpter de la matière en volume (voxels) | [Le système voxel](NKRenderer/Voxel.md) |
| Sculpter en espace-écran à la ZBrush (pixol) | [Le sculpting](NKRenderer/Sculpt.md) |
| Animer, faire de l'IK, des particules, de la simulation, capturer pour une IA | [Les systèmes](NKRenderer/FeatureSystems.md) |

Chaque page décrit l'**API publique réelle** du module, ses structures de données, ses idiomes de
cycle de vie (`Create`/`Destroy` ou `Init`/`Shutdown`) et ses pièges concrets.

---

## Aperçu des familles

- **Cœur** (`NkRenderer.h`, `Core/`) — la façade `NkRenderer` (interface) + son impl `NkRendererImpl`,
  la `NkRendererConfig` (presets `ForGame`/`ForFilm`/`ForMobile`…, flags de sous-systèmes opt-in),
  le code d'erreur `NkRResult`, les types fondamentaux (handles, vertex layouts, lumières, draw calls,
  stats), le `NkRenderGraph` (frame graph à passes fluentes) et `NkRenderTarget` (render-to-texture).
- **Scène** (`Core/NkScene*.h`, `NkCamera*.h`, `NkI*.h`) — `NkSceneContext` (contrat d'entrée d'une
  frame), `NkSceneNode` (scene graph), `NkCamera3D`/`NkCamera2D` + `NkOrbitCameraController3D`, et
  les interfaces duales `NkITransformable` / `NkIDrawable`.
- **Ressources** (`Core/NkResources.h`, `NkTexture*.h`, `Mesh/`, `Streaming/`) — `NkResources`
  (helpers RHI partagés), `NkTextureLibrary` (cache ref-compté), `NkTextureAsset` (`.nkasset`),
  `NkMeshSystem` (VBO/IBO + primitives), `NkStreamingSystem` (budget VRAM, mip/LOD streaming).
- **Matériaux & shaders** (`Materials/`, `Shader/`) — `NkMaterial` (API Unreal-style), `NkMaterialInstance`,
  `NkMaterialSystem`, `NkMaterialCollection`, `NkMaterialLibrary`, et côté shader `NkShaderBackend`
  (6 backends + NkSL), `NkShaderIncludeResolver`, `NkShaderLibrary`.
- **Éclairage & ombres** (`Passes/`, `Tools/Shadow/`, `Tools/Culling/`, `Tools/Environment/`,
  `Tools/Reflection/`, `Tools/VoxelAO/`) — `NkDeferredPass`, `NkVirtualShadowMaps` (+ packer),
  `NkShadowSystem`, `NkCullingSystem`, `NkEnvironmentSystem` (IBL), `NkPlanarReflectionSystem`,
  `NkVoxelAOSystem`.
- **Post-process & outils** (`Tools/PostProcess/`, `Denoiser/`, `Overlay/`, `Render2D/`, `Render3D/`,
  `Text/`, `Offscreen/`) — `NkPostProcessStack`, `NkDenoiserSystem`, `NkOverlayRenderer`, `NkRender2D`,
  `NkRender3D`, `NkTextRenderer`, `NkOffscreenTarget`.
- **Voxel** (`Tools/Voxel/`) — `NkVoxelSystem` + volume/brush/stroke/pipelines/types (squelette).
- **Sculpt** (`Tools/PixolSculpt/`) — `NkPixolSculptSystem` + buffer/brush/stroke/pipelines/types
  (squelette, sculpt 2.5D « pixol »).
- **Systèmes** (`Tools/Animation/`, `IK/`, `VFX/`, `Simulation/`, `AIRendering/`) — `NkAnimationSystem`,
  `NkIKSystem`, `NkVFXSystem`, `NkSimulationRenderer` (stub), `NkAIRenderingTarget`.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkRenderer.h` | Parapluie + façade `NkRenderer`. | [Cœur](NKRenderer/Core.md) |
| `Core/NkRendererImpl.h` | Impl concrète `NkRendererImpl`. | [Cœur](NKRenderer/Core.md) |
| `Core/NkRendererConfig.h` | `NkRendererConfig`, flags, presets, unités. | [Cœur](NKRenderer/Core.md) |
| `Core/NkRendererResult.h` | `NkRResult` + helpers/macros. | [Cœur](NKRenderer/Core.md) |
| `Core/NkRendererTypes.h` | Handles, vertex, lumières, draw calls, stats. | [Cœur](NKRenderer/Core.md) |
| `Core/NkRenderGraph.h` | Frame graph `NkRenderGraph` + `NkPassBuilder`. | [Cœur](NKRenderer/Core.md) |
| `Core/NkRenderTarget.h` | Render-to-texture `NkRenderTarget`. | [Cœur](NKRenderer/Core.md) |
| `Core/NkSceneContext.h` | `NkSceneContext` (contrat de frame). | [Scène](NKRenderer/Scene.md) |
| `Core/NkSceneNode.h` | `NkSceneNode` (scene graph). | [Scène](NKRenderer/Scene.md) |
| `Core/NkCamera.h` | `NkCamera`/`NkCamera3D`/`NkCamera2D`. | [Scène](NKRenderer/Scene.md) |
| `Core/NkCameraController.h` | `NkOrbitCameraController3D`. | [Scène](NKRenderer/Scene.md) |
| `Core/NkIDrawable.h` | `NkIDrawable` + catégories. | [Scène](NKRenderer/Scene.md) |
| `Core/NkITransformable.h` | `NkITransformable` + `NkTransform`. | [Scène](NKRenderer/Scene.md) |
| `Core/NkResources.h` | `NkResources` (helpers RHI partagés). | [Ressources](NKRenderer/Resources.md) |
| `Core/NkTextureAsset.h` | `NkTextureAsset` (`.nkasset`). | [Ressources](NKRenderer/Resources.md) |
| `Core/NkTextureLibrary.h` | `NkTextureLibrary` (cache). | [Ressources](NKRenderer/Resources.md) |
| `Mesh/NkMeshSystem.h` | `NkMeshSystem` + layouts + primitives. | [Ressources](NKRenderer/Resources.md) |
| `Streaming/NkStreamingSystem.h` | `NkStreamingSystem` (monde ouvert). | [Ressources](NKRenderer/Resources.md) |
| `Materials/NkMaterial.h` | `NkMaterial` (API haute Unreal-style). | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| `Materials/NkMaterialSystem.h` | `NkMaterialSystem`, `NkMaterialInstance`, enums/params. | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| `Materials/NkMaterialAsset.h` | `NkMaterialAsset`, `NkMaterialTextureRef`. | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| `Materials/NkMaterialCollection.h` | `NkMaterialCollection` (params partagés). | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| `Materials/NkMaterialLibrary.h` | `NkMaterialLibrary` (scan/hot-reload). | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| `Shader/NkShaderBackend.h` | `NkShaderBackend` + backends + NkSL. | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| `Shader/NkShaderIncludeResolver.h` | `NkShaderIncludeResolver`. | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| `Shader/NkShaderLibrary.h` | `NkShaderLibrary`, `NkShaderProgram`. | [Matériaux & shaders](NKRenderer/Materials-Shaders.md) |
| `Passes/Deferred/NkDeferredPass.h` | `NkDeferredPass` + G-buffer. | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| `Tools/Shadow/NkShadowSystem.h` | `NkShadowSystem` (CSM mono-dir). | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| `Tools/Shadow/NkShadowAtlasPacker.h` | `NkShadowAtlasPacker`. | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| `Tools/Shadow/NkVirtualShadowMaps.h` | `NkVirtualShadowMaps` (multi-lights). | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| `Tools/Culling/NkCullingSystem.h` | `NkCullingSystem`. | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| `Tools/Environment/NkEnvironmentSystem.h` | `NkEnvironmentSystem` (IBL). | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| `Tools/Reflection/NkPlanarReflectionSystem.h` | `NkPlanarReflectionSystem`. | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| `Tools/VoxelAO/NkVoxelAOSystem.h` | `NkVoxelAOSystem`. | [Éclairage & ombres](NKRenderer/Lighting-Shadows.md) |
| `Tools/PostProcess/NkPostProcessStack.h` | `NkPostProcessStack`. | [Post-process & outils](NKRenderer/PostProcess.md) |
| `Tools/Denoiser/NkDenoiserSystem.h` | `NkDenoiserSystem`. | [Post-process & outils](NKRenderer/PostProcess.md) |
| `Tools/Overlay/NkOverlayRenderer.h` | `NkOverlayRenderer`. | [Post-process & outils](NKRenderer/PostProcess.md) |
| `Tools/Render2D/NkRender2D.h` | `NkRender2D` (batché). | [Post-process & outils](NKRenderer/PostProcess.md) |
| `Tools/Render3D/NkRender3D.h` | `NkRender3D` (PBR forward). | [Post-process & outils](NKRenderer/PostProcess.md) |
| `Tools/Text/NkTextRenderer.h` | `NkTextRenderer`. | [Post-process & outils](NKRenderer/PostProcess.md) |
| `Tools/Offscreen/NkOffscreenTarget.h` | `NkOffscreenTarget`. | [Post-process & outils](NKRenderer/PostProcess.md) |
| `Tools/Voxel/NkVoxelSystem.h` | `NkVoxelSystem` (façade). | [Voxel](NKRenderer/Voxel.md) |
| `Tools/Voxel/NkVoxelVolume.h` | `NkVoxelVolume` (storage 3D). | [Voxel](NKRenderer/Voxel.md) |
| `Tools/Voxel/NkVoxelBrush.h` | `NkVoxelBrush`, `NkVoxelDab`. | [Voxel](NKRenderer/Voxel.md) |
| `Tools/Voxel/NkVoxelStroke.h` | `NkVoxelStroke`. | [Voxel](NKRenderer/Voxel.md) |
| `Tools/Voxel/NkVoxelPipelines.h` | `NkVoxelPipelines`. | [Voxel](NKRenderer/Voxel.md) |
| `Tools/Voxel/NkVoxelBrickPool.h` | `NkVoxelBrickPool` (futur). | [Voxel](NKRenderer/Voxel.md) |
| `Tools/Voxel/NkVoxelTypes.h` | Enums/structs/config voxel. | [Voxel](NKRenderer/Voxel.md) |
| `Tools/PixolSculpt/NkPixolSculptSystem.h` | `NkPixolSculptSystem` (façade). | [Sculpt](NKRenderer/Sculpt.md) |
| `Tools/PixolSculpt/NkPixolBuffer.h` | `NkPixolBuffer` (canvas pixol). | [Sculpt](NKRenderer/Sculpt.md) |
| `Tools/PixolSculpt/NkSculptBrush.h` | `NkSculptBrush`, `NkSculptDab`. | [Sculpt](NKRenderer/Sculpt.md) |
| `Tools/PixolSculpt/NkSculptStroke.h` | `NkSculptStroke`. | [Sculpt](NKRenderer/Sculpt.md) |
| `Tools/PixolSculpt/NkSculptPipelines.h` | `NkSculptPipelines`. | [Sculpt](NKRenderer/Sculpt.md) |
| `Tools/PixolSculpt/NkSculptTileStore.h` | `NkSculptTileStore` (futur). | [Sculpt](NKRenderer/Sculpt.md) |
| `Tools/PixolSculpt/NkSculptTypes.h` | Enums/structs/config sculpt. | [Sculpt](NKRenderer/Sculpt.md) |
| `Tools/Animation/NkAnimationSystem.h` | `NkAnimationSystem` (skinning). | [Systèmes](NKRenderer/FeatureSystems.md) |
| `Tools/IK/NkIKSystem.h` | `NkIKSystem`. | [Systèmes](NKRenderer/FeatureSystems.md) |
| `Tools/VFX/NkVFXSystem.h` | `NkVFXSystem` (particules). | [Systèmes](NKRenderer/FeatureSystems.md) |
| `Tools/Simulation/NkSimulationRenderer.h` | `NkSimulationRenderer` (stub). | [Systèmes](NKRenderer/FeatureSystems.md) |
| `Tools/AIRendering/NkAIRenderingTarget.h` | `NkAIRenderingTarget`. | [Systèmes](NKRenderer/FeatureSystems.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
