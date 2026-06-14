# Noge — documentation détaillée

Le module **Noge** (framework applicatif / moteur de jeu + suite de création), partie par
partie. Pour une vue d'ensemble et un guide « par où commencer », voir le récap :
[../Noge.md](../Noge.md).

> ⚠️ Le **Core** et l'**ECS gameplay de base** sont implémentés ; les familles
> **Modeling, Animation, Design2D, IO, Viewport, Physics**, ainsi que **VisualScript** et une
> partie du **Scripting**, sont en grande majorité des **headers de spécification sans `.cpp`**
> (parfois avec des incohérences de compilation). Chaque page signale le statut au cas par cas.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Core.md](Core.md) | Framework applicatif : `NkApplication`/`NkApplicationConfig`, point d'entrée `NkMainApp`, `NkLayer`/`NkLayerStack`/`NkEngineLayer`, `NkEventBus`, `NkProfiler`. | `Core/NkApplication.h`, `Core/NkApplicationConfig.h`, `Core/NkMainApp.h`, `Core/NkLayer.h`, `Core/NkLayerStack.h`, `Core/NkEngineLayer.h`, `Layers/NkEngineLayer.h`, `Core/NkEventBus.h`, `Core/NkProfiler.h`, `Nkentseu.h` |
| [Components.md](Components.md) | Composants ECS gameplay : handles/registre + core (tag, transform), animation, audio, physique, rendu (caméra, render), scène, UI. | `ECS/Components/NkComponentHandle.h`, `NkComponentRegistry.h`, `Core/NkCoreComponents.h`, `Core/NkTag.h`, `Core/NkTransform.h`, `Animation/NkAnimation.h`, `Audio/NkAudio*.h`, `Physics/NkPhysics*.h`, `Rendering/NkCamera.h`, `Rendering/NkRender*.h`, `SceneComponent/NkSceneComponent.h`, `UI/NkUIComponent.h` |
| [Entities.md](Entities.md) | Entités gameplay : `NkGameObject`, `NkActor`/`NkPawn`/`NkCharacter`, `NkBehaviour` + systèmes, fabrique d'objets. | `ECS/Entities/NkActor.h`, `NkBehaviour.h`, `NkBehaviourSystem.h`, `NkGameObject.h`, `ECS/Factory/NkGameObjectFactory.h` |
| [Scene.md](Scene.md) | Scène : graphe de scène, gestionnaire + transitions, cycle de vie, script de scène, prefab. | `ECS/Scene/NkSceneGraph.h`, `NkSceneManager.h`, `NkSceneLifecycleSystem.h`, `NkSceneScript.h`, `ECS/Prefab/NkPrefab.h` |
| [Systems.md](Systems.md) | Systèmes ECS : rendu (pont NKRenderer), transform hiérarchique, sérialisation de scène, réflexion des composants. | `ECS/Systems/NkRenderSystem.h`, `NkTransformSystem.h`, `NkSceneSerializer.h`, `NkReflectComponents.h` |
| [Scripting.md](Scripting.md) | Scripting : composant/système natif, pont DLL hot-reload, backends C# (Mono) et Python (CPython). | `ECS/Scripting/NkScriptComponent.h`, `NkScriptSystem.h`, `NkScriptBridge.h`, `CSharp/NkScriptCSharp.h`, `Python/NkScriptPython.h` |
| [VisualScript.md](VisualScript.md) | Visual scripting (blueprint) + réplication réseau : `NkBlueprint`, hot-reload, graphe valide, `NkNetWorld`. **Spec.** | `ECS/VisualScript/NkBlueprint.h`, `NkBlueprintHotReload.h`, `NkValidGraph.h`, `ECS/Replication/NkNetWorld.h` |
| [Modeling.md](Modeling.md) | Modélisation 3D : mesh éditable, modificateurs, mesh procédural, undo stack, booléens BSP, half-edge, sculpt, éditeur UV. | `Modeling/NkEditableMesh.h`, `NkMeshModifier.h`, `NkProceduralMesh.h`, `NkUndoStack.h`, `Topology/NkBooleanOp.h`, `Topology/NkHalfEdge.h`, `Sculpt/NkSculpting.h`, `UV/NkUVEditor.h` |
| [Animation.md](Animation.md) | Animation : locomotion/motion-matching, anim 2D (atlas, tween), IK, séquenceur, foule, facial (rig FACS, peau). | `Anim/NkLocomotion.h`, `Crowd/NkCrowdSim.h`, `Anim2D/NkAtlas2D.h`, `Anim2D/NkTween.h`, `Rigging/NkIKSolver.h`, `Sequencer/NkSequencer.h`, `Facial/NkFacialRig.h`, `Facial/NkSkinMaterial.h` |
| [Design2D.md](Design2D.md) | Design 2D : pile de calques, canvas raster, chemin vectoriel, documents (hybride/vectoriel), gestion colorimétrique, texte sur chemin. | `Design/NkLayerStack.h`, `Design/Raster/NkRasterCanvas.h`, `Design/Vector/NkVectorPath.h`, `Doc/NkHybridDocument.h`, `Doc/NkVectorDocument.h`, `Color/NkColorManager.h`, `Text/NkTextPath.h` |
| [IO.md](IO.md) | Import/export 3D et 2D : FBX, glTF/GLB, OBJ/MTL, SVG. | `IO/NkFBXImporter.h`, `NkGLTFIO.h`, `NkOBJIO.h`, `NkSVGIO.h` |
| [Viewport.md](Viewport.md) | Viewport éditeur : gizmo, selection buffer (picking GPU), caméra de viewport, système de sélection raster. | `Viewport/NkGizmo.h`, `NkSelectionBuffer.h`, `NkViewportCamera.h`, `Selection/NkSelectionSystem.h` |
| [Physics.md](Physics.md) | Physique : mesh de physique (cloth/hair/softbody/ragdoll/mocap/jiggle), systèmes de physique. | `Physics/NkPhysicsMesh.h`, `Systems/NkPhysicsSystems.h` |

[← Récap Noge](../Noge.md) · [← Couche Engine](../README.md)
