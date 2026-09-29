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
//     corps mous, fluides  -> NKPhysics  (NkParticules2D, couple aux rigides ;
//                                         ajoute le 2026-09-29)
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
#include "NKPhysics/NkParticules2D.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "Unkeny/Scene/NkUnkenyCamera.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"

#include <cstring>
#include <type_traits>

namespace nkentseu {
	namespace unkeny {

		struct NkSceneConfig {
				/// false = aucun monde physique n'est cree. Voir l'en-tete.
				bool physique = false;
				/// true = la scene porte aussi un monde de PARTICULES (corps mous,
				/// fluides, sable, cristaux, tissus). Facultatif pour la meme raison
				/// que la physique ; il n'a de sens qu'avec elle (couplage), et
				/// l'active donc s'il est demande seul -- en le DISANT.
				bool particules = false;

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

		class NkScene;

		// =====================================================================
		// La logique de jeu : SYSTEMES et CONTACTS
		//
		// ⚠️ POURQUOI PAS ecs::NkScheduler (mesure du 2026-09-29, apres la
		//    recherche que la regle du depot impose)
		//   NKECS porte un ordonnanceur complet (DAG, conflits lecture/ecriture,
		//   execution parallele). Il demarre TOUJOURS au moins un fil
		//   (NkJobPool : hardware_concurrency - 1, minimum 1) et repose sur
		//   std::function / std::thread. Une NkScene par niveau, une par
		//   miniature d'editeur : autant de fils. Et le Web d'Unkeny tourne sans
		//   fils (ASYNCIFY, pas de SharedArrayBuffer). Unkeny garde donc un
		//   planificateur SEQUENTIEL et sans allocation par trame ; un jeu qui
		//   veut le parallele peut toujours faire tourner un NkScheduler sur
		//   scene.Monde() — c'est le meme NkWorld.
		// =====================================================================
		enum class NkPhaseSysteme : uint8 {
			NK_PAS_FIXE = 0, ///< a chaque pas fixe, AVANT la physique (forces, commandes)
			NK_TRAME		 ///< une fois par Pas, APRES physique, synchro et animations
		};

		using NkFonctionSysteme = void (*)(NkScene &scene, float32 dt, void *donnees);

		enum class NkPhaseContact : uint8 {
			NK_DEBUT = 0, ///< les deux corps viennent de se toucher
			NK_FIN		  ///< ils viennent de se separer
		};

		/// Un choc ou une entree de zone, en ENTITES. `declencheur` : `a` est une
		/// zone (collisionneur `declencheur`), `b` ce qui y entre ou en sort.
		struct NkContact2D {
				ecs::NkEntityId a;
				ecs::NkEntityId b;
				NkPhaseContact phase = NkPhaseContact::NK_DEBUT;
				bool declencheur = false;
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

				// --- Particules (corps mous, fluides) --------------------------
				/// Le monde de particules, ou nul si `NkSceneConfig::particules` est
				/// faux. Les fabriques de NKPhysics (NkParticules2DFabrique.h) y
				/// creent la matiere ; `CreerCorpsMou` en fait une ENTITE.
				physics::NkParticules2D *Particules() noexcept {
					return mParticules;
				}
				const physics::NkParticules2D *Particules() const noexcept {
					return mParticules;
				}
				/// Fait une entite d'un corps de particules deja cree (son INDEX,
				/// tel que le rend une fabrique). Rend une entite invalide -- et le
				/// DIT -- si la scene n'a pas de particules ou si l'index est faux.
				ecs::NkEntityId CreerCorpsMou(const char *nom, int32 indexCorps, uint32 couleur);
				/// L'entite qui porte le corps de particules `corpsId`.
				ecs::NkEntityId EntiteDuCorpsMou(uint32 corpsId) const noexcept;
				bool PhysiqueActive() const noexcept {
					return mPhysique != nullptr;
				}

				// --- Simulation ------------------------------------------------

				/// Avance la scene de `deltaTime` secondes : physique a pas fixe,
				/// puis recopie des positions, puis vitesses manuelles.
				void Pas(float32 deltaTime);

				// --- Logique de jeu --------------------------------------------
				/// Ajoute un systeme. Ils tournent par phase, dans l'ordre croissant
				/// d'`ordre` puis d'ajout. Rend un identifiant (jamais 0).
				/// `nom` : chaine STATIQUE, pour le debogage et l'editeur.
				uint32 AjouterSysteme(const char *nom, NkPhaseSysteme phase, NkFonctionSysteme fonction,
									  void *donnees = nullptr, int32 ordre = 0);
				bool RetirerSysteme(uint32 id);
				void ActiverSysteme(uint32 id, bool actif);
				uint32 NbSystemes() const noexcept {
					return static_cast<uint32>(mSystemes.Size());
				}
				const char *NomSysteme(uint32 index) const noexcept {
					return index < mSystemes.Size() ? mSystemes[index].nom : nullptr;
				}

				/// Les chocs et entrees de zone du DERNIER Pas (tous ses pas fixes
				/// reunis). Lus par un systeme NK_TRAME, ou par le jeu apres Pas.
				/// ⚠️ Une entite detruite PENDANT le pas peut y figurer : tester
				/// Monde().IsAlive avant de s'en servir.
				const NkVector<NkContact2D> &Contacts() const noexcept {
					return mContacts;
				}

				NkVue2D &Camera() noexcept {
					return mCamera;
				}
				const NkVue2D &Camera() const noexcept {
					return mCamera;
				}

				const NkSceneConfig &Config() const noexcept {
					return mConfig;
				}

				/// Toutes les entites qui ont un transform -- c'est-a-dire toutes
				/// celles que la scene a creees. Pour un Outliner, un inspecteur.
				void Entites(NkVector<ecs::NkEntityId> &out);

				// --- Photo (Jouer / Arreter d'un editeur) ----------------------
				/// Ce qu'il faut pour REFAIRE la scene a l'identique : les
				/// composants d'Unkeny de chaque entite, l'etat de chaque corps
				/// rigide, et tout le monde de particules.
				/// Un composant propre a un JEU n'y est que s'il a ete DECLARE par
				/// PhotographierAussi<T>() — voir plus bas.
				struct NkPhotoEntite {
						NkTransform2D transform;
						NkEtiquette etiquette;
						NkSprite2D sprite;
						NkCollisionneur2D collisionneur;
						NkCorps2D corps;
						physics::NkRigidBody etatRigide;
						NkCorpsMou2D mou;
						bool aEtiquette = false, aSprite = false, aCollisionneur = false, aCorps = false, aMou = false;
						/// Les composants declares par PhotographierAussi, bout a bout,
						/// et le masque de ceux que l'entite portait (bit i = copieur i).
						NkVector<uint8> extra;
						uint32 extraPresents = 0u;
				};
				struct NkPhoto {
						NkVector<NkPhotoEntite> entites;
						physics::NkParticules2D particules;
						bool valide = false;
				};
				void Photographier(NkPhoto &photo);

				/// Les composants declares par PhotographierAussi, dans l'ordre de
				/// leur declaration — celui des octets de NkPhotoEntite::extra.
				uint32 NbComposantsPhoto() const noexcept {
					return static_cast<uint32>(mCopieurs.Size());
				}
				const char *NomComposantPhoto(uint32 i) const noexcept {
					return i < mCopieurs.Size() ? mCopieurs[i].nom : nullptr;
				}
				uint32 TailleComposantPhoto(uint32 i) const noexcept {
					return i < mCopieurs.Size() ? mCopieurs[i].taille : 0u;
				}

				/// Declare un composant PROPRE AU JEU que la photo doit porter.
				///
				/// ⚠️ POURQUOI CECI EXISTE — mesure du 2026-09-29. L'en-tete disait
				/// « un jeu qui en a les rajoute apres Restaurer ». Le premier jeu
				/// qui s'en est servi (la demo Physic2D) ne l'a PAS fait : apres
				/// Jouer puis Arreter, son marqueur de decor avait disparu, et le
				/// sol devenait effacable a la gomme. Une consigne qu'on doit
				/// relire pour ne pas perdre de donnees finit toujours oubliee ;
				/// une declaration faite une fois, a l'initialisation, non.
				///
				/// T doit etre copiable bit a bit (pas de pointeur possede, pas de
				/// chaine allouee) : il est recopie tel quel. 32 types au plus.
				///
				/// `nom` : sous ce nom, le composant est AUSSI ecrit dans les fichiers
				/// de scene (NkUnkenySauvegarde.h). Sans nom, il ne vit que dans les
				/// photos en memoire. Le nom est la cle du fichier : le changer rend
				/// les anciennes sauvegardes muettes sur ce composant.
				template <typename T> void PhotographierAussi(const char *nom = nullptr) {
					static_assert(std::is_trivially_copyable<T>::value,
								  "un composant photographie doit etre copiable bit a bit");
					static const char cle = 0; // une adresse par type T
					for (uint32 i = 0; i < mCopieurs.Size(); ++i) {
						if (mCopieurs[i].cle == &cle) {
							return; // deja declare
						}
					}
					if (mCopieurs.Size() >= 32u) {
						return;
					}
					NkCopieurPhoto k;
					k.cle = &cle;
					k.nom = nom;
					k.taille = static_cast<uint32>(sizeof(T));
					k.lire = [](ecs::NkWorld &w, ecs::NkEntityId id, uint8 *dst) {
						const T *x = w.Get<T>(id);
						if (x != nullptr) {
							std::memcpy(dst, x, sizeof(T));
						}
						return x != nullptr;
					};
					k.ecrire = [](ecs::NkWorld &w, ecs::NkEntityId id, const uint8 *src) {
						T v;
						std::memcpy(&v, src, sizeof(T));
						w.Add<T>(id, v);
					};
					mCopieurs.PushBack(k);
				}
				/// Detruit toutes les entites et les refait depuis la photo. Les
				/// identifiants d'entite CHANGENT ; ceux des corps mous, non.
				void Restaurer(const NkPhoto &photo);

				/// Nombre de pas fixes joues a la derniere trame. Zero est normal
				/// quand la trame est courte ; le plafond atteint signale un
				/// retard qu'on a jete.
				int32 DernierNbPas() const noexcept {
					return mDernierNbPas;
				}

			private:
				struct NkCopieurPhoto {
						const void *cle = nullptr;
						const char *nom = nullptr; ///< chaine STATIQUE (litteral) : jamais copiee
						uint32 taille = 0;
						bool (*lire)(ecs::NkWorld &, ecs::NkEntityId, uint8 *) = nullptr;
						void (*ecrire)(ecs::NkWorld &, ecs::NkEntityId, const uint8 *) = nullptr;
				};
				NkVector<NkCopieurPhoto> mCopieurs;

				void SynchroniserDepuisPhysique();
				/// Centre des corps mous -> NkTransform2D, et destruction des
				/// entites dont le corps a disparu (tombe hors du monde, gomme).
				void SynchroniserCorpsMous();
				void AppliquerVitessesManuelles(float32 dt);

				struct NkSysteme {
						uint32 id = 0;
						const char *nom = nullptr;
						NkPhaseSysteme phase = NkPhaseSysteme::NK_TRAME;
						NkFonctionSysteme fonction = nullptr;
						void *donnees = nullptr;
						int32 ordre = 0;
						bool actif = true;
				};
				void LancerSystemes(NkPhaseSysteme phase, float32 dt);
				void Relever();

				NkVector<NkSysteme> mSystemes;
				uint32 mProchainSysteme = 1;
				NkVector<NkContact2D> mContacts;
				struct NkCorpsEntite {
						uint32 corps = 0;
						ecs::NkEntityId entite;
				};
				NkVector<NkCorpsEntite> mCorpsEntite; ///< refait a chaque releve qui a des evenements

				NkSceneConfig mConfig;
				ecs::NkWorld mMonde;
				physics::NkPhysicsWorld *mPhysique = nullptr;
				physics::NkParticules2D *mParticules = nullptr;
				NkVue2D mCamera;
				float32 mAccumulateur = 0.f;
				int32 mDernierNbPas = 0;
		};

	} // namespace unkeny
} // namespace nkentseu
