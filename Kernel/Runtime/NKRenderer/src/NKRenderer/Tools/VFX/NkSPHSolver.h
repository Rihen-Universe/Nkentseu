#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSPHSolver.h — fluide SPH sur le stockage des particules (2026-09-04).
// Rodolf : « si tu ne trouves pas, on crée. »
//
// Un fluide = un émetteur dont NkEmitterDesc::solver pointe un NkSPHSolver :
// le stockage CPU (NkParticleStoreCPU) l'appelle à la place de sa gravité, le
// dessin (quad instancié, texture, mélange) ne change pas. Le solveur vit DU
// CÔTÉ DU PROPRIÉTAIRE de l'état (NkVFXSystem) ; il déménagera avec le
// stockage le jour où NKSimulation naît, sans toucher au dessin.
//
// MÉTHODE : DFSPH (Bender & Koschier 2015), décision du 04/09 nuit après six
// expériences sur le WCSPH explicite (voir DECISIONS) : le fluide s'éjectait au
// lieu de se comprimer, pire à chaque cran de raideur, et restait immobile sans
// gravité -- la formulation, pas le code. DFSPH n'a pas d'équation d'état :
//   1. voisinage (grille uniforme O(N), fantômes de paroi dans la même grille),
//      densité rho_i et facteur alpha_i = rho_i / (|sum m gradW|^2 + sum |m gradW|^2) ;
//   2. solveur de DIVERGENCE NULLE : kappa^v = (Drho/Dt) alpha / dt, corrige v ;
//   3. forces non-pression : XSPH (viscosité) et gravité ;
//   4. solveur de DENSITÉ CONSTANTE : rho* prédit -> kappa = (rho* - rho0) alpha / dt^2,
//      corrige v, itéré jusqu'à |rho* - rho0|/rho0 moyen < 0,1 % (borne d'itérations dite) ;
//   5. x += dt v. Pas de temps : CFL 0,4 h / vmax MESURÉ, sous-pas comptés.
// Noyau cubique (SPlisHSPlasH) pour W et gradW -- un seul noyau, alpha cohérent.
// Parois : particules fantômes (Akinci), deux couches fixes, jamais intégrées ;
// le clamp de boîte n'est plus qu'un filet.
// =============================================================================
#include "NkParticleStore.h"
#include "NkForceField.h"

namespace nkentseu {
	namespace renderer {

		struct NkSPHParams {
				float32 h = 0.1f;			  // rayon de support (m) = cellule de la grille ; espacement des particules h/2
				float32 restDensity = 1000.f; // rho0 (kg/m3)
				float32 stiffness = 200.f;	  // (WCSPH historique, inutilisé par DFSPH ; gardé pour la trace)
				float32 viscosity = 0.05f;	  // coefficient XSPH (0 = aucune viscosité)
				float32 particleMass = 0.f;	  // 0 = CALIBRÉE : rho0 / somme W sur le réseau idéal d'espacement h/2
				float32 restitution = 0.3f;	  // rebond du filet de boîte
				float32 maxSpeed = 20.f;	  // borne de sécurité (m/s), dite si elle mord
				uint32 maxSubSteps = 24;	  // plafond des sous-pas par image
				float32 cfl = 0.4f;			  // dt_sous-pas <= cfl * h / vmax mesuré
				uint32 maxIterDensity = 100;  // bornes des solveurs, dites quand atteintes
				uint32 maxIterDivergence = 100;
				float32 tolDensity = 0.001f;  // 0,1 % : |rho* - rho0| / rho0 moyen
				float32 tolDivergence = 0.001f;
				// ── Boutons d'EXPÉRIENCE (04/09 nuit, chasse au front trop rapide) ──
				// surfaceMode : 1 = borne dure rho* >= rho0 (défaut DFSPH), 0 = aucune borne
				// (kappa négatif autorisé : traction), 2 = borne douce (la moitié de la traction).
				uint32 surfaceMode = 1;
				// Viscosité artificielle de Monaghan (alpha), fluide-fluide et fluide-paroi ;
				// c nominal = artSoundSpeed. 0 = désactivée.
				// MESURE (04/09 nuit) : sans viscosite le front de rupture de barrage est 1,5-2x trop
				// rapide ; alpha = 0,075 (c = 50, h = 0,1) le met a 13 % de la reference SPH 2D (Cebron &
				// Sigrist) et a 26 % de Martin & Moyce -- MAIS le repos n'est plus calme (vmax 1,2 m/s au
				// lieu de 0,04) : la viscosite artificielle explicite injecte du mouvement au repos. Les
				// deux temoins se paient l'un l'autre : le DEFAUT reste 0 (repos vert, front rouge, dits),
				// et la suite nommee est une viscosite physique (laminaire) avec sa CFL visqueuse.
				float32 artViscosity = 0.f;
				float32 artSoundSpeed = 50.f;
				// VISCOSITÉ PHYSIQUE laminaire (Morris, Fox & Zhu, « Modeling Low Reynolds Number Incompressible
				// Flows Using SPH », J. Comput. Phys. 136, 214-226, 1997), nu cinématique en m²/s : nulle au repos
				// par construction (elle ne voit que les différences de vitesse), indépendante de la résolution.
				// Sa condition de pas, telle que donnée par Morris et al. : dt <= 0,125 hs² / nu, hs = longueur de
				// LISSAGE = h/2 ici (h est le rayon de support du noyau cubique) ; comptée dans les sous-pas et
				// dite (NkSPHStats::subStepsViscous). 0 = désactivée. Eau : 1e-6 (hors de portée d'un banc à 16
				// particules par largeur : le nu retenu est dit avec son Reynolds).
				// DÉFAUT 0,02 m²/s (mesuré le 04/09 23h, Release, scènes de la sonde, binaire reconstruit après la
				// coupure) : avec sa condition de pas (2 sous-pas à h = 0,1, 5 à h = 0,05 -- elle mord), les cinq
				// témoins du repos restent verts sur 10 s (1,001, sol 1,001, vmax 0,019 m/s, conservation, 0 NaN) ET le
				// front de rupture de barrage est à 10 % de la référence SPH 2D (Cébron & Sigrist) -- 12 % à h/2 sur la
				// MÊME colonne de 0,8 m (32 768 particules) : indépendant de la résolution à 2 points près, ce que la
				// viscosité artificielle n'avait pas. Martin & Moyce : 65 % brut / 28 % avec le retard de vanne sur la
				// colonne carrée (table n² = 2 appliquée sous l'adimensionnement de M&M) ; sur la géométrie de la table
				// (n² = 2, canal 3D, fantômes sur les six faces) : 9 % brut, 11 % à h/2 -- là, la comparaison est
				// légitime. Balayage : nu = 0 -> 41 %, 0,01 -> 27 %, 0,02 -> 10 %, 0,03 -> 10 % (repos vert aussi) ;
				// 0,02 retenu (le plus petit qui tient la cible). Reynolds du banc : U a / nu = sqrt(2 g a) a / nu ~
				// 3,96 x 0,8 / 0,02 ~ 160. L'eau (1e-6, Re ~ 3e6) est hors de portée d'un maillage à 16 particules par largeur.
				float32 kinematicViscosity = 0.02f;
				// Frottement de paroi : poids du terme XSPH des fantômes (0 = glissement libre,
				// 1 = même poids qu'une voisine fluide à vitesse nulle = non-glissement partiel).
				float32 wallFriction = 1.f;
				// DÉMARRAGE À CHAUD du solveur de densité (comme SPlisHSPlasH, TimeStepDFSPH::warmstartPressureSolve) :
				// le kappa TOTAL appliqué au pas précédent, mémorisé par emplacement du stockage tel quel (kappa
				// est une pression, invariant au pas -- pas en kappa·dt² : mesuré, ça explose), est appliqué une
				// fois avant d'itérer. But mesuré : des itérations qui ne
				// croissent plus avec le domaine (7 -> 13 de 1 000 à 50 000 sans lui). MESURÉ (04/09 23h30, s = 0,5) :
				// itérations 6/4 -> 4/3 (1 000), 21/12 -> 13/8 (10 648), 25/15 -> 15/8 (50 653) aux images 30/60 ;
				// en régime établi (image 60) les deux grands domaines sont à 8 -- la croissance avec N s'arrête là,
				// pas pendant l'effondrement ; repos et front inchangés. Coût 50 653 : 497-686 ms/image, ROUGE contre
				// 100 ms (3 sous-pas x 8-15 traversées x 0,35 µs/particule) : le CPU ne suffira pas, dit.
				bool warmStart = true;
				// Amortissement de la poussée à chaud : kappa_chaud = warmStartScale x kappa total du pas précédent.
				// MESURÉ (04/09, 23h30) à 1,0 : le solveur ne sait pas tirer (kappa >= 0, borne de surface, et le
				// solveur de divergence ignore la détente) -> tout excès rejoué est irréversible et s'accumule :
				// repos à 0,41 rho0 en 30 images. À s < 1 un excès décroît en s^n et les itérations fournissent le reste.
				float32 warmStartScale = 0.5f;
				NkVec3f gravity = {0.f, -9.8f, 0.f};
				NkVec3f boundsMin = {-1.f, 0.f, -1.f};
				NkVec3f boundsMax = {1.f, 2.f, 1.f};

				float32 Mass() const; // calibrée sur le réseau (NkSPHSolver.cpp)
				float32 SoundSpeed() const;
		};

		// Ce que le dernier pas a mesuré — les témoins lisent ici, pas une impression.
		struct NkSPHStats {
				uint32 alive = 0;
				uint32 subSteps = 0;
				uint32 speedClamped = 0; // particules bornées par maxSpeed (0 attendu)
				float32 densityMean = 0.f, densityMin = 0.f, densityMax = 0.f;
				float32 maxSpeed = 0.f;
				float32 ms = 0.f;
				float32 maxX = 0.f, maxY = 0.f, minY = 0.f; // front (rupture de barrage), hauteur (repos)
				float32 frontDenseX = 0.f;					  // max x des particules a rho >= 0,5 rho0 : le front que l'on FILME,
															  // pas l'eclat isole qui vole devant (mesure du 04/09)
				uint32 boundary = 0;						  // particules fantômes de paroi (fixes)
				float32 densityFloorMean = 0.f;				  // densité moyenne de la couche du sol (y < ymin + h)
				// DFSPH : par image (moyennes sur les sous-pas), la trace les dit
				float32 iterDensity = 0.f, iterDivergence = 0.f; // itérations moyennes par sous-pas
				float32 residualDensity = 0.f;					 // résidu final moyen |rho*-rho0|/rho0 (fraction)
				float32 residualDivergence = 0.f;				 // résidu final moyen Drho/Dt dt / rho0
				uint32 iterCapHits = 0;							 // fois où une borne d'itérations a été atteinte
				uint32 warmStarts = 0;							 // sous-pas où le kappa du pas précédent a été appliqué avant d'itérer
				// Instrument du démarrage à chaud (dernier sous-pas) : résidu NON BORNÉ |rho* - rho0|/rho0 moyen
				// (la détente comptée, que la borne de surface cache au solveur) avant et après la poussée, et le
				// minimum de rho*/rho0 après elle.
				float32 warmResidualBefore = 0.f, warmResidualAfter = 0.f, warmMinRatio = 0.f;
				uint32 subStepsViscous = 0;						 // sous-pas imposés par la CFL visqueuse (0 = la CFL de vitesse a décidé)
				uint32 clumped = 0;								 // particules à rho > 1,1 rho0 (agglutination = instabilité de traction)
				uint32 syncs = 0;								 // GPU : relectures (synchronisations) de l'image -- une par itération, dites
				// PROFIL GPU PAR PASSE (2026-09-05, NK_SPH_PROFILE=1, OpenGL seul : Vulkan n'a pas de chrono) : ms GPU
				// et nombre de dispatchs par sorte de noyau (ordre de l'énumération de NkSPHStoreGPU), et l'attente CPU
				// des relectures, à part. Sans ce tableau, aucun levier (mesure du 05/09 : le tri n'était pas le coût).
				bool gpuProfile = false;
				float32 gpuPassMs[16] = {};
				uint32 gpuPassN[16] = {};
				float32 cpuWaitMs = 0.f;
		};

		class NkSPHSolver final : public NkIParticleSolver {
			public:
				NkSPHParams params;
				bool pressureEnabled = true; // faux = mutation « projection coupée » (le témoin repos doit rougir)

				void Apply(NkParticleStoreCPU &store, const NkEmitterDesc &desc, float32 dt) override;
				// Le MÊME fluide sur GPU (NkSPHStoreGPU, 2026-09-05) : mêmes paramètres, mêmes statistiques.
				NkIParticleStore *CreateGPUStore(NkIDevice *device, const NkEmitterDesc &desc) override;
				// Les fantômes de paroi : une seule recette pour le CPU et le GPU.
				static void BuildBoundaryPositions(const NkSPHParams &p, NkVector<NkVec3f> &out);
				const NkSPHStats &Stats() const {
					return mStats;
				}

				// Remplit une boîte de naissances sur un réseau d'espacement `spacing`
				// (h/2 par défaut) — pour les témoins (bloc au repos, rupture de barrage).
				static uint32 FillBlock(NkVector<NkParticleBirth> &out, NkVec3f min, NkVec3f max, float32 spacing,
										float32 life = 1.0e9f);

			private:
				friend class NkSPHStoreGPU; // écrit mStats et mLastVmax à la place d'Apply
				void StepOnce(NkParticleStoreCPU &store, float32 dt);
				void BuildBoundary();
				void BuildNeighbors(const NkVec3f *X, uint32 n, uint32 M);
				void ComputeDensityAndAlpha(uint32 n);
				// grille et tampons réutilisés (aucune allocation par pas en régime établi)
				NkVector<uint32> mAlive;	   // indices vivants dans le stockage
				NkVector<uint32> mCellOf, mCellStart, mCellCount, mSorted;
				NkVector<NkVec3f> mPosAll; // vivantes puis fantômes
				NkVector<NkVec3f> mVel;	   // vitesses de travail des vivantes
				NkVector<NkVec3f> mVisc;   // accélérations visqueuses (Morris), lues avant application
				// listes de voisines (une construction par sous-pas, 10-20 traversées ensuite)
				NkVector<uint32> mNbStart; // n + 1
				NkVector<uint32> mNbIdx;   // indice dans mPosAll (< n : fluide, sinon fantôme)
				NkVector<NkVec3f> mNbGrad; // gradW_ij (de i vers j : gradient en x_i)
				NkVector<float32> mNbW;	   // W_ij
				NkVector<float32> mDensity, mAlpha, mKappa, mDensityAdv;
				NkVector<float32> mKappaSlot; // démarrage à chaud : kappa total du pas précédent, par emplacement du stockage
				NkVector<float32> mKappaSum;  // kappa total du pas courant (vivantes)
				NkVector<uint32> mNbCount;
				// PAROIS PAR PARTICULES FANTÔMES : deux couches fixes autour de la boîte,
				// espacement h/2, même masse ; comptées dans la densité, jamais intégrées.
				NkVector<NkVec3f> mBound;
				NkVec3f mBoundMin = {0, 0, 0}, mBoundMax = {0, 0, 0};
				float32 mBoundH = 0.f;
				float32 mLastVmax = 0.f; // pour la CFL du pas suivant
				NkForceField mField;	 // le vent de l'émetteur (copié dans Apply), ajouté à la gravité
				float32 mTime = 0.f;
				NkSPHStats mStats;
		};

	} // namespace renderer
} // namespace nkentseu
