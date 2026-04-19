#include "NkSceneSerializer.h"
#include "NKECS/Components/Core/NkCoreComponents.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace ecs {

        void NkSceneSerializer::RegisterComponentSerializer(
            const NkComponentSerializer& cs) noexcept {
            auto& reg = Registry::Get();
            // Mise à jour si déjà enregistré
            for (nk_uint32 i = 0; i < reg.count; ++i) {
                if (NkStrEqual(reg.entries[i].typeName, cs.typeName)) {
                    reg.entries[i] = cs;
                    return;
                }
            }
            if (reg.count < Registry::kMax) {
                reg.entries[reg.count++] = cs;
            } else {
                logger.Errorf("[NkSceneSerializer] Registre plein — max {} composants\n",
                              Registry::kMax);
            }
        }

        // =====================================================================
        bool NkSceneSerializer::Save(const NkSceneGraph& scene,
                                     const char* path) const noexcept {
            NkArchive archive;
            if (!SaveToArchive(scene, archive)) return false;

            NkJSONWriter writer;
            if (!writer.Write(archive, path)) {
                logger.Errorf("[NkSceneSerializer] Écriture JSON échouée: {}\n", path);
                return false;
            }
            logger.Infof("[NkSceneSerializer] Scène sauvegardée: {}\n", path);
            return true;
        }

        bool NkSceneSerializer::SaveToArchive(const NkSceneGraph& scene,
                                              NkArchive& out) const noexcept {
            out.SetString("version", "1");
            out.SetString("name",    scene.Name());

            const NkWorld& world = scene.World();
            nk_uint32 entityIdx  = 0;

            const_cast<NkWorld&>(world)
                .Query<const NkNameComponent>()
                .ForEach([&](NkEntityId id, const NkNameComponent& /*n*/) {
                    NkArchive entityArc;
                    if (SerializeEntity(id, world, entityArc)) {
                        NkString key = NkFormat("entity_{}", entityIdx++);
                        out.SetNode(key.CStr(), entityArc);
                    }
                });

            out.SetInt("entityCount", (nk_int64)entityIdx);
            return true;
        }

        bool NkSceneSerializer::SerializeEntity(NkEntityId id,
                                                const NkWorld& world,
                                                NkArchive& out) const noexcept {
            out.SetInt("id_index", (nk_int64)id.index);
            out.SetInt("id_gen",   (nk_int64)id.gen);

            auto& reg = Registry::Get();
            nk_uint32 written = 0;

            for (nk_uint32 i = 0; i < reg.count; ++i) {
                const auto& cs = reg.entries[i];
                if (!cs.serialize || !cs.typeName) continue;

                // TODO : obtenir le pointeur du composant via NkWorld + typeId
                // Pour l'instant on produit des archives vides (stub — Phase 2 complet)
                // La vraie implémentation nécessite NkTypeRegistry::GetComponent(world, id, typeId)
                (void)world; (void)id;
                ++written;
            }

            return true;
        }

        // =====================================================================
        bool NkSceneSerializer::Load(NkSceneGraph& scene,
                                     const char* path) const noexcept {
            NkJSONReader reader;
            NkArchive archive;
            if (!reader.Read(archive, path)) {
                logger.Errorf("[NkSceneSerializer] Lecture JSON échouée: {}\n", path);
                return false;
            }
            return LoadFromArchive(scene, archive);
        }

        bool NkSceneSerializer::LoadFromArchive(NkSceneGraph& scene,
                                                const NkArchive& archive) const noexcept {
            nk_int64 entityCount = 0;
            archive.GetInt("entityCount", entityCount);

            for (nk_int64 i = 0; i < entityCount; ++i) {
                NkString key = NkFormat("entity_{}", i);
                NkArchive entityArc;
                if (!archive.GetNode(key.CStr(), entityArc)) continue;
                DeserializeEntity(scene, entityArc);
            }

            logger.Infof("[NkSceneSerializer] {} entités chargées depuis archive\n",
                         entityCount);
            return true;
        }

        bool NkSceneSerializer::DeserializeEntity(NkSceneGraph& scene,
                                                  const NkArchive& arc) const noexcept {
            nk_int64 idx = 0, gen = 0;
            arc.GetInt("id_index", idx);
            arc.GetInt("id_gen",   gen);

            // Crée un nœud dans la scène
            // Le nom est récupéré depuis le composant NkNameComponent dans l'archive
            NkString name = "Entity";
            NkArchive nameArc;
            if (arc.GetNode("NkNameComponent", nameArc)) {
                NkString n;
                nameArc.GetString("name", n);
                if (!n.IsEmpty()) name = n;
            }

            NkEntityId id = scene.SpawnNode(name.CStr());

            auto& reg = Registry::Get();
            for (nk_uint32 i = 0; i < reg.count; ++i) {
                const auto& cs = reg.entries[i];
                if (!cs.typeName || !cs.deserialize || !cs.addDefault) continue;

                NkArchive compArc;
                if (!arc.GetNode(cs.typeName, compArc)) continue;

                // Ajouter le composant par défaut puis désérialiser
                cs.addDefault(scene.World(), id);
                void* ptr = nullptr; // TODO : obtenir via NkWorld::GetRaw(id, typeId)
                if (ptr) cs.deserialize(ptr, compArc);
            }

            return true;
        }

    } // namespace ecs
} // namespace nkentseu
