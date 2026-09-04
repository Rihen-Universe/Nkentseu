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
		};

		class NkSPHSolver final : public NkIParticleSolver {
			public:
				NkSPHParams params;
				bool pressureEnabled = true; // faux = mutation « projection coupée » (le témoin repos doit rougir)

				void Apply(NkParticleStoreCPU &store, const NkEmitterDesc &desc, float32 dt) override;
				const NkSPHStats &Stats() const {
					return mStats;
				}

				// Remplit une boîte de naissances sur un réseau d'espacement `spacing`
				// (h/2 par défaut) — pour les témoins (bloc au repos, rupture de barrage).
				static uint32 FillBlock(NkVector<NkParticleBirth> &out, NkVec3f min, NkVec3f max, float32 spacing,
										float32 life = 1.0e9f);

			private:
				void StepOnce(NkParticleStoreCPU &store, float32 dt);
				void BuildBoundary();
				void BuildNeighbors(const NkVec3f *X, uint32 n, uint32 M);
				void ComputeDensityAndAlpha(uint32 n);
				// grille et tampons réutilisés (aucune allocation par pas en régime établi)
				NkVector<uint32> mAlive;	   // indices vivants dans le stockage
				NkVector<uint32> mCellOf, mCellStart, mCellCount, mSorted;
				NkVector<NkVec3f> mPosAll; // vivantes puis fantômes
				NkVector<NkVec3f> mVel;	   // vitesses de travail des vivantes
				// listes de voisines (une construction par sous-pas, 10-20 traversées ensuite)
				NkVector<uint32> mNbStart; // n + 1
				NkVector<uint32> mNbIdx;   // indice dans mPosAll (< n : fluide, sinon fantôme)
				NkVector<NkVec3f> mNbGrad; // gradW_ij (de i vers j : gradient en x_i)
				NkVector<float32> mNbW;	   // W_ij
				NkVector<float32> mDensity, mAlpha, mKappa, mDensityAdv;
				NkVector<uint32> mNbCount;
				// PAROIS PAR PARTICULES FANTÔMES : deux couches fixes autour de la boîte,
				// espacement h/2, même masse ; comptées dans la densité, jamais intégrées.
				NkVector<NkVec3f> mBound;
				NkVec3f mBoundMin = {0, 0, 0}, mBoundMax = {0, 0, 0};
				float32 mBoundH = 0.f;
				float32 mLastVmax = 0.f; // pour la CFL du pas suivant
				NkSPHStats mStats;
		};

	} // namespace renderer
} // namespace nkentseu
