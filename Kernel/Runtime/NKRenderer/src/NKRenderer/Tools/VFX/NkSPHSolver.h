#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSPHSolver.h — fluide SPH (Smoothed Particle Hydrodynamics) sur le stockage
// des particules (2026-09-04). Rodolf : « si tu ne trouves pas, on crée. »
//
// Un fluide = un émetteur dont NkEmitterDesc::solver pointe un NkSPHSolver :
// le stockage CPU (NkParticleStoreCPU) l'appelle à la place de sa gravité, le
// dessin (quad instancié, texture, mélange) ne change pas. Mesuré avant :
// aucun NkFluid*, aucun SPH, aucune hauteur d'eau dans le dépôt ; NKSimulation
// est une spécification sans code (sa feuille de route D4 dit « SPH d'abord »).
// Le solveur vit DU CÔTÉ DU PROPRIÉTAIRE de l'état (NkVFXSystem) — pas un
// troisième exemplaire ; il déménagera avec le stockage le jour où NKSimulation
// naît, sans toucher au dessin.
//
// Noyau WCSPH classique (Müller 2003 / Monaghan) :
//   voisinage : grille uniforme de cellule h, tri par comptage O(N), 27 cellules ;
//   densité   : rho_i = sum_j m W_poly6(r_ij, h) ;
//   pression  : p_i = k (rho_i - rho0) ;
//   forces    : pression -sum_j m (p_i + p_j)/(2 rho_j) grad W_spiky
//               viscosité mu sum_j m (v_j - v_i)/rho_j lap W_visc
//               gravité ; bornes (boîte) avec restitution.
//   pas       : sous-pas jusqu'à dt <= 0,4 h / maxSpeed (CFL), dit au profil.
// Le solveur INTÈGRE lui-même les positions (sous-pas) : le stockage ne les
// intègre pas quand un solveur est présent.
// =============================================================================
#include "NkParticleStore.h"

namespace nkentseu {
	namespace renderer {

		struct NkSPHParams {
				float32 h = 0.1f;			  // rayon de lissage (m) = cellule de la grille
				float32 restDensity = 1000.f; // rho0 (kg/m3)
				float32 stiffness = 200.f;	  // k de p = k (rho - rho0)
				float32 viscosity = 0.5f;	  // mu
				float32 particleMass = 0.f;	  // 0 = CALIBREE : rho0 / somme_j W_poly6 sur le reseau ideal d'espacement h/2
											  //     (mesure du 04/09 : rho0*d^3 donnait rho/rho0 = 0,92 au repos, pas 1)
				float32 restitution = 0.3f;	  // rebond sur les parois
				float32 maxSpeed = 20.f;	  // borne de stabilité (m/s), dite si elle mord
				uint32 maxSubSteps = 12;	  // plafond des sous-pas par image
				float32 cfl = 0.15f;		  // dt_sous-pas <= cfl * h / max(vmax, c) -- 0,4 faisait bouillir (04/09)
				NkVec3f gravity = {0.f, -9.8f, 0.f};
				NkVec3f boundsMin = {-1.f, 0.f, -1.f};
				NkVec3f boundsMax = {1.f, 2.f, 1.f};

				float32 Mass() const; // calibree sur le reseau (NkSPHSolver.cpp)
				// Vitesse du son de l'equation d'etat lineaire p = k (rho - rho0) : c = sqrt(k).
				// La CFL doit la compter : dt <= 0,4 h / max(vmax, c) -- mesure du 04/09 : sans
				// elle, un sous-pas de 4,2 ms (c = 14 m/s exigeait 2,8 ms) faisait bouillir le repos.
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
		};

		class NkSPHSolver final : public NkIParticleSolver {
			public:
				NkSPHParams params;
				bool pressureEnabled = true; // faux = mutation « pression coupée » (le témoin repos doit rougir)

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
				// grille et tampons réutilisés (aucune allocation par pas en régime établi)
				NkVector<uint32> mAlive;	   // indices vivants
				NkVector<uint32> mCellOf;	   // cellule de chaque vivant (indice dans mAlive)
				NkVector<uint32> mCellStart; // début de cellule dans mSorted (préfixe)
				NkVector<uint32> mCellCount;
				NkVector<uint32> mSorted; // indices de mAlive triés par cellule
				NkVector<float32> mDensity, mPressure;
				NkVector<NkVec3f> mAccel;
				NkSPHStats mStats;
		};

	} // namespace renderer
} // namespace nkentseu
