#include "NkSceneSerializer.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h" // serialiseurs standard (2026-09-30)
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

#include <cstdio>

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

			auto &reg = Registry::Get();
			nk_uint32 written = 0;

			for (nk_uint32 i = 0; i < reg.count; ++i) {
				const auto &cs = reg.entries[i];
				if (!cs.serialize || !cs.typeName)
					continue;

				// L'instance, par l'acces TYPE que le gabarit d'enregistrement a
				// pose (2026-09-30). Un serialiseur enregistre sans acces (forme
				// non typee de RegisterComponentSerializer) garde l'ancienne
				// conduite : il est saute -- et c'est compte, pas tu.
				if (!cs.get) {
					continue;
				}
				const void *comp = cs.get(const_cast<NkWorld &>(world), id);
				if (!comp) {
					continue; // l'entite ne porte pas ce composant
				}
				NkArchive sub;
				if (cs.serialize(comp, sub)) {
					out.SetObject(cs.typeName, sub);
					++written;
				}
			}
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

			for (nk_int64 i = 0; i < entityCount; ++i) {
				NkString key = NkFormat("entity_{}", i);
				NkArchive entityArc;
				if (!archive.GetObject(key.CStr(), entityArc))
					continue;
				DeserializeEntity(scene, entityArc);
			}

			logger.Infof("[NkSceneSerializer] {} entités chargées depuis archive\n", entityCount);
			return true;
		}

		bool NkSceneSerializer::DeserializeEntity(NkSceneGraph &scene, const NkArchive &arc) const noexcept {
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
				if (!cs.typeName || !cs.deserialize || !cs.addDefault)
					continue;

				NkArchive compArc;
				if (!arc.GetObject(cs.typeName, compArc))
					continue;

				// Ajouter le composant par défaut puis désérialiser. L'instance est
				// relue APRES l'ajout : un ajout deplace l'entite d'archetype, un
				// pointeur pris avant serait perime.
				cs.addDefault(scene.World(), id);
				void *ptr = cs.get ? cs.get(scene.World(), id) : nullptr;
				if (ptr)
					cs.deserialize(ptr, compArc);
			}

			return true;
		}

		// =====================================================================
		bool NkSceneSerializer::ReadArchiveFile(const char *path, NkArchive &out) const noexcept {
			if (path == nullptr) {
				return false;
			}
			return ReadArchiveFromFile(out, path, mFormat);
		}

		nk_uint32 NkSceneSerializer::RegisteredCount() noexcept {
			return Registry::Get().count;
		}

		// =====================================================================
		// Les serialiseurs des composants STANDARD (2026-09-30)
		// =====================================================================
		// Une cle par grandeur, lisible dans le JSON. Une cle ABSENTE a la
		// lecture laisse la valeur par defaut : un fichier ecrit avant l'ajout
		// d'un champ se relit sans erreur.
		namespace {

			void EcrireVec3(NkArchive &a, const char *prefixe, const NkVec3f &v) {
				char k[48];
				std::snprintf(k, sizeof(k), "%s_x", prefixe);
				a.SetFloat32(k, v.x);
				std::snprintf(k, sizeof(k), "%s_y", prefixe);
				a.SetFloat32(k, v.y);
				std::snprintf(k, sizeof(k), "%s_z", prefixe);
				a.SetFloat32(k, v.z);
			}

			void LireVec3(const NkArchive &a, const char *prefixe, NkVec3f &v) {
				char k[48];
				std::snprintf(k, sizeof(k), "%s_x", prefixe);
				(void)a.GetFloat32(k, v.x);
				std::snprintf(k, sizeof(k), "%s_y", prefixe);
				(void)a.GetFloat32(k, v.y);
				std::snprintf(k, sizeof(k), "%s_z", prefixe);
				(void)a.GetFloat32(k, v.z);
			}

			void EcrireCouleur(NkArchive &a, const char *prefixe, const NkColor4 &c) {
				char k[48];
				std::snprintf(k, sizeof(k), "%s_r", prefixe);
				a.SetFloat32(k, c.r);
				std::snprintf(k, sizeof(k), "%s_g", prefixe);
				a.SetFloat32(k, c.g);
				std::snprintf(k, sizeof(k), "%s_b", prefixe);
				a.SetFloat32(k, c.b);
				std::snprintf(k, sizeof(k), "%s_a", prefixe);
				a.SetFloat32(k, c.a);
			}

			void LireCouleur(const NkArchive &a, const char *prefixe, NkColor4 &c) {
				char k[48];
				std::snprintf(k, sizeof(k), "%s_r", prefixe);
				(void)a.GetFloat32(k, c.r);
				std::snprintf(k, sizeof(k), "%s_g", prefixe);
				(void)a.GetFloat32(k, c.g);
				std::snprintf(k, sizeof(k), "%s_b", prefixe);
				(void)a.GetFloat32(k, c.b);
				std::snprintf(k, sizeof(k), "%s_a", prefixe);
				(void)a.GetFloat32(k, c.a);
			}

			template <typename E> void LireEnum(const NkArchive &a, const char *cle, E &e) {
				nk_uint32 v = 0;
				if (a.GetUInt32(cle, v)) {
					e = static_cast<E>(v);
				}
			}

			void LireBool(const NkArchive &a, const char *cle, bool &b) {
				nk_bool v = b;
				if (a.GetBool(cle, v)) {
					b = v;
				}
			}

			// ── NkName ─────────────────────────────────────────────────────
			bool SerName(const void *c, NkArchive &a) {
				a.SetString("name", static_cast<const NkName *>(c)->Get());
				return true;
			}
			bool DesName(void *c, const NkArchive &a) {
				NkString n;
				if (a.GetString("name", n)) {
					static_cast<NkName *>(c)->Set(n.CStr());
				}
				return true;
			}

			// ── NkTransform (le LOCAL : le monde se recalcule) ──────────────
			bool SerTransform(const void *c, NkArchive &a) {
				const NkTransform &t = *static_cast<const NkTransform *>(c);
				EcrireVec3(a, "position", t.localPosition);
				a.SetFloat32("rotation_x", t.localRotation.x);
				a.SetFloat32("rotation_y", t.localRotation.y);
				a.SetFloat32("rotation_z", t.localRotation.z);
				a.SetFloat32("rotation_w", t.localRotation.w);
				EcrireVec3(a, "scale", t.localScale);
				return true;
			}
			bool DesTransform(void *c, const NkArchive &a) {
				NkTransform &t = *static_cast<NkTransform *>(c);
				LireVec3(a, "position", t.localPosition);
				(void)a.GetFloat32("rotation_x", t.localRotation.x);
				(void)a.GetFloat32("rotation_y", t.localRotation.y);
				(void)a.GetFloat32("rotation_z", t.localRotation.z);
				(void)a.GetFloat32("rotation_w", t.localRotation.w);
				LireVec3(a, "scale", t.localScale);
				t.worldDirty = true;
				return true;
			}

			// ── NkMeshComponent (le CHEMIN, pas le handle) ──────────────────
			bool SerMesh(const void *c, NkArchive &a) {
				const NkMeshComponent &m = *static_cast<const NkMeshComponent *>(c);
				a.SetString("meshPath", m.meshPath.CStr());
				a.SetUInt32("subMeshIndex", m.subMeshIndex);
				a.SetBool("visible", m.visible);
				a.SetBool("castShadow", m.castShadow);
				a.SetBool("receiveShadow", m.receiveShadow);
				return true;
			}
			bool DesMesh(void *c, const NkArchive &a) {
				NkMeshComponent &m = *static_cast<NkMeshComponent *>(c);
				(void)a.GetString("meshPath", m.meshPath);
				(void)a.GetUInt32("subMeshIndex", m.subMeshIndex);
				LireBool(a, "visible", m.visible);
				LireBool(a, "castShadow", m.castShadow);
				LireBool(a, "receiveShadow", m.receiveShadow);
				m.meshHandle = 0; // recree au premier rendu
				return true;
			}

			// ── NkMaterialComponent (chemins et couleurs, pas les handles) ──
			bool SerMaterial(const void *c, NkArchive &a) {
				const NkMaterialComponent &m = *static_cast<const NkMaterialComponent *>(c);
				a.SetUInt32("slotCount", m.slotCount);
				for (nk_uint32 i = 0; i < m.slotCount && i < NkMaterialComponent::kMaxSlots; ++i) {
					NkArchive slot;
					slot.SetString("materialPath", m.slots[i].materialPath.CStr());
					EcrireCouleur(slot, "albedo", m.slots[i].albedo);
					slot.SetFloat32("metallic", m.slots[i].metallic);
					slot.SetFloat32("roughness", m.slots[i].roughness);
					char k[16];
					std::snprintf(k, sizeof(k), "slot_%u", static_cast<unsigned>(i));
					a.SetObject(k, slot);
				}
				return true;
			}
			bool DesMaterial(void *c, const NkArchive &a) {
				NkMaterialComponent &m = *static_cast<NkMaterialComponent *>(c);
				nk_uint32 n = 0;
				(void)a.GetUInt32("slotCount", n);
				m.slotCount = n < NkMaterialComponent::kMaxSlots ? n : NkMaterialComponent::kMaxSlots;
				for (nk_uint32 i = 0; i < m.slotCount; ++i) {
					char k[16];
					std::snprintf(k, sizeof(k), "slot_%u", static_cast<unsigned>(i));
					NkArchive slot;
					if (!a.GetObject(k, slot)) {
						continue;
					}
					(void)slot.GetString("materialPath", m.slots[i].materialPath);
					LireCouleur(slot, "albedo", m.slots[i].albedo);
					(void)slot.GetFloat32("metallic", m.slots[i].metallic);
					(void)slot.GetFloat32("roughness", m.slots[i].roughness);
					m.slots[i].materialHandle = 0; // recree au premier rendu
				}
				return true;
			}

			// ── NkLightComponent ────────────────────────────────────────────
			bool SerLight(const void *c, NkArchive &a) {
				const NkLightComponent &l = *static_cast<const NkLightComponent *>(c);
				a.SetUInt32("type", static_cast<nk_uint32>(l.type));
				EcrireCouleur(a, "color", l.color);
				a.SetFloat32("intensity", l.intensity);
				a.SetFloat32("range", l.range);
				a.SetFloat32("innerAngle", l.innerAngle);
				a.SetFloat32("outerAngle", l.outerAngle);
				a.SetBool("castShadow", l.castShadow);
				return true;
			}
			bool DesLight(void *c, const NkArchive &a) {
				NkLightComponent &l = *static_cast<NkLightComponent *>(c);
				LireEnum(a, "type", l.type);
				LireCouleur(a, "color", l.color);
				(void)a.GetFloat32("intensity", l.intensity);
				(void)a.GetFloat32("range", l.range);
				(void)a.GetFloat32("innerAngle", l.innerAngle);
				(void)a.GetFloat32("outerAngle", l.outerAngle);
				LireBool(a, "castShadow", l.castShadow);
				return true;
			}

			// ── NkCameraComponent (les matrices se recalculent) ─────────────
			bool SerCamera(const void *c, NkArchive &a) {
				const NkCameraComponent &k = *static_cast<const NkCameraComponent *>(c);
				a.SetUInt32("projection", static_cast<nk_uint32>(k.projection));
				a.SetFloat32("fovDeg", k.fovDeg);
				a.SetFloat32("nearClip", k.nearClip);
				a.SetFloat32("farClip", k.farClip);
				a.SetFloat32("orthoSize", k.orthoSize);
				a.SetFloat32("aspect", k.aspect);
				a.SetInt32("priority", k.priority);
				return true;
			}
			bool DesCamera(void *c, const NkArchive &a) {
				NkCameraComponent &k = *static_cast<NkCameraComponent *>(c);
				LireEnum(a, "projection", k.projection);
				(void)a.GetFloat32("fovDeg", k.fovDeg);
				(void)a.GetFloat32("nearClip", k.nearClip);
				(void)a.GetFloat32("farClip", k.farClip);
				(void)a.GetFloat32("orthoSize", k.orthoSize);
				(void)a.GetFloat32("aspect", k.aspect);
				(void)a.GetInt32("priority", k.priority);
				return true;
			}

			// ── NkRigidbody3D (l'etat : les forces accumulees ne s'ecrivent pas)
			bool SerRigidbody(const void *c, NkArchive &a) {
				const NkRigidbody3D &r = *static_cast<const NkRigidbody3D *>(c);
				a.SetUInt32("bodyType", static_cast<nk_uint32>(r.bodyType));
				a.SetFloat32("mass", r.mass);
				a.SetFloat32("gravityScale", r.gravityScale);
				a.SetFloat32("drag", r.drag);
				a.SetFloat32("angularDrag", r.angularDrag);
				a.SetBool("freezeRotX", r.freezeRotX);
				a.SetBool("freezeRotY", r.freezeRotY);
				a.SetBool("freezeRotZ", r.freezeRotZ);
				a.SetBool("freezePosX", r.freezePosX);
				a.SetBool("freezePosY", r.freezePosY);
				a.SetBool("freezePosZ", r.freezePosZ);
				a.SetFloat32("friction", r.friction);
				a.SetFloat32("restitution", r.restitution);
				EcrireVec3(a, "velocity", r.velocity);
				EcrireVec3(a, "angularVelocity", r.angularVelocity);
				return true;
			}
			bool DesRigidbody(void *c, const NkArchive &a) {
				NkRigidbody3D &r = *static_cast<NkRigidbody3D *>(c);
				LireEnum(a, "bodyType", r.bodyType);
				(void)a.GetFloat32("mass", r.mass);
				(void)a.GetFloat32("gravityScale", r.gravityScale);
				(void)a.GetFloat32("drag", r.drag);
				(void)a.GetFloat32("angularDrag", r.angularDrag);
				LireBool(a, "freezeRotX", r.freezeRotX);
				LireBool(a, "freezeRotY", r.freezeRotY);
				LireBool(a, "freezeRotZ", r.freezeRotZ);
				LireBool(a, "freezePosX", r.freezePosX);
				LireBool(a, "freezePosY", r.freezePosY);
				LireBool(a, "freezePosZ", r.freezePosZ);
				(void)a.GetFloat32("friction", r.friction);
				(void)a.GetFloat32("restitution", r.restitution);
				LireVec3(a, "velocity", r.velocity);
				LireVec3(a, "angularVelocity", r.angularVelocity);
				return true;
			}

			// ── NkCollider3D (la forme ; le corps physique est recree) ──────
			bool SerCollider(const void *c, NkArchive &a) {
				const NkCollider3D &k = *static_cast<const NkCollider3D *>(c);
				a.SetUInt32("shape", static_cast<nk_uint32>(k.shape));
				EcrireVec3(a, "center", k.center);
				a.SetBool("isTrigger", k.isTrigger);
				a.SetUInt32("layer", k.layer);
				a.SetUInt32("layerMask", k.layerMask);
				a.SetFloat32("friction", k.friction);
				a.SetFloat32("restitution", k.restitution);
				a.SetBool("enabled", k.enabled);
				EcrireVec3(a, "boxSize", k.boxSize);
				a.SetFloat32("sphereRadius", k.sphereRadius);
				a.SetFloat32("capsuleRadius", k.capsuleRadius);
				a.SetFloat32("capsuleHeight", k.capsuleHeight);
				a.SetUInt32("capsuleDir", static_cast<nk_uint32>(k.capsuleDir));
				return true;
			}
			bool DesCollider(void *c, const NkArchive &a) {
				NkCollider3D &k = *static_cast<NkCollider3D *>(c);
				LireEnum(a, "shape", k.shape);
				LireVec3(a, "center", k.center);
				LireBool(a, "isTrigger", k.isTrigger);
				(void)a.GetUInt32("layer", k.layer);
				(void)a.GetUInt32("layerMask", k.layerMask);
				(void)a.GetFloat32("friction", k.friction);
				(void)a.GetFloat32("restitution", k.restitution);
				LireBool(a, "enabled", k.enabled);
				LireVec3(a, "boxSize", k.boxSize);
				(void)a.GetFloat32("sphereRadius", k.sphereRadius);
				(void)a.GetFloat32("capsuleRadius", k.capsuleRadius);
				(void)a.GetFloat32("capsuleHeight", k.capsuleHeight);
				LireEnum(a, "capsuleDir", k.capsuleDir);
				k.physicsBodyId = 0; // recree au prochain pas physique
				k.physicsShapeId = 0;
				return true;
			}

		} // namespace

		void NkSceneSerializer::RegisterCoreComponents() noexcept {
			// RegisterComponentSerializer remplace une entree du meme nom : appeler
			// deux fois ne double rien.
			RegisterComponentSerializer<NkName>("NkNameComponent", &SerName, &DesName, nullptr);
			RegisterComponentSerializer<NkTransform>("NkTransform", &SerTransform, &DesTransform, nullptr);
			RegisterComponentSerializer<NkMeshComponent>("NkMeshComponent", &SerMesh, &DesMesh, nullptr);
			RegisterComponentSerializer<NkMaterialComponent>("NkMaterialComponent", &SerMaterial, &DesMaterial,
															 nullptr);
			RegisterComponentSerializer<NkLightComponent>("NkLightComponent", &SerLight, &DesLight, nullptr);
			RegisterComponentSerializer<NkCameraComponent>("NkCameraComponent", &SerCamera, &DesCamera, nullptr);
			RegisterComponentSerializer<NkRigidbody3D>("NkRigidbody3D", &SerRigidbody, &DesRigidbody, nullptr);
			RegisterComponentSerializer<NkCollider3D>("NkCollider3D", &SerCollider, &DesCollider, nullptr);
		}

	} // namespace ecs
} // namespace nkentseu
