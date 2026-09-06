#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidGrid.h — FUMÉE : fluide EULÉRIEN sur grille 3D (2026-09-05).
//
// MÉTHODE : Jos STAM, « Stable Fluids », SIGGRAPH 1999, p. 121-128 — advection
// SEMI-LAGRANGIENNE (inconditionnellement stable) + PROJECTION de pression
// (décomposition de Helmholtz-Hodge, § 2.3 : on retranche le gradient du champ
// de pression qui annule la divergence). Flottabilité et champs de fumée :
// Ronald FEDKIW, Jos STAM, Henrik Wann JENSEN, « Visual Simulation of Smoke »,
// SIGGRAPH 2001, p. 15-22 — équation (8) :
//     f_flottabilité = -alpha * s * ŷ + beta * (T - T_ambiante) * ŷ
// où s est la densité de fumée et T la température.
//
// ⚠️ CE QUE CE FICHIER N'EST PAS : `NkGrid3D` (NKRHI/Tools/Grid3D) est la GRILLE
// DE SOL DU VIEWPORT (lignes, axes, fondu) — un décor, pas un solveur. Le nom
// est le même, l'objet n'a rien à voir. Mesuré le 05/09 avant d'écrire ici.
//
// ⚠️ POURQUOI PAS LE NOYAU DE PROJECTION DU DFSPH : mesuré le 05/09 (§ « ce qui
// existe »). `NkSPHSolver` n'expose AUCUN solveur de Poisson réutilisable — sa
// « projection » DFSPH est une correction de vitesse par facteur alpha_i sur des
// listes de VOISINES, écrite en ligne dans `StepOnce`, sans matrice ni
// Laplacien. Ici la discrétisation est un Laplacien à 6 voisins sur grille
// régulière : c'est un AUTRE opérateur. Le partage sera le stockage GPU (SSBO),
// pas le noyau. Dit plutôt que supposé.
//
// GRILLE : collocated (tout au centre de cellule, comme le code de référence de
// Stam), (nx+2) x (ny+2) x (nz+2) — une COUCHE DE BORD sur chaque face. La
// POPULATION que les témoins comptent est l'INTÉRIEUR : nx*ny*nz cellules. La
// résolution est pilotée par `cellSize` (m), jamais par un nombre figé.
//
// ORDRE D'UN PAS (Stam 1999, § 2.2, adapté fumée par Fedkiw 2001) :
//   1. sources (densité, température, carburant)   — Emit*()
//   2. forces  : flottabilité (Fedkiw eq. 8) + gravité de la fumée froide
//   3. advection de la VITESSE (semi-lagrangienne)
//   4. PROJECTION (Poisson, Gauss-Seidel, résidu et itérations MESURÉS)
//   5. advection des SCALAIRES (densité, température, carburant)
//   6. dissipation
//
// ZÉRO STL : NkVector (NKContainers), fonctions de NKMath. Aucun std::.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace renderer {

		// =====================================================================
		// Paramètres — la résolution vient de cellSize, pas d'un compte de cellules
		// =====================================================================
		struct NkFluidGridParams {
				// Domaine (m) et taille de cellule (m). nx = ceil((max.x-min.x)/cellSize), etc.
				math::NkVec3f boundsMin = {-0.5f, 0.f, -0.5f};
				math::NkVec3f boundsMax = {0.5f, 2.f, 0.5f};
				float32 cellSize = 0.05f;

				// FLOTTABILITÉ — Fedkiw, Stam & Jensen 2001, eq. (8).
				// buoyancyBeta <= 0 => BOUSSINESQ : beta = gravity / ambientTemperature,
				// c'est-à-dire l'accélération d'Archimède a = g (T - T_amb) / T_amb pour un
				// gaz parfait à pression constante. Le témoin (c) compare à CETTE loi.
				float32 ambientTemperature = 300.f; // K
				float32 gravity = 9.81f;			// m/s^2
				float32 buoyancyBeta = -1.f;		// <= 0 : Boussinesq (g / T_amb), en m/(s^2 K)
				float32 buoyancyAlpha = 0.f;		// poids de la densité (fumée qui pèse), m/s^2 par unité de densité

				// DISSIPATION (1/s) : d <- d * exp(-diss * dt). 0 = aucune (témoin de masse).
				float32 densityDissipation = 0.f;
				float32 temperatureDissipation = 0.f;

				// PROJECTION : Gauss-Seidel SUR-RELAXÉ (SOR). On sort dès que le résidu max
				// passe sous `pressureTolerance * résidu de départ` ; le nombre d'itérations
				// réellement faites et le résidu final sont dits dans les stats.
				// MESURÉ le 05/09 : sans sur-relaxation (omega = 1), 80 balayages laissaient
				// 0,90 % de divergence résiduelle sur 40^3 — le témoin (b) rougissait à cause
				// du SOLVEUR, pas de la formulation.
				uint32 pressureIterations = 400;
				float32 pressureTolerance = 1.0e-3f; // RELATIF au résidu de départ
				// <= 1 ou >= 2 : la valeur optimale théorique 2/(1 + sin(pi/N)) est utilisée
				// (Press & al., « Numerical Recipes » 3e éd., eq. 20.5.19), N = plus grande dimension.
				float32 pressureOmega = -1.f;
				// Le résidu n'est mesuré que tous les N balayages : le mesurer à chaque
				// balayage DOUBLE le coût du solveur (mesuré).
				uint32 residualCheckEvery = 8;
				// DEMARRAGE A CHAUD : on repart du champ de pression du pas precedent au lieu
				// de zero (comme le kappa du DFSPH). MESURE le 05/09 : 493 -> 63 balayages
				// moyens pour la meme tolerance. Le residu de reference reste celui de p = 0,
				// donc le critere ne devient pas plus facile parce qu'on part de plus pres.
				bool pressureWarmStart = true;
				// BACKTRACE : ordre 2 (point milieu, RK2) au lieu de l'ordre 1 de Stam.
				// MESURE le 05/09 : l'ordre 1 perdait 45 a 51 % de la masse en 500 pas dans un
				// panache (0,1 % seulement en translation pure) -- l'erreur en dt^2 |grad u|^2
				// du point de depart, pas les parois : c'est le cas T du banc qui l'a tranche.
				bool advectRK2 = true;

				// Borne de sécurité, dite si elle mord.
				float32 maxSpeed = 100.f;

				// ── INTERRUPTEURS DE MUTATION (les témoins DOIVENT rougir) ──────────
				bool projectionEnabled = true; // faux -> témoin (b) divergence rouge
				bool advectionEnabled = true;  // faux -> témoin (d) transport rouge
				bool buoyancyEnabled = true;   // faux -> témoin (c) rouge

				// ── COMBUSTION (palier ③) : 0 = éteinte, la grille reste de la fumée pure.
				// Taux de réaction (1/s) : carburant consommé par seconde à concentration 1.
				float32 burnRate = 0.f;
				// Chaleur dégagée par unité de carburant brûlé (K par unité de carburant).
				float32 heatPerFuel = 0.f;
				// Fumée (suie) produite par unité de carburant brûlé.
				float32 sootPerFuel = 0.f;
				// Refroidissement de la flamme vers l'ambiante (1/s) — Fedkiw 2001 § 5 utilise
				// un refroidissement en (T/T_max)^4 ; ici, exponentiel simple, DIT tel quel.
				float32 coolingRate = 0.f;
				float32 fuelDissipation = 0.f;
		};

		// =====================================================================
		// Ce que le dernier pas a MESURÉ — les témoins lisent ici, pas une impression.
		// La population comptée est écrite dans chaque champ.
		// =====================================================================
		struct NkFluidGridStats {
				uint32 cellsTotal = 0;	  // (nx+2)(ny+2)(nz+2), bords compris
				uint32 cellsInterior = 0; // nx*ny*nz — LA population de tous les témoins

				// Masse = somme(densité) * cellSize^3 sur l'INTÉRIEUR (unité : densité x m^3)
				float32 mass = 0.f;
				float32 heat = 0.f; // somme(T - T_amb) * cellSize^3, intérieur
				float32 fuel = 0.f; // somme(carburant) * cellSize^3, intérieur

				// DIVERGENCE, en m/s (div * cellSize), moyenne des |.| sur l'intérieur.
				// Le rapport que le témoin (b) lit est divAfterMean / velocityMean.
				float32 divBeforeMean = 0.f, divBeforeMax = 0.f;
				float32 divAfterMean = 0.f, divAfterMax = 0.f;
				float32 velocityMean = 0.f; // |u| moyen sur l'intérieur (m/s)
				float32 divRatio = 0.f;		// divAfterMean / velocityMean (sans unité)

				uint32 pressureIters = 0;		 // balayages SOR RÉELLEMENT faits
				float32 pressureResidual = 0.f;	 // résidu max final du Poisson (m/s)
				float32 pressureResidual0 = 0.f; // résidu max de départ (p = 0), m/s
				float32 pressureOmega = 0.f;	 // sur-relaxation réellement appliquée
				bool pressureCapHit = false;	 // la borne d'itérations a mordu

				float32 maxSpeed = 0.f;
				float32 maxTemperature = 0.f;
				uint32 speedClamped = 0; // cellules bornées par maxSpeed (0 attendu)
				uint32 nanCount = 0;	 // NaN/Inf trouvés (0 attendu)
				float32 ms = 0.f;
		};

		// =====================================================================
		// La grille
		// =====================================================================
		class NkFluidGrid final {
			public:
				NkFluidGrid() = default;
				~NkFluidGrid() = default;

				bool Init(const NkFluidGridParams &p);
				void Reset(); // remet les champs à zéro, garde l'allocation

				void Step(float32 dt);

				// ── Sources (coordonnées MONDE) ─────────────────────────────────
				// Sphère de rayon r : ajoute densité/température/carburant, additif.
				void EmitSphere(const math::NkVec3f &centerWorld, float32 radius, float32 density,
								float32 temperature, float32 fuel = 0.f);
				// Impose une vitesse uniforme à TOUT l'intérieur (témoin d'advection).
				void SetUniformVelocity(const math::NkVec3f &v);

				// ── Lecture (témoins et rendu) ──────────────────────────────────
				uint32 Nx() const { return mNx; }
				uint32 Ny() const { return mNy; }
				uint32 Nz() const { return mNz; }
				float32 CellSize() const { return mParams.cellSize; }
				const NkFluidGridParams &Params() const { return mParams; }
				NkFluidGridParams &Params() { return mParams; }
				const NkFluidGridStats &Stats() const { return mStats; }

				// Index dans les tableaux (i,j,k dans [0, N+1] : 0 et N+1 = bord).
				uint32 Idx(uint32 i, uint32 j, uint32 k) const {
					return i + (mNx + 2) * (j + (mNy + 2) * k);
				}

				const float32 *Density() const { return mDensity.Data(); }
				float32 *Density() { return mDensity.Data(); }
				const float32 *Temperature() const { return mTemperature.Data(); }
				float32 *Temperature() { return mTemperature.Data(); }
				const float32 *Fuel() const { return mFuel.Data(); }
				const float32 *VelocityX() const { return mU.Data(); }
				const float32 *VelocityY() const { return mV.Data(); }
				const float32 *VelocityZ() const { return mW.Data(); }

				// Échantillonnage trilinéaire d'un champ en coordonnées MONDE (rendu).
				float32 SampleDensityWorld(const math::NkVec3f &p) const;
				float32 SampleTemperatureWorld(const math::NkVec3f &p) const;

				// Barycentres pondérés (témoins de transport et de flottabilité), en MONDE.
				// Rendent false et ne touchent pas `out` si le poids total est nul.
				bool DensityCentroid(math::NkVec3f &out) const;
				bool TemperatureCentroid(math::NkVec3f &out) const; // pondéré par (T - T_amb) positif

				// Vitesse verticale moyenne pondérée par (T - T_amb) positif (témoin (c)).
				float32 HotVerticalVelocity() const;

				// Totaux sur l'INTÉRIEUR (les témoins les relisent hors d'un pas).
				float32 TotalMass() const;
				// Masse portee par la COUCHE INTERIEURE COLLEE AUX PAROIS (les six faces).
				// C'est le compteur qui dit si un temoin de conservation est encore VALIDE :
				// l'advection semi-lagrangienne DETRUIT la masse qui touche une paroi (voir .cpp).
				float32 WallLayerMass() const;
				float32 TotalHeat() const;
				float32 TotalFuel() const;

				// Le beta de flottabilité RÉELLEMENT appliqué (Boussinesq si params <= 0).
				float32 EffectiveBeta() const;

			private:
				void Allocate();
				void AddSources(float32 dt);
				void AddBuoyancy(float32 dt);
				void Combust(float32 dt);
				// Position de depart (coordonnees de GRILLE) d'ou vient ce qui arrive en (i,j,k).
				void Backtrace(uint32 i, uint32 j, uint32 k, float32 dt0, const NkVector<float32> &fu,
							   const NkVector<float32> &fv, const NkVector<float32> &fw, float32 &x, float32 &y,
							   float32 &z) const;
				void AdvectScalar(NkVector<float32> &dst, const NkVector<float32> &src, float32 dt, int32 bnd);
				void AdvectVelocity(float32 dt);
				void Project(float32 dt);
				void SetBoundary(int32 b, NkVector<float32> &f);
				float32 Trilinear(const NkVector<float32> &f, float32 x, float32 y, float32 z) const;
				void MeasureDivergence(float32 &meanOut, float32 &maxOut) const;
				void MeasureVelocity();

				NkFluidGridParams mParams;
				NkFluidGridStats mStats;
				uint32 mNx = 0, mNy = 0, mNz = 0; // cellules INTÉRIEURES
				uint32 mCount = 0;				  // (nx+2)(ny+2)(nz+2)
				bool mReady = false;

				NkVector<float32> mU, mV, mW;	  // vitesse (m/s), centre de cellule
				NkVector<float32> mU0, mV0, mW0;  // tampons d'advection / travail
				NkVector<float32> mDensity, mDensity0;
				NkVector<float32> mTemperature, mTemperature0;
				NkVector<float32> mFuel, mFuel0;
				NkVector<float32> mPressure, mDivergence;
		};

	} // namespace renderer
} // namespace nkentseu
