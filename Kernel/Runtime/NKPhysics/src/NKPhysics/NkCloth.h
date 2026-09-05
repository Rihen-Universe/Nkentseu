#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkCloth.h — TISSU en dynamique par positions (XPBD), sur le CPU (2026-09-05).
// Famille 3 de ROADMAP_PRODUITS.md §6 ; décisions dans Engine/Noge/DECISIONS_RODOLF.md
// (bloc « tissu »).
//
// MÉTHODE : Position-Based Dynamics (Müller, Heidelberger, Hennix, Ratcliff,
// « Position Based Dynamics », VRIPhys 2006 / JVCIR 2007) avec la RAIDEUR de
// XPBD (Macklin, Müller, Chentanez, « XPBD: Position-Based Simulation of
// Compliant Constrained Dynamics », MIG 2016), et les sous-pas de Macklin et al.,
// « Small Steps in Physics Simulation », SCA 2019. Un pas de temps dt, découpé
// en `substeps` sous-pas de h = dt / substeps :
//   1. prédiction : v += h (g + F_ext / m) ; x_prev = x ; x += h v         (Müller 2007 §3.1)
//   2. lambda = 0 ; puis `iterations` fois, Gauss-Seidel sur les contraintes :
//        C(x_a, x_b) = |x_a - x_b| - l0                                     (Müller 2007 §4.1)
//        alpha~ = alpha / h²         <- la compliance, INDÉPENDANTE de h et des itérations
//        dlambda = (-C - alpha~ lambda) / (w_a + w_b + alpha~)              (XPBD 2016 éq. 18)
//        lambda += dlambda ; x_a += w_a dlambda n ; x_b -= w_b dlambda n     (éq. 17)
//      C'est ce qui distingue XPBD de PBD : en PBD, dlambda = -k C / (w_a + w_b)
//      et l'élongation résiduelle dépend du nombre d'itérations et du pas
//      (Müller 2007 §3.3, k' = 1 - (1 - k)^(1/n)). Ici alpha est en m/N : 0 =
//      inextensible, et le témoin (d) « dt et dt/2 -> même élongation » le prouve.
//      La mutation `params.xpbd = false` rejoue PBD pur (alpha ignorée) : (d) rougit.
//      CONVERGENCE, mesurée le 05/09 (nappe 32 x 32, Release, témoins (a) et (d)) :
//      lambda est remis à zéro à chaque sous-pas (XPBD 2016 §3.3) et n_it passes de
//      Gauss-Seidel ne propagent pas la charge le long d'une colonne de k maillons
//      (diffusif, O(k²) itérations). À 4 sous-pas x 4 itérations, l'élongation d'équilibre
//      n'est pas celle de la compliance mais ~ alpha F_1 / n_it + 2 g h² / n_it (une
//      particule de charge, un terme en h²) : (d) donnait 0,35 % / 0,16 % entre dt et
//      dt/2 -- rouge sur un solveur juste mais non convergé. Ce sont les SOUS-PAS qui
//      convergent (Macklin 2019), pas les itérations :
//        sous-pas x itérations | (a) étirement max | (d) écart dt / dt/2
//                  4 x 4       |     21,8 %        |     55 %
//                  4 x 16      |      5,5 %        |     14 %
//                  4 x 64      |      1,5 %        |    0,02 %
//                 16 x 4       |      1,45 %       |     1,1 %
//                 16 x 8       |      0,82 %       |     0,06 %
//                 32 x 4       |      0,43 %       |     0,26 %   <- défaut
//      Le DÉMARRAGE À CHAUD de lambda (appliqué une fois avant d'itérer, comme le kappa
//      du DFSPH) a été essayé et RETIRÉ : échelle 1,0 et 0,9 -> NaN en moins d'une
//      seconde, 0,7 -> explosion (vmax 52 m/s), 0,5 -> stable mais pas plus convergé
//      (0,24 % / 0,14 %). Un lambda mémorisé se rejoue par la vitesse (la correction de
//      position du sous-pas précédent est déjà dans v) : il compte deux fois.
//   3. collisions avec les formes du monde (sphère, capsule, plan, boîte alignée)
//      par projection hors de la surface à `thickness` (Müller 2007 §4.4, contrainte
//      d'inégalité) ; auto-collision par hachage spatial (NkSpatialHash, Teschner
//      2003), paires non voisines à moins de 2 x thickness, projetées à parts de masse.
//      Frottement EN POSITION sur chaque contact (Macklin et al., « Unified Particle
//      Physics for Real-Time Applications », SIGGRAPH 2014, §6.1 éq. 23) : le
//      glissement tangentiel du sous-pas est annulé s'il est < mu d (statique), sinon
//      réduit de mu d (cinétique). Mesuré : en vitesse (v_t *= 1 - mu), la nappe
//      posée sur la sphère glissait et tombait à t = 4 s.
//   4. vitesses : v = (x - x_prev) / h ; amortissement v *= (1 - damping h).
//
// FLEXION : contrainte de DISTANCE entre sommets opposés (deux arêtes d'écart,
// « bend spring » de Provot, « Deformation Constraints in a Mass-Spring Model »,
// GI 1995), pas l'angle dièdre de Müller 2007 §4.3. Choix, dit : même
// projection que les autres contraintes (un seul code, une seule formule XPBD),
// pas de singularité à plat (le gradient de l'angle dièdre s'annule quand les
// deux triangles sont coplanaires : c'est précisément la position de repos
// d'une nappe), coût d'une arête. Ce qu'on perd : la flexion « vraie »
// (indépendante de l'étirement) sur un maillage triangulé quelconque — nommé,
// pour le jour où le tissu quitte la grille.
//
// STOCKAGE : colonnes SoA propres (pos, prev, vel, invMass, mass), pas
// `NkIParticleStore`. Mesuré : le stockage des particules est celui d'un
// ÉMETTEUR (naissances/morts, pile d'emplacements libres, tampon d'instances
// du quad, `NkIDevice`, `NkEmitterDesc`) et vit dans NKRenderer ; un tissu est
// un MAILLAGE à topologie fixe, sans naissance ni mort, dessiné comme un
// maillage, et NKPhysics ne voit pas NKRenderer. Le réutiliser aurait tiré le
// tissu dans le système d'effets ou le renderer dans la physique. Même forme
// (une colonne par attribut, pointeurs bruts pris une fois par pas — mesuré
// le 04/09 sur les particules : operator[] non inliné en Debug).
//
// Zéro STL. Aucune allocation par pas en régime établi.
// =============================================================================
#include "NKPhysics/NkPhysicsTypes.h"
#include "NKPhysics/NkSpatialHash.h"
#include "NKCollision/NkColShapes.h"
#include "NKMath/NkIForceField.h" // le contrat du vent, cherry-pick ebf6c348 (chantier Noge)
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace physics {

		class NkPhysicsWorld;

		struct NkClothParams {
				// Compliances XPBD, en m/N (0 = rigide). alpha~ = alpha / h².
				float32 compliance = 0.f;		// arêtes structurelles
				float32 shearCompliance = 0.f;	// diagonales (cisaillement)
				float32 bendCompliance = 0.01f; // sommets opposés (flexion, Provot)
				float32 damping = 1.f;			// 1/s : v *= max(0, 1 - damping h)
				float32 friction = 0.2f;		// coefficient mu (statique = cinétique) des contacts, en position
				float32 thickness = 0.005f;		// rayon d'une particule (m) : collision et auto-collision
				// 32 x 4 : le réglage qui tient les témoins (a) et (d) (table de l'en-tête) ; 128 passes
				// de contraintes par image. Les leviers pour moins cher sont nommés dans DECISIONS.
				uint32 substeps = 32;
				uint32 iterations = 4;
				NkVec3f gravity = {0.f, -9.81f, 0.f};
				bool selfCollision = false; // hachage spatial, paires à moins de 2 x thickness
				bool collisions = true;		// formes du monde (colliders)
				// Projection du champ de force sur la normale de la nappe (F_eff = n (n·F)) :
				// une voile ne prend le vent que de face. Faux = force brute par particule
				// (c'est ce que le témoin (e) mesure : atan(F / m g)).
				bool forceOnNormal = false;
				// MUTATION : faux = PBD pur, la compliance est IGNORÉE (dlambda = -C / (w_a + w_b)).
				// Le témoin (d) doit rougir. Jamais faux en production.
				bool xpbd = true;
		};

		// Ce que le dernier pas a mesuré — les témoins lisent ici.
		struct NkClothStats {
				uint32 particles = 0, pinned = 0;
				uint32 constraints = 0, structural = 0, shear = 0, bend = 0;
				float32 mass = 0.f;					// somme des masses (épinglées comprises)
				float32 maxStretch = 0.f;			// max |l - l0| / l0 sur les arêtes STRUCTURELLES
				float32 meanStretch = 0.f;			// moyenne de |l - l0| / l0 (structurelles)
				float32 maxSpeed = 0.f;				// m/s
				float32 kineticEnergy = 0.f;		// J
				float32 potentialEnergy = 0.f;		// J, m g·(-x) (référence : origine)
				float32 maxPenetration = 0.f;		// m : max(0, -(distance signée au collider)) des CENTRES
				float32 minSelfDistance = 0.f;		// m : plus petite distance entre particules non voisines (si auto-collision)
				uint32 contacts = 0, selfContacts = 0; // projections faites au dernier sous-pas
				uint32 collidersIgnored = 0;		// formes d'un type non traité (dites, pas simulées)
				uint32 substeps = 0, iterations = 0;
				float32 dt = 0.f;
		};

		class NkCloth {
			public:
				NkClothParams params;
				NkVector<collision::NkShape> colliders; // formes MONDE : NK_SPHERE, NK_CAPSULE3D, NK_PLANE3D, NK_BOX3D (alignée)
				const math::NkIForceField *forceField = nullptr; // vent : contrat NkIForceField, force en N par particule

				// ── Construction ─────────────────────────────────────────────
				void Clear();
				// Nappe nx x ny de particules ; particule (i, j) en origin + i du + j dv.
				// |du|, |dv| = espacement (les vecteurs portent la taille). Masse totale
				// répartie uniformément. Contraintes : structurelles (arêtes), cisaillement
				// (deux diagonales par quad), flexion (sommets à deux arêtes d'écart en i et
				// en j). Longueurs de repos = distances de cette construction.
				void BuildGrid(uint32 nx, uint32 ny, const NkVec3f &origin, const NkVec3f &du, const NkVec3f &dv,
							   float32 totalMass);
				uint32 AddParticle(const NkVec3f &p, float32 mass);
				enum Kind : uint8 { STRUCTURAL = 0, SHEAR = 1, BEND = 2 };
				// Longueur de repos = distance courante entre a et b.
				void AddDistance(uint32 a, uint32 b, Kind kind);
				void Pin(uint32 i, bool pinned = true);
				// Déplace une particule SANS toucher aux longueurs de repos (pli, pose).
				void SetPosition(uint32 i, const NkVec3f &p);
				// Ajoute les formes MONDE des corps de `world` dont la couche croise `layerMask`
				// (les os d'un ragdoll : passer son `group`). Types non traités comptés.
				void AddCollidersFromWorld(const NkPhysicsWorld &world, uint32 layerMask = 0xFFFFFFFFu);

				// ── Simulation ───────────────────────────────────────────────
				void Step(float32 dt, float32 time = 0.f);
				const NkClothStats &Stats() const noexcept {
					return mStats;
				}

				// ── Lecture ──────────────────────────────────────────────────
				uint32 ParticleCount() const noexcept {
					return (uint32)mPos.Size();
				}
				const NkVec3f *Positions() const noexcept {
					return mPos.Data();
				}
				const NkVec3f *Velocities() const noexcept {
					return mVel.Data();
				}
				const float32 *InvMasses() const noexcept {
					return mInvMass.Data();
				}
				uint32 GridWidth() const noexcept {
					return mGridW;
				}
				uint32 GridHeight() const noexcept {
					return mGridH;
				}
				uint32 GridIndex(uint32 i, uint32 j) const noexcept {
					return j * mGridW + i;
				}
				// Triangles de la nappe (deux par quad), pour le dessin ou les normales.
				void Triangles(NkVector<uint32> &outIndices) const;
				// Normales par particule (moyenne des triangles adjacents, unitaires).
				void ComputeNormals(NkVector<NkVec3f> &outNormals) const;

			private:
				void Predict(float32 h, float32 time);
				void SolveDistances(float32 h);
				void SolveColliders();
				void SolveSelf();
				void UpdateVelocities(float32 h);
				void Measure(float32 dt);
				bool Adjacent(uint32 a, uint32 b) const noexcept;

				// colonnes (SoA)
				NkVector<NkVec3f> mPos, mPrev, mVel, mNormal;
				NkVector<float32> mInvMass, mMass;
				NkVector<uint8> mContact;	// 1 si projeté contre un collider ce sous-pas
				NkVector<NkVec3f> mContactN; // normale du dernier contact
				// contraintes de distance
				NkVector<uint32> mCA, mCB;
				NkVector<float32> mRest, mLambda;
				NkVector<uint8> mKind;
				// adjacence (CSR) construite depuis les contraintes : exclut les voisines de l'auto-collision
				NkVector<uint32> mAdjStart, mAdjIdx;
				bool mAdjDirty = true;
				void BuildAdjacency();
				NkSpatialHash mHash;
				uint32 mGridW = 0, mGridH = 0;
				NkClothStats mStats;
		};

	} // namespace physics
} // namespace nkentseu
