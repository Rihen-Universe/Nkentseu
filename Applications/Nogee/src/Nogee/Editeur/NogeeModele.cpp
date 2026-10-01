// -----------------------------------------------------------------------------
// @File    NogeeModele.cpp
// @Brief   Le modele de l'editeur Nogee (voir NogeeModele.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Nogee/Editeur/NogeeModele.h"

#include "NKECS/Hierarchy/NkHierarchy.h"
#include "NKEditorKit/Famille/NkFamillePlacer.h" // NkFamilleBitOnglet
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Noge/ECS/Systems/NkPhysicsSystem.h"
#include "Noge/ECS/Systems/NkSceneSerializer.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nogee {

		using namespace ecs;
		using namespace math;
		using editorkit::NkFamilleBitOnglet;
		using O = editorkit::NkFamilleOngletPlacer;

		namespace {
			constexpr float32 kDegVersRad = 0.01745329252f;
			constexpr uint32 kMaxHistorique = 40u;

			bool CommencePar(const char *s, const char *p) noexcept {
				return s != nullptr && p != nullptr && std::strncmp(s, p, std::strlen(p)) == 0;
			}

			void PreparerDossier(const char *chemin) {
				char dossier[512];
				std::snprintf(dossier, sizeof(dossier), "%s", chemin != nullptr ? chemin : "");
				char *fin = nullptr;
				for (char *c = dossier; *c != '\0'; ++c) {
					if (*c == '/' || *c == '\\') {
						fin = c;
					}
				}
				if (fin != nullptr) {
					*fin = '\0';
					if (!NkDirectory::Exists(dossier)) {
						(void)NkDirectory::CreateRecursive(dossier);
					}
				}
			}

			const NogeeElement kCatalogue[] = {
				// Base
				{"Acteur vide", "Une entité seule : un nom et une transformation.", NogeeGenre::ActeurVide,
				 NkFamilleBitOnglet(O::Base)},
				{"Caméra", "Une caméra de jeu (NkCameraComponent) : c'est elle qui filme en jeu.", NogeeGenre::Camera,
				 NkFamilleBitOnglet(O::Base)},
				{"Cube physique", "Un cube rouge qui tombe (corps rigide + collisionneur boîte).", NogeeGenre::CubePhysique,
				 NkFamilleBitOnglet(O::Base) | NkFamilleBitOnglet(O::Formes)},
				{"Sphère physique", "Une balle qui tombe et rebondit (corps rigide + sphère).", NogeeGenre::SpherePhysique,
				 NkFamilleBitOnglet(O::Base) | NkFamilleBitOnglet(O::Formes)},
				{"Sol", "Un sol statique de 20 x 20 m (boîte + collisionneur).", NogeeGenre::Sol, NkFamilleBitOnglet(O::Base)},
				// Lumieres
				{"Lumière directionnelle", "Le soleil : une direction, des ombres.", NogeeGenre::LumiereDirectionnelle,
				 NkFamilleBitOnglet(O::Lumieres)},
				{"Lumière ponctuelle", "Une ampoule : elle éclaire autour d'elle, jusqu'à sa portée.", NogeeGenre::LumierePonctuelle,
				 NkFamilleBitOnglet(O::Lumieres)},
				{"Projecteur", "Un cône de lumière (angles intérieur et extérieur).", NogeeGenre::LumiereProjecteur,
				 NkFamilleBitOnglet(O::Lumieres)},
				// Formes : les primitives du NkMeshSystem
				{"Cube", "La primitive « cube » du moteur (1 m).", NogeeGenre::Cube, NkFamilleBitOnglet(O::Formes)},
				{"Sphère", "La primitive « sphere » du moteur (1 m de diamètre).", NogeeGenre::Sphere, NkFamilleBitOnglet(O::Formes)},
				{"Cylindre", "La primitive « cylinder » du moteur.", NogeeGenre::Cylindre, NkFamilleBitOnglet(O::Formes)},
				{"Plan", "La primitive « plane » du moteur.", NogeeGenre::Plan, NkFamilleBitOnglet(O::Formes)},
				{"Capsule", "La primitive « capsule » du moteur.", NogeeGenre::Capsule, NkFamilleBitOnglet(O::Formes)},
				{"Cône", "La primitive « cone » du moteur.", NogeeGenre::Cone, NkFamilleBitOnglet(O::Formes)},
			};
		} // namespace

		const char *NogeeNomOutil(NogeeOutil o) noexcept {
			switch (o) {
				case NogeeOutil::Deplacer: return "Déplacer";
				case NogeeOutil::Tourner:  return "Tourner";
				case NogeeOutil::Echelle:  return "Échelle";
				default:                   return "Sélection";
			}
		}

		const NogeeElement *NogeeCatalogue(int32 &nombre) noexcept {
			nombre = static_cast<int32>(sizeof(kCatalogue) / sizeof(kCatalogue[0]));
			return kCatalogue;
		}

		// =====================================================================
		// LE BRANCHEMENT ET LA SCENE
		// =====================================================================
		void NogeeModele::Brancher(NkWorld *monde, NkPhysicsSystem *physique) noexcept {
			mMonde = monde;
			mPhysique = physique;
			NkSceneSerializer::RegisterCoreComponents();
		}

		void NogeeModele::Vider() {
			if (!Pret()) {
				return;
			}
			if (mPhysique != nullptr) {
				mPhysique->ReleaseBodies(*mMonde);
			}
			NkVector<NkEntityId> ids;
			mMonde->Query<const NkTransform>().ForEach([&](NkEntityId id, const NkTransform &) { ids.PushBack(id); });
			mMonde->Query<const NkName>().ForEach([&](NkEntityId id, const NkName &) {
				for (uint32 i = 0; i < ids.Size(); ++i) {
					if (ids[i] == id) {
						return;
					}
				}
				ids.PushBack(id);
			});
			for (uint32 i = 0; i < ids.Size(); ++i) {
				if (mMonde->IsAlive(ids[i])) {
					mMonde->Destroy(ids[i]);
				}
			}
			cameraEditeur = NkEntityId::Invalid();
			mOrdre.Clear();
			mVerrous.Clear();
			Deselectionner();
		}

		void NogeeModele::NouvelleScene() {
			if (!Pret()) {
				return;
			}
			Vider();
			// La scene de NogeDemo, POSEE (en edition rien ne tombe ; « Jouer » la lance).
			(void)Poser(NogeeGenre::Sol, NkVec3f{0.f, -0.5f, 0.f}, "Sol");
			(void)Poser(NogeeGenre::CubePhysique, NkVec3f{0.f, 0.5f, 0.f}, "Joueur");
			(void)Poser(NogeeGenre::SpherePhysique, NkVec3f{0.15f, 3.f, 0.1f}, "Balle");
			{
				const NkEntityId cam = Poser(NogeeGenre::Camera, NkVec3f{6.f, 4.f, 8.f}, "Caméra");
				if (NkTransform *tf = mMonde->Get<NkTransform>(cam)) {
					tf->localRotation = NkQuatf::RotateY(NkAngle(35.f)) * NkQuatf::RotateX(NkAngle(-22.f));
				}
			}
			(void)Poser(NogeeGenre::LumiereDirectionnelle, NkVec3f{0.f, 6.f, 0.f}, "Soleil");
			CreerCameraEditeur();
			orbite = NogeeOrbite{};
			PoserCameraEditeur();
			mAvant.Clear();
			mApres.Clear();
			chemin = "Build/Nogee/scene.nkscene";
			modifie = false;
			Deselectionner();
			Annoncer("Nouvelle scène : la scène de NogeDemo (un sol, un cube, une balle, une caméra, un soleil)");
		}

		NkString NogeeModele::NomLibre(const char *base) {
			int32 pris = 0;
			const usize l = std::strlen(base);
			mMonde->Query<const NkName>().ForEach([&](NkEntityId, const NkName &n) {
				if (std::strncmp(n.value, base, l) == 0 && (n.value[l] == '\0' || n.value[l] == ' ')) {
					++pris;
				}
			});
			return pris == 0 ? NkString(base) : NkString::Format("%s %d", base, static_cast<int>(pris + 1));
		}

		NkEntityId NogeeModele::Poser(NogeeGenre g, const NkVec3f &position, const char *nom) {
			if (!Pret()) {
				return NkEntityId::Invalid();
			}
			auto Maillage = [&](NkEntityId e, const char *primitive, const NkColor4 &couleur) {
				NkMeshComponent mesh;
				mesh.meshPath = primitive;
				mMonde->Add<NkMeshComponent>(e, mesh);
				NkMaterialComponent mat;
				mat.SetColor(0, couleur);
				mMonde->Add<NkMaterialComponent>(e, mat);
			};
			auto Corps = [&](NkEntityId e, NkBodyType type, const NkCollider3D &col, float32 rebond) {
				NkRigidbody3D rb;
				rb.bodyType = type;
				rb.friction = 0.6f;
				rb.restitution = rebond;
				mMonde->Add<NkRigidbody3D>(e, rb);
				mMonde->Add<NkCollider3D>(e, col);
			};
			static const char *const kNoms[static_cast<int32>(NogeeGenre::Count)] = {
				"Acteur",	"Cube",					  "Sphère",				"Cylindre",	  "Plan",
				"Capsule",	"Cône",					  "Caméra",				"Soleil",	  "Lumière ponctuelle",
				"Projecteur", "Cube physique",		  "Sphère physique",	"Sol"};
			const NkString nomFinal = NomLibre(nom != nullptr ? nom : kNoms[static_cast<int32>(g)]);
			const NkEntityId e = mMonde->CreateEntity();
			mMonde->Add<NkName>(e, NkName(nomFinal.CStr()));
			NkTransform tf;
			tf.localPosition = position;
			mMonde->Add<NkTransform>(e, tf);
			switch (g) {
				case NogeeGenre::Cube:     Maillage(e, "primitive:cube", NkColor4(0.78f, 0.80f, 0.84f)); break;
				case NogeeGenre::Sphere:   Maillage(e, "primitive:sphere", NkColor4(0.78f, 0.80f, 0.84f)); break;
				case NogeeGenre::Cylindre: Maillage(e, "primitive:cylinder", NkColor4(0.78f, 0.80f, 0.84f)); break;
				case NogeeGenre::Plan:     Maillage(e, "primitive:plane", NkColor4(0.70f, 0.72f, 0.76f)); break;
				case NogeeGenre::Capsule:  Maillage(e, "primitive:capsule", NkColor4(0.78f, 0.80f, 0.84f)); break;
				case NogeeGenre::Cone:     Maillage(e, "primitive:cone", NkColor4(0.78f, 0.80f, 0.84f)); break;
				case NogeeGenre::Sol: {
					NkTransform *t = mMonde->Get<NkTransform>(e);
					t->localScale = NkVec3f{20.f, 1.f, 20.f};
					Maillage(e, "primitive:cube", NkColor4(0.36f, 0.42f, 0.38f));
					NkCollider3D col;
					col.shape = NkCollider3DShape::Box;
					col.boxSize = {20.f, 1.f, 20.f};
					Corps(e, NkBodyType::Static, col, 0.f);
					break;
				}
				case NogeeGenre::CubePhysique: {
					Maillage(e, "primitive:cube", NkColor4(0.86f, 0.16f, 0.12f));
					NkCollider3D col;
					col.shape = NkCollider3DShape::Box;
					col.boxSize = {1.f, 1.f, 1.f};
					Corps(e, NkBodyType::Dynamic, col, 0.f);
					break;
				}
				case NogeeGenre::SpherePhysique: {
					NkTransform *t = mMonde->Get<NkTransform>(e);
					t->localScale = NkVec3f{0.8f, 0.8f, 0.8f};
					Maillage(e, "primitive:sphere", NkColor4(0.95f, 0.78f, 0.18f));
					NkCollider3D col;
					col.shape = NkCollider3DShape::Sphere;
					col.sphereRadius = 0.4f;
					Corps(e, NkBodyType::Dynamic, col, 0.2f);
					break;
				}
				case NogeeGenre::Camera: {
					NkCameraComponent cam;
					cam.projection = NkCameraProjection::Perspective;
					cam.fovDeg = 55.f;
					cam.nearClip = 0.1f;
					cam.farClip = 300.f;
					cam.aspect = 0.f; // celui de la vue
					cam.priority = 10;
					mMonde->Add<NkCameraComponent>(e, cam);
					break;
				}
				case NogeeGenre::LumiereDirectionnelle:
				case NogeeGenre::LumierePonctuelle:
				case NogeeGenre::LumiereProjecteur: {
					NkLightComponent l;
					l.type = g == NogeeGenre::LumiereDirectionnelle ? NkLightType::Directional
							 : g == NogeeGenre::LumierePonctuelle	? NkLightType::Point
																	: NkLightType::Spot;
					l.color = g == NogeeGenre::LumiereDirectionnelle ? NkColor4(1.f, 0.96f, 0.9f) : NkColor4(1.f, 0.85f, 0.6f);
					l.intensity = g == NogeeGenre::LumiereDirectionnelle ? 3.f : 8.f;
					l.range = 10.f;
					l.castShadow = g == NogeeGenre::LumiereDirectionnelle;
					mMonde->Add<NkLightComponent>(e, l);
					NkTransform *t = mMonde->Get<NkTransform>(e);
					if (g == NogeeGenre::LumiereDirectionnelle) {
						t->localRotation = NkQuatf::RotateY(NkAngle(35.f)) * NkQuatf::RotateX(NkAngle(-55.f));
					} else if (g == NogeeGenre::LumiereProjecteur) {
						t->localRotation = NkQuatf::RotateX(NkAngle(-90.f)); // vers le bas
					}
					break;
				}
				default:
					break;
			}
			mOrdre.PushBack(e);
			return e;
		}

		NkEntityId NogeeModele::PoserElement(int32 k, const NkVec3f &position) {
			int32 n = 0;
			const NogeeElement *cat = NogeeCatalogue(n);
			if (!Pret() || k < 0 || k >= n) {
				return NkEntityId::Invalid();
			}
			Retenir();
			const NkEntityId e = Poser(cat[k].genre, position);
			Choisir(e);
			modifie = true;
			Annoncer(NkString::Format("Posé : %s", Nom(e)).CStr());
			return e;
		}

		void NogeeModele::Supprimer(NkEntityId id) {
			if (!Pret() || !mMonde->IsAlive(id) || EstEditeur(id)) {
				return;
			}
			Retenir();
			const NkString nom(Nom(id));
			// Les enfants partent avec le parent (Unreal : supprimer l'acteur et ses attaches).
			NkVector<NkEntityId> aDetruire;
			aDetruire.PushBack(id);
			for (uint32 i = 0; i < aDetruire.Size(); ++i) {
				if (const NkChildren *ch = mMonde->Get<NkChildren>(aDetruire[i])) {
					for (uint32 k = 0; k < ch->count; ++k) {
						aDetruire.PushBack(ch->children[k]);
					}
				}
			}
			Rattacher(id, NkEntityId::Invalid());
			if (mPhysique != nullptr) {
				mPhysique->ReleaseBodies(*mMonde);
			}
			for (uint32 i = 0; i < aDetruire.Size(); ++i) {
				if (mMonde->IsAlive(aDetruire[i])) {
					mMonde->Destroy(aDetruire[i]);
				}
			}
			Deselectionner();
			modifie = true;
			Annoncer(NkString::Format("Supprimé : %s", nom.CStr()).CStr());
		}

		NkEntityId NogeeModele::Dupliquer(NkEntityId id) {
			if (!Pret() || !mMonde->IsAlive(id) || EstEditeur(id)) {
				return NkEntityId::Invalid();
			}
			Retenir();
			const NkEntityId e = mMonde->CreateEntity();
			mMonde->Add<NkName>(e, NkName(NomLibre(Nom(id)).CStr()));
			if (const NkTransform *t = mMonde->Get<NkTransform>(id)) {
				NkTransform tf = *t;
				tf.localPosition = tf.localPosition + NkVec3f{1.f, 0.f, 0.f};
				tf.worldDirty = true;
				mMonde->Add<NkTransform>(e, tf);
			}
			if (const NkMeshComponent *m = mMonde->Get<NkMeshComponent>(id)) {
				NkMeshComponent mesh = *m;
				mesh.meshHandle = 0;
				mMonde->Add<NkMeshComponent>(e, mesh);
			}
			if (const NkMaterialComponent *m = mMonde->Get<NkMaterialComponent>(id)) {
				mMonde->Add<NkMaterialComponent>(e, *m);
			}
			if (const NkCameraComponent *c = mMonde->Get<NkCameraComponent>(id)) {
				mMonde->Add<NkCameraComponent>(e, *c);
			}
			if (const NkLightComponent *l = mMonde->Get<NkLightComponent>(id)) {
				mMonde->Add<NkLightComponent>(e, *l);
			}
			if (const NkRigidbody3D *rb = mMonde->Get<NkRigidbody3D>(id)) {
				mMonde->Add<NkRigidbody3D>(e, *rb);
			}
			if (const NkCollider3D *col = mMonde->Get<NkCollider3D>(id)) {
				NkCollider3D c = *col;
				c.physicsBodyId = 0;
				mMonde->Add<NkCollider3D>(e, c);
			}
			mOrdre.PushBack(e);
			Choisir(e);
			modifie = true;
			Annoncer(NkString::Format("Dupliqué : %s", Nom(e)).CStr());
			return e;
		}

		void NogeeModele::Renommer(NkEntityId id, const char *nom) {
			if (!Pret() || !mMonde->IsAlive(id) || nom == nullptr || nom[0] == '\0') {
				return;
			}
			if (std::strcmp(Nom(id), nom) == 0) {
				return;
			}
			Retenir();
			if (NkName *n = mMonde->Get<NkName>(id)) {
				*n = NkName(nom);
			} else {
				mMonde->Add<NkName>(id, NkName(nom));
			}
			modifie = true;
		}

		void NogeeModele::AssurerHierarchie(NkEntityId id) {
			if (!mMonde->Has<NkParent>(id)) {
				mMonde->Add<NkParent>(id);
			}
			if (!mMonde->Has<NkChildren>(id)) {
				mMonde->Add<NkChildren>(id);
			}
		}

		NkEntityId NogeeModele::Parent(NkEntityId id) const noexcept {
			return Pret() ? NkGetParent(*mMonde, id) : NkEntityId::Invalid();
		}

		void NogeeModele::Rattacher(NkEntityId enfant, NkEntityId parent) {
			if (!Pret() || !mMonde->IsAlive(enfant) || enfant == parent) {
				return;
			}
			// Une entite ne descend pas d'elle-meme.
			for (NkEntityId p = parent; p.IsValid(); p = NkGetParent(*mMonde, p)) {
				if (p == enfant) {
					Annoncer("Rattachement refusé : une entité ne descend pas d'elle-même", 2);
					return;
				}
			}
			AssurerHierarchie(enfant);
			NkParent *lien = mMonde->Get<NkParent>(enfant);
			if (lien->entity.IsValid() && mMonde->IsAlive(lien->entity)) {
				if (NkChildren *ancien = mMonde->Get<NkChildren>(lien->entity)) {
					ancien->Remove(enfant);
				}
			}
			NkTransform *tf = mMonde->Get<NkTransform>(enfant);
			const NkVec3f monde = tf != nullptr ? tf->worldPosition : NkVec3f{0.f, 0.f, 0.f};
			lien = mMonde->Get<NkParent>(enfant);
			lien->entity = parent;
			if (parent.IsValid() && mMonde->IsAlive(parent)) {
				AssurerHierarchie(parent);
				mMonde->Get<NkChildren>(parent)->Add(enfant);
				// L'enfant ne bouge pas a l'ecran : sa position devient relative.
				const NkTransform *tp = mMonde->Get<NkTransform>(parent);
				if (tf != nullptr && tp != nullptr) {
					const NkMat4f inv = tp->worldMatrix.Inverse();
					tf->localPosition = NkVec3f{inv[0][0] * monde.x + inv[1][0] * monde.y + inv[2][0] * monde.z + inv[3][0],
												inv[0][1] * monde.x + inv[1][1] * monde.y + inv[2][1] * monde.z + inv[3][1],
												inv[0][2] * monde.x + inv[1][2] * monde.y + inv[2][2] * monde.z + inv[3][2]};
				}
			} else if (tf != nullptr) {
				tf->localPosition = monde;
			}
			if (tf != nullptr) {
				tf->worldDirty = true;
			}
			modifie = true;
		}

		void NogeeModele::Entites(NkVector<NkEntityId> &sortie) {
			sortie.Clear();
			if (!Pret()) {
				return;
			}
			NkVector<NkEntityId> brut;
			mMonde->Query<const NkName>().ForEach([&](NkEntityId id, const NkName &n) {
				if (!CommencePar(n.value, "__")) {
					brut.PushBack(id);
				}
			});
			// L'ordre de la trame d'avant, les disparues retirees, les nouvelles a la
			// FIN : l'ECS range par archetype, ajouter un composant changerait la ligne.
			auto contient = [](const NkVector<NkEntityId> &v, NkEntityId e) {
				for (uint32 i = 0; i < v.Size(); ++i) {
					if (v[i] == e) {
						return true;
					}
				}
				return false;
			};
			for (uint32 i = 0; i < mOrdre.Size(); ++i) {
				if (contient(brut, mOrdre[i])) {
					sortie.PushBack(mOrdre[i]);
				}
			}
			for (uint32 i = 0; i < brut.Size(); ++i) {
				if (!contient(sortie, brut[i])) {
					sortie.PushBack(brut[i]);
				}
			}
			mOrdre = sortie;
		}

		const char *NogeeModele::Nom(NkEntityId id) const noexcept {
			if (!Pret() || !mMonde->IsAlive(id)) {
				return "";
			}
			const NkName *n = mMonde->Get<NkName>(id);
			return n != nullptr ? n->value : "Entité";
		}

		NogeeNature NogeeModele::Nature(NkEntityId id) const noexcept {
			if (!Pret() || !mMonde->IsAlive(id)) {
				return NogeeNature::Entite;
			}
			if (mMonde->Has<NkLightComponent>(id)) {
				return NogeeNature::Lumiere;
			}
			if (mMonde->Has<NkCameraComponent>(id)) {
				return NogeeNature::Camera;
			}
			if (const NkRigidbody3D *rb = mMonde->Get<NkRigidbody3D>(id)) {
				return rb->bodyType == NkBodyType::Dynamic ? NogeeNature::Rigide : NogeeNature::Decor;
			}
			if (mMonde->Has<NkMeshComponent>(id)) {
				return NogeeNature::Maillage;
			}
			return NogeeNature::Entite;
		}

		const char *NogeeModele::Type(NkEntityId id) const noexcept {
			switch (Nature(id)) {
				case NogeeNature::Lumiere:  return "lumière";
				case NogeeNature::Camera:   return "caméra";
				case NogeeNature::Rigide:   return "rigide";
				case NogeeNature::Decor:    return "décor";
				case NogeeNature::Maillage: return "maillage";
				default:                    return "entité";
			}
		}

		bool NogeeModele::EstEditeur(NkEntityId id) const noexcept {
			return CommencePar(Nom(id), "__");
		}

		bool NogeeModele::Boite(NkEntityId id, NkVec3f &mini, NkVec3f &maxi) const noexcept {
			if (!Pret() || !mMonde->IsAlive(id)) {
				return false;
			}
			const NkTransform *tf = mMonde->Get<NkTransform>(id);
			if (tf == nullptr) {
				return false;
			}
			const NkMat4f &m = tf->worldMatrix;
			const bool maillage = mMonde->Has<NkMeshComponent>(id);
			const float32 h = maillage ? 0.5f : 0.25f;
			for (int32 i = 0; i < 8; ++i) {
				const float32 x = (i & 1) ? h : -h;
				const float32 y = (i & 2) ? h : -h;
				const float32 z = (i & 4) ? h : -h;
				NkVec3f p;
				if (maillage) {
					p = NkVec3f{m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0], m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1],
								m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2]};
				} else {
					// Une entite sans maillage (camera, lumiere) : une petite boite
					// autour de sa position, quelle que soit son echelle.
					p = NkVec3f{m[3][0] + x, m[3][1] + y, m[3][2] + z};
				}
				if (i == 0) {
					mini = p;
					maxi = p;
				} else {
					mini = NkVec3f{NkMin(mini.x, p.x), NkMin(mini.y, p.y), NkMin(mini.z, p.z)};
					maxi = NkVec3f{NkMax(maxi.x, p.x), NkMax(maxi.y, p.y), NkMax(maxi.z, p.z)};
				}
			}
			return true;
		}

		// =====================================================================
		// LA SELECTION, L'ACTIF, L'OEIL, LE CADENAS
		// =====================================================================
		void NogeeModele::Choisir(NkEntityId id) noexcept {
			if (Pret() && mMonde->IsAlive(id) && !EstEditeur(id)) {
				selection = id;
				aSelection = true;
			}
		}

		void NogeeModele::Deselectionner() noexcept {
			selection = NkEntityId::Invalid();
			aSelection = false;
		}

		bool NogeeModele::SelectionValide() const noexcept {
			return aSelection && Pret() && mMonde->IsAlive(selection);
		}

		bool NogeeModele::EstActive(NkEntityId id) const noexcept {
			return Pret() && mMonde->IsAlive(id) && !mMonde->Has<NkInactive>(id);
		}

		void NogeeModele::Activer(NkEntityId id, bool actif) {
			if (!Pret() || !mMonde->IsAlive(id) || EstActive(id) == actif) {
				return;
			}
			Retenir();
			if (actif) {
				mMonde->Remove<NkInactive>(id);
			} else {
				mMonde->Add<NkInactive>(id);
			}
			modifie = true;
		}

		bool NogeeModele::EstCache(NkEntityId id) const noexcept {
			const NkMeshComponent *m = Pret() && mMonde->IsAlive(id) ? mMonde->Get<NkMeshComponent>(id) : nullptr;
			return m != nullptr && !m->visible;
		}

		void NogeeModele::BasculerCache(NkEntityId id) {
			NkMeshComponent *m = Pret() && mMonde->IsAlive(id) ? mMonde->Get<NkMeshComponent>(id) : nullptr;
			if (m == nullptr) {
				Annoncer("L'œil cache le maillage : cette entité n'en a pas", 2);
				return;
			}
			m->visible = !m->visible;
			modifie = true;
		}

		bool NogeeModele::EstVerrouille(NkEntityId id) const noexcept {
			for (uint32 i = 0; i < mVerrous.Size(); ++i) {
				if (mVerrous[i] == id) {
					return true;
				}
			}
			return false;
		}

		void NogeeModele::BasculerVerrou(NkEntityId id) {
			for (uint32 i = 0; i < mVerrous.Size(); ++i) {
				if (mVerrous[i] == id) {
					mVerrous.RemoveAt(i);
					return;
				}
			}
			mVerrous.PushBack(id);
		}

		// =====================================================================
		// JOUER
		// =====================================================================
		void NogeeModele::Jouer() {
			if (!Pret()) {
				return;
			}
			if (etat == NogeeEtatJeu::Pause) {
				etat = NogeeEtatJeu::Jeu;
				Annoncer("Reprise du jeu");
				return;
			}
			if (etat == NogeeEtatJeu::Jeu) {
				return;
			}
			// L'instantane d'AVANT : « Arreter » rend la scene telle qu'elle etait.
			mAvantJeuPris = Instantane(mAvantJeu);
			// Les corps se recreent aux poses de l'edition.
			if (mPhysique != nullptr) {
				mPhysique->ReleaseBodies(*mMonde);
			}
			etat = NogeeEtatJeu::Jeu;
			if (NkCameraComponent *c = mMonde->Get<NkCameraComponent>(cameraEditeur)) {
				c->priority = -1000; // la camera de la scene filme
			}
			Annoncer("Jeu lancé : la physique tourne (Échap ou ■ pour arrêter)");
		}

		void NogeeModele::Pause() {
			if (etat == NogeeEtatJeu::Jeu) {
				etat = NogeeEtatJeu::Pause;
				Annoncer("Jeu en pause");
			} else if (etat == NogeeEtatJeu::Pause) {
				etat = NogeeEtatJeu::Jeu;
				Annoncer("Reprise du jeu");
			}
		}

		void NogeeModele::Arreter() {
			if (etat == NogeeEtatJeu::Edition) {
				return;
			}
			etat = NogeeEtatJeu::Edition;
			const NkEntityId choix = selection;
			const NkString nomChoix(aSelection ? Nom(selection) : "");
			if (mAvantJeuPris) {
				(void)Restaurer(mAvantJeu);
				mAvantJeuPris = false;
			}
			// La selection survit si son entite existe encore (par son nom).
			(void)choix;
			Deselectionner();
			if (!nomChoix.Empty()) {
				mMonde->Query<const NkName>().ForEach([&](NkEntityId id, const NkName &n) {
					if (!aSelection && nomChoix == NkString(n.value)) {
						selection = id;
						aSelection = true;
					}
				});
			}
			Annoncer("Jeu arrêté : la scène est revenue à l'édition");
		}

		void NogeeModele::Pas() {
			if (etat == NogeeEtatJeu::Edition) {
				Jouer();
				etat = NogeeEtatJeu::Pause;
			}
			mPasDemande = true;
		}

		bool NogeeModele::PhysiqueAvance() noexcept {
			if (etat == NogeeEtatJeu::Jeu) {
				return true;
			}
			if (etat == NogeeEtatJeu::Pause && mPasDemande) {
				mPasDemande = false;
				return true;
			}
			return false;
		}

		// =====================================================================
		// LA CAMERA D'EDITION
		// =====================================================================
		void NogeeModele::CreerCameraEditeur() {
			if (!Pret()) {
				return;
			}
			if (mMonde->IsAlive(cameraEditeur)) {
				return;
			}
			cameraEditeur = mMonde->CreateEntity();
			mMonde->Add<NkName>(cameraEditeur, NkName(kNomCameraEditeur));
			mMonde->Add<NkTransform>(cameraEditeur);
			NkCameraComponent cam;
			cam.projection = NkCameraProjection::Perspective;
			cam.fovDeg = 55.f;
			cam.nearClip = 0.05f;
			cam.farClip = 500.f;
			cam.aspect = 0.f;
			cam.priority = etat == NogeeEtatJeu::Edition ? 1000 : -1000;
			mMonde->Add<NkCameraComponent>(cameraEditeur, cam);
		}

		void NogeeModele::DetruireCameraEditeur() {
			if (Pret() && mMonde->IsAlive(cameraEditeur)) {
				mMonde->Destroy(cameraEditeur);
			}
			cameraEditeur = NkEntityId::Invalid();
		}

		void NogeeModele::PoserCameraEditeur() {
			if (!Pret()) {
				return;
			}
			if (!mMonde->IsAlive(cameraEditeur)) {
				CreerCameraEditeur();
			}
			if (NkCameraComponent *c = mMonde->Get<NkCameraComponent>(cameraEditeur)) {
				c->priority = etat == NogeeEtatJeu::Edition ? 1000 : -1000;
			}
			NkTransform *tf = mMonde->Get<NkTransform>(cameraEditeur);
			if (tf == nullptr) {
				return;
			}
			orbite.tangageDeg = NkClamp(orbite.tangageDeg, -89.f, 89.f);
			orbite.distance = NkClamp(orbite.distance, 0.5f, 400.f);
			const float32 lacet = orbite.lacetDeg * kDegVersRad;
			const float32 tangage = orbite.tangageDeg * kDegVersRad;
			const NkVec3f dir{NkSin(lacet) * NkCos(tangage), NkSin(tangage), NkCos(lacet) * NkCos(tangage)};
			tf->localPosition = orbite.pivot + dir * orbite.distance;
			// Au lacet 0 la camera est sur +Z et regarde vers -Z (l'avant de
			// NkTransform) : le tangage l'incline vers le bas, le lacet la tourne.
			tf->localRotation = NkQuatf::RotateY(NkAngle(orbite.lacetDeg)) * NkQuatf::RotateX(NkAngle(-orbite.tangageDeg));
			// LA MATRICE MONDE EST POSEE ICI (la camera est une racine) : le systeme
			// de transforms est deja passe pour cette image (NogeDemoPoserCamera).
			tf->worldMatrix = tf->ComputeLocalMatrix();
			tf->worldPosition = tf->localPosition;
			tf->worldDirty = false;
		}

		void NogeeModele::Cadrer(NkEntityId id) {
			NkVec3f mini, maxi;
			if (!Boite(id, mini, maxi)) {
				return;
			}
			orbite.pivot = (mini + maxi) * 0.5f;
			const NkVec3f d = maxi - mini;
			const float32 rayon = NkSqrt(d.x * d.x + d.y * d.y + d.z * d.z) * 0.5f;
			orbite.distance = NkClamp(rayon * 2.6f + 1.f, 2.f, 200.f);
			PoserCameraEditeur();
		}

		// =====================================================================
		// LE FICHIER ET L'HISTORIQUE
		// =====================================================================
		bool NogeeModele::Instantane(NkArchive &a) {
			if (!Pret()) {
				return false;
			}
			// La camera d'edition n'est PAS la scene : elle sort de l'instantane.
			DetruireCameraEditeur();
			NkSceneGraph scene(*mMonde, "Nogee");
			NkSceneSerializer s;
			s.SetFormat(NkSerializationFormat::NK_JSON);
			const bool ok = s.SaveToArchive(scene, a);
			CreerCameraEditeur();
			PoserCameraEditeur();
			return ok;
		}

		bool NogeeModele::Restaurer(const NkArchive &a) {
			if (!Pret()) {
				return false;
			}
			const NkEntityId choix = selection;
			(void)choix;
			Vider();
			NkSceneGraph scene(*mMonde, "Nogee");
			NkSceneSerializer s;
			s.SetFormat(NkSerializationFormat::NK_JSON);
			const bool ok = s.LoadFromArchive(scene, a);
			CreerCameraEditeur();
			PoserCameraEditeur();
			return ok;
		}

		void NogeeModele::Retenir() {
			if (etat != NogeeEtatJeu::Edition) {
				return; // en jeu, rien ne s'annule : « Arreter » rend tout
			}
			NkArchive a;
			if (Instantane(a)) {
				mAvant.PushBack(a);
				while (mAvant.Size() > kMaxHistorique) {
					mAvant.RemoveAt(0);
				}
				mApres.Clear();
			}
		}

		bool NogeeModele::Annuler() {
			if (mAvant.Empty() || etat != NogeeEtatJeu::Edition) {
				return false;
			}
			NkArchive maintenant;
			if (Instantane(maintenant)) {
				mApres.PushBack(maintenant);
			}
			const NkArchive a = mAvant[mAvant.Size() - 1u];
			mAvant.PopBack();
			const NkString nomChoix(aSelection ? Nom(selection) : "");
			(void)Restaurer(a);
			modifie = true;
			if (!nomChoix.Empty()) {
				mMonde->Query<const NkName>().ForEach([&](NkEntityId id, const NkName &n) {
					if (!aSelection && nomChoix == NkString(n.value)) {
						selection = id;
						aSelection = true;
					}
				});
			}
			Annoncer("Annulé (Ctrl+Z)");
			return true;
		}

		bool NogeeModele::Refaire() {
			if (mApres.Empty() || etat != NogeeEtatJeu::Edition) {
				return false;
			}
			NkArchive maintenant;
			if (Instantane(maintenant)) {
				mAvant.PushBack(maintenant);
			}
			const NkArchive a = mApres[mApres.Size() - 1u];
			mApres.PopBack();
			(void)Restaurer(a);
			modifie = true;
			Annoncer("Rétabli (Ctrl+Y)");
			return true;
		}

		bool NogeeModele::Enregistrer() {
			if (!Pret()) {
				return false;
			}
			if (etat != NogeeEtatJeu::Edition) {
				Annoncer("Enregistrer : arrêtez d'abord le jeu (la scène en jeu n'est pas la scène éditée)", 2);
				return false;
			}
			PreparerDossier(chemin.CStr());
			DetruireCameraEditeur();
			NkSceneGraph scene(*mMonde, "Nogee");
			NkSceneSerializer s;
			s.SetFormat(NkSerializationFormat::NK_JSON);
			const bool ok = s.Save(scene, chemin.CStr());
			CreerCameraEditeur();
			PoserCameraEditeur();
			if (ok) {
				modifie = false;
				Annoncer(NkString::Format("Scène enregistrée : %s", chemin.CStr()).CStr(), 1);
			} else {
				Annoncer(NkString::Format("ÉCHEC de l'enregistrement : %s", chemin.CStr()).CStr(), 3);
			}
			return ok;
		}

		bool NogeeModele::Ouvrir(const char *fichier) {
			if (!Pret() || fichier == nullptr) {
				return false;
			}
			NkSceneSerializer s;
			s.SetFormat(NkSerializationFormat::NK_JSON);
			// Lire D'ABORD : un fichier illisible laisse la scene telle quelle.
			NkArchive archive;
			if (!s.ReadArchiveFile(fichier, archive)) {
				Annoncer(NkString::Format("Scène illisible : %s (rien n'a changé)", fichier).CStr(), 3);
				return false;
			}
			etat = NogeeEtatJeu::Edition;
			const bool ok = Restaurer(archive);
			mAvant.Clear();
			mApres.Clear();
			chemin = NkString(fichier);
			modifie = false;
			Annoncer(NkString::Format("Scène ouverte : %s", fichier).CStr(), ok ? 1 : 2);
			return ok;
		}

		// =====================================================================
		// LES ANNONCES
		// =====================================================================
		void NogeeModele::Annoncer(const char *texte, uint8 niveau) {
			message = NkString(texte != nullptr ? texte : "");
			messageAge = 0.f;
			aJournaliser.PushBack(message);
			niveaux.PushBack(niveau);
			logger.Infof("[Nogee] %s\n", message.CStr());
		}

	} // namespace nogee
} // namespace nkentseu
