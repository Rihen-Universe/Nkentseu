// =============================================================================
// NkUnkenyProprietes.cpp — lire, ecrire, animer les proprietes d'une entite
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Anim/NkUnkenyProprietes.h"

#include "NKECS/Reflect/NkReflect.h"
#include "NKECS/Serialization/NkEntitySerialization.h" // GetComponentRaw : un composant par son identite
#include "NKECS/World/NkWorld.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Scene/NkUnkenyActif.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"
#include "Unkeny/Scene/NkUnkenyHierarchie.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			using math::NkVec4f;
			using PK = anim::NkAnimationClip::NkPropertyKind;

			constexpr float32 kDegres = 57.29577951f;

			// ── 1. LE NOYAU : les composants qu'Unkeny photographie EN CLAIR ──────
			enum class NkNoyau : uint8 {
				POSITION = 0,
				ROTATION,
				ECHELLE,
				SP_COULEUR,
				SP_TAILLE,
				SP_VISIBLE,
				SP_COUCHE,
				SP_IMAGE,
				LU_COULEUR,
				LU_INTENSITE,
				LU_PORTEE,
				LU_HALO,
				LU_ACTIF,
				COUNT
			};

			struct NkDescNoyau {
					const char *nom;
					const char *libelle;
					const char *groupe;
					PK genre;
					uint8 canaux;
			};

			const NkDescNoyau kNoyau[(uint32)NkNoyau::COUNT] = {
				{"Transform.position", "Position", "Transform", PK::NK_VEC2, 2},
				{"Transform.rotation", "Rotation (degres)", "Transform", PK::NK_NUMBER, 1},
				{"Transform.echelle", "Echelle", "Transform", PK::NK_VEC2, 2},
				{"Sprite.couleur", "Couleur", "Sprite", PK::NK_COLOR, 4},
				{"Sprite.taille", "Taille", "Sprite", PK::NK_VEC2, 2},
				{"Sprite.visible", "Visible", "Sprite", PK::NK_STEP, 1},
				{"Sprite.couche", "Couche", "Sprite", PK::NK_STEP, 1},
				{"Sprite.image", "Image de l'atlas", "Sprite", PK::NK_STEP, 1},
				{"Lumiere.couleur", "Couleur", "Lumiere", PK::NK_COLOR, 3},
				{"Lumiere.intensite", "Intensite", "Lumiere", PK::NK_NUMBER, 1},
				{"Lumiere.portee", "Portee", "Lumiere", PK::NK_NUMBER, 1},
				{"Lumiere.halo", "Halo", "Lumiere", PK::NK_NUMBER, 1},
				{"Lumiere.actif", "Allumee", "Lumiere", PK::NK_STEP, 1},
			};

			/// Un enfant s'anime dans le repere de son parent (voir l'en-tete).
			NkTransform2D *TransformAnime(NkScene &scene, ecs::NkEntityId id) {
				ecs::NkWorld &w = scene.Monde();
				if (scene.Parent(id).IsValid()) {
					if (NkLocal2D *l = w.Get<NkLocal2D>(id)) {
						return &l->local;
					}
				}
				return w.Get<NkTransform2D>(id);
			}

			bool NoyauPresent(NkScene &scene, ecs::NkEntityId id, NkNoyau k) {
				ecs::NkWorld &w = scene.Monde();
				switch (k) {
					case NkNoyau::POSITION:
					case NkNoyau::ROTATION:
					case NkNoyau::ECHELLE:
						return w.Has<NkTransform2D>(id);
					case NkNoyau::SP_IMAGE:
						return w.Has<NkSprite2D>(id) && w.Has<NkAnimSprite2D>(id);
					case NkNoyau::SP_COULEUR:
					case NkNoyau::SP_TAILLE:
					case NkNoyau::SP_VISIBLE:
					case NkNoyau::SP_COUCHE:
						return w.Has<NkSprite2D>(id);
					default:
						return w.Has<NkLumiere2D>(id);
				}
			}

			int32 IndexNoyau(const NkString &nom) {
				for (uint32 k = 0; k < (uint32)NkNoyau::COUNT; ++k) {
					if (nom == kNoyau[k].nom) {
						return (int32)k;
					}
				}
				return -1;
			}

			NkVec4f Couleur(uint32 rgba) {
				return static_cast<NkVec4f>(math::NkColor(rgba));
			}

			uint32 Rgba(const NkVec4f &v) {
				return math::NkColor(v).ToUint32A();
			}

			bool LireNoyau(NkScene &scene, ecs::NkEntityId id, NkNoyau k, NkVec4f &v) {
				ecs::NkWorld &w = scene.Monde();
				v = NkVec4f(0.f, 0.f, 0.f, 0.f);
				if (!NoyauPresent(scene, id, k)) {
					return false;
				}
				const NkTransform2D *t = TransformAnime(scene, id);
				const NkSprite2D *s = w.Get<NkSprite2D>(id);
				const NkLumiere2D *l = w.Get<NkLumiere2D>(id);
				switch (k) {
					case NkNoyau::POSITION:
						v = NkVec4f(t->position.x, t->position.y, 0.f, 0.f);
						break;
					case NkNoyau::ROTATION:
						v.x = t->rotation * kDegres;
						break;
					case NkNoyau::ECHELLE:
						v = NkVec4f(t->echelle.x, t->echelle.y, 0.f, 0.f);
						break;
					case NkNoyau::SP_COULEUR:
						v = Couleur(s->couleur);
						break;
					case NkNoyau::SP_TAILLE:
						v = NkVec4f(s->taille.x, s->taille.y, 0.f, 0.f);
						break;
					case NkNoyau::SP_VISIBLE:
						v.x = s->visible ? 1.f : 0.f;
						break;
					case NkNoyau::SP_COUCHE:
						v.x = static_cast<float32>(s->couche);
						break;
					case NkNoyau::SP_IMAGE: {
						// L'image MONTREE : retrouvee depuis la region UV et la grille.
						const NkAnimSprite2D *a = w.Get<NkAnimSprite2D>(id);
						const float32 col = a->colonnes > 0 ? static_cast<float32>(a->colonnes) : 1.f;
						const float32 lig = a->lignes > 0 ? static_cast<float32>(a->lignes) : 1.f;
						const float32 cx = std::floor(s->uv0.x * col + 0.5f);
						const float32 cy = std::floor(s->uv0.y * lig + 0.5f);
						v.x = cy * col + cx;
						break;
					}
					case NkNoyau::LU_COULEUR:
						v = Couleur(l->couleur);
						break;
					case NkNoyau::LU_INTENSITE:
						v.x = l->intensite;
						break;
					case NkNoyau::LU_PORTEE:
						v.x = l->portee;
						break;
					case NkNoyau::LU_HALO:
						v.x = l->halo;
						break;
					case NkNoyau::LU_ACTIF:
						v.x = l->actif ? 1.f : 0.f;
						break;
					default:
						return false;
				}
				return true;
			}

			/// `hierarchie` passe a vrai si une place LOCALE a ete ecrite : le monde
			/// de l'enfant est a recalculer.
			bool EcrireNoyau(NkScene &scene, ecs::NkEntityId id, NkNoyau k, const NkVec4f &v, bool &hierarchie) {
				ecs::NkWorld &w = scene.Monde();
				if (!NoyauPresent(scene, id, k)) {
					return false;
				}
				NkTransform2D *t = TransformAnime(scene, id);
				NkSprite2D *s = w.Get<NkSprite2D>(id);
				NkLumiere2D *l = w.Get<NkLumiere2D>(id);
				const bool local = t != nullptr && t != w.Get<NkTransform2D>(id);
				switch (k) {
					case NkNoyau::POSITION:
						t->position = NkVec2f(v.x, v.y);
						hierarchie = hierarchie || local;
						break;
					case NkNoyau::ROTATION:
						t->rotation = v.x / kDegres;
						hierarchie = hierarchie || local;
						break;
					case NkNoyau::ECHELLE:
						t->echelle = NkVec2f(v.x, v.y);
						hierarchie = hierarchie || local;
						break;
					case NkNoyau::SP_COULEUR:
						s->couleur = Rgba(v);
						break;
					case NkNoyau::SP_TAILLE:
						s->taille = NkVec2f(v.x, v.y);
						break;
					case NkNoyau::SP_VISIBLE:
						s->visible = v.x > 0.5f;
						break;
					case NkNoyau::SP_COUCHE:
						s->couche = static_cast<int32>(std::floor(v.x + 0.5f));
						break;
					case NkNoyau::SP_IMAGE: {
						// La region de l'image `v.x` dans la grille de l'atlas -- le meme
						// calcul que NkAnimSprite2D::RegionUV. ⚠️ Un sprite ANIME (des
						// clips) reecrit ses UV a chaque trame : une piste d'image vise
						// un sprite dont NkAnimSprite2D ne porte que la grille.
						const NkAnimSprite2D *a = w.Get<NkAnimSprite2D>(id);
						const uint16 col = a->colonnes > 0 ? a->colonnes : 1;
						const uint16 lig = a->lignes > 0 ? a->lignes : 1;
						const int32 img = static_cast<int32>(std::floor(v.x + 0.5f));
						const uint32 n = img < 0 ? 0u : static_cast<uint32>(img);
						const float32 fw = 1.f / static_cast<float32>(col);
						const float32 fh = 1.f / static_cast<float32>(lig);
						s->uv0 = NkVec2f(static_cast<float32>(n % col) * fw, static_cast<float32>((n / col) % lig) * fh);
						s->uv1 = NkVec2f(s->uv0.x + fw, s->uv0.y + fh);
						break;
					}
					case NkNoyau::LU_COULEUR:
						l->couleur = (Rgba(v) & 0xFFFFFF00u) | (l->couleur & 0xFFu);
						break;
					case NkNoyau::LU_INTENSITE:
						l->intensite = v.x;
						break;
					case NkNoyau::LU_PORTEE:
						l->portee = v.x;
						break;
					case NkNoyau::LU_HALO:
						l->halo = v.x;
						break;
					case NkNoyau::LU_ACTIF:
						l->actif = v.x > 0.5f;
						break;
					default:
						return false;
				}
				return true;
			}

			// ── 2. LES COMPOSANTS DECRITS (NkScene::PhotographierAussi + champs) ──
			bool GenreChamp(const NkChampSauve &c, PK &g, uint8 &canaux) {
				if (c.nombre != 1u || c.transitoire) {
					return false; // un tableau ne s'anime pas champ par champ ici
				}
				canaux = 1;
				switch (c.type) {
					case NkTypeChamp::NK_F32:
					case NkTypeChamp::NK_F64:
						g = PK::NK_NUMBER;
						return true;
					case NkTypeChamp::NK_VEC2:
						g = PK::NK_VEC2;
						canaux = 2;
						return true;
					case NkTypeChamp::NK_BOOL:
					case NkTypeChamp::NK_I8:
					case NkTypeChamp::NK_U8:
					case NkTypeChamp::NK_I16:
					case NkTypeChamp::NK_U16:
					case NkTypeChamp::NK_I32:
					case NkTypeChamp::NK_U32:
					case NkTypeChamp::NK_SON:
					case NkTypeChamp::NK_TEXTURE:
					case NkTypeChamp::NK_PREFAB:
						// Booleen, entier, enumeration, reference d'asset : PAR PALIERS.
						g = PK::NK_STEP;
						return true;
					default:
						return false; // texte, entite, entiers 64 bits
				}
			}

			template <typename T> float32 Lu(const uint8 *p) {
				T x;
				std::memcpy(&x, p, sizeof(T));
				return static_cast<float32>(x);
			}

			template <typename T> void Ecrit(uint8 *p, float32 v, bool entier) {
				const T x = static_cast<T>(entier ? std::floor(v + 0.5f) : v);
				std::memcpy(p, &x, sizeof(T));
			}

			void LireChamp(const NkChampSauve &c, const uint8 *octets, NkVec4f &v) {
				const uint8 *p = octets + c.decalage;
				v = NkVec4f(0.f, 0.f, 0.f, 0.f);
				switch (c.type) {
					case NkTypeChamp::NK_BOOL:
						v.x = *p != 0u ? 1.f : 0.f;
						break;
					case NkTypeChamp::NK_I8:
						v.x = Lu<int8>(p);
						break;
					case NkTypeChamp::NK_U8:
						v.x = Lu<uint8>(p);
						break;
					case NkTypeChamp::NK_I16:
						v.x = Lu<int16>(p);
						break;
					case NkTypeChamp::NK_U16:
						v.x = Lu<uint16>(p);
						break;
					case NkTypeChamp::NK_I32:
						v.x = Lu<int32>(p);
						break;
					case NkTypeChamp::NK_U32:
					case NkTypeChamp::NK_SON:
					case NkTypeChamp::NK_TEXTURE:
					case NkTypeChamp::NK_PREFAB:
						v.x = Lu<uint32>(p);
						break;
					case NkTypeChamp::NK_F32:
						v.x = Lu<float32>(p);
						break;
					case NkTypeChamp::NK_F64:
						v.x = Lu<float64>(p);
						break;
					case NkTypeChamp::NK_VEC2:
						v.x = Lu<float32>(p);
						v.y = Lu<float32>(p + 4);
						break;
					default:
						break;
				}
			}

			void EcrireChamp(const NkChampSauve &c, uint8 *octets, const NkVec4f &v) {
				uint8 *p = octets + c.decalage;
				switch (c.type) {
					case NkTypeChamp::NK_BOOL:
						*p = v.x > 0.5f ? 1u : 0u;
						break;
					case NkTypeChamp::NK_I8:
						Ecrit<int8>(p, v.x, true);
						break;
					case NkTypeChamp::NK_U8:
						Ecrit<uint8>(p, v.x < 0.f ? 0.f : v.x, true);
						break;
					case NkTypeChamp::NK_I16:
						Ecrit<int16>(p, v.x, true);
						break;
					case NkTypeChamp::NK_U16:
						Ecrit<uint16>(p, v.x < 0.f ? 0.f : v.x, true);
						break;
					case NkTypeChamp::NK_I32:
						Ecrit<int32>(p, v.x, true);
						break;
					case NkTypeChamp::NK_U32:
					case NkTypeChamp::NK_SON:
					case NkTypeChamp::NK_TEXTURE:
					case NkTypeChamp::NK_PREFAB:
						Ecrit<uint32>(p, v.x < 0.f ? 0.f : v.x, true);
						break;
					case NkTypeChamp::NK_F32:
						Ecrit<float32>(p, v.x, false);
						break;
					case NkTypeChamp::NK_F64:
						Ecrit<float64>(p, v.x, false);
						break;
					case NkTypeChamp::NK_VEC2:
						Ecrit<float32>(p, v.x, false);
						Ecrit<float32>(p + 4, v.y, false);
						break;
					default:
						break;
				}
			}

			/// « Composant.champ » -> le copieur et le champ decrits. Faux sinon.
			bool TrouverDecrit(NkScene &scene, const NkString &nom, uint32 &copieur, const NkChampSauve *&champ) {
				const char *s = nom.CStr();
				const char *point = std::strchr(s, '.');
				if (point == nullptr) {
					return false;
				}
				const NkString comp(s, static_cast<usize>(point - s));
				const int32 k = scene.IndexComposantPhoto(comp.CStr());
				if (k < 0) {
					return false;
				}
				uint32 n = 0;
				const NkChampSauve *champs = scene.ChampsComposantPhoto(static_cast<uint32>(k), n);
				for (uint32 c = 0; champs != nullptr && c < n; ++c) {
					if (std::strcmp(champs[c].nom, point + 1) == 0) {
						PK g;
						uint8 canaux = 0;
						if (!GenreChamp(champs[c], g, canaux)) {
							return false;
						}
						copieur = static_cast<uint32>(k);
						champ = &champs[c];
						return true;
					}
				}
				return false;
			}

			// ── 3. LES COMPOSANTS REFLECHIS PAR NKECS ─────────────────────────────
			bool GenreReflechi(const ecs::reflect::NkFieldInfo &f, PK &g, uint8 &canaux) {
				using FT = ecs::reflect::NkFieldType;
				if (f.count != 1u || !f.IsEditable() || f.HasFlag(ecs::reflect::NkMeta_HideInEditor)) {
					return false;
				}
				canaux = 1;
				switch (f.type) {
					case FT::Float32:
					case FT::Float64:
						g = PK::NK_NUMBER;
						return true;
					case FT::Vec2:
						g = PK::NK_VEC2;
						canaux = 2;
						return true;
					case FT::Vec3:
						g = f.HasFlag(ecs::reflect::NkMeta_ColorPicker) ? PK::NK_COLOR : PK::NK_VEC3;
						canaux = 3;
						return true;
					case FT::Vec4:
						g = f.HasFlag(ecs::reflect::NkMeta_ColorPicker) ? PK::NK_COLOR : PK::NK_VEC4;
						canaux = 4;
						return true;
					case FT::Bool:
					case FT::Int8:
					case FT::UInt8:
					case FT::Int16:
					case FT::UInt16:
					case FT::Int32:
					case FT::UInt32:
					case FT::Enum:
						g = PK::NK_STEP;
						return true;
					default:
						return false;
				}
			}

			void LireReflechi(const ecs::reflect::NkFieldInfo &f, const uint8 *base, NkVec4f &v) {
				using FT = ecs::reflect::NkFieldType;
				const uint8 *p = base + f.offset;
				v = NkVec4f(0.f, 0.f, 0.f, 0.f);
				switch (f.type) {
					case FT::Float32:
						v.x = Lu<float32>(p);
						break;
					case FT::Float64:
						v.x = Lu<float64>(p);
						break;
					case FT::Vec2:
					case FT::Vec3:
					case FT::Vec4: {
						const uint32 n = f.type == FT::Vec2 ? 2u : (f.type == FT::Vec3 ? 3u : 4u);
						float32 tmp[4] = {0.f, 0.f, 0.f, 0.f};
						std::memcpy(tmp, p, n * sizeof(float32));
						v = NkVec4f(tmp[0], tmp[1], tmp[2], tmp[3]);
						break;
					}
					case FT::Bool:
						v.x = *p != 0u ? 1.f : 0.f;
						break;
					case FT::Int8:
						v.x = Lu<int8>(p);
						break;
					case FT::UInt8:
						v.x = Lu<uint8>(p);
						break;
					case FT::Int16:
						v.x = Lu<int16>(p);
						break;
					case FT::UInt16:
						v.x = Lu<uint16>(p);
						break;
					case FT::Enum:
						// Une enumeration se lit a la taille de son champ.
						v.x = f.size == 1u ? Lu<uint8>(p) : (f.size == 2u ? Lu<uint16>(p) : Lu<int32>(p));
						break;
					default:
						v.x = Lu<int32>(p);
						break;
				}
			}

			void EcrireReflechi(const ecs::reflect::NkFieldInfo &f, uint8 *base, const NkVec4f &v) {
				using FT = ecs::reflect::NkFieldType;
				uint8 *p = base + f.offset;
				switch (f.type) {
					case FT::Float32:
						Ecrit<float32>(p, v.x, false);
						break;
					case FT::Float64:
						Ecrit<float64>(p, v.x, false);
						break;
					case FT::Vec2:
					case FT::Vec3:
					case FT::Vec4: {
						const uint32 n = f.type == FT::Vec2 ? 2u : (f.type == FT::Vec3 ? 3u : 4u);
						const float32 tmp[4] = {v.x, v.y, v.z, v.w};
						std::memcpy(p, tmp, n * sizeof(float32));
						break;
					}
					case FT::Bool:
						*p = v.x > 0.5f ? 1u : 0u;
						break;
					case FT::Int8:
						Ecrit<int8>(p, v.x, true);
						break;
					case FT::UInt8:
						Ecrit<uint8>(p, v.x < 0.f ? 0.f : v.x, true);
						break;
					case FT::Int16:
						Ecrit<int16>(p, v.x, true);
						break;
					case FT::UInt16:
						Ecrit<uint16>(p, v.x < 0.f ? 0.f : v.x, true);
						break;
					case FT::Enum:
						if (f.size == 1u) {
							Ecrit<uint8>(p, v.x < 0.f ? 0.f : v.x, true);
						} else if (f.size == 2u) {
							Ecrit<uint16>(p, v.x < 0.f ? 0.f : v.x, true);
						} else {
							Ecrit<int32>(p, v.x, true);
						}
						break;
					default:
						Ecrit<int32>(p, v.x, true);
						break;
				}
			}

			/// « Type.champ » d'un composant reflechi que `id` porte.
			bool TrouverReflechi(NkScene &scene, ecs::NkEntityId id, const NkString &nom, const ecs::reflect::NkFieldInfo *&champ,
								 uint8 *&base) {
				const char *s = nom.CStr();
				const char *point = std::strchr(s, '.');
				if (point == nullptr) {
					return false;
				}
				const NkString type(s, static_cast<usize>(point - s));
				const ecs::reflect::NkTypeInfo *ti = ecs::reflect::NkReflectRegistry::Global().GetByName(type.CStr());
				if (ti == nullptr) {
					return false;
				}
				const ecs::reflect::NkFieldInfo *f = ti->FindField(point + 1);
				PK g;
				uint8 canaux = 0;
				if (f == nullptr || !GenreReflechi(*f, g, canaux)) {
					return false;
				}
				void *p = ecs::serialization::GetComponentRaw(scene.Monde(), id, ti->componentId);
				if (p == nullptr) {
					return false;
				}
				champ = f;
				base = static_cast<uint8 *>(p);
				return true;
			}

			bool EcrireInterne(NkScene &scene, ecs::NkEntityId id, const NkString &nom, const NkVec4f &v, bool &hierarchie) {
				if (!scene.Monde().IsAlive(id)) {
					return false;
				}
				const int32 k = IndexNoyau(nom);
				if (k >= 0) {
					return EcrireNoyau(scene, id, static_cast<NkNoyau>(k), v, hierarchie);
				}
				uint32 copieur = 0;
				const NkChampSauve *champ = nullptr;
				if (TrouverDecrit(scene, nom, copieur, champ)) {
					NkVector<uint8> octets;
					octets.Resize(scene.TailleComposantPhoto(copieur));
					if (!scene.LireComposantBrut(copieur, id, octets.Data())) {
						return false;
					}
					EcrireChamp(*champ, octets.Data(), v);
					return scene.EcrireComposantBrut(copieur, id, octets.Data());
				}
				const ecs::reflect::NkFieldInfo *f = nullptr;
				uint8 *base = nullptr;
				if (TrouverReflechi(scene, id, nom, f, base)) {
					EcrireReflechi(*f, base, v);
					return true;
				}
				return false;
			}

			void CopierNom(char *dst, int32 taille, const char *src) noexcept {
				int32 i = 0;
				if (src != nullptr) {
					for (; i < taille - 1 && src[i] != '\0'; ++i) {
						dst[i] = src[i];
					}
				}
				dst[i] = '\0';
			}
		} // namespace

		// =====================================================================
		// LISTER, LIRE, ECRIRE
		// =====================================================================
		void NkListerProprietesAnimables(NkScene &scene, ecs::NkEntityId id, NkVector<NkProprieteAnimable> &out) {
			out.Clear();
			if (!scene.Monde().IsAlive(id)) {
				return;
			}
			for (uint32 k = 0; k < (uint32)NkNoyau::COUNT; ++k) {
				if (!NoyauPresent(scene, id, static_cast<NkNoyau>(k))) {
					continue;
				}
				NkProprieteAnimable p;
				p.nom = kNoyau[k].nom;
				p.libelle = kNoyau[k].libelle;
				p.groupe = kNoyau[k].groupe;
				p.genre = kNoyau[k].genre;
				p.canaux = kNoyau[k].canaux;
				out.PushBack(p);
			}
			// Les composants DECRITS que l'entite porte.
			NkVector<uint8> octets;
			for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
				uint32 n = 0;
				const NkChampSauve *champs = scene.ChampsComposantPhoto(k, n);
				const char *comp = scene.NomComposantPhoto(k);
				if (champs == nullptr || comp == nullptr) {
					continue;
				}
				octets.Resize(scene.TailleComposantPhoto(k));
				if (!scene.LireComposantBrut(k, id, octets.Data())) {
					continue;
				}
				for (uint32 c = 0; c < n; ++c) {
					NkProprieteAnimable p;
					if (!GenreChamp(champs[c], p.genre, p.canaux)) {
						continue;
					}
					p.groupe = comp;
					p.nom = comp;
					p.nom.Append(".");
					p.nom.Append(champs[c].nom);
					p.libelle = champs[c].nom;
					out.PushBack(p);
				}
			}
			// Les composants REFLECHIS par NKECS que l'entite porte.
			ecs::reflect::NkReflectRegistry::Global().ForEach([&](const ecs::reflect::NkTypeInfo &ti) {
				if (ti.fields == nullptr || ti.fieldCount == 0 ||
					ecs::serialization::GetComponentRaw(scene.Monde(), id, ti.componentId) == nullptr) {
					return;
				}
				for (uint32 f = 0; f < ti.fieldCount; ++f) {
					NkProprieteAnimable p;
					if (!GenreReflechi(ti.fields[f], p.genre, p.canaux)) {
						continue;
					}
					p.groupe = ti.name;
					p.nom = ti.name;
					p.nom.Append(".");
					p.nom.Append(ti.fields[f].name);
					p.libelle = ti.fields[f].displayName != nullptr ? ti.fields[f].displayName : ti.fields[f].name;
					out.PushBack(p);
				}
			});
		}

		bool NkTrouverProprieteAnimable(NkScene &scene, ecs::NkEntityId id, const NkString &nom, NkProprieteAnimable &out) {
			NkVector<NkProprieteAnimable> toutes;
			NkListerProprietesAnimables(scene, id, toutes);
			for (uint32 i = 0; i < (uint32)toutes.Size(); ++i) {
				if (toutes[i].nom == nom) {
					out = toutes[i];
					return true;
				}
			}
			return false;
		}

		bool NkLireProprieteAnimee(NkScene &scene, ecs::NkEntityId id, const NkString &nom, math::NkVec4f &v) {
			v = NkVec4f(0.f, 0.f, 0.f, 0.f);
			if (!scene.Monde().IsAlive(id)) {
				return false;
			}
			const int32 k = IndexNoyau(nom);
			if (k >= 0) {
				return LireNoyau(scene, id, static_cast<NkNoyau>(k), v);
			}
			uint32 copieur = 0;
			const NkChampSauve *champ = nullptr;
			if (TrouverDecrit(scene, nom, copieur, champ)) {
				NkVector<uint8> octets;
				octets.Resize(scene.TailleComposantPhoto(copieur));
				if (!scene.LireComposantBrut(copieur, id, octets.Data())) {
					return false;
				}
				LireChamp(*champ, octets.Data(), v);
				return true;
			}
			const ecs::reflect::NkFieldInfo *f = nullptr;
			uint8 *base = nullptr;
			if (TrouverReflechi(scene, id, nom, f, base)) {
				LireReflechi(*f, base, v);
				return true;
			}
			return false;
		}

		bool NkEcrireProprieteAnimee(NkScene &scene, ecs::NkEntityId id, const NkString &nom, const math::NkVec4f &v) {
			bool hierarchie = false;
			const bool ok = EcrireInterne(scene, id, nom, v, hierarchie);
			if (hierarchie) {
				scene.PropagerHierarchie();
			}
			return ok;
		}

		// =====================================================================
		// LES CIBLES PAR CHEMIN
		// =====================================================================
		ecs::NkEntityId NkResoudreCible(NkScene &scene, ecs::NkEntityId racine, const NkString &chemin) {
			ecs::NkEntityId cur = racine;
			const char *s = chemin.CStr();
			NkVector<ecs::NkEntityId> enfants;
			while (*s != '\0' && scene.Monde().IsAlive(cur)) {
				const char *fin = std::strchr(s, '/');
				const usize n = fin != nullptr ? static_cast<usize>(fin - s) : std::strlen(s);
				ecs::NkEntityId trouve;
				scene.Enfants(cur, enfants);
				for (uint32 i = 0; i < (uint32)enfants.Size(); ++i) {
					const NkEtiquette *e = scene.Monde().Get<NkEtiquette>(enfants[i]);
					if (e != nullptr && std::strlen(e->nom) == n && std::strncmp(e->nom, s, n) == 0) {
						trouve = enfants[i];
						break;
					}
				}
				if (!trouve.IsValid()) {
					return ecs::NkEntityId();
				}
				cur = trouve;
				s += n;
				if (*s == '/') {
					++s;
				}
			}
			return scene.Monde().IsAlive(cur) ? cur : ecs::NkEntityId();
		}

		bool NkCheminCible(NkScene &scene, ecs::NkEntityId racine, ecs::NkEntityId cible, NkString &chemin) {
			chemin = NkString();
			NkVector<NkString> noms;
			ecs::NkEntityId cur = cible;
			for (int32 garde = 0; cur.IsValid() && !(cur == racine) && garde < 64; ++garde) {
				const NkEtiquette *e = scene.Monde().Get<NkEtiquette>(cur);
				noms.PushBack(e != nullptr ? NkString(e->nom) : NkString());
				cur = scene.Parent(cur);
			}
			if (!(cur == racine)) {
				return false;
			}
			for (int32 k = (int32)noms.Size() - 1; k >= 0; --k) {
				chemin.Append(noms[(uint32)k]);
				if (k > 0) {
					chemin.Append("/");
				}
			}
			return true;
		}

		uint32 NkAppliquerClipProprietes(NkScene &scene, ecs::NkEntityId racine, const anim::NkAnimationClip &clip,
										 float32 t) {
			// (01/10 soir) Une SEQUENCE (des clips poses, NLA) : la pose melangee de
			// NKAnima, ses pistes propres comprises.
			if (!clip.clipTracks.Empty()) {
				const anim::NkClipLookup lookup = NkRechercheClipsProprietes();
				anim::NkAnimPose pose;
				anim::NkSampleClip(clip, t, pose, &lookup);
				return NkAppliquerPoseProprietes(scene, racine, pose);
			}
			uint32 n = 0;
			bool hierarchie = false;
			for (uint32 i = 0; i < (uint32)clip.propertyTracks.Size(); ++i) {
				const anim::NkAnimationClip::NkPropertyTrack &p = clip.propertyTracks[i];
				if (!p.curve.enabled || p.curve.Empty()) {
					continue;
				}
				const ecs::NkEntityId cible = NkResoudreCible(scene, racine, p.target);
				if (!cible.IsValid()) {
					continue;
				}
				// L'interpolation est CELLE DE NKAnima (NkAnimationTrack::Evaluate) :
				// l'apercu de l'editeur et le jeu jouent la meme courbe.
				// (01/10 soir) NkPropertyTrack::Evaluate : la « Courbe » y est une vraie
				// courbe (Hermite, tangentes), les autres interpolations inchangees.
				if (EcrireInterne(scene, cible, p.property, p.Evaluate(t), hierarchie)) {
					++n;
				}
			}
			if (hierarchie) {
				scene.PropagerHierarchie();
			}
			return n;
		}

		uint32 NkAppliquerPoseProprietes(NkScene &scene, ecs::NkEntityId racine, const anim::NkAnimPose &pose) {
			uint32 n = 0;
			bool hierarchie = false;
			for (uint32 i = 0; i < (uint32)pose.props.Size(); ++i) {
				const anim::NkPropValue &p = pose.props[i];
				if (p.weight <= 1e-4f) {
					continue;
				}
				const ecs::NkEntityId cible = NkResoudreCible(scene, racine, p.target);
				if (!cible.IsValid()) {
					continue;
				}
				math::NkVec4f v = p.value;
				if (p.weight < 0.999f && p.kind != anim::NkAnimationClip::NkPropertyKind::NK_STEP) {
					// Une couverture partielle : le fondu part de la valeur VIVANTE.
					math::NkVec4f vivante;
					if (NkLireProprieteAnimee(scene, cible, p.property, vivante)) {
						for (uint32 c = 0; c < 4; ++c) {
							v[c] = vivante[c] + (p.value[c] - vivante[c]) * p.weight;
						}
					}
				} else if (p.weight < 0.5f) {
					continue; // un palier peu couvert ne bascule pas
				}
				if (EcrireInterne(scene, cible, p.property, v, hierarchie)) {
					++n;
				}
			}
			if (hierarchie) {
				scene.PropagerHierarchie();
			}
			return n;
		}

		anim::NkClipLookup NkRechercheClipsProprietes() {
			return [](const NkString &nom) -> const anim::NkAnimationClip * { return NkClipProprietesEnregistre(nom.CStr()); };
		}

		// =====================================================================
		// LE CLIP EN JEU
		// =====================================================================
		void NkClipProprietes2D::Jouer(const char *nom) noexcept {
			if (nom == nullptr) {
				return;
			}
			if (std::strncmp(clip, nom, NK_UNKENY_CLIP_NOM_MAX) == 0 && !termine) {
				return;
			}
			CopierNom(clip, NK_UNKENY_CLIP_NOM_MAX, nom);
			temps = 0.f;
			termine = false;
			clipAbsent = false;
		}

		namespace {
			struct NkClipNomme {
					char nom[NK_UNKENY_CLIP_NOM_MAX] = {};
					anim::NkAnimationClip *clip = nullptr;
			};
			/// Meme forme que le registre des modeles d'animateur (et meme raison
			/// de toucher l'allocateur dans le constructeur).
			struct NkRegistreClips {
					NkVector<NkClipNomme> clips;
					NkRegistreClips() {
						(void)memory::NkGetDefaultAllocator();
					}
					~NkRegistreClips() {
						for (uint32 i = 0; i < clips.Size(); ++i) {
							memory::NkGetDefaultAllocator().Delete(clips[i].clip);
						}
					}
			};
			NkRegistreClips &Clips() {
				static NkRegistreClips r;
				return r;
			}
			NkClipNomme *TrouverClip(const char *nom) {
				NkRegistreClips &r = Clips();
				for (uint32 i = 0; nom != nullptr && i < r.clips.Size(); ++i) {
					if (std::strncmp(r.clips[i].nom, nom, NK_UNKENY_CLIP_NOM_MAX) == 0) {
						return &r.clips[i];
					}
				}
				return nullptr;
			}
		} // namespace

		bool NkEnregistrerClipProprietes(const char *nom, const anim::NkAnimationClip &clip) {
			if (nom == nullptr || nom[0] == '\0') {
				return false;
			}
			if (NkClipNomme *c = TrouverClip(nom)) {
				*c->clip = clip;
				return true;
			}
			NkClipNomme c;
			CopierNom(c.nom, NK_UNKENY_CLIP_NOM_MAX, nom);
			c.clip = memory::NkGetDefaultAllocator().New<anim::NkAnimationClip>(clip);
			if (c.clip == nullptr) {
				return false;
			}
			Clips().clips.PushBack(c);
			return true;
		}

		bool NkChargerClipProprietes(const char *nom, const char *chemin) {
			anim::NkAnimationClip c;
			if (chemin == nullptr || !c.LoadBinary(NkString(chemin))) {
				logger.Warn("[unkeny] clip de proprietes '{0}' non lu : {1}", nom != nullptr ? nom : "",
							chemin != nullptr ? chemin : "");
				return false;
			}
			return NkEnregistrerClipProprietes(nom, c);
		}

		const anim::NkAnimationClip *NkClipProprietesEnregistre(const char *nom) {
			const NkClipNomme *c = TrouverClip(nom);
			return c != nullptr ? c->clip : nullptr;
		}

		void NkAvancerClipsProprietes(NkScene &scene, float32 dt) {
			ecs::NkWorld &monde = scene.Monde();
			// Les entites d'abord, les ecritures ensuite : ecrire un composant
			// pendant le parcours de sa requete n'est pas permis par l'ECS.
			NkVector<ecs::NkEntityId> ids;
			monde.Query<NkClipProprietes2D>().ForEach([&](ecs::NkEntityId id, NkClipProprietes2D &) { ids.PushBack(id); });
			for (uint32 i = 0; i < (uint32)ids.Size(); ++i) {
				NkClipProprietes2D *c = monde.Get<NkClipProprietes2D>(ids[i]);
				if (c == nullptr || c->clip[0] == '\0' || !NkEntiteActive(monde, ids[i])) {
					continue;
				}
				const anim::NkAnimationClip *clip = NkClipProprietesEnregistre(c->clip);
				if (clip == nullptr) {
					if (!c->clipAbsent) {
						logger.Warn("[unkeny] clip de proprietes '{0}' inconnu (NkEnregistrerClipProprietes)", c->clip);
						c->clipAbsent = true;
					}
					continue;
				}
				if (!c->enPause && !c->termine) {
					c->temps += dt * c->vitesse;
				}
				const float32 d = clip->duration;
				float32 t = c->temps;
				if (d > 1e-6f && t > d) {
					if (c->boucle) {
						t = std::fmod(t, d);
						c->temps = t;
					} else {
						t = d;
						c->termine = true;
					}
				}
				NkAppliquerClipProprietes(scene, ids[i], *clip, t);
			}
		}

	} // namespace unkeny
} // namespace nkentseu
