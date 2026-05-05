#pragma once
// =============================================================================
// NKECS/Serialization/NkSceneSerializer.h
// =============================================================================
// Sérialise et désérialise une NkSceneGraph complète en JSON.
//
// Format .nkscene :
//   {
//     "version": 1,
//     "name": "MainLevel",
//     "entities": [
//       {
//         "id": 42,
//         "components": {
//           "NkTransformComponent": { "position": [0,0,0], "scale": [1,1,1], ... },
//           "NkNameComponent": { "name": "Player" },
//           "NkMeshComponent": { "meshPath": "Assets/cube.nkmesh" }
//         }
//       }
//     ]
//   }
//
// Lien avec NkISerializable :
//   Chaque composant peut implémenter NkISerializable pour définir
//   son propre Serialize/Deserialize. Si non implémenté, la réflexion
//   NkReflect est utilisée comme fallback automatique.
//
// Usage :
//   NkSceneSerializer s;
//   s.Save(sceneGraph, "Levels/MainLevel.nkscene");
//   s.Load(sceneGraph, "Levels/MainLevel.nkscene");
// =============================================================================

#include "NKECS/World/NkWorld.h"
#include "NKECS/Scene/NkSceneGraph.h"
#include "NKSerialization/NkSerializer.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // NkComponentSerializer — interface de sérialisation d'un composant
        // =====================================================================
        // Chaque composant enregistre un sérialiseur via
        // NkSceneSerializer::RegisterComponentSerializer<T>().
        // Si non enregistré, le composant est ignoré à la sérialisation.
        struct NkComponentSerializer {
            using SerializeFn   = bool(*)(const void* comp, NkArchive& out);
            using DeserializeFn = bool(*)(void* comp, const NkArchive& in);
            using AddFn         = void(*)(NkWorld& world, NkEntityId id);

            const char*   typeName     = nullptr;
            SerializeFn   serialize    = nullptr;
            DeserializeFn deserialize  = nullptr;
            AddFn         addDefault   = nullptr; // crée le composant par défaut
        };

        // =====================================================================
        // NkSceneSerializer
        // =====================================================================
        class NkSceneSerializer {
        public:
            NkSceneSerializer() = default;

            // ── Enregistrement des types de composants ────────────────────────
            // À appeler une fois au démarrage pour chaque composant sérialisable.
            //
            // Exemple :
            //   NkSceneSerializer::RegisterComponentSerializer<NkTransformComponent>(
            //       "NkTransformComponent",
            //       &SerializeTransform,
            //       &DeserializeTransform,
            //       &AddDefaultTransform);
            static void RegisterComponentSerializer(
                const NkComponentSerializer& cs) noexcept;

            // Template helper — auto-enregistrement via NK_SERIALIZE_COMPONENT macro
            template<typename T>
            static void RegisterComponentSerializer(
                const char* name,
                NkComponentSerializer::SerializeFn   sfn,
                NkComponentSerializer::DeserializeFn dfn,
                NkComponentSerializer::AddFn         addFn) noexcept {
                NkComponentSerializer cs;
                cs.typeName    = name;
                cs.serialize   = sfn;
                cs.deserialize = dfn;
                cs.addDefault  = addFn;
                RegisterComponentSerializer(cs);
            }

            // ── Sauvegarde ────────────────────────────────────────────────────
            bool Save(const NkSceneGraph& scene, const char* path) const noexcept;

            // Version vers archive en mémoire (pour réseau, undo/redo, etc.)
            bool SaveToArchive(const NkSceneGraph& scene,
                               NkArchive& outArchive) const noexcept;

            // ── Chargement ────────────────────────────────────────────────────
            // Reconstruit la scène depuis un fichier .nkscene.
            // Les entités existantes dans la scène sont préservées.
            // Utiliser scene.World().FlushDeferred() + recréer la scène vide
            // si tu veux un chargement propre.
            bool Load(NkSceneGraph& scene, const char* path) const noexcept;

            bool LoadFromArchive(NkSceneGraph& scene,
                                 const NkArchive& archive) const noexcept;

        private:
            bool SerializeEntity(NkEntityId id,
                                 const NkWorld& world,
                                 NkArchive& entityArchive) const noexcept;

            bool DeserializeEntity(NkSceneGraph& scene,
                                   const NkArchive& entityArchive) const noexcept;

            // Registre global (singleton partagé)
            struct Registry {
                static constexpr nk_uint32 kMax = 128;
                NkComponentSerializer entries[kMax] = {};
                nk_uint32             count         = 0;

                static Registry& Get() noexcept {
                    static Registry r;
                    return r;
                }

                const NkComponentSerializer* Find(const char* name) const noexcept {
                    for (nk_uint32 i = 0; i < count; ++i)
                        if (NkStrEqual(entries[i].typeName, name)) return &entries[i];
                    return nullptr;
                }
            };
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// Macro d'enregistrement automatique — à placer dans le .cpp du composant
// =============================================================================
//
// Usage :
//   NK_SERIALIZE_COMPONENT(NkTransformComponent,
//     [](const void* c, NkArchive& a) -> bool {
//         auto& t = *static_cast<const NkTransformComponent*>(c);
//         a.SetFloat3("position", t.position.x, t.position.y, t.position.z);
//         a.SetFloat4("rotation", t.rotation.x, t.rotation.y,
//                                 t.rotation.z, t.rotation.w);
//         a.SetFloat3("scale",    t.scale.x,    t.scale.y,    t.scale.z);
//         return true;
//     },
//     [](void* c, const NkArchive& a) -> bool {
//         auto& t = *static_cast<NkTransformComponent*>(c);
//         float px,py,pz; a.GetFloat3("position",px,py,pz);
//         t.SetPosition(px,py,pz);
//         return true;
//     },
//     [](NkWorld& w, NkEntityId id) { w.Add<NkTransformComponent>(id); }
//   );
//
#define NK_SERIALIZE_COMPONENT(Type, SerializeFn, DeserializeFn, AddFn) \
    static struct _NkSerializeAutoReg_##Type { \
        _NkSerializeAutoReg_##Type() noexcept { \
            nkentseu::ecs::NkSceneSerializer::RegisterComponentSerializer<Type>( \
                #Type, SerializeFn, DeserializeFn, AddFn); \
        } \
    } _nk_serialize_autoreg_##Type
