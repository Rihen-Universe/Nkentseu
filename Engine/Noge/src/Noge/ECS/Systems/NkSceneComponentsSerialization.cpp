// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Noge/ECS/Systems/NkSceneComponentsSerialization.cpp (cf. .h)
// =============================================================================
#include "Noge/ECS/Systems/NkSceneComponentsSerialization.h"
#include "Noge/ECS/Systems/NkSceneSerializer.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include <cstdio>

namespace nkentseu {
	namespace noge {

		using namespace nkentseu::ecs;

		namespace {

			// ── NkName ────────────────────────────────────────────────────────
			bool EcrireNom(const NkWorld &w, NkEntityId id, NkArchive &out) {
				const NkName *n = const_cast<NkWorld &>(w).Get<NkName>(id);
				if (!n)
					return false; // pas ce composant : aucun bloc ecrit
				out.SetString("name", n->value);
				return true;
			}

			bool LireNom(NkWorld &w, NkEntityId id, const NkArchive &in) {
				NkString v;
				if (!in.GetString("name", v))
					return false;
				w.Add<NkName>(id, NkName(v.CStr()));
				return true;
			}

			// ── NkTransform ───────────────────────────────────────────────────
			// On ecrit les valeurs LOCALES, jamais la matrice monde : celle-ci est
			// un RESULTAT, recalcule par NkTransformSystem a partir de la hierarchie.
			// Sauvegarder un resultat, c'est le figer et le voir contredire son
			// parent au premier deplacement.
			bool EcrireTransform(const NkWorld &w, NkEntityId id, NkArchive &out) {
				const NkTransform *t = const_cast<NkWorld &>(w).Get<NkTransform>(id);
				if (!t)
					return false;
				out.SetFloat32("px", t->localPosition.x);
				out.SetFloat32("py", t->localPosition.y);
				out.SetFloat32("pz", t->localPosition.z);
				out.SetFloat32("rx", t->localRotation.x);
				out.SetFloat32("ry", t->localRotation.y);
				out.SetFloat32("rz", t->localRotation.z);
				out.SetFloat32("rw", t->localRotation.w);
				out.SetFloat32("sx", t->localScale.x);
				out.SetFloat32("sy", t->localScale.y);
				out.SetFloat32("sz", t->localScale.z);
				return true;
			}

			bool LireTransform(NkWorld &w, NkEntityId id, const NkArchive &in) {
				NkTransform t;
				float32 v = 0.f;
				if (in.GetFloat32("px", v)) t.localPosition.x = v;
				if (in.GetFloat32("py", v)) t.localPosition.y = v;
				if (in.GetFloat32("pz", v)) t.localPosition.z = v;
				if (in.GetFloat32("rx", v)) t.localRotation.x = v;
				if (in.GetFloat32("ry", v)) t.localRotation.y = v;
				if (in.GetFloat32("rz", v)) t.localRotation.z = v;
				if (in.GetFloat32("rw", v)) t.localRotation.w = v;
				if (in.GetFloat32("sx", v)) t.localScale.x = v;
				if (in.GetFloat32("sy", v)) t.localScale.y = v;
				if (in.GetFloat32("sz", v)) t.localScale.z = v;
				t.worldDirty = true; // le monde se recalcule, il ne se relit pas
				w.Add<NkTransform>(id, t);
				return true;
			}

			// ── NkMeshComponent ───────────────────────────────────────────────
			// ⚠️ On ecrit le CHEMIN, jamais `meshHandle` : la poignee est un numero
			// valable dans UN processus, attribue dans l'ordre des imports. La
			// relire dans un autre processus designerait un autre maillage, ou
			// aucun — et sans la moindre erreur.
			bool EcrireMesh(const NkWorld &w, NkEntityId id, NkArchive &out) {
				const NkMeshComponent *m = const_cast<NkWorld &>(w).Get<NkMeshComponent>(id);
				if (!m)
					return false;
				out.SetString("meshPath", m->meshPath.CStr());
				out.SetInt64("visible", m->visible ? 1 : 0);
				out.SetInt64("castShadow", m->castShadow ? 1 : 0);
				out.SetInt64("subMeshIndex", (nk_int64)m->subMeshIndex);
				return true;
			}

			bool LireMesh(NkWorld &w, NkEntityId id, const NkArchive &in) {
				NkMeshComponent m;
				NkString p;
				if (in.GetString("meshPath", p))
					m.meshPath = p;
				nk_int64 i64 = 0;
				if (in.GetInt64("visible", i64))
					m.visible = (i64 != 0);
				if (in.GetInt64("castShadow", i64))
					m.castShadow = (i64 != 0);
				if (in.GetInt64("subMeshIndex", i64))
					m.subMeshIndex = (nk_uint32)i64;
				// `meshHandle` reste a 0 : NkRenderSystem::SubmitMeshes l'importe
				// paresseusement depuis `meshPath` (NkRenderSystem.cpp l.152-156).
				// C'est le contrat du moteur, on ne le double pas ici.
				w.Add<NkMeshComponent>(id, m);
				return true;
			}

			// ── NkMaterialComponent ───────────────────────────────────────────
			// Meme regle : les CHEMINS, pas les poignees GPU.
			bool EcrireMateriau(const NkWorld &w, NkEntityId id, NkArchive &out) {
				const NkMaterialComponent *m = const_cast<NkWorld &>(w).Get<NkMaterialComponent>(id);
				if (!m)
					return false;
				out.SetInt64("slotCount", (nk_int64)m->slotCount);
				for (nk_uint32 s = 0; s < m->slotCount && s < NkMaterialComponent::kMaxSlots; ++s) {
					char cle[24];
					std::snprintf(cle, sizeof(cle), "slot%u", (unsigned)s);
					out.SetString(cle, m->slots[s].materialPath.CStr());
				}
				return true;
			}

			bool LireMateriau(NkWorld &w, NkEntityId id, const NkArchive &in) {
				NkMaterialComponent m;
				nk_int64 n = 0;
				in.GetInt64("slotCount", n);
				if (n < 0)
					n = 0;
				if (n > (nk_int64)NkMaterialComponent::kMaxSlots)
					n = (nk_int64)NkMaterialComponent::kMaxSlots;
				m.slotCount = (nk_uint32)n;
				for (nk_uint32 s = 0; s < m.slotCount; ++s) {
					char cle[24];
					std::snprintf(cle, sizeof(cle), "slot%u", (unsigned)s);
					NkString p;
					if (in.GetString(cle, p))
						m.slots[s].materialPath = p;
				}
				w.Add<NkMaterialComponent>(id, m);
				return true;
			}


			// ── NkCameraComponent ─────────────────────────────────────────────
			// On ecrit les PARAMETRES, jamais les matrices : `viewMatrix`,
			// `projMatrix` et `viewProjMatrix` sont RECALCULEES a chaque image par
			// `NkRenderSystem::UpdateActiveCamera` depuis la transformation et ces
			// parametres. Les sauver serait sauver un resultat — et `aspect`
			// dependant de la taille de la vue, une matrice relue contredirait la
			// fenetre des le premier redimensionnement.
			bool EcrireCamera(const NkWorld &w, NkEntityId id, NkArchive &out) {
				const NkCameraComponent *c = const_cast<NkWorld &>(w).Get<NkCameraComponent>(id);
				if (!c)
					return false;
				out.SetInt64("projection", (nk_int64)(c->projection == NkCameraProjection::Perspective ? 0 : 1));
				out.SetFloat32("fovDeg", c->fovDeg);
				out.SetFloat32("nearClip", c->nearClip);
				out.SetFloat32("farClip", c->farClip);
				out.SetFloat32("orthoSize", c->orthoSize);
				out.SetInt64("priority", (nk_int64)c->priority);
				// `aspect` N'EST PAS SAUVE : il appartient a la VUE, pas a la scene.
				// NkRenderSystem le recoit du viewport a chaque image.
				return true;
			}

			bool LireCamera(NkWorld &w, NkEntityId id, const NkArchive &in) {
				NkCameraComponent c;
				nk_int64 i64 = 0;
				if (in.GetInt64("projection", i64))
					c.projection = (i64 == 0) ? NkCameraProjection::Perspective : NkCameraProjection::Orthographic;
				float32 v = 0.f;
				if (in.GetFloat32("fovDeg", v)) c.fovDeg = v;
				if (in.GetFloat32("nearClip", v)) c.nearClip = v;
				if (in.GetFloat32("farClip", v)) c.farClip = v;
				if (in.GetFloat32("orthoSize", v)) c.orthoSize = v;
				if (in.GetInt64("priority", i64)) c.priority = (nk_int32)i64;
				w.Add<NkCameraComponent>(id, c);
				return true;
			}

			// ── NkLightComponent ──────────────────────────────────────────────
			// La DIRECTION n'est pas ici : `NkRenderSystem::CollectLights` la prend
			// de `tf.GetWorldForward()`. Une direction sauvee a part pourrait
			// contredire la transformation — deux verites pour une orientation,
			// c'est exactement le defaut qui m'a coute un viewport vide.
			bool EcrireLumiere(const NkWorld &w, NkEntityId id, NkArchive &out) {
				const NkLightComponent *l = const_cast<NkWorld &>(w).Get<NkLightComponent>(id);
				if (!l)
					return false;
				out.SetInt64("type", (nk_int64)l->type);
				out.SetFloat32("cr", l->color.r);
				out.SetFloat32("cg", l->color.g);
				out.SetFloat32("cb", l->color.b);
				out.SetFloat32("ca", l->color.a);
				out.SetFloat32("intensity", l->intensity);
				out.SetFloat32("range", l->range);
				out.SetFloat32("innerAngle", l->innerAngle);
				out.SetFloat32("outerAngle", l->outerAngle);
				out.SetInt64("castShadow", l->castShadow ? 1 : 0);
				return true;
			}

			bool LireLumiere(NkWorld &w, NkEntityId id, const NkArchive &in) {
				NkLightComponent l;
				nk_int64 i64 = 0;
				if (in.GetInt64("type", i64)) l.type = (NkLightType)i64;
				float32 v = 0.f;
				if (in.GetFloat32("cr", v)) l.color.r = v;
				if (in.GetFloat32("cg", v)) l.color.g = v;
				if (in.GetFloat32("cb", v)) l.color.b = v;
				if (in.GetFloat32("ca", v)) l.color.a = v;
				if (in.GetFloat32("intensity", v)) l.intensity = v;
				if (in.GetFloat32("range", v)) l.range = v;
				if (in.GetFloat32("innerAngle", v)) l.innerAngle = v;
				if (in.GetFloat32("outerAngle", v)) l.outerAngle = v;
				if (in.GetInt64("castShadow", i64)) l.castShadow = (i64 != 0);
				w.Add<NkLightComponent>(id, l);
				return true;
			}

		} // namespace

		void RegisterNogeSceneComponents() noexcept {
			// Les noms sont des LITTERAUX : le registre garde le pointeur, pas la
			// chaine (NkSceneSerializer.h). Une NkString locale serait un pointeur
			// mort des la sortie de cette fonction.
			ecs::NkSceneSerializer::RegisterComponentSerializer("NkNameComponent", &EcrireNom, &LireNom);
			ecs::NkSceneSerializer::RegisterComponentSerializer("NkTransformComponent", &EcrireTransform,
														  &LireTransform);
			ecs::NkSceneSerializer::RegisterComponentSerializer("NkMeshComponent", &EcrireMesh, &LireMesh);
			ecs::NkSceneSerializer::RegisterComponentSerializer("NkMaterialComponent", &EcrireMateriau,
														  &LireMateriau);
			ecs::NkSceneSerializer::RegisterComponentSerializer("NkCameraComponent", &EcrireCamera,
														  &LireCamera);
			ecs::NkSceneSerializer::RegisterComponentSerializer("NkLightComponent", &EcrireLumiere,
														  &LireLumiere);
		}

	} // namespace noge
} // namespace nkentseu
