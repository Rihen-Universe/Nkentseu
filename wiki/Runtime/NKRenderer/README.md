# NKRenderer — documentation détaillée

Le module **NKRenderer**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKRenderer.md](../NKRenderer.md).

Chaque page décrit l'**API publique réelle** d'un sous-ensemble du module : ses classes et
structures, leurs signatures, les idiomes de cycle de vie (`Create`/`Destroy` pour la façade et les
matériaux, `Init`/`Shutdown` pour les sous-systèmes) et les pièges concrets d'ownership et de GPU.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Core.md](Core.md) | Façade `NkRenderer` + impl, config et presets, codes d'erreur `NkRResult`, types fondamentaux (handles/vertex/lumières/draw calls), frame graph et render target. | `NkRenderer.h`, `Core/NkRendererImpl.h`, `Core/NkRendererConfig.h`, `Core/NkRendererResult.h`, `Core/NkRendererTypes.h`, `Core/NkRenderGraph.h`, `Core/NkRenderTarget.h` |
| [Scene.md](Scene.md) | Contexte de scène, scene graph (`NkSceneNode`), caméras 3D/2D + contrôleur orbit, interfaces duales drawable/transformable. | `Core/NkSceneContext.h`, `Core/NkSceneNode.h`, `Core/NkCamera.h`, `Core/NkCameraController.h`, `Core/NkIDrawable.h`, `Core/NkITransformable.h` |
| [Resources.md](Resources.md) | Helpers RHI partagés, bibliothèque/asset de textures, système de mesh (primitives, LOD), streaming de monde ouvert. | `Core/NkResources.h`, `Core/NkTextureAsset.h`, `Core/NkTextureLibrary.h`, `Mesh/NkMeshSystem.h`, `Streaming/NkStreamingSystem.h` |
| [Materials-Shaders.md](Materials-Shaders.md) | Matériaux PBR/toon (matériau, instance, collection, bibliothèque, système) et shaders (backends, résolveur d'includes, bibliothèque). | `Materials/NkMaterial.h`, `Materials/NkMaterialAsset.h`, `Materials/NkMaterialCollection.h`, `Materials/NkMaterialLibrary.h`, `Materials/NkMaterialSystem.h`, `Shader/NkShaderBackend.h`, `Shader/NkShaderIncludeResolver.h`, `Shader/NkShaderLibrary.h` |
| [Lighting-Shadows.md](Lighting-Shadows.md) | Passe différée, ombres (système CSM, atlas packer, virtual shadow maps), culling, environnement/IBL, réflexion planaire, voxel AO. | `Passes/NkDeferredPass.h`, `Tools/Shadow/NkShadowSystem.h`, `Tools/Shadow/NkShadowAtlasPacker.h`, `Tools/Shadow/NkVirtualShadowMaps.h`, `Tools/Culling/NkCullingSystem.h`, `Tools/Environment/NkEnvironmentSystem.h`, `Tools/Reflection/NkPlanarReflectionSystem.h`, `Tools/VoxelAO/NkVoxelAOSystem.h` |
| [PostProcess.md](PostProcess.md) | Pile de post-process (bloom, tonemapping, SSAO…), denoiser, overlay HUD, rendu 2D/3D, rendu de texte, cible offscreen. | `Tools/PostProcess/NkPostProcessStack.h`, `Tools/Denoiser/NkDenoiserSystem.h`, `Tools/Overlay/NkOverlayRenderer.h`, `Tools/Render2D/NkRender2D.h`, `Tools/Render3D/NkRender3D.h`, `Tools/Text/NkTextRenderer.h`, `Tools/Offscreen/NkOffscreenTarget.h` |
| [Voxel.md](Voxel.md) | Système de voxels (squelette) : volume storage 3D, brick pool, brush, stroke, pipelines compute, types. | `Tools/Voxel/NkVoxelSystem.h`, `Tools/Voxel/NkVoxelVolume.h`, `Tools/Voxel/NkVoxelBrickPool.h`, `Tools/Voxel/NkVoxelBrush.h`, `Tools/Voxel/NkVoxelStroke.h`, `Tools/Voxel/NkVoxelPipelines.h`, `Tools/Voxel/NkVoxelTypes.h` |
| [Sculpt.md](Sculpt.md) | Sculpting espace-écran « pixol » (squelette) : système, canvas buffer, brush, stroke, pipelines, tile store, types. | `Tools/PixolSculpt/NkPixolSculptSystem.h`, `Tools/PixolSculpt/NkPixolBuffer.h`, `Tools/PixolSculpt/NkSculptBrush.h`, `Tools/PixolSculpt/NkSculptStroke.h`, `Tools/PixolSculpt/NkSculptPipelines.h`, `Tools/PixolSculpt/NkSculptTileStore.h`, `Tools/PixolSculpt/NkSculptTypes.h` |
| [FeatureSystems.md](FeatureSystems.md) | Systèmes : animation (skinning, clips, blend), IK, VFX/particules/trails/decals, rendu de simulation (FACS/émotion), cible de rendu pour l'IA. | `Tools/Animation/NkAnimationSystem.h`, `Tools/IK/NkIKSystem.h`, `Tools/VFX/NkVFXSystem.h`, `Tools/Simulation/NkSimulationRenderer.h`, `Tools/AIRendering/NkAIRenderingTarget.h` |

[← Récap NKRenderer](../NKRenderer.md) · [← Couche Runtime](../README.md)
