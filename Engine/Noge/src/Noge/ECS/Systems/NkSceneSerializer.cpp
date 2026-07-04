#include "NkSceneSerializer.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

    // Helpers fichier : écrit/lit une NkArchive avec le format choisi (binaire ou
    // texte). Binaire (NK_NATIVE/NK_BINARY) = compact/rapide ; texte = JSON/XML/YAML.
    namespace {
        bool WriteArchiveToFile(const NkArchive& archive, const char* path,
                                NkSerializationFormat fmt) noexcept {
            NkFile file;
            if (!file.Open(path, NkFileMode::NK_WRITE_BINARY)) return false;
            bool ok = false;
            if (fmt == NkSerializationFormat::NK_BINARY ||
                fmt == NkSerializationFormat::NK_NATIVE) {
                NkVector<nk_uint8> data;
                if (SerializeArchiveBinary(archive, fmt, data, nullptr))
                    ok = (file.Write(data.Data(), data.Size()) == data.Size());
            } else {
                NkString text;
                if (SerializeArchiveText(archive, fmt, text, true, nullptr))
                    ok = (file.Write(text.CStr(), text.Length()) == text.Length());
            }
            file.Close();
            return ok;
        }

        bool ReadArchiveFromFile(NkArchive& out, const char* path,
                                 NkSerializationFormat fmt) noexcept {
            NkFile file;
            if (!file.Open(path, NkFileMode::NK_READ_BINARY)) return false;
            file.SeekToEnd();
            const nk_int64 size = file.Tell();
            file.SeekToBegin();
            if (size <= 0) { file.Close(); return false; }
            NkVector<nk_uint8> buf;
            buf.Resize((nk_size)size);
            const usize read = file.Read(buf.Data(), (usize)size);
            file.Close();
            if (read != (usize)size) return false;
            if (fmt == NkSerializationFormat::NK_BINARY ||
                fmt == NkSerializationFormat::NK_NATIVE) {
                return DeserializeArchiveBinary(buf.Data(), (nk_size)size, fmt, out, nullptr);
            }
            NkString text(reinterpret_cast<const char*>(buf.Data()),
                          (NkString::SizeType)size);
            return DeserializeArchiveText(text.CStr(), fmt, out, nullptr);
        }
    } // namespace

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

            if (!WriteArchiveToFile(archive, path, mFormat)) {
                logger.Errorf("[NkSceneSerializer] Écriture échouée: {}\n", path);
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
                .Query<const NkName>()
                .ForEach([&](NkEntityId id, const NkName& /*n*/) {
                    NkArchive entityArc;
                    if (SerializeEntity(id, world, entityArc)) {
                        NkString key = NkFormat("entity_{}", entityIdx++);
                        out.SetObject(key.CStr(), entityArc);
                    }
                });

            out.SetInt64("entityCount", (nk_int64)entityIdx);
            return true;
        }

        bool NkSceneSerializer::SerializeEntity(NkEntityId id,
                                                const NkWorld& world,
                                                NkArchive& out) const noexcept {
            out.SetInt64("id_index", (nk_int64)id.index);
            out.SetInt64("id_gen",   (nk_int64)id.gen);

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
            NkArchive archive;
            if (!ReadArchiveFromFile(archive, path, mFormat)) {
                logger.Errorf("[NkSceneSerializer] Lecture échouée: {}\n", path);
                return false;
            }
            return LoadFromArchive(scene, archive);
        }

        bool NkSceneSerializer::LoadFromArchive(NkSceneGraph& scene,
                                                const NkArchive& archive) const noexcept {
            nk_int64 entityCount = 0;
            archive.GetInt64("entityCount", entityCount);

            for (nk_int64 i = 0; i < entityCount; ++i) {
                NkString key = NkFormat("entity_{}", i);
                NkArchive entityArc;
                if (!archive.GetObject(key.CStr(), entityArc)) continue;
                DeserializeEntity(scene, entityArc);
            }

            logger.Infof("[NkSceneSerializer] {} entités chargées depuis archive\n",
                         entityCount);
            return true;
        }

        bool NkSceneSerializer::DeserializeEntity(NkSceneGraph& scene,
                                                  const NkArchive& arc) const noexcept {
            nk_int64 idx = 0, gen = 0;
            arc.GetInt64("id_index", idx);
            arc.GetInt64("id_gen",   gen);

            // Crée un nœud dans la scène
            // Le nom est récupéré depuis le composant NkNameComponent dans l'archive
            NkString name = "Entity";
            NkArchive nameArc;
            if (arc.GetObject("NkNameComponent", nameArc)) {
                NkString n;
                nameArc.GetString("name", n);
                if (!n.Empty()) name = n;
            }

            NkEntityId id = scene.SpawnNode(name.CStr());

            auto& reg = Registry::Get();
            for (nk_uint32 i = 0; i < reg.count; ++i) {
                const auto& cs = reg.entries[i];
                if (!cs.typeName || !cs.deserialize || !cs.addDefault) continue;

                NkArchive compArc;
                if (!arc.GetObject(cs.typeName, compArc)) continue;

                // Ajouter le composant par défaut puis désérialiser
                cs.addDefault(scene.World(), id);
                void* ptr = nullptr; // TODO : obtenir via NkWorld::GetRaw(id, typeId)
                if (ptr) cs.deserialize(ptr, compArc);
            }

            return true;
        }

    } // namespace ecs
} // namespace nkentseu
