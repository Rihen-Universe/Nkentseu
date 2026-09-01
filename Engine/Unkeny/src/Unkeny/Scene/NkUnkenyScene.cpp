// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Scene/NkUnkenyScene.cpp
// DESCRIPTION: La scene 2D. Elle COMPOSE NKECS, NKPhysics et NKCollision.
//
// LE PONT 2D <-> 3D, ET IL N'EXISTE QU'ICI
//   NKPhysics et NKCollision travaillent en 3D. Unkeny travaille dans le plan
//   XY. La conversion (z = 0, angle autour de Z) est faite dans CE fichier et
//   nulle part ailleurs : deux convertisseurs divergeraient au premier ajout
//   d'axe, et le defaut sortirait comme « les objets tombent de travers ».
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Scene/NkUnkenyScene.h"

#include "NKLogger/NkLog.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			/// L'angle 2D d'un quaternion qui ne tourne QUE autour de Z.
			/// ⚠️ Valable parce que le pont n'ecrit jamais d'autre rotation. Si un
			/// jour un corps tourne autour d'un autre axe, cette fonction rendra
			/// un angle plausible et FAUX — c'est pour cela qu'elle vit ici, a
			/// cote de l'endroit qui garantit l'hypothese.
			float32 AngleZ(const math::NkQuatf &q) noexcept {
				return 2.f * math::NkAtan2(q.z, q.w);
			}

			math::NkQuatf DepuisAngleZ(float32 angle) noexcept {
				math::NkQuatf q;
				q.x = 0.f;
				q.y = 0.f;
				q.z = math::NkSin(angle * 0.5f);
				q.w = math::NkCos(angle * 0.5f);
				return q;
			}

			/// Traduit un collisionneur 2D en NkShape. Le centre est en MONDE :
			/// NKCollision place ses formes en absolu.
			collision::NkShape VersShape(const NkCollisionneur2D &col, const NkVec2f &centre) noexcept {
				const NkVec2f c(centre.x + col.decalage.x, centre.y + col.decalage.y);
				switch (col.forme) {
					case NkForme2D::NK_CERCLE:
						return collision::NkShape::Circle2D(c, col.rayon);
					case NkForme2D::NK_CAPSULE: {
						// La capsule est un segment horizontal + un rayon : c'est
						// la forme d'un personnage, et elle ne s'accroche pas aux
						// jointures du sol comme le ferait une boite.
						const NkVec2f a(c.x - col.demiTaille.x, c.y);
						const NkVec2f b(c.x + col.demiTaille.x, c.y);
						return collision::NkShape::Capsule2D(a, b, col.rayon);
					}
					default:
						return collision::NkShape::Box2D(c, col.demiTaille, 0.f);
				}
			}
		} // namespace

		// =====================================================================
		NkScene::~NkScene() {
			Liberer();
		}

		bool NkScene::Init(const NkSceneConfig &config) {
			Liberer();
			mConfig = config;

			if (mConfig.physique) {
				// ⚠️ Alloue par NKMemory, jamais par new : melanger l'allocateur
				// maison et le tas CRT corrompt le tas sous Windows (c0000374).
				mPhysique = memory::NkGetDefaultAllocator().New<physics::NkPhysicsWorld>();
				if (mPhysique == nullptr) {
					logger.Error("[unkeny] creation du monde physique IMPOSSIBLE");
					return false;
				}
				mPhysique->SetGravity(math::NkVec3f(mConfig.gravite.x, mConfig.gravite.y, 0.f));
				logger.Info("[unkeny] scene avec physique, gravite ({0}, {1}), pas fixe {2} s", mConfig.gravite.x,
							mConfig.gravite.y, mConfig.pasFixe);
			} else {
				// On le DIT. Un moteur qui ignore une demande en silence fait
				// chercher le defaut ailleurs pendant des heures.
				logger.Info("[unkeny] scene SANS physique (NkSceneConfig::physique = false)");
			}
			return true;
		}

		void NkScene::Liberer() {
			if (mPhysique != nullptr) {
				memory::NkGetDefaultAllocator().Delete(mPhysique);
				mPhysique = nullptr;
			}
			mAccumulateur = 0.f;
			mDernierNbPas = 0;
		}

		// =====================================================================
		ecs::NkEntityId NkScene::Creer(const NkVec2f &position) {
			const ecs::NkEntityId id = mMonde.CreateEntity();
			NkTransform2D t;
			t.position = position;
			mMonde.Add<NkTransform2D>(id, t);
			return id;
		}

		ecs::NkEntityId NkScene::Creer(const char *nom, const NkVec2f &position) {
			const ecs::NkEntityId id = Creer(position);
			NkEtiquette e;
			if (nom != nullptr) {
				int32 i = 0;
				// Copie bornee a la main : le nom est un tableau FIXE, et une
				// copie non bornee ecrirait dans le composant suivant.
				for (; i < 31 && nom[i] != '\0'; ++i) {
					e.nom[i] = nom[i];
				}
				e.nom[i] = '\0';
			}
			mMonde.Add<NkEtiquette>(id, e);
			return id;
		}

		void NkScene::Detruire(ecs::NkEntityId id) {
			// ⚠️ Le corps physique D'ABORD. Detruire l'entite seule laisserait un
			// corps orphelin qui continue de collisionner avec du vide : rien ne
			// plante, et des objets invisibles bloquent le passage.
			if (mPhysique != nullptr) {
				if (NkCorps2D *c = mMonde.Get<NkCorps2D>(id)) {
					if (c->corpsId != physics::NK_INVALID_BODY) {
						mPhysique->DestroyBody(c->corpsId);
						c->corpsId = physics::NK_INVALID_BODY;
					}
				}
			}
			mMonde.Destroy(id);
		}

		// =====================================================================
		bool NkScene::AjouterCorps(ecs::NkEntityId id, const NkCorps2D &corps) {
			if (mPhysique == nullptr) {
				logger.Warn("[unkeny] AjouterCorps refuse : la scene n'a pas de physique");
				return false;
			}
			const NkTransform2D *t = mMonde.Get<NkTransform2D>(id);
			const NkCollisionneur2D *col = mMonde.Get<NkCollisionneur2D>(id);
			if (t == nullptr || col == nullptr) {
				// Un refus se DIT, et il dit CE QUI MANQUE. « rend false » sans
				// raison envoie chercher le defaut dans le solveur.
				logger.Warn("[unkeny] AjouterCorps refuse : il manque {0}",
							t == nullptr ? "NkTransform2D" : "NkCollisionneur2D");
				return false;
			}

			physics::NkBodyDef def;
			switch (corps.type) {
				case NkTypeCorps::NK_STATIQUE: def.type = physics::NkBodyType::STATIC; break;
				case NkTypeCorps::NK_CINEMATIQUE: def.type = physics::NkBodyType::KINEMATIC; break;
				default: def.type = physics::NkBodyType::DYNAMIC; break;
			}
			def.position = math::NkVec3f(t->position.x, t->position.y, 0.f);
			def.orientation = DepuisAngleZ(t->rotation);
			def.linearDamping = corps.amortissementLineaire;
			def.angularDamping = corps.amortissementAngulaire;
			def.gravityScale = corps.echelleGravite;
			def.layer = col->couche;
			def.mask = col->masque;
			if (corps.rotationBloquee) {
				def.flags |= physics::NK_BODY_FIXED_ROT;
			}
			if (col->declencheur) {
				def.flags |= physics::NK_BODY_TRIGGER;
			}

			const physics::NkBodyId bid = mPhysique->CreateBody(def, VersShape(*col, t->position));
			if (bid == physics::NK_INVALID_BODY) {
				logger.Error("[unkeny] le solveur a refuse le corps");
				return false;
			}

			NkCorps2D copie = corps;
			copie.corpsId = bid;
			// ⚠️ Add, PAS Set. `Set<T>` ecrit dans un composant EXISTANT ; sur
			// une entite qui n'en a pas, il ne fait RIEN — sans erreur, sans
			// avertissement.
			//
			// Defaut mesure le 2026-09-01, et il est instructif : le solveur
			// simulait bel et bien ses sept corps, la gravite s'appliquait, tout
			// etait juste de ce cote. Mais aucune entite ne portait NkCorps2D,
			// donc la requete de synchronisation n'appariait RIEN et aucune
			// position ne revenait dans les transforms. A l'ecran : des caisses
			// parfaitement immobiles au-dessus d'un sol, et une physique
			// « qui ne marche pas ».
			//
			// Ce qui l'a trouve n'est pas une relecture — le code se lisait
			// bien. C'est une sonde qui imprimait l'etat d'un corps et qui n'a
			// rien imprime du tout : le VIDE etait la mesure.
			if (mMonde.Has<NkCorps2D>(id)) {
				mMonde.Set<NkCorps2D>(id, copie);
			} else {
				mMonde.Add<NkCorps2D>(id, copie);
			}
			return true;
		}

		void NkScene::TeleporterEntite(ecs::NkEntityId id, const NkVec2f &position) {
			if (NkTransform2D *t = mMonde.Get<NkTransform2D>(id)) {
				t->position = position;
			}
			// Le solveur doit etre prevenu : sans cela il continue depuis
			// l'ancienne position et l'objet revient d'un coup au pas suivant.
			if (mPhysique != nullptr) {
				if (const NkCorps2D *c = mMonde.Get<NkCorps2D>(id)) {
					if (physics::NkRigidBody *b = mPhysique->GetBody(c->corpsId)) {
						b->position = math::NkVec3f(position.x, position.y, 0.f);
						b->linearVelocity = math::NkVec3f(0.f, 0.f, 0.f);
					}
				}
			}
		}

		void NkScene::PoserVitesse(ecs::NkEntityId id, const NkVec2f &vitesse) {
			if (mPhysique != nullptr) {
				if (const NkCorps2D *c = mMonde.Get<NkCorps2D>(id)) {
					mPhysique->SetLinearVelocity(c->corpsId, math::NkVec3f(vitesse.x, vitesse.y, 0.f));
					return;
				}
			}
			// Pas de corps physique : la vitesse manuelle prend le relais, et le
			// resultat est le meme pour l'appelant. C'est ce qui permet a un jeu
			// de commencer sans physique et d'en ajouter plus tard.
			NkVitesse2D v;
			v.lineaire = vitesse;
			mMonde.Set<NkVitesse2D>(id, v);
		}

		NkVec2f NkScene::Vitesse(ecs::NkEntityId id) const {
			if (mPhysique != nullptr) {
				if (const NkCorps2D *c = mMonde.Get<NkCorps2D>(id)) {
					if (const physics::NkRigidBody *b = mPhysique->GetBody(c->corpsId)) {
						return NkVec2f(b->linearVelocity.x, b->linearVelocity.y);
					}
				}
			}
			if (const NkVitesse2D *v = mMonde.Get<NkVitesse2D>(id)) {
				return v->lineaire;
			}
			return NkVec2f(0.f, 0.f);
		}

		// =====================================================================
		// Pas — pas FIXE, avec plafond de rattrapage
		// =====================================================================
		void NkScene::Pas(float32 deltaTime) {
			mDernierNbPas = 0;
			if (mPhysique != nullptr && mConfig.pasFixe > 0.f) {
				mAccumulateur += deltaTime;
				while (mAccumulateur >= mConfig.pasFixe && mDernierNbPas < mConfig.pasMaxParTrame) {
					mPhysique->Step(mConfig.pasFixe);
					mAccumulateur -= mConfig.pasFixe;
					++mDernierNbPas;
				}
				// ⚠️ Le retard qui reste est JETE, pas garde. Le garder ferait
				// rejouer des dizaines de pas a la trame suivante — et des corps
				// rapides traverseraient les murs. On perd du temps simule
				// plutot que la coherence.
				if (mAccumulateur > mConfig.pasFixe * static_cast<float32>(mConfig.pasMaxParTrame)) {
					mAccumulateur = 0.f;
				}
				SynchroniserDepuisPhysique();
			}

			AppliquerVitessesManuelles(deltaTime);
		}

		void NkScene::SynchroniserDepuisPhysique() {
			// SENS UNIQUE : physique -> transform. L'inverse passerait par
			// TeleporterEntite, qui previent le solveur.
			physics::NkPhysicsWorld *monde = mPhysique;
			mMonde.Query<NkTransform2D, NkCorps2D>().ForEach(
				[monde](ecs::NkEntityId, NkTransform2D &t, NkCorps2D &c) {
					if (c.corpsId == physics::NK_INVALID_BODY) {
						return;
					}
					if (const physics::NkRigidBody *b = monde->GetBody(c.corpsId)) {
						t.position = NkVec2f(b->position.x, b->position.y);
						t.rotation = AngleZ(b->orientation);
					}
				});
		}

		void NkScene::AppliquerVitessesManuelles(float32 dt) {
			mMonde.Query<NkTransform2D, NkVitesse2D>().ForEach(
				[dt](ecs::NkEntityId, NkTransform2D &t, NkVitesse2D &v) {
					t.position.x += v.lineaire.x * dt;
					t.position.y += v.lineaire.y * dt;
					t.rotation += v.angulaire * dt;
				});
		}

	} // namespace unkeny
} // namespace nkentseu
