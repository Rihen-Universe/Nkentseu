#include "NkSceneSerializer.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

	// Helpers fichier : écrit/lit une NkArchive avec le format choisi (binaire ou
	// texte). Binaire (NK_NATIVE/NK_BINARY) = compact/rapide ; texte = JSON/XML/YAML.
	namespace {
		bool WriteArchiveToFile(const NkArchive &archive, const char *path, NkSerializationFormat fmt) noexcept {
			NkFile file;
			if (!file.Open(path, NkFileMode::NK_WRITE_BINARY))
				return false;
			bool ok = false;
			if (fmt == NkSerializationFormat::NK_BINARY || fmt == NkSerializationFormat::NK_NATIVE) {
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

		bool ReadArchiveFromFile(NkArchive &out, const char *path, NkSerializationFormat fmt) noexcept {
			NkFile file;
			if (!file.Open(path, NkFileMode::NK_READ_BINARY))
				return false;
			file.SeekToEnd();
			const nk_int64 size = file.Tell();
			file.SeekToBegin();
			if (size <= 0) {
				file.Close();
				return false;
			}
			NkVector<nk_uint8> buf;
			buf.Resize((nk_size)size);
			const usize read = file.Read(buf.Data(), (usize)size);
			file.Close();
			if (read != (usize)size)
				return false;
			if (fmt == NkSerializationFormat::NK_BINARY || fmt == NkSerializationFormat::NK_NATIVE) {
				return DeserializeArchiveBinary(buf.Data(), (nk_size)size, fmt, out, nullptr);
			}
			NkString text(reinterpret_cast<const char *>(buf.Data()), (NkString::SizeType)size);
			return DeserializeArchiveText(text.CStr(), fmt, out, nullptr);
		}
	} // namespace

	namespace ecs {

		void NkSceneSerializer::RegisterComponentSerializer(const NkComponentSerializer &cs) noexcept {
			auto &reg = Registry::Get();
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
				logger.Errorf("[NkSceneSerializer] Registre plein — max {} composants\n", Registry::kMax);
			}
		}

		// =====================================================================
		bool NkSceneSerializer::Save(const NkSceneGraph &scene, const char *path) const noexcept {
			NkArchive archive;
			if (!SaveToArchive(scene, archive))
				return false;

			if (!WriteArchiveToFile(archive, path, mFormat)) {
				logger.Errorf("[NkSceneSerializer] Écriture échouée: {}\n", path);
				return false;
			}
			logger.Infof("[NkSceneSerializer] Scène sauvegardée: {}\n", path);
			return true;
		}

		bool NkSceneSerializer::SaveToArchive(const NkSceneGraph &scene, NkArchive &out) const noexcept {
			out.SetString("version", "1");
			out.SetString("name", scene.Name());

			const NkWorld &world = scene.World();
			nk_uint32 entityIdx = 0;

			const_cast<NkWorld &>(world).Query<const NkName>().ForEach([&](NkEntityId id, const NkName & /*n*/) {
				NkArchive entityArc;
				if (SerializeEntity(id, world, entityArc)) {
					NkString key = NkFormat("entity_{}", entityIdx++);
					out.SetObject(key.CStr(), entityArc);
				}
			});

			out.SetInt64("entityCount", (nk_int64)entityIdx);
			return true;
		}

		bool NkSceneSerializer::SerializeEntity(NkEntityId id, const NkWorld &world, NkArchive &out) const noexcept {
			out.SetInt64("id_index", (nk_int64)id.index);
			out.SetInt64("id_gen", (nk_int64)id.gen);

			// ── LA HIERARCHIE N'EST PAS UN COMPOSANT COMME LES AUTRES ────
			// Elle porte une REFERENCE D'ENTITE, et une reference ne se relit
			// pas comme un nombre : l'identifiant qu'on ecrit ici ne sera pas
			// celui que le monde attribuera a la relecture. On ecrit donc
			// l'identifiant SAUVEGARDE du parent, et la relecture le traduit
			// (deuxieme passe, cf. LoadFromArchive). `NkChildren` n'est PAS
			// ecrit : il derive de NkParent, et `SetParent` le reconstruit.
			// Sauver les deux, c'est se donner deux verites a contredire.
			if (const NkParent *p = const_cast<NkWorld &>(world).Get<NkParent>(id)) {
				if (p->entity.IsValid()) {
					out.SetInt64("parent_index", (nk_int64)p->entity.index);
					out.SetInt64("parent_gen", (nk_int64)p->entity.gen);
				}
			}

			auto &reg = Registry::Get();
			nk_uint32 written = 0;

			for (nk_uint32 i = 0; i < reg.count; ++i) {
				const auto &cs = reg.entries[i];
				if (!cs.serialize || !cs.typeName)
					continue;
				// Chaque serialiseur fait son propre Get<T> typé et rend FAUX si
				// l'entite ne porte pas ce composant : une entite sans maillage
				// n'ecrit pas de bloc maillage vide.
				NkArchive compArc;
				if (!cs.serialize(world, id, compArc))
					continue;
				out.SetObject(cs.typeName, compArc);
				++written;
			}

			// UN COMPTE ECRIT DANS LE FICHIER. Sans lui, une archive qui perd
			// tous ses composants se relit sans une seule erreur -- c'est
			// exactement ce que faisait la version precedente, qui rendait
			// `true` apres n'avoir rien ecrit.
			out.SetInt64("componentCount", (nk_int64)written);
			return true;
		}

		// =====================================================================
		bool NkSceneSerializer::Load(NkSceneGraph &scene, const char *path) const noexcept {
			NkArchive archive;
			if (!ReadArchiveFromFile(archive, path, mFormat)) {
				logger.Errorf("[NkSceneSerializer] Lecture échouée: {}\n", path);
				return false;
			}
			return LoadFromArchive(scene, archive);
		}

		bool NkSceneSerializer::LoadFromArchive(NkSceneGraph &scene, const NkArchive &archive) const noexcept {
			nk_int64 entityCount = 0;
			archive.GetInt64("entityCount", entityCount);

			// ── DEUX PASSES, ET C'EST LA HIERARCHIE QUI L'EXIGE ──────────
			// Deux dangers, un seul remede. (1) L'ORDRE : rien ne garantit
			// qu'un parent soit ecrit avant son enfant — l'ecriture parcourt
			// les archetypes, pas l'arbre. (2) LES IDENTIFIANTS : celui qu'on
			// relit n'est pas celui que le monde attribue, qui depend des
			// entites deja vivantes.
			// Passe 1 : creer TOUTES les entites et noter sauvegarde -> neuf.
			// Passe 2 : rattacher, en traduisant par cette table. Un enfant
			// ecrit avant son parent se rattache donc sans difficulte : a la
			// passe 2, tout le monde existe.
			struct Corresp {
					nk_int64 vieuxIndex = 0, vieuxGen = 0;
					NkEntityId neuf{};
					nk_int64 parentIndex = -1, parentGen = -1;
			};
			NkVector<Corresp> table;

			for (nk_int64 i = 0; i < entityCount; ++i) {
				NkString key = NkFormat("entity_{}", i);
				NkArchive entityArc;
				if (!archive.GetObject(key.CStr(), entityArc))
					continue;
				Corresp c;
				entityArc.GetInt64("id_index", c.vieuxIndex);
				entityArc.GetInt64("id_gen", c.vieuxGen);
				entityArc.GetInt64("parent_index", c.parentIndex);
				entityArc.GetInt64("parent_gen", c.parentGen);
				c.neuf = DeserializeEntityId(scene, entityArc);
				if (c.neuf.IsValid())
					table.PushBack(c);
			}

			nk_uint32 rattachees = 0;
			for (nk_uint32 a = 0; a < (nk_uint32)table.Size(); ++a) {
				if (table[a].parentIndex < 0)
					continue; // racine : pas de parent a traduire
				for (nk_uint32 b = 0; b < (nk_uint32)table.Size(); ++b) {
					if (table[b].vieuxIndex == table[a].parentIndex &&
						table[b].vieuxGen == table[a].parentGen) {
						scene.SetParent(table[a].neuf, table[b].neuf);
						++rattachees;
						break;
					}
				}
			}

			logger.Infof("[NkSceneSerializer] {} entites chargees, {} rattachees a un parent\n",
							 (nk_int64)table.Size(), (nk_int64)rattachees);
			return true;
		}

		NkEntityId NkSceneSerializer::DeserializeEntityId(NkSceneGraph &scene, const NkArchive &arc) const noexcept {
			nk_int64 idx = 0, gen = 0;
			arc.GetInt64("id_index", idx);
			arc.GetInt64("id_gen", gen);

			// Crée un nœud dans la scène
			// Le nom est récupéré depuis le composant NkNameComponent dans l'archive
			NkString name = "Entity";
			NkArchive nameArc;
			if (arc.GetObject("NkNameComponent", nameArc)) {
				NkString n;
				nameArc.GetString("name", n);
				if (!n.Empty())
					name = n;
			}

			NkEntityId id = scene.SpawnNode(name.CStr());

			auto &reg = Registry::Get();
			for (nk_uint32 i = 0; i < reg.count; ++i) {
				const auto &cs = reg.entries[i];
				if (!cs.typeName || !cs.deserialize)
					continue;

				NkArchive compArc;
				if (!arc.GetObject(cs.typeName, compArc))
					continue;

				// Le deserialiseur AJOUTE le composant puis le remplit : un seul
				// geste, donc jamais d'etat intermediaire ou le composant existe
				// et reste a sa valeur par defaut.
				cs.deserialize(scene.World(), id, compArc);
			}

			return id;
		}

	} // namespace ecs
} // namespace nkentseu
