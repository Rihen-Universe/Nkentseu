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
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Noge/ECS/NkEcsUtil.h"			  // NkStrEqual
#include "NKSerialization/NkSerializer.h" // NkArchive, NkSerializationFormat, Serialize/Deserialize
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ecs {

		// =====================================================================
		// NkComponentSerializer — interface de sérialisation d'un composant
		// =====================================================================
		// Chaque composant enregistre un sérialiseur via
		// NkSceneSerializer::RegisterComponentSerializer<T>().
		// Si non enregistré, le composant est ignoré à la sérialisation.
		struct NkComponentSerializer {
				// CONTRAT CHANGE LE 2026-09-13, et c'est ce qui rend ce serialiseur
				// UTILISABLE. L'ancienne signature prenait un `void *comp` -- le
				// pointeur du composant -- et le corps ne pouvait pas l'obtenir :
				// `NkWorld` n'expose AUCUN acces type-efface (pas de GetRaw(id,
				// typeId)). Le TODO qui le reclamait ne pouvait donc pas etre leve
				// sans ajouter l'effacement de type a NKECS, un module fondation.
				// On passe desormais le MONDE et l'ENTITE : chaque serialiseur fait
				// son propre `world.Get<T>(id)`, typé, sans effacement. Le format
				// du fichier ne change pas d'un octet.
				using SerializeFn = bool (*)(const NkWorld &world, NkEntityId id, NkArchive &out);
				using DeserializeFn = bool (*)(NkWorld &world, NkEntityId id, const NkArchive &in);

				// DOIT etre un litteral : le registre garde le POINTEUR, pas la chaine.
				const char *typeName = nullptr;
				SerializeFn serialize = nullptr;
				DeserializeFn deserialize = nullptr;
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
				static void RegisterComponentSerializer(const NkComponentSerializer &cs) noexcept;

				// Template helper — auto-enregistrement via NK_SERIALIZE_COMPONENT macro
				static void RegisterComponentSerializer(const char *name,
												NkComponentSerializer::SerializeFn sfn,
												NkComponentSerializer::DeserializeFn dfn) noexcept {
					NkComponentSerializer cs;
					cs.typeName = name;
					cs.serialize = sfn;
					cs.deserialize = dfn;
					RegisterComponentSerializer(cs);
				}

				// ── Sauvegarde ────────────────────────────────────────────────────
				bool Save(const NkSceneGraph &scene, const char *path) const noexcept;

				// Version vers archive en mémoire (pour réseau, undo/redo, etc.)
				bool SaveToArchive(const NkSceneGraph &scene, NkArchive &outArchive) const noexcept;

				// ── Chargement ────────────────────────────────────────────────────
				// Reconstruit la scène depuis un fichier .nkscene.
				// Les entités existantes dans la scène sont préservées.
				// Utiliser scene.World().FlushDeferred() + recréer la scène vide
				// si tu veux un chargement propre.
				bool Load(NkSceneGraph &scene, const char *path) const noexcept;

				bool LoadFromArchive(NkSceneGraph &scene, const NkArchive &archive) const noexcept;

				// ── Format de sérialisation (configurable) ────────────────────────
				// Défaut : NK_NATIVE (binaire propriétaire optimisé). JSON/XML/YAML
				// disponibles pour debug/interop/versioning lisible.
				void SetFormat(NkSerializationFormat fmt) noexcept {
					mFormat = fmt;
				}

				[[nodiscard]] NkSerializationFormat GetFormat() const noexcept {
					return mFormat;
				}

			private:
				NkSerializationFormat mFormat = NkSerializationFormat::NK_NATIVE;

				bool SerializeEntity(NkEntityId id, const NkWorld &world, NkArchive &entityArchive) const noexcept;

				// Rend l'identifiant NEUF de l'entite creee : la deuxieme passe en a
				// besoin pour traduire les references de parent.
				NkEntityId DeserializeEntityId(NkSceneGraph &scene, const NkArchive &entityArchive) const noexcept;

				// Registre global (singleton partagé)
				struct Registry {
						static constexpr nk_uint32 kMax = 128;
						NkComponentSerializer entries[kMax] = {};
						nk_uint32 count = 0;

						static Registry &Get() noexcept {
							static Registry r;
							return r;
						}

						const NkComponentSerializer *Find(const char *name) const noexcept {
							for (nk_uint32 i = 0; i < count; ++i)
								if (NkStrEqual(entries[i].typeName, name))
									return &entries[i];
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
#define NK_SERIALIZE_COMPONENT(Type, SerializeFn, DeserializeFn, AddFn)                                                \
	static struct _NkSerializeAutoReg_##Type {                                                                         \
			_NkSerializeAutoReg_##Type() noexcept {                                                                    \
				nkentseu::ecs::NkSceneSerializer::RegisterComponentSerializer<Type>(#Type, SerializeFn, DeserializeFn, \
																					AddFn);                            \
			}                                                                                                          \
	} _nk_serialize_autoreg_##Type
