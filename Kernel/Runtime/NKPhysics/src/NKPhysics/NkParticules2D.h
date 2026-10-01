// AUTEUR : Rihen
// =============================================================================
// NkParticules2D.h — SOLVEUR UNIFIE DE PARTICULES 2D : corps mous, blobs,
// fluides, sable, cristaux, tissus, cordes — couple aux corps rigides (2026-09-29).
//
// POURQUOI UN SOLVEUR A COTE DE NkPhysicsWorld, ET PAS DEDANS
//   NkPhysicsWorld resout des CORPS RIGIDES par impulses sequentielles. Une
//   gelee, un blob ou de l'eau n'ont pas de pose : ce sont des NUAGES de
//   particules tenus par des contraintes. La methode qui les traite tous d'un
//   seul geste est la dynamique par positions (Muller et al., « Position Based
//   Dynamics », 2006 ; Macklin et al., « Unified Particle Physics for Real-Time
//   Applications », SIGGRAPH 2014). Tout ce qui est ICI se touche : un blob
//   tombe dans l'eau, le sable s'amasse sur un tissu, un ballon rebondit sur un
//   cristal. C'est precisement ce que deux solveurs separes ne savent pas faire.
//
// ET POURQUOI PAS NkCloth POUR LE TISSU
//   NkCloth est un tissu 3D haute fidelite pour VETEMENTS : champ de distance du
//   corps, colliders animes, auto-collision par hachage. Il ne collisionne
//   qu'avec des FORMES. Un tissu 2D qui retient de l'eau ou recoit un blob doit
//   vivre dans le meme solveur que cette eau et ce blob. Les deux coexistent :
//   ils ne repondent pas a la meme question.
//
// LE COUPLAGE AVEC LES CORPS RIGIDES (dans les deux sens)
//   `Pas(dt, rigides)` lit les formes du NkPhysicsWorld (cercle, boite
//   orientee, capsule, segment 2D). Chaque contact particule / forme fait deux
//   choses :
//     POSITION   la particule sort de la forme (contrainte d'inegalite PBD) ;
//                le corps, lui, n'est deplace que par SON solveur ;
//     IMPULSION  sur la vitesse relative d'approche, impulse par impulse, la
//                vitesse du corps etant tenue a jour entre deux particules
//                (impulses sequentielles), frottement de Coulomb compris.
//   Un tas pose sur une caisse lui transmet ainsi son poids, et un blob lance
//   sur une caisse libre conserve la quantite de mouvement (temoin q6).
//   Un corps statique ou cinematique n'est que repoussant.
//   ⚠️ Appeler `Pas` AVANT `NkPhysicsWorld::Step` du meme pas fixe : les
//   impulses donnees ici sont integrees par le solveur rigide juste apres.
//
// LA METHODE D'UN SOUS-PAS (N sous-pas d'une iteration : Macklin 2019)
//   1. Predire     v += a h ; x* = x + v h           (gravite, vent, chaleur)
//   2. Grille      une construction par sous-pas, une liste de paires relue 4 fois
//   3. Saisie      la souris tire AVANT les contraintes
//   4. Liens       XPBD (Macklin 2016), rupture, plasticite de Maxwell (blob)
//   5. Pression    le ballon : contrainte d'AIRE (anneau gonfle)
//   6. Forme       appariement de forme (Muller 2005) : la gelee
//   7. Particules  double relaxation de densite (Clavet 2005) + collisions,
//                  frottement de Coulomb en positions (Macklin 2014 §6.1)
//   8. Rigides     contacts avec les corps de NkPhysicsWorld (voir plus haut)
//  8b. Attaches    particules tenues a un point d'un rigide (2026-09-29)
//   9. Limites     sol, murs, plafond (facultatifs)
//  10. Vitesses    v = (x - x_prec) / h, restitution
//  11. Viscosite   XSPH, moyenne ponderee (stable a toute viscosite <= 0,5)
//  12. Amortisseurs le long des liens (corps mous qui ne doivent pas rebondir)
//
// ET POUR LE JEU (2026-09-29, NkParticules2DJeu.cpp) : poussee AJOUTEE, contacts
// tenus sur le PAS ENTIER (le drapeau `contact` d'une particule, lui, meurt a
// chaque sous-pas), evenements DEBUT / FIN (rigides, zones, autres corps mous),
// parties nommees, attaches, saisies par pointeur, corps relies. Les zones
// (corps rigides NK_BODY_TRIGGER) se jugent une fois par pas, apres les sous-pas.
//
// UNITES : le metre, la seconde, le kilogramme — comme NkPhysicsWorld. Y vers le
// HAUT. La raideur est une COMPLIANCE (XPBD) : elle ne depend ni du pas ni du
// nombre de sous-pas, et pas davantage de l'unite de longueur.
//
// Zero STL. Aucune allocation par pas en regime etabli (les tampons grandissent
// puis restent).
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkVec.h"
#include "NKPhysics/NkPhysicsTypes.h"

namespace nkentseu {
	namespace physics {

		class NkPhysicsWorld;

		// ---------------------------------------------------------------------
		// Materiaux
		// ---------------------------------------------------------------------
		enum class NkMateriauP2D : uint8 {
			NK_BALLON = 0, ///< anneau + pression de gaz
			NK_BLOB,	   ///< fluide visco-elastique : coule comme une gelee, se coupe, se ressoude
			NK_GELEE,	   ///< grille + appariement de forme mou
			NK_EAU,		   ///< fluide peu visqueux
			NK_MIEL,	   ///< fluide tres visqueux, sans memoire de forme
			NK_SABLE,	   ///< granulaire : collisions + frottement fort
			NK_ATOMES,	   ///< reseau cristallin : liaisons cassables, chaleur
			NK_TISSU,	   ///< grille de liens, dechirable
			NK_CORDE,	   ///< chaine de liens (corde, pont)
			NK_COUNT
		};

		const char *NkMateriauP2DNom(NkMateriauP2D m) noexcept;
		bool NkEstFluideP2D(NkMateriauP2D m) noexcept;
		/// Corps dont les liens forment une GRILLE nx * ny : ils ne sont jamais
		/// purges, car leur ordre sert d'index au rendu (cellules dechirees).
		bool NkEstGrilleP2D(NkMateriauP2D m) noexcept;
		/// Corps qui gagnent et perdent des liaisons en cours de route.
		bool NkEstSoudableP2D(NkMateriauP2D m) noexcept;

		constexpr uint32 NK_P2D_AUCUN = 0xFFFFFFFFu;
		constexpr int32 NK_P2D_MAX_VOISINS = 8;

		// ---------------------------------------------------------------------
		// Donnees
		// ---------------------------------------------------------------------
		struct NkParticule2D {
				NkVec2f pos;
				NkVec2f prec; ///< position au debut du sous-pas
				NkVec2f vit;
				NkVec2f repos;	 ///< position de repos LOCALE (appariement de forme)
				NkVec2f normale; ///< normale du dernier contact
				float32 masse = 1.f;
				float32 invMasse = 1.f;
				float32 rayon = 0.05f;
				float32 densite = 0.f;
				float32 densiteProche = 0.f;
				uint32 corps = 0; ///< INDEX du corps (change a la compaction ; l'id, lui, reste)
				uint32 voisins[NK_P2D_MAX_VOISINS] = {};
				uint8 nbVoisins = 0;
				bool epingle = false;
				bool contact = false;
				bool saisie = false;
		};

		enum class NkGenreLien2D : uint8 {
			NK_STRUCTURE = 0, ///< arete de grille, maillon de corde
			NK_CISAILLEMENT,  ///< diagonale de grille (plus souple)
			NK_FLEXION,		  ///< saut d'un voisin (raideur de pliage)
			NK_LIAISON		  ///< liaison atomique ou ressort de blob (dynamique)
		};

		struct NkLien2D {
				uint32 a = 0;
				uint32 b = 0;
				uint32 corps = 0;
				float32 repos = 0.1f;
				float32 reposInitial = 0.1f;
				float32 tension = 0.f; ///< allongement relatif du dernier sous-pas (pour un rendu "Contraintes")
				NkGenreLien2D genre = NkGenreLien2D::NK_STRUCTURE;
				bool casse = false;
		};

		struct NkCorpsP2D {
				NkMateriauP2D mat = NkMateriauP2D::NK_BALLON;
				uint32 id = 0; ///< identifiant STABLE (l'index bouge a la compaction)
				uint64 utilisateur = 0; ///< donnee de l'appelant (une entite ECS emballee, par ex.)
				uint32 debut = 0;
				uint32 nombre = 0;
				uint32 lienDebut = 0; ///< grilles seulement
				uint32 lienNombre = 0;
				int32 nx = 0; ///< colonnes de la grille, ou nombre de points de l'anneau
				int32 ny = 0;
				float32 espacement = 0.1f;
				float32 aireRepos = 0.f;

				// --- Parametres (editables a chaud) --------------------------
				float32 raideur = 0.8f;		 ///< [0,1] 1 = rigide (convertie en compliance XPBD)
				float32 pression = 1.f;		 ///< ballon : aire cible / aire de repos. 0 = creve
				float32 friction = 0.4f;	 ///< [0,1]
				float32 rebond = 0.2f;		 ///< [0,1] restitution sur les statiques et les rigides
				float32 viscosite = 0.05f;	 ///< fluides : lissage XSPH [0, 0.5]
				float32 cohesion = 0.1f;	 ///< fluides : tension de surface [0, 1.5]
				float32 plasticite = 0.f;	 ///< blob : 1/s, vitesse a laquelle un ressort oublie sa longueur (Maxwell)
				float32 resistance = 0.f;	 ///< allongement de rupture (1.3 = casse a +30 %). 0 = incassable
				float32 masseParticule = 1.f; ///< kg
				float32 formeRaideur = 0.f;	 ///< appariement de forme [0,1] par sous-pas. 0 = desactive
				/// [0,1] amortisseur le long des liens (fraction de la vitesse relative
				/// d'etirement retiree par sous-pas). C'est ce qui empeche un blob de
				/// REBONDIR comme une balle : la viscosite XSPH lisse le cisaillement
				/// mais laisse passer le rebond d'ensemble (mesure : 0,99 m -> 0,81 m
				/// de remontee en passant de 0,1 a 0,4 de viscosite).
				float32 amortissement = 0.f;
				bool autoCollision = true;	 ///< collisions entre SES propres particules
				bool couplageRigide = true;	 ///< touche les corps de NkPhysicsWorld
				/// (2026-09-30) false : le corps est ETEINT (l'entite « inactive »
				/// d'Unkeny). Ses particules restent OU ELLES SONT : ni integrees, ni
				/// resolues, hors de la grille (donc sans aucun contact, ni entre
				/// elles, ni avec les rigides, ni avec les autres corps), ni saisies,
				/// ni dans les zones. Rallume, il repart immobile. AJOUTE A LA FIN,
				/// vrai par defaut : tout corps existant se simule comme avant.
				bool actif = true;
		};

		/// La boite du monde. Facultative : un monde de jeu a son sol en corps
		/// statiques ; un bac a sable veut une boite fermee sans en poser.
		struct NkLimites2D {
				bool actif = true;
				float32 gauche = -12.f;
				float32 droite = 12.f;
				float32 bas = 0.f;
				float32 haut = 14.f;
				bool murs = true;	 ///< false : le sol s'arrete aux bords, ce qui tombe disparait
				bool plafond = true;
		};

		struct NkReglagesP2D {
				NkVec2f gravite = NkVec2f(0.f, -9.81f);
				int32 sousPas = 8;
				float32 echelleTemps = 1.f;
				float32 temperature = 0.f; ///< [0,100] agitation thermique (atomes, fluides)
				float32 vent = 0.f;		   ///< m/s^2, horizontal
				float32 amortAir = 0.05f;  ///< 1/s
				bool collisions = true;
				bool soudure = true; ///< atomes et blobs refont des liaisons
				NkLimites2D limites;
		};

		struct NkStatsP2D {
				uint32 contacts = 0;
				uint32 contactsRigides = 0;
				uint32 ruptures = 0; ///< liaisons rompues pendant le DERNIER pas
				uint32 soudures = 0;
				float32 energieCinetique = 0.f; ///< J
				uint32 rupturesAttaches = 0; ///< attaches particule <-> rigide rompues pendant le DERNIER pas (2026-09-29)
		};

		// ---------------------------------------------------------------------
		// LE JEU (2026-09-29) : ce qu'il faut pour qu'un corps mou soit un
		// PERSONNAGE. Voir Applications/UnkenyEditor/design/00, § 4 (demande R15),
		// et NkParticules2DJeu.cpp, ou tout ceci est ecrit.
		// ---------------------------------------------------------------------

		/// Contre quoi une particule a touche. Un OU de ces bits par particule.
		enum NkNatureContactP2D : uint8 {
			NK_P2D_CONTACT_STATIQUE = 1u << 0,	  ///< corps rigide STATIQUE (sol, mur)
			NK_P2D_CONTACT_CINEMATIQUE = 1u << 1, ///< corps rigide cinematique (plateforme mobile)
			NK_P2D_CONTACT_DYNAMIQUE = 1u << 2,	  ///< corps rigide dynamique (caisse)
			NK_P2D_CONTACT_LIMITES = 1u << 3,	  ///< la boite du monde (NkLimites2D)
			NK_P2D_CONTACT_MOU = 1u << 4,		  ///< un AUTRE corps de particules
		};

		/// Resume des contacts d'un corps (ou d'une partie) sur le DERNIER PAS
		/// ENTIER, tous sous-pas reunis.
		/// ⚠️ POURQUOI « PAS ENTIER » : le drapeau `NkParticule2D::contact` est
		/// remis a faux a la fin de CHAQUE sous-pas (MajVitesses) -- apres Pas(),
		/// il n'en reste rien, et un jeu ne pouvait pas savoir qu'une gelee touchait
		/// le sol. Ce resume, lui, est tenu jusqu'au Pas suivant.
		struct NkContactCorpsP2D {
				uint32 particules = 0; ///< particules qui ont touche (et passent le filtre de pente)
				NkVec2f normale;	   ///< moyenne unitaire des normales, VERS le corps ((0, 1) = pose sur un sol)
				uint8 natures = 0;	   ///< OU des NkNatureContactP2D rencontrees
				uint32 statique = 0;   ///< particules par nature (une particule peut compter deux fois)
				uint32 cinematique = 0;
				uint32 dynamique = 0;
				uint32 limites = 0;
				uint32 mou = 0;
				NkBodyId rigide = 0; ///< le corps rigide le plus touche (0 = aucun)
				uint32 autreCorps = 0; ///< l'id STABLE de l'autre corps mou le plus touche (0 = aucun)
				bool Touche() const noexcept {
					return particules > 0u;
				}
		};

		/// Un DEBUT ou une FIN de contact d'un corps mou, pour les evenements de jeu.
		enum class NkGenreEvenementP2D : uint8 {
			NK_RIGIDE = 0, ///< `autre` = NkBodyId d'un corps rigide solide
			NK_ZONE,	   ///< `autre` = NkBodyId d'un corps rigide DECLENCHEUR (NK_BODY_TRIGGER)
			NK_CORPS_MOU   ///< `autre` = id STABLE d'un autre corps mou (`corps` < `autre`)
		};
		struct NkEvenementP2D {
				uint32 corps = 0; ///< id STABLE du corps mou
				uint32 autre = 0;
				NkGenreEvenementP2D genre = NkGenreEvenementP2D::NK_RIGIDE;
		};

		/// Une PARTIE NOMMEE d'un corps : un sous-ensemble de ses particules (la
		/// tete, un bras, les pieds), poussee et lue separement.
		/// ⚠️ Les particules sont tenues par INDEX, remappe a chaque compaction : une
		/// partie survit a la disparition d'un AUTRE corps, et perd seulement les
		/// particules gommees d'elle-meme.
		struct NkPartieP2D {
				uint32 id = 0;	  ///< identifiant STABLE de la partie
				uint32 corps = 0; ///< id STABLE du corps
				char nom[24] = {};
				uint32 debut = 0; ///< dans NkParticules2D::partiesParticules
				uint32 nombre = 0;
		};

		/// Une particule ATTACHEE a un point d'un corps rigide (des parties molles
		/// sur un personnage rigide). Resolue a chaque sous-pas apres les contacts
		/// rigides ; le rigide recoit les impulses en retour (les deux sens).
		struct NkAttacheP2D {
				uint32 corps = 0;	  ///< id STABLE du corps mou (verification)
				uint32 particule = 0; ///< INDEX, remappe a chaque compaction
				NkBodyId rigide = 0;
				NkVec2f ancre;			///< point dans le REPERE du rigide (position + angle)
				float32 raideur = 1.f;	///< [0,1] comme les liens (compliance XPBD)
				float32 rupture = 0.f;	///< m : au-dela de cet ecart, l'attache CASSE. 0 = incassable
				float32 ecart = 0.f;	///< ecart particule / ancre qui RESTE apres correction, dernier sous-pas (m)
				float32 tension = 0.f;	///< ecart AVANT correction, dernier sous-pas (m) : ce que la matiere a tire
				bool casse = false;
		};

		struct NkParamsFluide2D {
				float32 h = 0.22f;		  ///< rayon d'interaction (m)
				float32 rho0 = 1.f;		  ///< densite de repos, calculee sur un reseau hexagonal
				float32 kPression = 0.2f; ///< sans dimension : deplacement en fraction de h
				float32 kProche = 0.12f;
		};

		// ---------------------------------------------------------------------
		// NkParticules2D
		// ---------------------------------------------------------------------
		class NkParticules2D {
			public:
				NkParticules2D();

				void Vider() noexcept;

				/// Un pas FIXE de duree dt, redecoupe en sous-pas. `rigides` peut
				/// etre nul : aucun couplage. Voir l'en-tete pour l'ORDRE d'appel.
				void Pas(float32 dt, NkPhysicsWorld *rigides = nullptr) noexcept;

				// --- Construction (voir NkParticules2DFabrique.h) --------------
				/// Ouvre un corps. Ses particules doivent etre ajoutees AVANT
				/// d'en ouvrir un autre : un corps est une plage contigue.
				uint32 NouveauCorps(NkMateriauP2D mat) noexcept;
				uint32 AjouterParticule(uint32 corps, const NkVec2f &pos, float32 rayon) noexcept;
				/// Repos = distance actuelle.
				uint32 AjouterLien(uint32 a, uint32 b, NkGenreLien2D genre) noexcept;
				/// Lien DYNAMIQUE : tient a jour les listes de voisins. false si
				/// deja relies ou si l'un des deux est sature.
				bool Relier(uint32 a, uint32 b, float32 repos) noexcept;
				/// Ce qui depend de TOUTES les particules d'un corps : positions de
				/// repos, aire de l'anneau, plages de liens.
				void FinaliserCorps(uint32 corps) noexcept;

				// --- Requetes --------------------------------------------------
				int32 ParticuleProche(const NkVec2f &p, float32 rayonMax) const noexcept;
				int32 IndexCorps(uint32 id) const noexcept;
				NkVec2f CentreCorps(uint32 corps) const noexcept;
				NkVec2f VitesseCorps(uint32 corps) const noexcept;
				void BoiteCorps(uint32 corps, NkVec2f &mn, NkVec2f &mx) const noexcept;
				NkParamsFluide2D ParamsFluide(NkMateriauP2D m) const noexcept;
				uint32 LiensActifs() const noexcept;
				uint32 LiensActifsDuCorps(uint32 corps) const noexcept;

				// --- Editions --------------------------------------------------
				void SupprimerCorps(uint32 corps) noexcept;
				/// Gomme : les particules LIBRES (fluides, sable, atomes, blob) sont
				/// effacees une a une ; un corps structure l'est en entier.
				uint32 Gommer(const NkVec2f &p, float32 rayon) noexcept;
				/// Rompt tous les liens qui croisent le segment [a,b].
				uint32 Couper(const NkVec2f &a, const NkVec2f &b) noexcept;
				void Explosion(const NkVec2f &c, float32 rayon, float32 vitesse) noexcept;
				/// Attire (acceleration > 0) ou repousse vers c. A appeler a chaque pas.
				void Aimant(const NkVec2f &c, float32 rayon, float32 acceleration, float32 dt) noexcept;
				void Translater(uint32 corps, const NkVec2f &delta) noexcept;
				void AppliquerVitesse(uint32 corps, const NkVec2f &v) noexcept;
				void BasculerEpingle(uint32 particule) noexcept;
				void EpinglerCorps(uint32 corps, bool epingle) noexcept;
				void AppliquerMasse(uint32 corps) noexcept;

				// --- Saisie a la souris ---------------------------------------
				bool SaisirDebut(const NkVec2f &p, float32 rayon) noexcept;
				void SaisirVers(const NkVec2f &p) noexcept;
				void SaisirFin() noexcept;
				bool EnSaisie() const noexcept {
					return mSaisis.Size() > 0;
				}
				NkVec2f CibleSaisie() const noexcept {
					return mCibleSaisie;
				}

				// =============================================================
				// LE JEU (2026-09-29) — NkParticules2DJeu.cpp
				// Tout ce qui suit AJOUTE ; rien de ce qui precede ne change de sens.
				// =============================================================

				// --- Pousser SANS ecraser (manque M5b) -------------------------
				// ⚠️ AppliquerVitesse REMPLACE la vitesse de chaque particule : appelee a
				// chaque pas pour marcher, elle annule la gravite et fige la forme.
				// Celles-ci AJOUTENT : la deformation et la gravite sont gardees.
				/// v += dv pour chaque particule libre du corps.
				void AjouterVitesse(uint32 corps, const NkVec2f &dv) noexcept;
				/// Une FORCE totale (N) repartie selon la masse : chaque particule libre
				/// recoit l'acceleration F / M. A appeler a chaque pas, avec son dt.
				void AppliquerForce(uint32 corps, const NkVec2f &force, float32 dt) noexcept;
				/// Une acceleration (m/s^2) uniforme, quelle que soit la masse.
				void AppliquerAcceleration(uint32 corps, const NkVec2f &acceleration, float32 dt) noexcept;
				/// Un COUPLE (N.m) autour du centre de masse : le corps ROULE.
				void AppliquerCouple(uint32 corps, float32 couple, float32 dt) noexcept;
				/// La vitesse angulaire d'ensemble (rad/s, sens trigonometrique) :
				/// moment cinetique / moment d'inertie, autour du centre de masse.
				float32 VitesseAngulaireCorps(uint32 corps) const noexcept;
				/// Retire la part `fraction` [0,1] de la rotation d'ensemble, sans
				/// toucher a la translation ni a la deformation. C'est ce qui empeche un
				/// personnage mou pousse sur un sol qui frotte de ROULER -- et de
				/// rebondir sur ses coins (mesure du banc Gelee, 2026-09-29).
				void FreinerRotation(uint32 corps, float32 fraction) noexcept;

				// --- Parties nommees (decision de Rihen du 2026-09-29) ---------
				/// Cree une partie du corps `corps` (INDEX) avec les particules
				/// `relatives` (0 = premiere particule du corps). Rend son id (0 = refus :
				/// corps inconnu, nom vide ou deja pris, aucune particule valide).
				uint32 CreerPartie(uint32 corps, const char *nom, const uint32 *relatives, uint32 nombre) noexcept;
				/// Les particules du corps dans le disque (centre, rayon).
				uint32 CreerPartieZone(uint32 corps, const char *nom, const NkVec2f &centre, float32 rayon) noexcept;
				/// Les particules du corps dans la boite [mn, mx].
				uint32 CreerPartieBoite(uint32 corps, const char *nom, const NkVec2f &mn, const NkVec2f &mx) noexcept;
				void SupprimerPartie(uint32 partie) noexcept;
				/// L'id de la partie `nom` du corps d'id STABLE `corpsId`, ou 0.
				uint32 TrouverPartie(uint32 corpsId, const char *nom) const noexcept;
				int32 IndexPartie(uint32 partie) const noexcept;
				void AjouterVitessePartie(uint32 partie, const NkVec2f &dv) noexcept;
				void AppliquerForcePartie(uint32 partie, const NkVec2f &force, float32 dt) noexcept;
				NkVec2f CentrePartie(uint32 partie) const noexcept;
				NkVec2f VitessePartie(uint32 partie) const noexcept;

				// --- Contacts tenus sur le pas (manque M5a) --------------------
				/// Les contacts du corps pendant le DERNIER Pas. `normaleYMin` filtre par
				/// pente : 0,7 ne garde que ce qui porte (sol a moins de 45 degres) ;
				/// -2 (defaut) garde tout, murs et plafond compris.
				NkContactCorpsP2D ContactCorps(uint32 corps, float32 normaleYMin = -2.f) const noexcept;
				NkContactCorpsP2D ContactPartie(uint32 partie, float32 normaleYMin = -2.f) const noexcept;

				// --- Evenements DEBUT / FIN (manque M5e) -----------------------
				/// Les paires (corps mou, autre) qui ont COMMENCE, ou CESSE, de se
				/// toucher pendant le dernier Pas : rigides, zones (declencheurs),
				/// autres corps mous. Un corps disparu donne sa FIN au Pas suivant.
				const NkVector<NkEvenementP2D> &ContactsDebut() const noexcept {
					return mDebuts;
				}
				const NkVector<NkEvenementP2D> &ContactsFin() const noexcept {
					return mFins;
				}
				/// Les paires en contact au dernier Pas (la « photo » dont sont tires
				/// DEBUT et FIN).
				const NkVector<NkEvenementP2D> &ContactsEnCours() const noexcept {
					return mPairesPrec;
				}

				// --- Attache particule <-> rigide (manque M5f) ------------------
				/// Attache la particule `particule` (INDEX) au rigide `rigide` du monde,
				/// a l'endroit OU ELLE EST. Rend l'index de l'attache, ou -1.
				int32 Attacher(uint32 particule, const NkPhysicsWorld &monde, NkBodyId rigide, float32 raideur = 1.f,
							   float32 rupture = 0.f) noexcept;
				/// Attache chaque particule du corps a moins de `rayon` de `point`.
				uint32 AttacherZone(uint32 corps, const NkVec2f &point, float32 rayon, const NkPhysicsWorld &monde,
									NkBodyId rigide, float32 raideur = 1.f, float32 rupture = 0.f) noexcept;
				/// Retire toutes les attaches vers ce rigide (0 = toutes).
				uint32 Detacher(NkBodyId rigide) noexcept;
				uint32 AttachesActives() const noexcept;
				/// Les rigides ont ete REFAITS (photo restauree, scene rechargee) : leurs
				/// ids ont change. `anciens[i]` devient `nouveaux[i]`, tous a la fois --
				/// un remplacement un par un confondrait un ancien id et un nouveau.
				void RemapperRigides(const NkBodyId *anciens, const NkBodyId *nouveaux, uint32 n) noexcept;

				// --- Relier deux corps (decision de Rihen, 2026-09-29) ---------
				/// Relie chaque particule du corps `a` a la particule du corps `b` la
				/// plus proche, si elle est a moins de `portee` : des liens DYNAMIQUES
				/// (NK_LIAISON, par Relier), qui prennent les parametres du corps `a`
				/// (raideur, resistance). Rend le nombre de liens poses.
				/// ⚠️ Pas AjouterLien : un lien de structure pose apres coup casse la
				/// contiguite des liens d'une grille (gelee, tissu), dont le rendu se sert.
				uint32 RelierCorps(uint32 a, uint32 b, float32 portee) noexcept;

				// --- Saisie PAR POINTEUR (manque M5d) --------------------------
				// ⚠️ SaisirDebut/Vers/Fin ne tiennent qu'UNE saisie, pour tous les corps a
				// la fois. Celles-ci en tiennent une par POINTEUR (doigts, souris,
				// manettes), restreintes a un corps si demande, sans jamais voler une
				// particule deja tenue. Les deux coexistent.
				/// `corps` : INDEX du seul corps a prendre, ou -1 pour tous. Rend le
				/// nombre de particules prises (0 = rien sous le pointeur).
				uint32 SaisirPointeurDebut(uint32 pointeur, const NkVec2f &p, float32 rayon, int32 corps = -1) noexcept;
				void SaisirPointeurVers(uint32 pointeur, const NkVec2f &p) noexcept;
				void SaisirPointeurFin(uint32 pointeur) noexcept;
				void SaisirPointeursFin() noexcept;
				bool EnSaisiePointeur(uint32 pointeur) const noexcept;
				uint32 PointeursEnSaisie() const noexcept {
					return static_cast<uint32>(mPointeurs.Size());
				}
				/// Combien de particules le pointeur tient.
				uint32 ParticulesSaisies(uint32 pointeur) const noexcept;

				// --- Etat ------------------------------------------------------
				NkVector<NkParticule2D> particules;
				NkVector<NkLien2D> liens;
				NkVector<NkCorpsP2D> corps;
				NkReglagesP2D reglages;
				NkStatsP2D stats;
				uint32 rupturesTotal = 0;
				uint32 prochainId = 1;
				// Ajoutes le 2026-09-29 (le jeu). Publics comme `liens` : la sauvegarde
				// et un editeur les lisent tels quels.
				NkVector<NkPartieP2D> parties;
				NkVector<uint32> partiesParticules; ///< INDEX de particules, bout a bout
				uint32 prochainIdPartie = 1;
				NkVector<NkAttacheP2D> attaches;

			private:
				struct Rigide {
						void *corps = nullptr; ///< NkRigidBody*
						uint8 type = 0;		   ///< NkBodyType
						uint8 forme = 0;	   ///< 0 cercle, 1 boite, 2 capsule/segment
						NkVec2f centre;		   ///< forme monde au debut du pas
						NkVec2f a, b;		   ///< capsule / segment
						NkVec2f demi;		   ///< boite
						float32 angle = 0.f;
						float32 rayon = 0.f;
						NkVec2f posCorps;
						NkVec2f vitesse;		  ///< tenue a jour par les impulses du pas
						float32 omega = 0.f;
						NkVec2f vitessePose;	  ///< celle du DEBUT du pas : elle seule extrapole la pose
						float32 omegaPose = 0.f;
						float32 invMasse = 0.f;
						float32 invInertie = 0.f;
						float32 friction = 0.5f;
						float32 rebond = 0.f;
						uint32 contacts = 0;	  ///< contacts du sous-pas courant
						uint32 contactsPrec = 1;  ///< ceux du sous-pas precedent (partage de masse)
						float32 mnx = 0.f, mny = 0.f, mxx = 0.f, mxy = 0.f; ///< boite englobante (etendue)
						NkBodyId id = 0;		  ///< 2026-09-29 : pour les contacts et les attaches
						float32 angleCorps = 0.f; ///< angle du corps au debut du pas (ancres des attaches)
				};

				void SousPas(float32 h, int32 k) noexcept;
				void Predire(float32 h) noexcept;
				void ResoudreSaisie() noexcept;
				void ResoudreLiens(float32 h) noexcept;
				void ResoudrePression(float32 h) noexcept;
				void ResoudreForme() noexcept;
				void ResoudreParticules() noexcept;
				void PreparerRigides(NkPhysicsWorld *rigides) noexcept;
				void ResoudreRigides(float32 h, float32 t) noexcept;
				void ResoudreLimites() noexcept;
				void MajVitesses(float32 h) noexcept;
				void Viscosite() noexcept;
				void AmortirLiens() noexcept;
				void Souder() noexcept;
				void PurgerLiens() noexcept;
				void Compacter(const NkVector<uint8> &garder) noexcept;
				void RecalculerPlages() noexcept;
				void RetirerVoisin(uint32 a, uint32 b) noexcept;
				void Contact(NkParticule2D &p, const NkVec2f &n, float32 profondeur, float32 friction) noexcept;
				void ConstruireGrille() noexcept;
				void ConstruirePaires() noexcept;
				template <typename F> void PourPaires(F &&f) noexcept;
				template <typename F> void PourCellules(float32 mnx, float32 mny, float32 mxx, float32 mxy, F &&f) noexcept;

				NkVector<int32> mCellDebut;
				NkVector<int32> mCellFin;
				NkVector<uint32> mTri;
				NkVector<uint32> mCleTri;
				NkVector<uint32> mPaires;
				NkVector<uint8> mEstFluide;
				int32 mGx = 0;
				int32 mGy = 0;
				float32 mCell = 0.24f;
				NkVec2f mOrigine;
				bool mGrilleValide = false;

				NkVector<Rigide> mRigides;
				NkVector<uint32> mSaisis;
				NkVector<NkVec2f> mSaisisDecalage;
				NkVec2f mCibleSaisie;
				NkVector<NkVec2f> mTmpV2;
				NkVector<float32> mTmpF;

				NkParamsFluide2D mFluide[static_cast<int32>(NkMateriauP2D::NK_COUNT)];
				uint32 mAlea = 0x9E3779B9u;
				uint32 mCasseesPurgeables = 0;

				// --- Le jeu (2026-09-29), NkParticules2DJeu.cpp ---------------
				/// Contacts accumules sur le pas, PAR PARTICULE (remappes a la
				/// compaction) : bits de nature, somme des normales, dernier rigide et
				/// dernier autre corps touches.
				NkVector<uint8> mContactNature;
				NkVector<NkVec2f> mContactNormale;
				NkVector<uint32> mContactRigide;
				NkVector<uint32> mContactMou;
				void NoterContact(uint32 i, uint8 nature, const NkVec2f &n, uint32 autre) noexcept;
				void OuvrirContactsDuPas() noexcept;
				/// Paires du pas en cours, du pas precedent, et ce qui en sort.
				NkVector<NkEvenementP2D> mPairesPas;
				NkVector<NkEvenementP2D> mPairesPrec;
				NkVector<NkEvenementP2D> mDebuts;
				NkVector<NkEvenementP2D> mFins;
				void NoterPaire(uint32 corpsId, uint32 autre, NkGenreEvenementP2D genre) noexcept;
				void FermerEvenementsDuPas() noexcept;
				/// Les rigides DECLENCHEURS du pas (formes 0, 1, 2 comme Rigide).
				NkVector<Rigide> mZones;
				void DetecterZones() noexcept;
				/// Rigide de chaque attache pour ce pas (index dans mRigides, -1 = absent).
				NkVector<int32> mAttacheRigide;
				void PreparerAttaches(NkPhysicsWorld *monde) noexcept;
				void ResoudreAttaches(float32 h, float32 t, int32 sousPas) noexcept;
				/// Pose d'un rigide au temps t du pas (voir ResoudreRigides).
				void PoseRigide(const Rigide &r, float32 t, NkVec2f &pivot, float32 &dA) const noexcept;
				/// Saisies par pointeur : une entree par pointeur, puis les particules
				/// tenues, bout a bout (pointeur, particule, decalage).
				struct Pointeur {
						uint32 id = 0;
						NkVec2f cible;
				};
				NkVector<Pointeur> mPointeurs;
				NkVector<uint32> mPtrId;
				NkVector<uint32> mPtrParticule;
				NkVector<NkVec2f> mPtrDecalage;
				void ResoudreSaisiesPointeurs() noexcept;
				/// Suit les particules a travers une compaction (`remap` : ancien index
				/// -> nouveau, NK_P2D_AUCUN = disparue).
				void RemapperJeu(const NkVector<uint32> &remap) noexcept;

				float32 Alea() noexcept; ///< [-1, 1), deterministe (xorshift32)
		};

		/// Compliance XPBD pour une raideur [0,1] (1 = rigide).
		float32 NkComplianceP2D(float32 raideur) noexcept;
		/// Densite de repos d'un fluide range en reseau hexagonal d'espacement s.
		float32 NkDensiteReseauP2D(float32 s, float32 h) noexcept;

	} // namespace physics
} // namespace nkentseu
