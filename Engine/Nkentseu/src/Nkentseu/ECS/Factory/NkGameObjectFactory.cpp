// =============================================================================
// Nkentseu/ECS/Factory/NkGameObjectFactory.cpp
// =============================================================================
#include "Nkentseu/ECS/Factory/NkGameObjectFactory.h"
#include "Nkentseu/ECS/Prefab/NkPrefab.h"

namespace nkentseu {

    ecs::NkGameObject
    NkGameObjectFactory::InstantiatePrefab(ecs::NkWorld& world,
                                           const char* prefabPath,
                                           const char* instanceName) noexcept
    {
        if (!prefabPath) return ecs::NkGameObject{};

        const NkPrefab* prefab = NkPrefabRegistry::Global().Get(prefabPath);
        if (!prefab) {
            // Log d'erreur via NkLog si disponible
            return ecs::NkGameObject{};  // Handle invalide
        }

        return prefab->InstantiateGameObject(world, instanceName);
    }

    void NkGameObjectFactory::InstantiatePrefabBatch(
        ecs::NkWorld& world,
        const char* prefabPath,
        uint32 count,
        NkVector<ecs::NkGameObject>& out) noexcept
    {
        if (!prefabPath || count == 0) return;

        const NkPrefab* prefab = NkPrefabRegistry::Global().Get(prefabPath);
        if (!prefab) return;

        out.Reserve(out.Size() + count);
        for (uint32 i = 0; i < count; ++i) {
            char nameBuf[256];
            NkSNPrintf(nameBuf, sizeof(nameBuf), "Instance_%u", i);
            out.PushBack(prefab->InstantiateGameObject(world, nameBuf));
        }
    }

} // namespace nkentseu
