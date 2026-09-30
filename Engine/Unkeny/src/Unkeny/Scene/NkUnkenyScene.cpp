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
#include "Unkeny/Scene/NkUnkenyPrefab.h"
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

			// ---- Les champs des composants d'Unkeny que la photo porte ----------
			// (2026-09-29) Avant, ils allaient au fichier en OCTETS : illisibles sur
			// une autre ABI, decales au premier champ ajoute, et `NkSource2D::son`
			// y designait « le n-ieme son charge » — un autre son a la reouverture.
			const NkChampSauve kChampsAnim[] = {
				NK_UNKENY_CHAMP_TABLEAU(NkAnimSprite2D, clips, NkClipSprite, premiere, NkTypeChamp::NK_U16,
										"clips.premiere"),
				NK_UNKENY_CHAMP_TABLEAU(NkAnimSprite2D, clips, NkClipSprite, nombre, NkTypeChamp::NK_U16,
										"clips.nombre"),
				NK_UNKENY_CHAMP_TABLEAU(NkAnimSprite2D, clips, NkClipSprite, imagesParSeconde, NkTypeChamp::NK_F32,
										"clips.imagesParSeconde"),
				NK_UNKENY_CHAMP_TABLEAU(NkAnimSprite2D, clips, NkClipSprite, mode, NkTypeChamp::NK_U8, "clips.mode"),
				NK_UNKENY_CHAMP_TABLEAU(NkAnimSprite2D, clips, NkClipSprite, imageEvent, NkTypeChamp::NK_U16,
										"clips.imageEvent"),
				NK_UNKENY_CHAMP(NkAnimSprite2D, nbClips, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkAnimSprite2D, clipCourant, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkAnimSprite2D, temps, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkAnimSprite2D, enPause, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkAnimSprite2D, termine, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkAnimSprite2D, evenementAtteint, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkAnimSprite2D, colonnes, NkTypeChamp::NK_U16),
				NK_UNKENY_CHAMP(NkAnimSprite2D, lignes, NkTypeChamp::NK_U16),
			};
			const NkChampSauve kChampsVitesse[] = {
				NK_UNKENY_CHAMP(NkVitesse2D, lineaire, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkVitesse2D, angulaire, NkTypeChamp::NK_F32),
			};
			const NkChampSauve kChampsSource[] = {
				NK_UNKENY_CHAMP(NkSource2D, son, NkTypeChamp::NK_SON),
				NK_UNKENY_CHAMP(NkSource2D, volume, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkSource2D, pitch, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkSource2D, portee, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkSource2D, boucle, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkSource2D, auDemarrage, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkSource2D, spatial, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkSource2D, demande, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkSource2D, arret, NkTypeChamp::NK_BOOL),
				// Ecrits par le systeme : une voix NKAudio n'a aucun sens dans une
				// autre session (elle designerait la voix d'un autre son).
				NK_UNKENY_CHAMP_TRANSITOIRE(NkSource2D, voix, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP_TRANSITOIRE(NkSource2D, pan, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP_TRANSITOIRE(NkSource2D, gain, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkSource2D, lance, NkTypeChamp::NK_BOOL),
			};
			const NkChampSauve kChampsInstance[] = {
				NK_UNKENY_CHAMP(NkInstancePrefab2D, prefab, NkTypeChamp::NK_PREFAB),
				NK_UNKENY_CHAMP(NkInstancePrefab2D, noeud, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkInstancePrefab2D, racine, NkTypeChamp::NK_ENTITE),
			};
			template <typename T, uint32 N> constexpr uint32 NbChamps(const T (&)[N]) noexcept {
				return N;
			}
		} // namespace

		// =====================================================================
		NkScene::~NkScene() {
			Liberer();
		}

		bool NkScene::Init(const NkSceneConfig &config) {
			Liberer();
			mConfig = config;
			// Une scene neuve numerote ses identites depuis 1.
			mProchainUid = 1;
			mCacheUid.Clear();
			// Les composants d'Unkeny que NkPhotoEntite ne nomme pas passent par
			// le meme chemin que ceux d'un jeu — DECRITS depuis le 2026-09-29.
			PhotographierAussi<NkAnimSprite2D>("NkAnimSprite2D", kChampsAnim, NbChamps(kChampsAnim));
			// NkAnimateur2D (branche animation) : en octets, comme sa branche l'a
			// declare — le decrire champ par champ reste a faire.
			PhotographierAussi<NkAnimateur2D>("NkAnimateur2D");
			PhotographierAussi<NkVitesse2D>("NkVitesse2D", kChampsVitesse, NbChamps(kChampsVitesse));
			PhotographierAussi<NkSource2D>("NkSource2D", kChampsSource, NbChamps(kChampsSource));
			// Le lien d'une instance a son prefab (NkUnkenyPrefab.h) : declare ICI,
			// pour qu'une scene relue retrouve ses instances meme si le jeu n'a
			// encore touche aucun prefab.
			PhotographierAussi<NkInstancePrefab2D>("NkInstancePrefab2D", kChampsInstance, NbChamps(kChampsInstance));

			// Le monde d'Unkeny est PLAN : NkPhysicsConfig::enable2D, lu par
			// NKPhysics depuis le 2026-09-29, y ramene tout ce qui en sortirait
			// (rien, aujourd'hui : c'est une garde, pas un changement de calcul).
			physics::NkPhysicsConfig cfgPhysique;
			cfgPhysique.enable2D = true;
			if (mConfig.physique) {
				// ⚠️ Alloue par NKMemory, jamais par new : melanger l'allocateur
				// maison et le tas CRT corrompt le tas sous Windows (c0000374).
				mPhysique = memory::NkGetDefaultAllocator().New<physics::NkPhysicsWorld>(cfgPhysique);
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
					mPhysique = memory::NkGetDefaultAllocator().New<physics::NkPhysicsWorld>(cfgPhysique);
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
			// L'identite stable, des la naissance (voir NkUnkenyHierarchie.h).
			NkIdentite2D ident;
			ident.uid = mProchainUid++;
			mMonde.Add<NkIdentite2D>(id, ident);
			mCacheUid.Insert(ident.uid, id);
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
			// Les enfants restent, a leur place : un lien vers une entite morte se
			// lirait « racine » quand meme (NkHierarchy.h), mais leur NkLocal2D
			// resterait exprime dans un repere qui n'existe plus.
			{
				NkVector<ecs::NkEntityId> enfants;
				Enfants(id, enfants);
				for (uint32 i = 0; i < enfants.Size(); ++i) {
					Detacher(enfants[i], true);
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
						ReleverCorpsMous();
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
			// La hierarchie APRES tout ce qui ecrit le monde (physique, vitesses) :
			// un enfant voit son parent la ou il est a cette trame.
			PropagerHierarchie();
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

		void NkScene::ReleverCorpsMous() {
			if (mParticules == nullptr) {
				return;
			}
			const NkVector<physics::NkEvenementP2D> *listes[2] = {&mParticules->ContactsDebut(), &mParticules->ContactsFin()};
			if (listes[0]->Size() == 0u && listes[1]->Size() == 0u) {
				return;
			}
			// Meme table corps rigide -> entite que Relever, refaite ici pour la meme
			// raison : seulement quand il y a quelque chose a traduire.
			mCorpsEntite.Clear();
			mMonde.Query<NkCorps2D>().ForEach([this](ecs::NkEntityId id, NkCorps2D &c) {
				mCorpsEntite.PushBack(NkCorpsEntite{c.corpsId, id});
			});
			auto rigide = [this](uint32 b) {
				for (uint32 i = 0; i < mCorpsEntite.Size(); ++i) {
					if (mCorpsEntite[i].corps == b) {
						return mCorpsEntite[i].entite;
					}
				}
				return ecs::NkEntityId::Invalid();
			};
			for (int32 k = 0; k < 2; ++k) {
				for (uint32 i = 0; i < listes[k]->Size(); ++i) {
					const physics::NkEvenementP2D &ev = (*listes[k])[i];
					NkContact2D c;
					c.phase = k == 0 ? NkPhaseContact::NK_DEBUT : NkPhaseContact::NK_FIN;
					// Une FIN peut concerner un corps mou qui vient de DISPARAITRE (tombe,
					// gomme) : il n'a plus d'entite, l'evenement est tu.
					const ecs::NkEntityId mou = EntiteDuCorpsMou(ev.corps);
					switch (ev.genre) {
						case physics::NkGenreEvenementP2D::NK_ZONE:
							c.a = rigide(ev.autre);
							c.b = mou;
							c.declencheur = true;
							break;
						case physics::NkGenreEvenementP2D::NK_CORPS_MOU:
							c.a = mou;
							c.b = EntiteDuCorpsMou(ev.autre);
							break;
						default:
							c.a = mou;
							c.b = rigide(ev.autre);
							break;
					}
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
			// Toutes les identites D'ABORD : un composant peut designer une entite
			// qui vient apres lui, et sa reference doit trouver un uid.
			for (uint32 i = 0; i < ids.Size(); ++i) {
				AssurerUid(ids[i]);
			}
			for (uint32 i = 0; i < ids.Size(); ++i) {
				NkPhotoEntite e;
				PhotographierEntite(ids[i], e);
				photo.entites.PushBack(e);
			}
			if (mParticules != nullptr) {
				photo.particules = *mParticules;
			}
			photo.prochainUid = mProchainUid;
			photo.valide = true;
		}

		bool NkScene::PhotographierEntite(ecs::NkEntityId id, NkPhotoEntite &e) {
			e = NkPhotoEntite();
			if (!mMonde.IsAlive(id) || !mMonde.Has<NkTransform2D>(id)) {
				return false;
			}
			e.transform = *mMonde.Get<NkTransform2D>(id);
			// L'identite et la place dans la hierarchie (2026-09-29).
			e.uid = AssurerUid(id);
			const ecs::NkEntityId parent = Parent(id);
			if (parent.IsValid()) {
				e.parentUid = AssurerUid(parent);
				if (const NkLocal2D *l = mMonde.Get<NkLocal2D>(id)) {
					e.local = l->local;
				} else {
					e.local = NkDecomposer2D(*mMonde.Get<NkTransform2D>(parent), e.transform);
				}
			}
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
			// Les controleurs de personnage (branche physique, 2026-09-29).
			if (const NkControleRigide2D *x = mMonde.Get<NkControleRigide2D>(id)) {
				e.controleRigide = *x;
				e.aControleRigide = true;
			}
			if (const NkControleMou2D *x = mMonde.Get<NkControleMou2D>(id)) {
				e.controleMou = *x;
				e.aControleMou = true;
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
						// Une reference a une entite voyage par IDENTITE.
						ConvertirEntites(k, e.extra.Data() + decalage, true);
					}
					decalage += mCopieurs[k].taille;
				}
			}
			return true;
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
			mCacheUid.Clear();
			if (mParticules != nullptr) {
				*mParticules = photo.particules;
			}
			// 2. Tout refaire. Le compteur ne RECULE jamais : une identite donnee
			//    apres la photo (entite detruite depuis) ne resservira pas.
			if (photo.prochainUid > mProchainUid) {
				mProchainUid = photo.prochainUid;
			}
			RefaireEntites(photo.entites, nullptr);
			mAccumulateur = 0.f;
		}

		void NkScene::RefaireEntites(const NkVector<NkPhotoEntite> &entites, NkVector<ecs::NkEntityId> *crees,
									 bool nouvellesIdentites) {
			// ⚠️ TROIS PASSES, et c'est ce qui rend les references justes : un
			//    composant peut designer une entite qui vient APRES lui dans la
			//    photo, et un enfant peut preceder son parent.
			// 1. Les entites, leur transform et leur identite.
			NkVector<ecs::NkEntityId> faites;
			faites.Resize(entites.Size());
			NkUnorderedMap<uint64, ecs::NkEntityId> lot;
			// Les corps rigides vont renaitre sous des ids NEUFS : les attaches des
			// particules (et leurs paires de contact en cours) designent les
			// anciens. On note les deux, puis on remappe tout d'un coup (2026-09-29,
			// branche physique ; ici depuis la fusion, pour Restaurer, le fichier et
			// tout ajout de photo). Un noeud de prefab porte corpsId = 0 : rien a
			// remapper, ses attaches ne sortent pas du prefab.
			NkVector<physics::NkBodyId> anciensIds;
			NkVector<physics::NkBodyId> nouveauxIds;
			for (uint32 i = 0; i < entites.Size(); ++i) {
				const NkPhotoEntite &e = entites[i];
				const ecs::NkEntityId id = mMonde.CreateEntity();
				mMonde.Add<NkTransform2D>(id, e.transform);
				NkIdentite2D ident;
				ident.uid = e.uid;
				// Une identite deja portee par une entite VIVANTE (une photo qu'on
				// AJOUTE a une scene qui l'a deja) en recoit une neuve : deux
				// entites sous un meme uid rendraient EntiteParUid ambigu. Le lot
				// garde l'ancienne pour rebrancher ce que la photo y rattachait.
				if (nouvellesIdentites || ident.uid == 0u || EntiteParUid(ident.uid).IsValid()) {
					ident.uid = mProchainUid++;
				} else if (ident.uid >= mProchainUid) {
					mProchainUid = ident.uid + 1u;
				}
				mMonde.Add<NkIdentite2D>(id, ident);
				mCacheUid.Insert(ident.uid, id);
				if (e.uid != 0u) {
					lot.Insert(e.uid, id);
				}
				faites[i] = id;
			}
			// 2. Les composants, dans l'ordre d'avant.
			NkVector<uint8> tampon;
			for (uint32 i = 0; i < entites.Size(); ++i) {
				const NkPhotoEntite &e = entites[i];
				const ecs::NkEntityId id = faites[i];
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
							// Une COPIE : la photo reste rejouable (identites intactes).
							tampon.Resize(mCopieurs[k].taille);
							std::memcpy(tampon.Data(), e.extra.Data() + decalage, mCopieurs[k].taille);
							ConvertirEntites(k, tampon.Data(), false, &lot);
							mCopieurs[k].ecrire(mMonde, id, tampon.Data());
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
						if (e.corps.corpsId != physics::NK_INVALID_BODY) {
							anciensIds.PushBack(e.corps.corpsId);
							nouveauxIds.PushBack(nc->corpsId);
						}
					}
				}
				if (e.aControleRigide) {
					mMonde.Add<NkControleRigide2D>(id, e.controleRigide);
				}
				if (e.aControleMou) {
					mMonde.Add<NkControleMou2D>(id, e.controleMou);
				}
				if (e.aMou && mParticules != nullptr) {
					mMonde.Add<NkCorpsMou2D>(id, e.mou);
					const int32 ci = mParticules->IndexCorps(e.mou.corpsId);
					if (ci >= 0) {
						mParticules->corps[static_cast<uint32>(ci)].utilisateur = id.Pack();
					}
				}
			}
			if (mParticules != nullptr && anciensIds.Size() > 0u) {
				mParticules->RemapperRigides(anciensIds.Data(), nouveauxIds.Data(), static_cast<uint32>(anciensIds.Size()));
			}
			// 3. Les parents. Le monde de la photo est deja le bon : on pose le
			//    local SANS recalculer (produit = ce monde), pour que rien ne bouge
			//    d'un arrondi a la trame suivante.
			for (uint32 i = 0; i < entites.Size(); ++i) {
				const NkPhotoEntite &e = entites[i];
				if (e.parentUid == 0u) {
					continue;
				}
				const ecs::NkEntityId parent = Resoudre(e.parentUid, &lot);
				if (!parent.IsValid() || !ecs::NkSetParent(mMonde, faites[i], parent)) {
					continue; // parent absent de la photo et de la scene : l'enfant reste racine
				}
				NkLocal2D l;
				l.local = e.local;
				l.produit = e.transform;
				l.parentProduit = *mMonde.Get<NkTransform2D>(parent);
				mMonde.Add<NkLocal2D>(faites[i], l);
			}
			// 4. Un fichier RETOUCHE A LA MAIN peut deplacer un parent sans ses
			//    enfants : un monde qui ne vaut pas (parent o local) est recale sur
			//    le local, parents d'abord. Une photo est toujours coherente : ce
			//    pas n'y change rien, et la restauration reste exacte au bit pres.
			RecalerEnfants(faites);
			if (crees != nullptr) {
				*crees = faites;
			}
		}

		void NkScene::RecalerEnfants(const NkVector<ecs::NkEntityId> &ids) {
			struct NkAPlacer {
					ecs::NkEntityId id;
					uint32 profondeur = 0;
			};
			NkVector<NkAPlacer> enfants;
			for (uint32 i = 0; i < ids.Size(); ++i) {
				if (Parent(ids[i]).IsValid() && mMonde.Has<NkLocal2D>(ids[i])) {
					NkAPlacer a;
					a.id = ids[i];
					a.profondeur = ecs::NkHierarchyDepth(mMonde, ids[i]);
					enfants.PushBack(a);
				}
			}
			for (uint32 i = 1; i < enfants.Size(); ++i) {
				const NkAPlacer x = enfants[i];
				uint32 j = i;
				while (j > 0u && enfants[j - 1u].profondeur > x.profondeur) {
					enfants[j] = enfants[j - 1u];
					--j;
				}
				enfants[j] = x;
			}
			auto proche = [](float32 a, float32 b, float32 tol) {
				const float32 d = a > b ? a - b : b - a;
				const float32 m = (a > 0.f ? a : -a) > 1.f ? (a > 0.f ? a : -a) : 1.f;
				return d <= tol * m;
			};
			NkVector<ecs::NkEntityId> recales;
			for (uint32 i = 0; i < enfants.Size(); ++i) {
				const ecs::NkEntityId id = enfants[i].id;
				const ecs::NkEntityId parent = Parent(id);
				if (MeneParPhysique(id)) {
					continue; // la physique mene : son monde est la verite
				}
				bool parentRecale = false;
				for (uint32 k = 0; k < recales.Size() && !parentRecale; ++k) {
					parentRecale = recales[k] == parent;
				}
				const NkTransform2D p = *mMonde.Get<NkTransform2D>(parent);
				const NkTransform2D m = *mMonde.Get<NkTransform2D>(id);
				NkTransform2D attendu = NkComposer2D(p, mMonde.Get<NkLocal2D>(id)->local);
				const bool coherent = proche(attendu.position.x, m.position.x, 1.0e-4f) &&
									  proche(attendu.position.y, m.position.y, 1.0e-4f) &&
									  proche(attendu.rotation, m.rotation, 1.0e-4f) &&
									  proche(attendu.echelle.x, m.echelle.x, 1.0e-4f) &&
									  proche(attendu.echelle.y, m.echelle.y, 1.0e-4f);
				if (coherent && !parentRecale) {
					continue;
				}
				if (mMonde.Has<NkCorps2D>(id)) {
					attendu.rotation = PorterCorps(id, attendu);
				}
				*mMonde.Get<NkTransform2D>(id) = attendu;
				NkLocal2D *l = mMonde.Get<NkLocal2D>(id);
				l->produit = attendu;
				l->parentProduit = p;
				recales.PushBack(id);
			}
		}

		// =====================================================================
		// Les champs decrits (NkUnkenyChamps.h)
		// =====================================================================
		void NkScene::PoserChamps(const void *cle, const char *nom, const NkChampSauve *champs, uint32 nbChamps) {
			for (uint32 i = 0; i < mCopieurs.Size(); ++i) {
				if (mCopieurs[i].cle != cle) {
					continue;
				}
				for (uint32 c = 0; c < nbChamps; ++c) {
					if (!NkChampValide(champs[c], mCopieurs[i].taille)) {
						// Tout ou rien : une description a moitie juste ecrirait un
						// fichier a moitie faux, sans que personne le voie.
						logger.Warn("[unkeny] description de {0} REFUSEE : champ {1} ({2}) incoherent — le "
									"composant reste ecrit en octets",
									nom != nullptr ? nom : "?", c, champs[c].nom != nullptr ? champs[c].nom : "?");
						return;
					}
				}
				mCopieurs[i].champs = champs;
				mCopieurs[i].nbChamps = nbChamps;
				return;
			}
		}

		void NkScene::ConvertirEntites(uint32 copieur, uint8 *octets, bool versUid,
									   const NkUnorderedMap<uint64, ecs::NkEntityId> *lot) {
			if (copieur >= mCopieurs.Size() || mCopieurs[copieur].champs == nullptr) {
				return;
			}
			const NkCopieurPhoto &k = mCopieurs[copieur];
			for (uint32 c = 0; c < k.nbChamps; ++c) {
				const NkChampSauve &ch = k.champs[c];
				if (ch.type != NkTypeChamp::NK_ENTITE) {
					continue;
				}
				for (uint32 n = 0; n < ch.nombre; ++n) {
					uint8 *p = octets + ch.decalage + n * ch.Pas();
					if (versUid) {
						ecs::NkEntityId e;
						std::memcpy(&e, p, sizeof(e));
						const uint64 uid = Uid(e);
						std::memcpy(p, &uid, sizeof(uid));
					} else {
						uint64 uid = 0;
						std::memcpy(&uid, p, sizeof(uid));
						const ecs::NkEntityId e = uid != 0u ? Resoudre(uid, lot) : ecs::NkEntityId::Invalid();
						std::memcpy(p, &e, sizeof(e));
					}
				}
			}
		}

		bool NkScene::EcrireComposantPhoto(uint32 i, ecs::NkEntityId id, const uint8 *octets) {
			if (i >= mCopieurs.Size() || octets == nullptr || !mMonde.IsAlive(id)) {
				return false;
			}
			NkVector<uint8> tampon;
			tampon.Resize(mCopieurs[i].taille);
			std::memcpy(tampon.Data(), octets, mCopieurs[i].taille);
			ConvertirEntites(i, tampon.Data(), false, nullptr);
			mCopieurs[i].ecrire(mMonde, id, tampon.Data());
			return true;
		}

		bool NkScene::RetirerComposantPhoto(uint32 i, ecs::NkEntityId id) {
			if (i >= mCopieurs.Size() || mCopieurs[i].retirer == nullptr || !mMonde.IsAlive(id)) {
				return false;
			}
			mCopieurs[i].retirer(mMonde, id);
			return true;
		}

		ecs::NkEntityId NkScene::Resoudre(uint64 uid, const NkUnorderedMap<uint64, ecs::NkEntityId> *lot) {
			if (lot != nullptr) {
				if (const ecs::NkEntityId *e = lot->Find(uid)) {
					if (mMonde.IsAlive(*e)) {
						return *e;
					}
				}
			}
			return EntiteParUid(uid);
		}

		// =====================================================================
		// Le pont 2D <-> 3D pour un enfant porte (NkUnkenyHierarchie.cpp)
		// =====================================================================
		float32 NkScene::PorterCorps(ecs::NkEntityId id, const NkTransform2D &m) {
			if (mPhysique == nullptr) {
				return m.rotation;
			}
			const NkCorps2D *c = mMonde.Get<NkCorps2D>(id);
			physics::NkRigidBody *b = c != nullptr ? mPhysique->GetBody(c->corpsId) : nullptr;
			if (b == nullptr) {
				return m.rotation;
			}
			b->position = math::NkVec3f(m.position.x, m.position.y, 0.f);
			b->orientation = DepuisAngleZ(m.rotation);
			b->sleepTimer = 0.f;
			// LA rotation que SynchroniserDepuisPhysique relira : sans elle, l'arrondi
			// de l'aller-retour ferait croire a un geste a chaque trame.
			return AngleZ(b->orientation);
		}

		bool NkScene::MeneParPhysique(ecs::NkEntityId id) const noexcept {
			if (mMonde.Has<NkCorpsMou2D>(id)) {
				return true;
			}
			const NkCorps2D *c = mMonde.Get<NkCorps2D>(id);
			return c != nullptr && c->type == NkTypeCorps::NK_DYNAMIQUE && mPhysique != nullptr &&
				   mPhysique->GetBody(c->corpsId) != nullptr;
		}

	} // namespace unkeny
} // namespace nkentseu
