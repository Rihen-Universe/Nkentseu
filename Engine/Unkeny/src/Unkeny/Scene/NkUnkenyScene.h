// =============================================================================
// NkUnkenyScene.h — la scene 2D : entites, physique, camera
//
// A QUOI SERT CE FICHIER
//   Il tient tout ce qu'un jeu 2D manipule : le monde d'entites (NKECS), le
//   monde physique (NKPhysics), et la camera. C'est le point d'entree du moteur.
//
// ⚠️ IL NE REECRIT RIEN. Mesure du 2026-09-01 avant d'ecrire une ligne :
//     entites et requetes  -> NKECS      (NkWorld, Add<T>, Query<Ts...>)
//     formes de collision  -> NKCollision (NkShape, fabriques 2D deja ecrites)
//     corps et solveur     -> NKPhysics  (NkPhysicsWorld, CreateBody, Step)
//   Ecrire un second solveur ici serait la faute que ce depot a deja payee trois
//   fois — deux structures jumelles dont une seule est alimentee.
//
// LA PHYSIQUE EST FACULTATIVE, ET C'EST UNE DECISION
//   `NkSceneConfig::physique` vaut false par defaut. Un jeu de plateau, un menu,
//   un puzzle n'ont aucune raison de payer un solveur. Un jeu de plateforme met
//   le drapeau a true et obtient un monde physique complet.
//   ⚠️ Consequence a connaitre : `Pas()` ne fait rien de physique quand le
//   drapeau est false, et cela se DIT dans le journal au demarrage — un moteur
//   qui ignore silencieusement une demande fait chercher le defaut ailleurs.
//
// LE SENS UNIQUE QUI EVITE LA DIVERGENCE
//   Apres chaque pas de simulation, la scene RECOPIE la position des corps
//   physiques dans leur NkTransform2D. Jamais l'inverse — sauf par
//   `TeleporterEntite`, qui est explicite et qui previent le solveur.
//   Sans cette regle, deux positions coexistent et le sprite se decale du
//   collisionneur.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un composant        -> NkUnkenyComposants.h
//   - un systeme partage  -> ici, en methode de scene
//   - un systeme d'un jeu -> chez le jeu : Monde() est public, les requetes
//                            NKECS sont a lui
// =============================================================================
#pragma once

#include "NKCollision/NkCollisionWorld.h"
#include "NKECS/World/NkWorld.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "Unkeny/Scene/NkUnkenyCamera.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"

namespace nkentseu {
	namespace unkeny {

		struct NkSceneConfig {
				/// false = aucun monde physique n'est cree. Voir l'en-tete.
				bool physique = false;

				/// Gravite du monde, en unites par seconde carree. Y NEGATIF fait
				/// tomber : le monde a Y vers le haut (voir NkUnkenyCamera.h).
				NkVec2f gravite{0.f, -9.81f};

				/// Pas de simulation FIXE. Une physique qui suit le pas de temps
				/// reel change de comportement selon la machine — et le defaut
				/// n'apparait que chez celui qui a l'ordinateur le plus lent.
				float32 pasFixe = 1.f / 60.f;

				/// Plafond de rattrapage. Apres une pause ou un retour de veille,
				/// le retard peut valoir plusieurs secondes : les rejouer d'un
				/// coup ferait traverser les murs. On en jette le surplus, et on
				/// le DIT plutot que de faire semblant.
				int32 pasMaxParTrame = 5;
		};

		class NkScene {
			public:
				NkScene() = default;
				~NkScene();

				NkScene(const NkScene &) = delete;
				NkScene &operator=(const NkScene &) = delete;

				bool Init(const NkSceneConfig &config);
				void Liberer();

				// --- Entites ---------------------------------------------------

				/// Cree une entite avec un transform. C'est le minimum : une
				/// entite sans position ne peut ni se dessiner ni se simuler.
				ecs::NkEntityId Creer(const NkVec2f &position = NkVec2f(0.f, 0.f));
				ecs::NkEntityId Creer(const char *nom, const NkVec2f &position);

				/// Detruit l'entite ET son corps physique s'il en a un. Detruire
				/// l'entite seule laisserait un corps orphelin qui continue de
				/// collisionner avec du vide — defaut invisible et couteux.
				void Detruire(ecs::NkEntityId id);

				/// Le monde d'entites, en acces direct. Un jeu ecrit ses propres
				/// composants et ses propres requetes dessus : le moteur ne
				/// prevoit pas ce dont un jeu aura besoin.
				ecs::NkWorld &Monde() noexcept {
					return mMonde;
				}
				const ecs::NkWorld &Monde() const noexcept {
					return mMonde;
				}

				// --- Physique --------------------------------------------------

				/// Donne un corps physique a une entite qui a deja un transform et
				/// un collisionneur. Rend false — et le DIT — si la physique n'est
				/// pas activee ou si les composants manquent.
				bool AjouterCorps(ecs::NkEntityId id, const NkCorps2D &corps);

				/// Deplace une entite SANS que le solveur l'interprete comme une
				/// vitesse. C'est le seul sens autorise transform -> physique.
				void TeleporterEntite(ecs::NkEntityId id, const NkVec2f &position);

				void PoserVitesse(ecs::NkEntityId id, const NkVec2f &vitesse);
				NkVec2f Vitesse(ecs::NkEntityId id) const;

				physics::NkPhysicsWorld *MondePhysique() noexcept {
					return mPhysique;
				}
				bool PhysiqueActive() const noexcept {
					return mPhysique != nullptr;
				}

				// --- Simulation ------------------------------------------------

				/// Avance la scene de `deltaTime` secondes : physique a pas fixe,
				/// puis recopie des positions, puis vitesses manuelles.
				void Pas(float32 deltaTime);

				NkVue2D &Camera() noexcept {
					return mCamera;
				}
				const NkVue2D &Camera() const noexcept {
					return mCamera;
				}

				const NkSceneConfig &Config() const noexcept {
					return mConfig;
				}

				/// Nombre de pas fixes joues a la derniere trame. Zero est normal
				/// quand la trame est courte ; le plafond atteint signale un
				/// retard qu'on a jete.
				int32 DernierNbPas() const noexcept {
					return mDernierNbPas;
				}

			private:
				void SynchroniserDepuisPhysique();
				void AppliquerVitessesManuelles(float32 dt);

				NkSceneConfig mConfig;
				ecs::NkWorld mMonde;
				physics::NkPhysicsWorld *mPhysique = nullptr;
				NkVue2D mCamera;
				float32 mAccumulateur = 0.f;
				int32 mDernierNbPas = 0;
		};

	} // namespace unkeny
} // namespace nkentseu
