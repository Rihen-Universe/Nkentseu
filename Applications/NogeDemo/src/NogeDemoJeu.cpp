//
// NogeDemoJeu.cpp
// =============================================================================
// Description :
//   La scene et le gameplay de NogeDemo, sans fenetre (voir NogeDemoJeu.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NogeDemoJeu.h"

#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Noge/ECS/Systems/NkPhysicsSystem.h"
#include "Noge/ECS/Systems/NkSceneSerializer.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nogedemo {

		using namespace ecs;
		using namespace math;

		namespace {

			constexpr float32 kDegVersRad = 3.14159265358979f / 180.f;

			float32 Borne(float32 v, float32 lo, float32 hi) noexcept {
				return v < lo ? lo : (v > hi ? hi : v);
			}

			/// Une entite visible : nom, transform, maillage primitif, couleur.
			NkEntityId Visible(NkWorld &w, const char *nom, const NkVec3f &pos, const NkVec3f &echelle,
							   const char *primitive, const NkColor4 &couleur) {
				const NkEntityId e = w.CreateEntity();
				w.Add<NkName>(e, NkName(nom));
				NkTransform tf;
				tf.localPosition = pos;
				tf.localScale = echelle;
				w.Add<NkTransform>(e, tf);
				NkMeshComponent mesh;
				mesh.meshPath = primitive;
				w.Add<NkMeshComponent>(e, mesh);
				NkMaterialComponent mat;
				mat.SetColor(0, couleur);
				w.Add<NkMaterialComponent>(e, mat);
				return e;
			}

			/// Un corps (rigide + collisionneur) sur une entite existante.
			void Corps(NkWorld &w, NkEntityId e, NkBodyType type, const NkCollider3D &col, float32 restitution = 0.f,
					   bool rotationFigee = false) {
				NkRigidbody3D rb;
				rb.bodyType = type;
				rb.friction = 0.6f;
				rb.restitution = restitution;
				rb.freezeRotX = rb.freezeRotY = rb.freezeRotZ = rotationFigee;
				w.Add<NkRigidbody3D>(e, rb);
				w.Add<NkCollider3D>(e, col);
			}

			bool CommencePar(const char *s, const char *prefixe) noexcept {
				return s != nullptr && std::strncmp(s, prefixe, std::strlen(prefixe)) == 0;
			}

		} // namespace

		// =====================================================================
		const char *NogeDemoTexteEntrees() noexcept {
			return "# NogeDemo -- les entrees du jeu (format de NkInputMap::Load, NKEvent)\n"
				   "# Les touches sont des POSITIONS : Key:W est le Z d'un clavier AZERTY.\n"
				   "# Modifier ce fichier puis relancer : le jeu lit ses ACTIONS, jamais les touches.\n"
				   "action Deplacer axe2\n"
				   "action OrbiterSouris axe2\n"
				   "action OrbiterClavier axe2\n"
				   "action Zoom axe1\n"
				   "action Sauter bouton\n"
				   "action Lacher bouton\n"
				   "action Sauver bouton\n"
				   "action Charger bouton\n"
				   "action Quitter bouton\n"
				   "contexte Jeu priorite=0 consomme\n"
				   "lier Deplacer Composite:Key:D,Key:A,Key:W,Key:S\n"
				   "lier Deplacer Stick:LEFT zmr=0.2\n"
				   // La souris ne tourne la camera que bouton DROIT tenu (accord).
				   // ⚠️ L'axe 2D est borne a une longueur 1 : l'echelle ramene les
				   //    pixels de l'image sous cette borne (200 px = 1).
				   "lier OrbiterSouris MouseDelta echelle=0.005,0.005 accord=Mouse:RIGHT\n"
				   "lier OrbiterClavier Composite:Key:RIGHT,Key:LEFT,Key:UP,Key:DOWN\n"
				   "lier OrbiterClavier Stick:RIGHT zmr=0.2\n"
				   "lier Zoom Wheel:V\n"
				   "lier Sauter Key:SPACE quand=presse\n"
				   "lier Sauter Gamepad:SOUTH quand=presse\n"
				   "lier Lacher Key:E quand=presse\n"
				   "lier Lacher Gamepad:WEST quand=presse\n"
				   "lier Sauver Key:F5 quand=presse\n"
				   "lier Charger Key:F9 quand=presse\n"
				   "lier Quitter Key:ESCAPE quand=presse\n";
		}

		// =====================================================================
		bool NogeDemoDeclarerEntrees(NkInputMap &carte, NkDemoEtat &etat, const char *texte, NkString *erreur) {
			const NkInputMapReport r = carte.Load(NkString(texte != nullptr ? texte : ""));
			if (!r.Ok()) {
				if (erreur != nullptr) {
					*erreur = r.errors[0];
				}
				return false;
			}
			NkDemoActions &a = etat.actions;
			a.deplacer = carte.FindAction("Deplacer");
			a.orbiterSouris = carte.FindAction("OrbiterSouris");
			a.orbiterClavier = carte.FindAction("OrbiterClavier");
			a.zoom = carte.FindAction("Zoom");
			a.sauter = carte.FindAction("Sauter");
			a.lacher = carte.FindAction("Lacher");
			a.sauver = carte.FindAction("Sauver");
			a.charger = carte.FindAction("Charger");
			a.quitter = carte.FindAction("Quitter");
			const NkInputActionId toutes[] = {a.deplacer, a.orbiterSouris, a.orbiterClavier, a.zoom,   a.sauter,
											  a.lacher,	  a.sauver,		   a.charger,		 a.quitter};
			for (NkInputActionId id : toutes) {
				if (id == NK_INPUT_ACTION_INVALID) {
					if (erreur != nullptr) {
						*erreur = "une action du jeu manque dans le texte d'entrees";
					}
					return false;
				}
			}
			return true;
		}

		// =====================================================================
		void NogeDemoConstruireScene(NkWorld &w) {
			// Le sol : une boite statique de 20 x 1 x 20 m, dessus a y = 0.
			{
				const NkEntityId sol = Visible(w, kNomSol, {0.f, -kSolEpaisseur * 0.5f, 0.f},
											   {kSolDemiLargeur * 2.f, kSolEpaisseur, kSolDemiLargeur * 2.f},
											   "primitive:cube", NkColor4(0.36f, 0.42f, 0.38f));
				NkCollider3D col;
				col.shape = NkCollider3DShape::Box;
				col.boxSize = {kSolDemiLargeur * 2.f, kSolEpaisseur, kSolDemiLargeur * 2.f};
				col.layer = kCoucheDecor;
				Corps(w, sol, NkBodyType::Static, col);
			}
			// Le joueur : un cube rouge d'un metre, qui TOMBE au depart. Sa
			// rotation est figee (NK_BODY_FIXED_ROT) : pousse, il glisse sans
			// basculer.
			{
				const NkEntityId j = Visible(w, kNomJoueur, {0.f, kJoueurDepartY, 0.f},
											 {kJoueurCote, kJoueurCote, kJoueurCote}, "primitive:cube",
											 NkColor4(0.86f, 0.16f, 0.12f));
				NkCollider3D col;
				col.shape = NkCollider3DShape::Box;
				col.boxSize = {kJoueurCote, kJoueurCote, kJoueurCote};
				col.layer = kCoucheJoueur;
				Corps(w, j, NkBodyType::Dynamic, col, 0.f, true);
			}
			// La balle : jaune, a l'aplomb du joueur, plus haut -- elle lui tombe
			// DESSUS (collision entre deux corps dynamiques).
			{
				const NkEntityId b = Visible(w, kNomBalle, {0.15f, kBalleDepartY, 0.1f},
											 {kBalleRayon * 2.f, kBalleRayon * 2.f, kBalleRayon * 2.f},
											 "primitive:sphere", NkColor4(0.95f, 0.78f, 0.18f));
				NkCollider3D col;
				col.shape = NkCollider3DShape::Sphere;
				col.sphereRadius = kBalleRayon;
				col.layer = kCoucheDecor;
				Corps(w, b, NkBodyType::Dynamic, col, 0.2f);
			}
			// La camera (le transform est pose par l'orbite).
			{
				const NkEntityId c = w.CreateEntity();
				w.Add<NkName>(c, NkName(kNomCamera));
				w.Add<NkTransform>(c);
				NkCameraComponent cam;
				cam.projection = NkCameraProjection::Perspective;
				cam.fovDeg = 55.f;
				cam.nearClip = 0.1f;
				cam.farClip = 300.f;
				cam.aspect = 0.f; // 0 = celui de la fenetre (NkRenderSystem)
				cam.priority = 10;
				w.Add<NkCameraComponent>(c, cam);
				NogeDemoPoserCamera(w, NkDemoCameraOrbite{});
			}
			// Le soleil : une lumiere directionnelle qui descend en biais.
			{
				const NkEntityId s = w.CreateEntity();
				w.Add<NkName>(s, NkName(kNomSoleil));
				NkTransform tf;
				tf.localRotation = NkQuatf::RotateY(NkAngle(35.f)) * NkQuatf::RotateX(NkAngle(-55.f));
				w.Add<NkTransform>(s, tf);
				NkLightComponent l;
				l.type = NkLightType::Directional;
				l.color = NkColor4(1.f, 0.96f, 0.9f);
				l.intensity = 3.f;
				l.castShadow = true;
				w.Add<NkLightComponent>(s, l);
			}
		}

		// =====================================================================
		NkEntityId NogeDemoTrouver(NkWorld &w, const char *nom) {
			NkEntityId trouve = NkEntityId::Invalid();
			if (nom == nullptr) {
				return trouve;
			}
			w.Query<const NkName>().ForEach([&](NkEntityId id, const NkName &n) {
				if (!trouve.IsValid() && std::strcmp(n.value, nom) == 0) {
					trouve = id;
				}
			});
			return trouve;
		}

		// =====================================================================
		void NogeDemoPoserCamera(NkWorld &w, const NkDemoCameraOrbite &c) {
			const NkEntityId id = NogeDemoTrouver(w, kNomCamera);
			NkTransform *tf = id.IsValid() ? w.Get<NkTransform>(id) : nullptr;
			if (tf == nullptr) {
				return;
			}
			const float32 lacet = c.lacetDeg * kDegVersRad;
			const float32 tangage = c.tangageDeg * kDegVersRad;
			const NkVec3f dir{NkSin(lacet) * NkCos(tangage), NkSin(tangage), NkCos(lacet) * NkCos(tangage)};
			tf->localPosition = c.pivot + dir * c.distance;
			// Au lacet 0 la camera est sur +Z et regarde vers -Z (l'avant de
			// NkTransform) : le tangage l'incline vers le bas, le lacet la tourne.
			tf->localRotation = NkQuatf::RotateY(NkAngle(c.lacetDeg)) * NkQuatf::RotateX(NkAngle(-c.tangageDeg));
			// ⚠️ LA MATRICE MONDE EST POSEE ICI, pas a l'image suivante : le
			//    systeme de transforms est deja passe pour cette image (il tourne
			//    dans NkEngineLayer::OnUpdate, AVANT le jeu), et le rendu lirait
			//    la pose d'avant -- une camera en retard d'une image. La camera
			//    est une RACINE (pas de parent) : sa matrice monde est sa matrice
			//    locale.
			tf->worldMatrix = tf->ComputeLocalMatrix();
			tf->worldPosition = tf->localPosition;
			tf->worldDirty = false;
		}

		// =====================================================================
		void NogeDemoPas(NkDemoEtat &e, NkWorld &w, const NkInputMap &carte, float32 dt) {
			const NkDemoActions &a = e.actions;

			// --- La camera ---------------------------------------------------
			// Souris (bouton droit tenu) : un DEPLACEMENT de l'image, pas une vitesse.
			const NkVec2f souris = carte.Value2D(a.orbiterSouris);
			e.camera.lacetDeg -= souris.x * e.orbiteSourisDeg;
			e.camera.tangageDeg += souris.y * e.orbiteSourisDeg;
			// Clavier / stick droit : une VITESSE, multipliee par dt.
			const NkVec2f clavier = carte.Value2D(a.orbiterClavier);
			e.camera.lacetDeg += clavier.x * e.vitesseOrbiteDeg * dt;
			e.camera.tangageDeg += clavier.y * e.vitesseOrbiteDeg * dt;
			e.camera.tangageDeg = Borne(e.camera.tangageDeg, 5.f, 85.f);
			e.camera.distance = Borne(e.camera.distance - carte.Value(a.zoom) * e.pasZoom, 3.f, 60.f);
			NogeDemoPoserCamera(w, e.camera);

			// --- Le joueur -----------------------------------------------------
			// L'axe « Deplacer » est lu dans le repere de la CAMERA, a plat : avant
			// = la ou elle regarde. On pose la VITESSE horizontale sur le corps ;
			// le pont physique la donne au solveur (qui garde les collisions).
			const NkEntityId joueur = NogeDemoTrouver(w, kNomJoueur);
			NkRigidbody3D *rb = joueur.IsValid() ? w.Get<NkRigidbody3D>(joueur) : nullptr;
			if (rb != nullptr) {
				const float32 lacet = e.camera.lacetDeg * kDegVersRad;
				const NkVec3f avant{-NkSin(lacet), 0.f, -NkCos(lacet)};
				const NkVec3f droite{NkCos(lacet), 0.f, -NkSin(lacet)};
				const NkVec2f d = carte.Value2D(a.deplacer);
				const NkVec3f v = (droite * d.x + avant * d.y) * e.vitesseJoueur;
				rb->velocity.x = v.x;
				rb->velocity.z = v.z;
				// Sauter : seulement s'il ne monte ni ne tombe deja.
				if (carte.WasPressed(a.sauter) && NkFabs(rb->velocity.y) < 0.3f) {
					rb->velocity.y = e.impulsionSaut;
				}
			}

			if (carte.WasPressed(a.lacher)) {
				(void)NogeDemoLacherCaisse(w, e);
			}
			if (carte.WasPressed(a.sauver)) {
				e.demandeSauver = true;
			}
			if (carte.WasPressed(a.charger)) {
				e.demandeCharger = true;
			}
			if (carte.WasPressed(a.quitter)) {
				e.demandeQuitter = true;
			}
		}

		// =====================================================================
		NkEntityId NogeDemoLacherCaisse(NkWorld &w, NkDemoEtat &e) {
			if (e.caissesLachees >= kCaissesMax) {
				return NkEntityId::Invalid();
			}
			NkVec3f au{0.f, 0.f, 0.f};
			const NkEntityId joueur = NogeDemoTrouver(w, kNomJoueur);
			if (const NkTransform *tj = joueur.IsValid() ? w.Get<NkTransform>(joueur) : nullptr) {
				au = tj->localPosition;
			}
			const int32 k = e.caissesLachees;
			char nom[32];
			std::snprintf(nom, sizeof(nom), "%s%02d", kPrefixeCaisse, static_cast<int>(k));
			// Un leger decalage par caisse : elles ne tombent pas en colonne parfaite.
			const float32 dx = 0.18f * static_cast<float32>((k % 3) - 1);
			const float32 dz = 0.18f * static_cast<float32>(((k / 3) % 3) - 1);
			const float32 cote = 0.7f;
			const NkEntityId c = Visible(w, nom, {au.x + dx, 6.f + 0.2f * static_cast<float32>(k), au.z + dz},
										 {cote, cote, cote}, "primitive:cube",
										 NkColor4(0.2f + 0.05f * static_cast<float32>(k % 4), 0.45f, 0.85f));
			NkCollider3D col;
			col.shape = NkCollider3DShape::Box;
			col.boxSize = {cote, cote, cote};
			col.layer = kCoucheDecor;
			Corps(w, c, NkBodyType::Dynamic, col, 0.05f);
			++e.caissesLachees;
			return c;
		}

		// =====================================================================
		void NogeDemoViderMonde(NkWorld &w, NkPhysicsSystem *physique) {
			if (physique != nullptr) {
				physique->ReleaseBodies(w);
			}
			NkVector<NkEntityId> ids;
			w.Query<const NkTransform>().ForEach([&](NkEntityId id, const NkTransform &) { ids.PushBack(id); });
			w.Query<const NkName>().ForEach([&](NkEntityId id, const NkName &) {
				for (nk_usize i = 0; i < ids.Size(); ++i) {
					if (ids[i] == id) {
						return;
					}
				}
				ids.PushBack(id);
			});
			for (nk_usize i = 0; i < ids.Size(); ++i) {
				if (w.IsAlive(ids[i])) {
					w.Destroy(ids[i]);
				}
			}
		}

		// =====================================================================
		bool NogeDemoSauver(NkWorld &w, const char *chemin) {
			if (chemin == nullptr) {
				return false;
			}
			NkSceneSerializer::RegisterCoreComponents();
			NkSceneGraph scene(w, "NogeDemo");
			NkSceneSerializer s;
			s.SetFormat(NkSerializationFormat::NK_JSON);
			return s.Save(scene, chemin);
		}

		// =====================================================================
		bool NogeDemoCharger(NkWorld &w, NkPhysicsSystem *physique, const char *chemin, NkDemoEtat &e) {
			if (chemin == nullptr) {
				return false;
			}
			NkSceneSerializer::RegisterCoreComponents();
			NkSceneSerializer s;
			s.SetFormat(NkSerializationFormat::NK_JSON);
			// Lire D'ABORD : un fichier illisible laisse le monde tel quel.
			NkArchive archive;
			if (!s.ReadArchiveFile(chemin, archive)) {
				logger.Warnf("[NogeDemo] scene illisible : {0} (le monde reste tel quel)\n", chemin);
				return false;
			}
			NogeDemoViderMonde(w, physique);
			NkSceneGraph scene(w, "NogeDemo");
			if (!s.LoadFromArchive(scene, archive)) {
				return false;
			}
			// Les caisses : le compteur reprend apres la plus haute relue.
			int32 plusHaute = -1;
			w.Query<const NkName>().ForEach([&](NkEntityId, const NkName &n) {
				if (CommencePar(n.value, kPrefixeCaisse)) {
					int32 k = 0;
					if (std::sscanf(n.value + std::strlen(kPrefixeCaisse), "%d", &k) == 1 && k > plusHaute) {
						plusHaute = k;
					}
				}
			});
			e.caissesLachees = plusHaute + 1;
			NogeDemoPoserCamera(w, e.camera);
			return true;
		}

		// =====================================================================
		int32 NogeDemoCompterNommees(NkWorld &w) {
			int32 n = 0;
			w.Query<const NkName>().ForEach([&](NkEntityId, const NkName &) { ++n; });
			return n;
		}

	} // namespace nogedemo
} // namespace nkentseu
