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
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKECS/Hierarchy/NkHierarchy.h"
#include "NKECS/World/NkWorld.h"
#include "NKPhysics/NkParticules2D.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "Unkeny/Scene/NkUnkenyCamera.h"
#include "Unkeny/Scene/NkUnkenyChamps.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"
#include "Unkeny/Scene/NkUnkenyHierarchie.h"

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
				/// Ses ENFANTS ne sont pas detruits : ils deviennent des racines, a
				/// leur place dans le monde (comme UE5). DetruireAvecEnfants pour
				/// la cascade.
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

				/// Applique au solveur un NkCorps2D MODIFIE (type, masse, friction,
				/// rebond, amortissements, gravite, rotation bloquee) ou un
				/// collisionneur modifie. Le corps est refait ; sa position, son
				/// orientation et ses vitesses sont gardees.
				///
				/// ⚠️ Ecrire dans le composant ne suffit PAS : NkCorps2D est la
				/// DESCRIPTION, le corps vit dans NKPhysics (regle du fichier : les
				/// composants portent des identifiants, pas l'etat). Un inspecteur
				/// qui oublie cet appel montre une masse que le solveur n'a pas.
				bool ActualiserCorps(ecs::NkEntityId id);

				/// Retire le corps RIGIDE d'une entite (le composant et le corps du
				/// solveur). L'entite reste, avec son transform et le reste.
				bool RetirerCorps(ecs::NkEntityId id);

				/// Donne a une entite EXISTANTE la matiere du corps de particules
				/// `indexCorps` (fabrique par NKPhysics/NkParticules2DFabrique). C'est
				/// le « Ajouter un composant > Corps mou » d'un editeur ; CreerCorpsMou
				/// fait la meme chose en creant l'entite.
				bool AttacherCorpsMou(ecs::NkEntityId id, int32 indexCorps, uint32 couleur);
				/// Retire le corps mou (sa matiere disparait), l'entite reste.
				bool RetirerCorpsMou(ecs::NkEntityId id);

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

				// --- Identite stable (2026-09-29, NkUnkenyHierarchie.cpp) --------
				// La poignee ecs::NkEntityId CHANGE a Restaurer et au chargement ;
				// l'identite, non. C'est elle que le fichier, les prefabs, la
				// hierarchie et les references entre entites (NK_ENTITE) ecrivent.
				// Creer la donne ; une entite faite a la main par Monde() la recoit
				// au premier AssurerUid (Photographier le fait pour toutes).

				/// L'identite de `id`, 0 s'il n'en a pas (ou s'il est mort).
				uint64 Uid(ecs::NkEntityId id) const noexcept;
				/// L'identite de `id`, donnee maintenant s'il n'en avait pas.
				uint64 AssurerUid(ecs::NkEntityId id);
				/// L'entite VIVANTE qui porte `uid`, ou Invalid(). Un uid ne sert
				/// jamais deux fois dans la vie d'une scene : une entite detruite
				/// ne se fait pas remplacer par une autre sous le meme numero.
				ecs::NkEntityId EntiteParUid(uint64 uid);
				uint64 ProchainUid() const noexcept {
					return mProchainUid;
				}

				// --- Hierarchie (2026-09-29, NkUnkenyHierarchie.cpp) ------------
				// NkTransform2D reste le MONDE (le rendu, la physique, les gizmos n'y
				// voient aucune difference). Un enfant porte en plus ecs::NkParent et
				// NkLocal2D ; PropagerHierarchie recalcule son monde. Voir
				// NkUnkenyHierarchie.h pour la regle « qui gagne ».
				//
				// ⚠️ UN ENFANT A CORPS DYNAMIQUE OU MOU N'EST PAS PORTE : c'est la
				//    physique qui le mene (comme un Rigidbody enfant chez Unity) ; son
				//    local suit son monde. Un corps STATIQUE ou CINEMATIQUE, lui, est
				//    porte — son corps est deplace avec lui.

				/// Rattache `enfant` a `parent`. `garderMonde` : il ne bouge pas a
				/// l'ecran (son local est calcule) ; sinon son transform actuel
				/// DEVIENT son local. Refuse (false) : entite morte, soi-meme, boucle.
				bool Rattacher(ecs::NkEntityId enfant, ecs::NkEntityId parent, bool garderMonde = true);
				/// En fait une racine. `garderMonde` : il reste ou il est ; sinon son
				/// local devient son monde.
				bool Detacher(ecs::NkEntityId enfant, bool garderMonde = true);
				/// Le parent vivant, ou Invalid() pour une racine.
				ecs::NkEntityId Parent(ecs::NkEntityId id) const noexcept;
				void Enfants(ecs::NkEntityId id, NkVector<ecs::NkEntityId> &out);
				/// Toute la descendance, parents AVANT enfants.
				void Descendants(ecs::NkEntityId id, NkVector<ecs::NkEntityId> &out);
				/// Le local d'un enfant, ou nul pour une racine.
				const NkTransform2D *Local(ecs::NkEntityId id) const noexcept;
				/// Pose le local d'un enfant (le monde suit a la propagation, faite
				/// ici). false pour une racine : son local EST son monde.
				bool PoserLocal(ecs::NkEntityId enfant, const NkTransform2D &local);
				/// Detruit l'entite ET toute sa descendance (corps compris).
				void DetruireAvecEnfants(ecs::NkEntityId id);
				/// Recalcule le monde de chaque enfant, parents d'abord. Pas() l'appelle ;
				/// un editeur qui ne JOUE pas l'appelle a chaque trame, pour que
				/// deplacer un parent emporte ses enfants a l'ecran.
				void PropagerHierarchie();

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
						/// ⚠️ Un champ NK_ENTITE d'un composant DECRIT y porte l'IDENTITE
						///    (uid, 8 octets) de sa cible, pas sa poignee : c'est ce qui
						///    lui permet de survivre a Restaurer et au fichier.
						NkVector<uint8> extra;
						uint32 extraPresents = 0u;
						// --- Ajoutes le 2026-09-29 (identite et hierarchie) ---------
						uint64 uid = 0;		  ///< NkIdentite2D ; 0 = en donner une neuve
						uint64 parentUid = 0; ///< identite du parent ; 0 = racine
						NkTransform2D local;  ///< NkLocal2D::local, si parentUid != 0
				};
				struct NkPhoto {
						NkVector<NkPhotoEntite> entites;
						physics::NkParticules2D particules;
						bool valide = false;
						uint64 prochainUid = 0; ///< le compteur d'identites (2026-09-29)
				};
				void Photographier(NkPhoto &photo);

				/// AJOUTE a la scene les entites d'une photo, SANS rien detruire —
				/// Restaurer, c'est « tout detruire » puis ceci. Les identites de la
				/// photo sont reprises telles quelles (0 = une neuve) : l'appelant qui
				/// en veut de nouvelles (un prefab qu'on instancie) les renumerote
				/// avant. Les parents et les champs NK_ENTITE sont rebranches par
				/// identite, APRES que toutes les entites existent.
				/// `crees` : les entites faites, dans l'ordre de la photo.
				/// `nouvellesIdentites` : chaque entite recoit une identite NEUVE (une
				/// instance de prefab, une copie) ; celles de la photo ne servent plus
				/// qu'a rebrancher parents et references ENTRE entites de la photo.
				void RefaireEntites(const NkVector<NkPhotoEntite> &entites, NkVector<ecs::NkEntityId> *crees = nullptr,
									bool nouvellesIdentites = false);
				/// La photo d'UNE entite (celle que Photographier prend pour chacune).
				/// false si elle est morte ou n'a pas de transform.
				bool PhotographierEntite(ecs::NkEntityId id, NkPhotoEntite &e);

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
					const void *cle = CleType<T>(); // une adresse par type T
					for (uint32 i = 0; i < mCopieurs.Size(); ++i) {
						if (mCopieurs[i].cle == cle) {
							return; // deja declare
						}
					}
					if (mCopieurs.Size() >= 32u) {
						return;
					}
					NkCopieurPhoto k;
					k.cle = cle;
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
					k.defaut = [](uint8 *dst) {
						const T v{};
						std::memcpy(dst, &v, sizeof(T));
					};
					k.retirer = [](ecs::NkWorld &w, ecs::NkEntityId id) {
						if (w.Has<T>(id)) {
							w.Remove<T>(id);
						}
					};
					mCopieurs.PushBack(k);
				}

				/// La meme declaration, AVEC la description de ses champs
				/// (NkUnkenyChamps.h) : le composant est alors ecrit CHAMP PAR CHAMP
				/// dans le fichier (portable, et relu apres l'ajout d'un champ), ses
				/// references (entite, son, texture, prefab) sont ecrites par
				/// identite ou par nom, et un prefab le fusionne champ par champ.
				/// Sans description, rien ne change : les octets, comme avant.
				///
				/// `champs` : tableau STATIQUE (il n'est pas copie). Un champ faux
				/// (genre et taille en desaccord, hors du composant) fait refuser TOUTE
				/// la description — et le dit : le composant reste en octets.
				/// Un composant deja declare sans description la recoit ici.
				template <typename T> void PhotographierAussi(const char *nom, const NkChampSauve *champs, uint32 nbChamps) {
					PhotographierAussi<T>(nom);
					PoserChamps(CleType<T>(), nom, champs, nbChamps);
				}

				/// La description des champs du copieur `i` (nul s'il n'en a pas).
				const NkChampSauve *ChampsComposantPhoto(uint32 i, uint32 &nombre) const noexcept {
					nombre = i < mCopieurs.Size() ? mCopieurs[i].nbChamps : 0u;
					return i < mCopieurs.Size() ? mCopieurs[i].champs : nullptr;
				}
				/// Ecrit dans `dst` (TailleComposantPhoto(i) octets) la valeur par
				/// DEFAUT du composant `i` : la base d'une relecture champ par champ,
				/// ou un champ absent du fichier garde sa valeur par defaut.
				bool DefautComposantPhoto(uint32 i, uint8 *dst) const noexcept {
					if (i >= mCopieurs.Size() || mCopieurs[i].defaut == nullptr) {
						return false;
					}
					mCopieurs[i].defaut(dst);
					return true;
				}
				/// Pose sur `id` le composant `i` depuis ses octets DE PHOTO (champs
				/// NK_ENTITE en identites : ils sont rebranches ici). C'est le geste
				/// qu'un prefab fait sur ses instances.
				bool EcrireComposantPhoto(uint32 i, ecs::NkEntityId id, const uint8 *octets);
				/// Retire de `id` le composant `i`.
				bool RetirerComposantPhoto(uint32 i, ecs::NkEntityId id);
				/// L'indice du copieur declare sous `nom`, ou -1.
				int32 IndexComposantPhoto(const char *nom) const noexcept {
					for (uint32 i = 0; nom != nullptr && i < mCopieurs.Size(); ++i) {
						if (mCopieurs[i].nom != nullptr && std::strcmp(mCopieurs[i].nom, nom) == 0) {
							return static_cast<int32>(i);
						}
					}
					return -1;
				}
				/// Detruit toutes les entites et les refait depuis la photo. Les
				/// POIGNEES d'entite (ecs::NkEntityId) CHANGENT ; ceux des corps mous,
				/// non — et depuis le 2026-09-29 les IDENTITES (Uid) non plus, ni les
				/// parents, ni les references NK_ENTITE des composants decrits.
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
						// --- 2026-09-29 : la description champ par champ ------------
						void (*defaut)(uint8 *) = nullptr;
						void (*retirer)(ecs::NkWorld &, ecs::NkEntityId) = nullptr;
						const NkChampSauve *champs = nullptr; ///< tableau STATIQUE, ou nul
						uint32 nbChamps = 0;
				};
				NkVector<NkCopieurPhoto> mCopieurs;

				/// Une adresse par type T : la cle d'un copieur.
				template <typename T> static const void *CleType() noexcept {
					static const char cle = 0;
					return &cle;
				}
				void PoserChamps(const void *cle, const char *nom, const NkChampSauve *champs, uint32 nbChamps);
				/// Dans les octets d'un composant decrit : poignee -> identite
				/// (`versUid`), ou identite -> poignee (l'inverse), pour chaque champ
				/// NK_ENTITE. C'est ce qui garde une reference juste a travers
				/// Restaurer et le fichier.
				/// `lot` : les identites d'un RefaireEntites en cours, cherchees AVANT
				/// celles de la scene (une photo peut reprendre un uid deja vivant).
				void ConvertirEntites(uint32 copieur, uint8 *octets, bool versUid,
									  const NkUnorderedMap<uint64, ecs::NkEntityId> *lot = nullptr);
				ecs::NkEntityId Resoudre(uint64 uid, const NkUnorderedMap<uint64, ecs::NkEntityId> *lot);

				// --- Identite (NkUnkenyHierarchie.cpp) ---------------------------
				uint64 mProchainUid = 1;
				/// uid -> entite, refait quand une recherche tombe sur une entree
				/// perimee. Jamais cru sans verification : une entree n'est rendue
				/// que si l'entite vit ET porte toujours cet uid.
				NkUnorderedMap<uint64, ecs::NkEntityId> mCacheUid;
				void RefaireCacheUid();
				/// Un enfant a corps STATIQUE ou CINEMATIQUE suit son parent : son
				/// corps est pose au monde `m` (le pont 2D <-> 3D vit dans
				/// NkUnkenyScene.cpp). Rend la rotation EXACTE que la synchronisation
				/// physique reecrira, pour que la propagation suivante ne prenne pas
				/// l'arrondi de l'aller-retour angle -> quaternion pour un geste.
				float32 PorterCorps(ecs::NkEntityId id, const NkTransform2D &m);
				/// Le corps de l'entite est-il mene par la physique (dynamique, mou) ?
				bool MeneParPhysique(ecs::NkEntityId id) const noexcept;

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
