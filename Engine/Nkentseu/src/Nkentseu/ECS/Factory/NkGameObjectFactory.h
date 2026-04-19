#pragma once
// =============================================================================
// Nkentseu/ECS/Factory/NkGameObjectFactory.h
// =============================================================================
// Fabrique de GameObjects — PONT entre NkWorld (ECS autonome) et NkGameObject.
//
// ARCHITECTURE :
//   NkWorld ne connaît PAS NkGameObject (séparation de responsabilités).
//   NkGameObjectFactory est la couche d'adaptation qui :
//     1. Appelle NkWorld::CreateEntity()
//     2. Ajoute les composants invariants (NkTransform, NkName, NkTag...)
//     3. Construit et retourne le NkGameObject (handle moteur)
//
// USAGE :
//   Auto player = NkGameObjectFactory::Create<Player>(world, "Hero");
//   Auto enemy  = NkGameObjectFactory::InstantiatePrefab(world, "prefabs/orc.prefab");
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "Nkentseu/ECS/Entities/NkGameObject.h"
#include "Nkentseu/ECS/Components/Core/NkTransform.h"
#include "Nkentseu/ECS/Components/Core/NkTag.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {

    // Forward declarations
    class NkPrefab;
    class NkPrefabRegistry;

    // =========================================================================
    // NkGameObjectFactory
    // =========================================================================
    class NkGameObjectFactory {
        public:
            NkGameObjectFactory() = delete;  // Classe utilitaire stateless

            // ── Création d'un GameObject unique ────────────────────────────
            /**
             * @brief Crée un NkGameObject avec les composants ECS invariants.
             *
             * Composants ajoutés automatiquement :
             *   NkTransform, NkName, NkTag, NkParent, NkChildren, NkBehaviourHost
             */
            template<typename T = ecs::NkGameObject, typename... Args>
            [[nodiscard]] static T Create(ecs::NkWorld& world,
                                          const char* name,
                                          Args&&... args) noexcept
            {
                static_assert(std::is_base_of_v<ecs::NkGameObject, T>,
                              "T must derive from NkGameObject");

                const ecs::NkEntityId id = world.CreateEntity();

                world.Add<ecs::NkName>(id, ecs::NkName(name));
                world.Add<ecs::NkTag>(id);
                world.Add<ecs::NkTransform>(id);
                world.Add<ecs::NkParent>(id);
                world.Add<ecs::NkChildren>(id);
                world.Add<ecs::NkBehaviourHost>(id);

                return T(id, &world, traits::NkForward<Args>(args)...);
            }

            template<typename T = ecs::NkGameObject, typename... Args>
            [[nodiscard]] static T Create(ecs::NkWorld& world, Args&&... args) noexcept {
                const char* typeName = ecs::NkTypeRegistry::Global().TypeName<T>();
                return Create<T>(world, typeName, traits::NkForward<Args>(args)...);
            }

            // ── Création en masse ──────────────────────────────────────────
            template<typename T = ecs::NkGameObject>
            static void CreateBatch(ecs::NkWorld& world,
                                    const char* baseName,
                                    uint32 count,
                                    NkVector<T>& out) noexcept
            {
                static_assert(std::is_base_of_v<ecs::NkGameObject, T>,
                              "T must derive from NkGameObject");
                char nameBuf[256];
                for (uint32 i = 0; i < count; ++i) {
                    NkSNPrintf(nameBuf, sizeof(nameBuf), "%s_%u", baseName, i);
                    out.PushBack(Create<T>(world, nameBuf));
                }
            }

            // ── Instanciation de prefab ────────────────────────────────────
            [[nodiscard]] static ecs::NkGameObject
            InstantiatePrefab(ecs::NkWorld& world,
                              const char* prefabPath,
                              const char* instanceName = nullptr) noexcept;

            static void InstantiatePrefabBatch(
                ecs::NkWorld& world,
                const char* prefabPath,
                uint32 count,
                NkVector<ecs::NkGameObject>& out) noexcept;
    };

} // namespace nkentseu
