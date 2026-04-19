#pragma once
// =============================================================================
// Nkentseu/Nkentseu.h — Point d'entrée unique du moteur Nkentseu
// =============================================================================
// Inclure ce fichier unique pour accéder à l'ensemble des fonctionnalités
// du moteur Nkentseu (couche applicative au-dessus de NKECS + NKRenderer).
//
// ARCHITECTURE DES COUCHES :
//   ┌──────────────────────────────────────────────────────┐
//   │  Application (jeu, simulation, éditeur...)           │
//   ├──────────────────────────────────────────────────────┤
//   │  Nkentseu  ← CE MODULE                               │
//   │  NkGameObject, NkActor, NkPrefab, NkSceneGraph...    │
//   ├──────────────────────────────────────────────────────┤
//   │  NKECS (ECS autonome)    │  NKRenderer (rendu)       │
//   │  NkWorld, NkScheduler    │  NkRenderer, NkRender3D   │
//   ├──────────────────────────┼───────────────────────────┤
//   │  NKRHI (abstraction GPU — Vulkan/DX11/DX12/Metal/GL) │
//   └──────────────────────────────────────────────────────┘
// =============================================================================

// ── Cœur de l'application ────────────────────────────────
#include "Core/NkApplication.h"
#include "Core/NkApplicationConfig.h"
#include "Core/NkLayer.h"
#include "Core/NkLayerStack.h"
#include "Core/NkMainApp.h"
#include "Core/NkProfiler.h"

// ── ECS - Composants de base ──────────────────────────────
#include "ECS/Components/Core/NkCoreComponents.h"   // NkTransform, NkName, NkTag, NkParent, NkChildren
#include "ECS/Components/NkComponentHandle.h"        // NkComponentHandle, NkRequiredComponent, NkOptionalComponent
#include "ECS/Components/Rendering/NkRenderComponents.h"
#include "ECS/Components/Physics/NkPhysicsComponents.h"
#include "ECS/Components/Audio/NkAudioComponents.h"
#include "ECS/Components/Animation/NkAnimation.h"
#include "ECS/Components/UI/NkUIComponent.h"
#include "ECS/Components/SceneComponent/NkSceneComponent.h"

// ── ECS - Entités (GameObjects) ───────────────────────────
#include "ECS/Entities/NkGameObject.h"   // Handle ECS léger (16 bytes)
#include "ECS/Entities/NkActor.h"        // NkActor, NkPawn, NkCharacter
#include "ECS/Entities/NkBehaviour.h"    // Base des scripts MonoBehaviour-style
#include "ECS/Entities/NkBehaviourSystem.h"

// ── ECS - Factory (pont NkWorld → NkGameObject) ───────────
#include "ECS/Factory/NkGameObjectFactory.h"

// ── ECS - Préfabriqués ────────────────────────────────────
#include "ECS/Prefab/NkPrefab.h"         // NkPrefab, NkPrefabRegistry, NkPrefabInstance

// ── ECS - Scène et Hiérarchie ─────────────────────────────
#include "ECS/Scene/NkSceneGraph.h"      // NkSceneGraph (wrapper NkWorld + hiérarchie)
#include "ECS/Scene/NkSceneScript.h"     // Base des scripts de scène
#include "ECS/Scene/NkSceneManager.h"    // Chargement/transitions de scènes
#include "ECS/Scene/NkSceneLifecycleSystem.h"

// ── ECS - Systèmes ────────────────────────────────────────
#include "ECS/Systems/NkTransformSystem.h"
#include "ECS/Systems/NkReflectComponents.h"

// ── ECS - Scripting ───────────────────────────────────────
#include "ECS/Scripting/NkScriptComponent.h"
#include "ECS/Scripting/NkScriptSystem.h"

// ── ECS - Visual Scripting ────────────────────────────────
#include "ECS/VisualScript/NkBlueprint.h"

// ── Assets ────────────────────────────────────────────────
#include "Asset/NkAssetImporter.h"
#include "Asset/NkAssetMetadata.h"

// ── Maths ─────────────────────────────────────────────────
#include "Maths/NkTransform.h"

// ── Aliases de commodité ──────────────────────────────────
namespace nkentseu {
    // Raccourcis fréquents
    using NkGameObject  = ecs::NkGameObject;
    using NkActor       = ecs::NkActor;
    using NkPawn        = ecs::NkPawn;
    using NkCharacter   = ecs::NkCharacter;
    using NkBehaviour   = ecs::NkBehaviour;
    using NkSceneGraph  = ecs::NkSceneGraph;
    using NkSceneScript = ecs::NkSceneScript;

    // Accès facile à la factory
    using NkGOFactory = NkGameObjectFactory;
}
