#pragma once
// =============================================================================
// Nkentseu/ECS/Prefab/NkPrefab.h — Système de Prefabs (STL-free)
// =============================================================================
/**
 * @file NkPrefab.h
 * @brief Préfabriqués sérialisables et instanciables.
 *
 * 🔹 CORRECTIONS PAR RAPPORT À LA VERSION PRÉCÉDENTE :
 *   • std::string    → NkString   (conteneur maison)
 *   • std::vector    → NkVector   (conteneur maison)
 *   • std::unordered_map → NkUnorderedMap (conteneur maison)
 *   • std::function  → NkFunction (wrapper léger)
 *   • SUPPRESSION de la dépendance directe à NkGameObject dans l'API publique
 *     → NkPrefab::Instantiate() retourne un NkEntityId, le caller wraps avec NkGameObject
 *     → NkPrefab::InstantiateGameObject() reste disponible pour la couche Nkentseu
 *
 * 🔹 NkProfiler est DÉPLACÉ dans Nkentseu/Core/NkProfiler.h (n'a rien à faire
 *    dans Prefab.h — il était là par erreur dans la version précédente).
 *
 * 🔹 ARCHITECTURE :
 *   NkPrefab → template data-driven (composants + hiérarchie + blueprint)
 *   NkPrefabInstance → suivi runtime des overrides par instance
 *   NkPrefabRegistry → registre global singleton des prefabs chargés
 */

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "Nkentseu/ECS/Entities/NkGameObject.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Associative/NkUnorderedMap.h"

namespace nkentseu {

    // =========================================================================
    // NkPrefabComponentData — Données d'un composant sérialisé dans un prefab
    // =========================================================================
    struct NkPrefabComponentData {
        NkString typeName;        ///< Nom du type (ex: "NkTransform")
        NkString jsonValue;       ///< Données sérialisées en JSON
        bool     isOverridden = false;

        NkPrefabComponentData() noexcept = default;
        NkPrefabComponentData(const char* type, const char* json) noexcept
            : typeName(type), jsonValue(json) {}
    };

    // =========================================================================
    // NkPrefabChild — Nœud enfant dans la hiérarchie du prefab
    // =========================================================================
    struct NkPrefabChild {
        NkString  name;
        NkString  prefabPath;           ///< Prefab imbriqué (vide = nœud inline)
        NkVec3f   localPosition = {0, 0, 0};
        NkQuatf   localRotation = NkQuatf::Identity();
        NkVec3f   localScale    = {1, 1, 1};
        bool      isActive = true;

        NkPrefabChild() noexcept = default;
        NkPrefabChild(const char* n, const char* path = nullptr) noexcept
            : name(n), prefabPath(path ? path : "") {}
    };

    // =========================================================================
    // NkPrefab — Template d'entité data-driven et sérialisable
    // =========================================================================
    class NkPrefab {
        public:
            // ── Métadonnées ────────────────────────────────────────────────
            NkString name;
            NkString path;              ///< "Assets/Prefabs/Player.prefab"
            NkString version = "1.0";
            uint64   guid    = 0;

            // ── Données ────────────────────────────────────────────────────
            NkUnorderedMap<NkString, NkPrefabComponentData> components;
            NkVector<NkPrefabChild>                          children;
            NkString blueprintPath;
            uint64   tagBits = 0;
            uint32   layer   = 0;

            // ── Constructeurs ──────────────────────────────────────────────
            NkPrefab() noexcept = default;
            explicit NkPrefab(const char* prefabName) noexcept
                : name(prefabName) {
                guid = ecs::detail::FNV1a(prefabName);
            }

            // ── Builder fluide ─────────────────────────────────────────────
            template<typename T, typename... Args>
            NkPrefab& WithComponent(Args&&... args) noexcept {
                T temp{std::forward<Args>(args)...};
                // Sérialisation via NkECSSerializer (implémentation dans .cpp)
                char buf[4096];
                if (SerializeComponent(&temp, ecs::NkIdOf<T>(), buf, sizeof(buf)))
                    components[ecs::NkTypeRegistry::Global().TypeName<T>()] =
                        NkPrefabComponentData(ecs::NkTypeRegistry::Global().TypeName<T>(), buf);
                return *this;
            }

            NkPrefab& WithTag(uint64 tagBit) noexcept {
                this->tagBits |= tagBit;
                return *this;
            }

            NkPrefab& WithLayer(uint32 layerId) noexcept {
                this->layer = layerId;
                return *this;
            }

            NkPrefab& WithChild(const NkPrefabChild& child) noexcept {
                children.PushBack(child);
                return *this;
            }

            NkPrefab& WithBlueprint(const char* bpPath) noexcept {
                blueprintPath = bpPath ? bpPath : "";
                return *this;
            }

            // ── Instanciation ──────────────────────────────────────────────
            /**
             * @brief Instancie le prefab et retourne l'entité ECS racine.
             * @note Retourne NkEntityId (niveau bas) — la couche moteur wraps
             *       avec NkGameObject si besoin.
             */
            [[nodiscard]] ecs::NkEntityId
            Instantiate(ecs::NkWorld& world,
                        const char* instanceName = nullptr) const noexcept;

            /**
             * @brief Instancie et retourne directement un NkGameObject.
             * @note Couche de commodité pour Nkentseu. Appelle Instantiate() en interne.
             */
            [[nodiscard]] ecs::NkGameObject
            InstantiateGameObject(ecs::NkWorld& world,
                                  const char* instanceName = nullptr) const noexcept {
                ecs::NkEntityId id = Instantiate(world, instanceName);
                return ecs::NkGameObject(id, &world);
            }

            /**
             * @brief Instancie plusieurs copies (batch).
             */
            void InstantiateBatch(ecs::NkWorld& world,
                                  uint32 count,
                                  NkVector<ecs::NkEntityId>& out) const noexcept;

            // ── Sérialisation JSON ─────────────────────────────────────────
            [[nodiscard]] bool Serialize(char* buffer, uint32 bufSize) const noexcept;
            [[nodiscard]] bool Deserialize(const char* json) noexcept;

            // ── Helpers ────────────────────────────────────────────────────
            [[nodiscard]] bool HasComponent(const char* typeName) const noexcept {
                return components.Contains(NkString(typeName));
            }

            [[nodiscard]] const NkPrefabComponentData*
            GetComponentData(const char* typeName) const noexcept {
                return components.Find(NkString(typeName));
            }

        private:
            static bool SerializeComponent(void* data,
                                           ecs::NkComponentId cid,
                                           char* outJson,
                                           uint32 bufSize) noexcept;
    };

    // =========================================================================
    // NkPrefabInstance — Tracking runtime des overrides d'une instance
    // =========================================================================
    /**
     * @struct NkPrefabInstance
     * @brief Composant ECS attaché à une entité instanciée depuis un prefab.
     *
     * Permet de tracer les modifications par rapport au prefab source
     * (pour l'éditeur et la re-sérialisation).
     */
    struct NkPrefabInstance {
        ecs::NkEntityId rootEntity  = ecs::NkEntityId::Invalid();
        NkString        prefabPath;
        NkString        instanceName;
        bool            isOverridden = false;

        /// Overrides : "NomType.NomChamp" → valeur JSON
        NkUnorderedMap<NkString, NkString> overrides;

        /// Entités enfants de la hiérarchie instanciée
        NkUnorderedMap<NkString, ecs::NkEntityId> childEntities;

        NkPrefabInstance() noexcept = default;
        NkPrefabInstance(ecs::NkEntityId root,
                         const char* path,
                         const char* name) noexcept
            : rootEntity(root)
            , prefabPath(path ? path : "")
            , instanceName(name ? name : "") {}

        template<typename T>
        bool SetOverride(const char* fieldName, const T& /*value*/) noexcept {
            isOverridden = true;
            // TODO: sérialiser value → JSON
            (void)fieldName;
            return true;
        }

        bool RevertOverride(const char* key) noexcept {
            return overrides.Remove(NkString(key));
        }

        void RevertAll() noexcept {
            overrides.Clear();
            isOverridden = false;
        }
    };
    NK_COMPONENT(NkPrefabInstance)

    // =========================================================================
    // NkPrefabRegistry — Registre singleton des prefabs chargés
    // =========================================================================
    class NkPrefabRegistry {
        public:
            [[nodiscard]] static NkPrefabRegistry& Global() noexcept {
                static NkPrefabRegistry instance;
                return instance;
            }

            bool Register(const NkPrefab& prefab) noexcept {
                if (prefab.path.IsEmpty()) return false;
                mPrefabs.Insert(prefab.path, prefab);
                return true;
            }

            bool LoadFromFile(const char* path) noexcept;

            [[nodiscard]] const NkPrefab*
            Get(const char* path) const noexcept {
                return mPrefabs.Find(NkString(path));
            }

            [[nodiscard]] const NkPrefab*
            Get(const NkString& path) const noexcept {
                return mPrefabs.Find(path);
            }

            template<typename Fn>
            void ForEach(Fn&& fn) const noexcept {
                mPrefabs.ForEach([&](const NkString& path, const NkPrefab& prefab) {
                    fn(path.CStr(), prefab);
                });
            }

            [[nodiscard]] uint32 Count() const noexcept {
                return static_cast<uint32>(mPrefabs.Size());
            }

        private:
            NkUnorderedMap<NkString, NkPrefab> mPrefabs;
    };

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
// ── Création et enregistrement d'un prefab
void Example_CreatePrefab(nkentseu::ecs::NkWorld& world) {
    nkentseu::NkPrefab enemyPrefab("Enemy_Orc");
    enemyPrefab.path = "Assets/Prefabs/Enemy_Orc.prefab";
    enemyPrefab
        .WithComponent<nkentseu::ecs::NkTransform>()
        .WithTag(nkentseu::ecs::NkTagBits::Enemy)
        .WithLayer(1)
        .WithChild(nkentseu::NkPrefabChild("Weapon_Slot", nullptr));

    nkentseu::NkPrefabRegistry::Global().Register(enemyPrefab);

    // Instanciation via NkGameObject (couche moteur)
    auto orc = enemyPrefab.InstantiateGameObject(world, "Orc_01");
    orc.SetPosition({10, 0, 5});

    // Ou via NkGameObjectFactory pour les prefabs enregistrés
    auto orc2 = nkentseu::NkGameObjectFactory::InstantiatePrefab(
        world, "Assets/Prefabs/Enemy_Orc.prefab", "Orc_02");
}

// ── Override d'une instance
void Example_Override(nkentseu::ecs::NkWorld& world) {
    const auto* prefab = nkentseu::NkPrefabRegistry::Global().Get("Assets/Prefabs/Player.prefab");
    if (!prefab) return;

    auto player = prefab->InstantiateGameObject(world, "Player_Custom");
    // Modifier la position après instanciation
    player.SetPosition({0, 0, 10});

    // Tracker l'override via NkPrefabInstance
    if (auto inst = player.GetComponent<nkentseu::NkPrefabInstance>()) {
        inst->SetOverride("NkTransform.localPosition", player.GetPosition());
    }
}
*/
