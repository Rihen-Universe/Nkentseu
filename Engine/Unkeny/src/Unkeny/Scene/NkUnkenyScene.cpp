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
#include "Unkeny/Anim/NkUnkenyAnimateur.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Son/NkUnkenySon.h"

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
			// Les composants d'Unkeny que NkPhotoEntite ne nomme pas passent par
			// le meme chemin que ceux d'un jeu.
			PhotographierAussi<NkAnimSprite2D>("NkAnimSprite2D");
			PhotographierAussi<NkAnimateur2D>("NkAnimateur2D");
			PhotographierAussi<NkVitesse2D>("NkVitesse2D");
			PhotographierAussi<NkSource2D>("NkSource2D");

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
			if (mConfig.particules) {
				if (mPhysique == nullptr) {
					// Des particules sans monde rigide ne toucheraient AUCUN sol pose en
					// corps statique : on cree le monde plutot que de rendre une scene
					// ou tout tombe a travers le decor. Et on le dit.
					logger.Warn("[unkeny] particules demandees SANS physique : le monde physique est cree quand meme");
					mConfig.physique = true;
					mPhysique = memory::NkGetDefaultAllocator().New<physics::NkPhysicsWorld>();
					if (mPhysique == nullptr) {
						return false;
					}
					mPhysique->SetGravity(math::NkVec3f(mConfig.gravite.x, mConfig.gravite.y, 0.f));
				}
				mParticules = memory::NkGetDefaultAllocator().New<physics::NkParticules2D>();
				if (mParticules == nullptr) {
					logger.Error("[unkeny] creation du monde de particules IMPOSSIBLE");
					return false;
				}
				mParticules->reglages.gravite = mConfig.gravite;
				// Le sol d'un jeu est un corps statique : pas de boite implicite.
				mParticules->reglages.limites.actif = false;
				logger.Info("[unkeny] scene avec particules (corps mous, fluides), couplees aux rigides");
			}
			return true;
		}

		void NkScene::Liberer() {
			// Les ENTITES aussi : leurs NkCorps2D designent des corps du monde
			// physique qu'on va detruire. Un Init() sur une scene deja remplie
			// laissait des entites dont le corpsId pointait dans le vide.
			{
				NkVector<ecs::NkEntityId> ids;
				Entites(ids);
				for (uint32 i = 0; i < ids.Size(); ++i) {
					mMonde.Destroy(ids[i]);
				}
			}
			if (mParticules != nullptr) {
				memory::NkGetDefaultAllocator().Delete(mParticules);
				mParticules = nullptr;
			}
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
			// Meme regle pour un corps mou : sa matiere disparait avec lui.
			if (mParticules != nullptr) {
				if (const NkCorpsMou2D *m = mMonde.Get<NkCorpsMou2D>(id)) {
					const int32 ci = mParticules->IndexCorps(m->corpsId);
					if (ci >= 0) {
						mParticules->SupprimerCorps(static_cast<uint32>(ci));
					}
				}
			}
			mMonde.Destroy(id);
		}

		// =====================================================================
		bool NkScene::ActualiserCorps(ecs::NkEntityId id) {
			if (mPhysique == nullptr) {
				return false;
			}
			NkCorps2D *c = mMonde.Get<NkCorps2D>(id);
			if (c == nullptr) {
				return false;
			}
			physics::NkRigidBody etat;
			bool avaitEtat = false;
			if (const physics::NkRigidBody *b = mPhysique->GetBody(c->corpsId)) {
				etat = *b;
				avaitEtat = true;
				mPhysique->DestroyBody(c->corpsId);
			}
			NkCorps2D copie = *c;
			copie.corpsId = physics::NK_INVALID_BODY;
			if (!AjouterCorps(id, copie)) {
				return false;
			}
			if (avaitEtat) {
				const NkCorps2D *n = mMonde.Get<NkCorps2D>(id);
				if (physics::NkRigidBody *b = mPhysique->GetBody(n->corpsId)) {
					b->position = etat.position;
					b->orientation = etat.orientation;
					if (b->type == physics::NkBodyType::DYNAMIC) {
						b->linearVelocity = etat.linearVelocity;
						b->angularVelocity = etat.angularVelocity;
					}
					b->sleepTimer = 0.f;
				}
			}
			return true;
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
			def.material.dynamicFriction = corps.friction;
			def.material.staticFriction = corps.friction * 1.2f;
			def.material.restitution = corps.rebond;
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

			// La MASSE demandee. Le solveur la deduit de la densite et de l'aire
			// (une caisse de 60 cm pesait 0,36 kg, et un filet d'eau la faisait
			// voler) : on la pose, et l'inertie suit dans la meme proportion.
			if (physics::NkRigidBody *b = mPhysique->GetBody(bid)) {
				if (b->type == physics::NkBodyType::DYNAMIC && corps.masse > 0.f && b->invMass > 0.f) {
					const float32 k = (1.f / b->invMass) / corps.masse; // ancienne / nouvelle
					b->invMass = 1.f / corps.masse;
					b->invInertiaDiag = b->invInertiaDiag * k;
				}
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
					// ⚠️ ET LE REVEILLER. SetLinearVelocity ne touche pas au
					// sommeil : une caisse endormie gardait la vitesse ecrite
					// sans jamais l'integrer — saisie a la souris, elle ne
					// bougeait pas.
					if (physics::NkRigidBody *b = mPhysique->GetBody(c->corpsId)) {
						b->flags &= ~static_cast<uint32>(physics::NK_BODY_SLEEPING);
						b->sleepTimer = 0.f;
					}
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
			mContacts.Clear();
			if (mConfig.pasFixe > 0.f) {
				mAccumulateur += deltaTime;
				while (mAccumulateur >= mConfig.pasFixe && mDernierNbPas < mConfig.pasMaxParTrame) {
					// La logique de jeu AVANT la physique : une force posee ici est
					// integree dans ce pas, pas dans le suivant.
					LancerSystemes(NkPhaseSysteme::NK_PAS_FIXE, mConfig.pasFixe);
					if (mPhysique != nullptr) {
						// Les particules AVANT les rigides : leurs impulses sont
						// integrees par le solveur rigide dans le meme pas
						// (NkParticules2D.h).
						if (mParticules != nullptr) {
							mParticules->Pas(mConfig.pasFixe, mPhysique);
						}
						mPhysique->Step(mConfig.pasFixe);
						Relever();
					}
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
				if (mPhysique != nullptr) {
					SynchroniserDepuisPhysique();
					SynchroniserCorpsMous();
				}
			}

			AppliquerVitessesManuelles(deltaTime);
			// Les animations suivent le temps de la TRAME, pas le pas fixe : une
			// marche a 12 images/s ne doit pas dependre de la physique.
			// L'animateur (machine a etats de NKAnima) AVANT : le clip qu'il
			// choisit a cette trame est avance a cette trame.
			NkAvancerAnimateurs(mMonde, deltaTime);
			NkAvancerAnimations(mMonde, deltaTime);
			LancerSystemes(NkPhaseSysteme::NK_TRAME, deltaTime);
		}

		// =====================================================================
		// Systemes
		// =====================================================================
		uint32 NkScene::AjouterSysteme(const char *nom, NkPhaseSysteme phase, NkFonctionSysteme fonction, void *donnees,
									   int32 ordre) {
			if (fonction == nullptr) {
				return 0u;
			}
			NkSysteme s;
			s.id = mProchainSysteme++;
			s.nom = nom;
			s.phase = phase;
			s.fonction = fonction;
			s.donnees = donnees;
			s.ordre = ordre;
			// Insertion TRIEE par ordre, stable : a ordre egal, l'ordre d'ajout.
			uint32 at = static_cast<uint32>(mSystemes.Size());
			while (at > 0u && mSystemes[at - 1u].ordre > ordre) {
				--at;
			}
			mSystemes.PushBack(s);
			for (uint32 i = static_cast<uint32>(mSystemes.Size()) - 1u; i > at; --i) {
				mSystemes[i] = mSystemes[i - 1u];
			}
			mSystemes[at] = s;
			return s.id;
		}

		bool NkScene::RetirerSysteme(uint32 id) {
			for (uint32 i = 0; i < mSystemes.Size(); ++i) {
				if (mSystemes[i].id == id) {
					// Pas de trou et pas de reordonnancement : on decale.
					for (uint32 k = i; k + 1u < mSystemes.Size(); ++k) {
						mSystemes[k] = mSystemes[k + 1u];
					}
					mSystemes.PopBack();
					return true;
				}
			}
			return false;
		}

		void NkScene::ActiverSysteme(uint32 id, bool actif) {
			for (uint32 i = 0; i < mSystemes.Size(); ++i) {
				if (mSystemes[i].id == id) {
					mSystemes[i].actif = actif;
				}
			}
		}

		void NkScene::LancerSystemes(NkPhaseSysteme phase, float32 dt) {
			// ⚠️ Par INDEX et en relisant la taille : un systeme peut en retirer
			// ou en ajouter un autre (ou lui-meme) pendant qu'on les parcourt.
			// Apres chaque appel, on REPREND juste apres le systeme qui vient de
			// tourner, retrouve par son id : s'il s'est retire, le suivant a pris
			// sa place (sans cela il etait saute — mesure du banc, j3) ; si un
			// systeme a ete insere avant lui, il ne tourne pas deux fois.
			uint32 i = 0;
			while (i < mSystemes.Size()) {
				const NkSysteme s = mSystemes[i];
				if (!(s.actif && s.phase == phase)) {
					++i;
					continue;
				}
				s.fonction(*this, dt, s.donnees);
				uint32 j = 0;
				while (j < mSystemes.Size() && mSystemes[j].id != s.id) {
					++j;
				}
				i = j < mSystemes.Size() ? j + 1u : (i < mSystemes.Size() ? i : static_cast<uint32>(mSystemes.Size()));
			}
		}

		void NkScene::Relever() {
			const NkVector<physics::NkTriggerEvent> *listes[4] = {&mPhysique->ContactEnter(), &mPhysique->ContactExit(),
																  &mPhysique->TriggerEnter(), &mPhysique->TriggerExit()};
			bool vide = true;
			for (int32 k = 0; k < 4; ++k) {
				vide = vide && listes[k]->Size() == 0u;
			}
			if (vide) {
				return;
			}
			// Corps -> entite : refait ici, seulement quand il y a quelque chose a
			// traduire. Un index tenu a jour a chaque AjouterCorps / Detruire
			// serait plus rapide et deux fois plus fragile.
			mCorpsEntite.Clear();
			mMonde.Query<NkCorps2D>().ForEach([this](ecs::NkEntityId id, NkCorps2D &c) {
				mCorpsEntite.PushBack(NkCorpsEntite{c.corpsId, id});
			});
			auto entite = [this](physics::NkBodyId b) {
				for (uint32 i = 0; i < mCorpsEntite.Size(); ++i) {
					if (mCorpsEntite[i].corps == b) {
						return mCorpsEntite[i].entite;
					}
				}
				return ecs::NkEntityId::Invalid();
			};
			for (int32 k = 0; k < 4; ++k) {
				for (uint32 i = 0; i < listes[k]->Size(); ++i) {
					const physics::NkTriggerEvent &ev = (*listes[k])[i];
					NkContact2D c;
					c.a = entite(ev.trigger);
					c.b = entite(ev.other);
					c.phase = (k % 2 == 0) ? NkPhaseContact::NK_DEBUT : NkPhaseContact::NK_FIN;
					c.declencheur = k >= 2;
					if (c.a.IsValid() && c.b.IsValid()) {
						mContacts.PushBack(c);
					}
				}
			}
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

		ecs::NkEntityId NkScene::CreerCorpsMou(const char *nom, int32 indexCorps, uint32 couleur) {
			if (mParticules == nullptr || indexCorps < 0 || indexCorps >= static_cast<int32>(mParticules->corps.Size())) {
				logger.Warn("[unkeny] CreerCorpsMou refuse : {0}",
							mParticules == nullptr ? "la scene n'a pas de particules" : "index de corps invalide");
				return ecs::NkEntityId::Invalid();
			}
			const math::NkVec2f centre = mParticules->CentreCorps(static_cast<uint32>(indexCorps));
			const ecs::NkEntityId id = Creer(nom, centre);
			physics::NkCorpsP2D &c = mParticules->corps[static_cast<uint32>(indexCorps)];
			NkCorpsMou2D m;
			m.corpsId = c.id;
			m.couleur = couleur;
			mMonde.Add<NkCorpsMou2D>(id, m);
			c.utilisateur = id.Pack();
			return id;
		}

		bool NkScene::AttacherCorpsMou(ecs::NkEntityId id, int32 indexCorps, uint32 couleur) {
			if (mParticules == nullptr || indexCorps < 0 || indexCorps >= static_cast<int32>(mParticules->corps.Size()) ||
				!mMonde.IsAlive(id) || mMonde.Has<NkCorpsMou2D>(id)) {
				return false;
			}
			physics::NkCorpsP2D &c = mParticules->corps[static_cast<uint32>(indexCorps)];
			NkCorpsMou2D m;
			m.corpsId = c.id;
			m.couleur = couleur;
			mMonde.Add<NkCorpsMou2D>(id, m);
			c.utilisateur = id.Pack();
			if (NkTransform2D *t = mMonde.Get<NkTransform2D>(id)) {
				t->position = mParticules->CentreCorps(static_cast<uint32>(indexCorps));
			}
			return true;
		}

		bool NkScene::RetirerCorpsMou(ecs::NkEntityId id) {
			const NkCorpsMou2D *m = mMonde.Get<NkCorpsMou2D>(id);
			if (m == nullptr) {
				return false;
			}
			const uint32 corpsId = m->corpsId;
			// Le composant D'ABORD : SynchroniserCorpsMous detruit l'entite d'un
			// corps mou dont la matiere a disparu — on veut garder l'entite.
			mMonde.Remove<NkCorpsMou2D>(id);
			if (mParticules != nullptr) {
				const int32 ci = mParticules->IndexCorps(corpsId);
				if (ci >= 0) {
					mParticules->SupprimerCorps(static_cast<uint32>(ci));
				}
			}
			return true;
		}

		bool NkScene::RetirerCorps(ecs::NkEntityId id) {
			NkCorps2D *c = mMonde.Get<NkCorps2D>(id);
			if (c == nullptr) {
				return false;
			}
			if (mPhysique != nullptr && c->corpsId != physics::NK_INVALID_BODY) {
				mPhysique->DestroyBody(c->corpsId);
			}
			mMonde.Remove<NkCorps2D>(id);
			return true;
		}

		ecs::NkEntityId NkScene::EntiteDuCorpsMou(uint32 corpsId) const noexcept {
			if (mParticules == nullptr) {
				return ecs::NkEntityId::Invalid();
			}
			const int32 ci = mParticules->IndexCorps(corpsId);
			if (ci < 0) {
				return ecs::NkEntityId::Invalid();
			}
			return ecs::NkEntityId::Unpack(mParticules->corps[static_cast<uint32>(ci)].utilisateur);
		}

		void NkScene::SynchroniserCorpsMous() {
			if (mParticules == nullptr) {
				return;
			}
			physics::NkParticules2D *p = mParticules;
			NkVector<ecs::NkEntityId> orphelines;
			mMonde.Query<NkTransform2D, NkCorpsMou2D>().ForEach(
				[p, &orphelines](ecs::NkEntityId id, NkTransform2D &t, NkCorpsMou2D &m) {
					const int32 ci = p->IndexCorps(m.corpsId);
					if (ci < 0) {
						orphelines.PushBack(id); // plus de matiere : l'entite n'a plus d'objet
						return;
					}
					t.position = p->CentreCorps(static_cast<uint32>(ci));
				});
			// Hors de l'iteration : detruire PENDANT un Query invalide les iterateurs.
			for (uint32 i = 0; i < orphelines.Size(); ++i) {
				mMonde.Destroy(orphelines[i]);
			}
		}

		void NkScene::Entites(NkVector<ecs::NkEntityId> &out) {
			out.Clear();
			mMonde.Query<NkTransform2D>().ForEach([&out](ecs::NkEntityId id, NkTransform2D &) { out.PushBack(id); });
		}

		void NkScene::Photographier(NkPhoto &photo) {
			photo.entites.Clear();
			NkVector<ecs::NkEntityId> ids;
			Entites(ids);
			for (uint32 i = 0; i < ids.Size(); ++i) {
				const ecs::NkEntityId id = ids[i];
				NkPhotoEntite e;
				e.transform = *mMonde.Get<NkTransform2D>(id);
				if (const NkEtiquette *x = mMonde.Get<NkEtiquette>(id)) {
					e.etiquette = *x;
					e.aEtiquette = true;
				}
				if (const NkSprite2D *x = mMonde.Get<NkSprite2D>(id)) {
					e.sprite = *x;
					e.aSprite = true;
				}
				if (const NkCollisionneur2D *x = mMonde.Get<NkCollisionneur2D>(id)) {
					e.collisionneur = *x;
					e.aCollisionneur = true;
				}
				if (const NkCorps2D *x = mMonde.Get<NkCorps2D>(id)) {
					e.corps = *x;
					e.aCorps = true;
					if (mPhysique != nullptr) {
						if (const physics::NkRigidBody *b = mPhysique->GetBody(x->corpsId)) {
							e.etatRigide = *b;
						}
					}
				}
				if (const NkCorpsMou2D *x = mMonde.Get<NkCorpsMou2D>(id)) {
					e.mou = *x;
					e.aMou = true;
				}
				// Les composants du jeu declares par PhotographierAussi.
				uint32 total = 0;
				for (uint32 k = 0; k < mCopieurs.Size(); ++k) {
					total += mCopieurs[k].taille;
				}
				if (total > 0u) {
					e.extra.Resize(total);
					uint32 decalage = 0;
					for (uint32 k = 0; k < mCopieurs.Size(); ++k) {
						if (mCopieurs[k].lire(mMonde, id, e.extra.Data() + decalage)) {
							e.extraPresents |= 1u << k;
						}
						decalage += mCopieurs[k].taille;
					}
				}
				photo.entites.PushBack(e);
			}
			if (mParticules != nullptr) {
				photo.particules = *mParticules;
			}
			photo.valide = true;
		}

		void NkScene::Restaurer(const NkPhoto &photo) {
			if (!photo.valide) {
				return;
			}
			// 1. Tout detruire. Les corps mous ne sont PAS supprimes un par un : le
			//    monde de particules est remplace en bloc juste apres.
			NkVector<ecs::NkEntityId> ids;
			Entites(ids);
			for (uint32 i = 0; i < ids.Size(); ++i) {
				if (mPhysique != nullptr) {
					if (NkCorps2D *c = mMonde.Get<NkCorps2D>(ids[i])) {
						mPhysique->DestroyBody(c->corpsId);
					}
				}
				mMonde.Destroy(ids[i]);
			}
			if (mParticules != nullptr) {
				*mParticules = photo.particules;
			}
			// 2. Tout refaire.
			for (uint32 i = 0; i < photo.entites.Size(); ++i) {
				const NkPhotoEntite &e = photo.entites[i];
				const ecs::NkEntityId id = mMonde.CreateEntity();
				mMonde.Add<NkTransform2D>(id, e.transform);
				if (e.aEtiquette) {
					mMonde.Add<NkEtiquette>(id, e.etiquette);
				}
				{
					uint32 decalage = 0;
					for (uint32 k = 0; k < mCopieurs.Size(); ++k) {
						// Un copieur declare APRES la photo ne trouve rien a rendre :
						// on s'arrete au bout de ce qui a ete ecrit.
						if (decalage + mCopieurs[k].taille > e.extra.Size()) {
							break;
						}
						if ((e.extraPresents & (1u << k)) != 0u) {
							mCopieurs[k].ecrire(mMonde, id, e.extra.Data() + decalage);
						}
						decalage += mCopieurs[k].taille;
					}
				}
				if (e.aSprite) {
					mMonde.Add<NkSprite2D>(id, e.sprite);
				}
				if (e.aCollisionneur) {
					mMonde.Add<NkCollisionneur2D>(id, e.collisionneur);
				}
				if (e.aCorps && e.aCollisionneur && mPhysique != nullptr) {
					NkCorps2D c = e.corps;
					c.corpsId = 0;
					if (AjouterCorps(id, c)) {
						const NkCorps2D *nc = mMonde.Get<NkCorps2D>(id);
						if (physics::NkRigidBody *b = mPhysique->GetBody(nc->corpsId)) {
							// L'ETAT, pas l'identite : id et collisionId sont neufs.
							b->position = e.etatRigide.position;
							b->orientation = e.etatRigide.orientation;
							b->linearVelocity = e.etatRigide.linearVelocity;
							b->angularVelocity = e.etatRigide.angularVelocity;
							b->sleepTimer = 0.f;
						}
					}
				}
				if (e.aMou && mParticules != nullptr) {
					mMonde.Add<NkCorpsMou2D>(id, e.mou);
					const int32 ci = mParticules->IndexCorps(e.mou.corpsId);
					if (ci >= 0) {
						mParticules->corps[static_cast<uint32>(ci)].utilisateur = id.Pack();
					}
				}
			}
			mAccumulateur = 0.f;
		}

	} // namespace unkeny
} // namespace nkentseu
