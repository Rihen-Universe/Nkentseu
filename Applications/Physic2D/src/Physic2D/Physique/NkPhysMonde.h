// =============================================================================
// NkPhysMonde.h — le moteur physique 2D de Physic2D
//
// CE QUE CE FICHIER EST, ET CE QU'IL N'EST PAS
//   Il simule. Il ne dessine rien, n'ouvre aucune fenetre, ne lit aucun
//   evenement : c'est ce qui permet au banc (--selftest) de le faire tourner
//   sans ecran ni GPU, et de rendre son verdict par code de sortie.
//
// LA METHODE : DYNAMIQUE PAR POSITIONS (PBD / XPBD) EN SOUS-PAS
//   Tout est fait de PARTICULES. Un corps n'est qu'une plage contigue de
//   particules plus des CONTRAINTES qui les tiennent ensemble :
//
//     lien de distance (XPBD)   ressorts, barres, liaisons atomiques, tissu
//     pression (aire cible)     le ballon : un anneau gonfle par un gaz
//     appariement de forme      la gelee et la caisse : elles veulent revenir
//                               a leur forme de repos, a une rotation pres
//     double relaxation de      l'eau, le miel, le blob : fluide
//     densite (Clavet 2005)     visco-elastique a base de particules
//     liaisons plastiques       le blob : des ressorts qui cedent, se
//                               rompent et se RESSOUDENT — il coule comme
//                               une gelee, se coupe en deux et fusionne
//     collisions + frottement   particules entre elles, obstacles, murs
//
//   Chaque pas fixe (1/60 s) est decoupe en N sous-pas d'UNE iteration : a
//   cout egal, c'est ce qui converge le mieux (Macklin et al., "Small Steps
//   in Physics Simulation", 2019). La raideur se regle par COMPLIANCE
//   (XPBD) : elle ne depend ni du pas de temps ni du nombre de sous-pas.
//
// UNITES : le centimetre et la seconde. Axe Y vers le HAUT, sol en y = 0.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"
#include "Physic2D/Physique/NkPhysMath.h"

namespace nkentseu {
	namespace physic2d {

		// ---------------------------------------------------------------------
		// Materiaux
		// ---------------------------------------------------------------------
		enum class NkMateriau : uint8 {
			NK_BALLON = 0, ///< anneau + pression de gaz
			NK_BLOB,	   ///< fluide visco-elastique : coule comme une gelee
			NK_GELEE,	   ///< grille + appariement de forme mou
			NK_EAU,		   ///< fluide peu visqueux
			NK_MIEL,	   ///< fluide tres visqueux, sans memoire de forme
			NK_SABLE,	   ///< granulaire : collisions + frottement fort
			NK_ATOMES,	   ///< reseau cristallin : liaisons cassables, chaleur
			NK_CAISSE,	   ///< quasi rigide (appariement de forme integral)
			NK_TISSU,	   ///< grille de liens, dechirable
			NK_CORDE,	   ///< chaine de liens
			NK_COUNT
		};

		const char *NkMateriauNom(NkMateriau m) noexcept;
		bool NkEstFluide(NkMateriau m) noexcept;
		/// Corps dont le rendu lit la GRILLE (nx * ny) : ses liens ne sont
		/// jamais purges, leur ordre sert d'index.
		bool NkEstGrille(NkMateriau m) noexcept;
		/// Corps qui gagnent et perdent des liaisons en cours de route.
		bool NkEstSoudable(NkMateriau m) noexcept;

		// ---------------------------------------------------------------------
		// Donnees
		// ---------------------------------------------------------------------
		constexpr uint32 NK_PHYS_AUCUN = 0xFFFFFFFFu;
		constexpr int32 NK_PHYS_MAX_VOISINS = 8;

		struct NkParticule {
				NkV2 pos;
				NkV2 prec; ///< position au debut du sous-pas
				NkV2 vit;
				NkV2 repos;	  ///< position de repos LOCALE (appariement de forme)
				NkV2 normale; ///< normale du dernier contact statique
				float32 masse = 1.f;
				float32 invMasse = 1.f;
				float32 rayon = 5.f;
				float32 densite = 0.f;
				float32 densiteProche = 0.f;
				uint32 corps = 0;
				uint32 voisins[NK_PHYS_MAX_VOISINS] = {};
				uint8 nbVoisins = 0;
				bool epingle = false;
				bool contact = false;
				bool saisie = false;
		};

		enum class NkGenreLien : uint8 {
			NK_STRUCTURE = 0, ///< arete de grille, maillon de corde
			NK_CISAILLEMENT,  ///< diagonale de grille (plus souple)
			NK_FLEXION,		  ///< saut d'un voisin (raideur de pliage)
			NK_LIAISON		  ///< liaison atomique ou ressort de blob (dynamique)
		};

		struct NkLien {
				uint32 a = 0;
				uint32 b = 0;
				uint32 corps = 0;
				float32 repos = 10.f;
				float32 reposInitial = 10.f;
				float32 tension = 0.f; ///< allongement relatif du dernier sous-pas (rendu "Contraintes")
				NkGenreLien genre = NkGenreLien::NK_STRUCTURE;
				bool casse = false;
		};

		struct NkCorps {
				char nom[40] = {};
				NkMateriau mat = NkMateriau::NK_BALLON;
				uint32 id = 0; ///< identifiant STABLE (l'index, lui, bouge a la compaction)
				uint32 debut = 0;
				uint32 nombre = 0;
				uint32 lienDebut = 0; ///< grilles seulement
				uint32 lienNombre = 0;
				int32 nx = 0; ///< colonnes de la grille, ou nombre de points de l'anneau
				int32 ny = 0;
				float32 espacement = 10.f;
				float32 aireRepos = 0.f;

				// --- Parametres editables (panneau Details) -------------------
				float32 raideur = 0.8f;		///< [0,1] 1 = rigide
				float32 pression = 1.f;		///< ballon : aire cible / aire de repos
				float32 friction = 0.4f;	///< [0,1]
				float32 rebond = 0.2f;		///< [0,1] restitution sur les statiques
				float32 viscosite = 0.05f;	///< fluides : lissage XSPH [0, 0.5]
				float32 cohesion = 0.1f;	///< fluides : tension de surface [0,1]
				float32 plasticite = 0.3f;	///< blob : 1/s, vitesse a laquelle un ressort oublie sa longueur (Maxwell)
				float32 resistance = 0.f;	///< allongement de rupture (1.3 = casse a +30 %). 0 = incassable
				float32 masseParticule = 1.f;
				float32 formeRaideur = 0.f; ///< appariement de forme [0,1]. 0 = desactive
				uint8 couleur[3] = {200, 200, 200};
				bool visible = true;
				bool autoCollision = true; ///< collisions entre SES propres particules
		};

		/// Obstacle statique : une capsule (segment epaissi). a == b : un disque.
		struct NkObstacle {
				NkV2 a;
				NkV2 b;
				float32 rayon = 8.f;
		};

		struct NkReglages {
				NkV2 gravite = NkV2(0.f, -981.f);
				int32 sousPas = 8;
				float32 echelleTemps = 1.f;
				float32 temperature = 0.f; ///< [0,100] agitation thermique (atomes, fluides)
				float32 vent = 0.f;		   ///< cm/s^2 horizontal
				float32 amortAir = 0.05f;  ///< [0,1] par seconde
				float32 largeur = 2400.f;  ///< boite du monde : x dans [-L/2, L/2]
				float32 hauteur = 1400.f;  ///< y dans [0, H]
				bool collisions = true;
				bool soudure = true; ///< atomes et blobs refont des liaisons
				bool murs = true;
		};

		struct NkStatsPas {
				float32 msPhysique = 0.f;
				uint32 contacts = 0;
				uint32 ruptures = 0; ///< liaisons rompues pendant le DERNIER pas
				uint32 soudures = 0;
				float32 energieCinetique = 0.f;
		};

		struct NkParamsFluide {
				float32 h = 22.f;		  ///< rayon d'interaction
				float32 rho0 = 1.f;		  ///< densite de repos (calculee sur un reseau hexagonal)
				float32 kPression = 0.1f; ///< [0,1] sans dimension (deplacement en fraction de h)
				float32 kProche = 0.3f;
		};

		// ---------------------------------------------------------------------
		// NkMonde
		// ---------------------------------------------------------------------
		class NkMonde {
			public:
				NkMonde();

				void Vider() noexcept;

				/// Un pas FIXE de duree dt (le pas est redecoupe en sous-pas).
				void Pas(float32 dt) noexcept;

				// --- Construction (voir NkPhysFabrique) ------------------------
				/// Ouvre un corps. Ses particules doivent etre ajoutees AVANT
				/// d'en ouvrir un autre : un corps est une plage contigue.
				uint32 NouveauCorps(NkMateriau mat, const char *prefixeNom) noexcept;
				uint32 AjouterParticule(uint32 corps, const NkV2 &pos, float32 rayon) noexcept;
				/// Repos = distance actuelle.
				uint32 AjouterLien(uint32 a, uint32 b, NkGenreLien genre) noexcept;
				/// Lien DYNAMIQUE : tient a jour les listes de voisins. false si
				/// deja relies ou si l'un des deux est sature.
				bool Relier(uint32 a, uint32 b, float32 repos) noexcept;
				/// Calcule ce qui depend de TOUTES les particules : positions de
				/// repos, aire du ballon, plages de liens.
				void FinaliserCorps(uint32 corps) noexcept;

				// --- Requetes --------------------------------------------------
				int32 ParticuleProche(const NkV2 &p, float32 rayonMax) const noexcept;
				int32 IndexCorps(uint32 id) const noexcept;
				NkV2 CentreCorps(uint32 corps) const noexcept;
				void BoiteCorps(uint32 corps, NkV2 &mn, NkV2 &mx) const noexcept;
				NkParamsFluide ParamsFluide(NkMateriau m) const noexcept;
				uint32 LiensActifs() const noexcept;

				// --- Editions --------------------------------------------------
				void SupprimerCorps(uint32 corps) noexcept;
				/// Gomme : les particules libres (fluides, sable, atomes, blob)
				/// sont effacees une a une ; un corps structure l'est en entier.
				/// Les obstacles touches aussi. Rend le nombre d'elements effaces.
				uint32 Gommer(const NkV2 &p, float32 rayon) noexcept;
				/// Rompt tous les liens qui croisent le segment [a,b].
				uint32 Couper(const NkV2 &a, const NkV2 &b) noexcept;
				void Explosion(const NkV2 &c, float32 rayon, float32 vitesse) noexcept;
				/// Attire (force > 0) ou repousse vers c. A appeler a chaque pas.
				void Aimant(const NkV2 &c, float32 rayon, float32 acceleration, float32 dt) noexcept;
				void Translater(uint32 corps, const NkV2 &delta) noexcept;
				void AppliquerVitesse(uint32 corps, const NkV2 &v) noexcept;
				void BasculerEpingle(uint32 particule) noexcept;
				void EpinglerCorps(uint32 corps, bool epingle) noexcept;
				void AppliquerMasse(uint32 corps) noexcept;

				// --- Saisie a la souris ---------------------------------------
				bool SaisirDebut(const NkV2 &p, float32 rayon) noexcept;
				void SaisirVers(const NkV2 &p) noexcept;
				void SaisirFin() noexcept;
				bool EnSaisie() const noexcept {
					return mSaisis.Size() > 0;
				}
				NkV2 CibleSaisie() const noexcept {
					return mCibleSaisie;
				}

				// --- Etat ------------------------------------------------------
				NkVector<NkParticule> particules;
				NkVector<NkLien> liens;
				NkVector<NkCorps> corps;
				NkVector<NkObstacle> obstacles;
				NkReglages reglages;
				NkStatsPas stats;
				uint32 rupturesTotal = 0;
				uint32 prochainId = 1;
				uint32 compteurNoms[static_cast<int32>(NkMateriau::NK_COUNT)] = {};

			private:
				void SousPas(float32 h) noexcept;
				void Predire(float32 h) noexcept;
				void ResoudreSaisie() noexcept;
				void ResoudreLiens(float32 h) noexcept;
				void ResoudrePression(float32 h) noexcept;
				void ResoudreForme() noexcept;
				void ResoudreParticules() noexcept;
				void ResoudreStatiques() noexcept;
				void MajVitesses(float32 h) noexcept;
				void Viscosite() noexcept;
				void Souder() noexcept;
				void PurgerLiens() noexcept;
				/// Retire les particules dont garder[i] == 0. Remappe tout.
				void Compacter(const NkVector<uint8> &garder) noexcept;
				void RecalculerPlages() noexcept;
				void RetirerVoisin(uint32 a, uint32 b) noexcept;
				void Contact(NkParticule &p, const NkV2 &n, float32 profondeur, float32 friction) noexcept;

				void ConstruireGrille() noexcept;
				void ConstruirePaires() noexcept;
				template <typename F> void PourPaires(F &&f) noexcept;
				int32 CelluleX(float32 x) const noexcept;
				int32 CelluleY(float32 y) const noexcept;

				NkVector<int32> mCellDebut;
				NkVector<int32> mCellFin;
				NkVector<uint32> mTri;
				NkVector<uint32> mCleTri;
				int32 mGx = 0;
				int32 mGy = 0;
				float32 mCell = 24.f;
				NkV2 mOrigine;
				bool mGrilleValide = false;

				NkVector<uint32> mSaisis;
				NkVector<NkV2> mSaisisDecalage;
				NkV2 mCibleSaisie;

				NkParamsFluide mFluide[static_cast<int32>(NkMateriau::NK_COUNT)];
				NkVector<uint32> mPaires;
				NkVector<uint8> mEstFluide;
				NkVector<NkV2> mTmpV2;
				NkVector<float32> mTmpF;
				NkAlea mAlea;
				uint32 mCasseesPurgeables = 0;
		};

		/// Compliance XPBD pour une raideur [0,1] (1 = rigide).
		float32 NkCompliance(float32 raideur) noexcept;
		/// Densite de repos d'un fluide range en reseau hexagonal d'espacement s.
		float32 NkDensiteReseau(float32 s, float32 h) noexcept;

	} // namespace physic2d
} // namespace nkentseu
