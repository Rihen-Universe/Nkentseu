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
//                 32 x 4       |      0,43 %       |     0,26 %
//      LOT BUDGET (05/09, 04h — Rodolf : « est-ce que ça va supporter du temps réel ? ») :
//      la table ci-dessus a été mesurée avec bendCompliance = 0,01 m/N, soit alpha~ = 576
//      contre 2w = 10 240 -- une flexion quasi RIGIDE (une plaque, pas un tissu), et
//      c'est elle qui exigeait 32 sous-pas. À bendCompliance = 1 m/N (alpha~ = 57 600,
//      la flexion cède) la même mesure donne, écart (d) dt / dt/2 :
//        sous-pas x it | bend 0,01 | bend 1,0 || (a) max, bend 1,0
//             8 x 1    |   52 %    |   19 %   ||   21 %
//            16 x 1    |   27 %    |   7,0 %  ||   6,4 %
//            32 x 1    |   11 %    |   2,9 %  ||   1,7 %
//             8 x 2    |   40 %    |   7,4 %  ||   12 %
//            16 x 2    |   9,6 %   |   0,64 % ||   3,4 %     <- DÉFAUT (32 passes)
//            32 x 2    |   1,3 %   |   0,27 % ||   0,88 %
//      Défaut 16 x 2 : (b) (c) (d) (e) (g) verts. La scène (a) (deux coins épinglés à
//      exactement la largeur : la rangée haute est une corde tendue à sa longueur,
//      tension infinie sans sag) reste la scène singulière et garde 32 x 2 dans son
//      témoin, dit là-bas.
//      Le DÉMARRAGE À CHAUD de lambda (appliqué une fois avant d'itérer, comme le kappa
//      du DFSPH) a été essayé et RETIRÉ : échelle 1,0 et 0,9 -> NaN en moins d'une
//      seconde, 0,7 -> explosion (vmax 52 m/s), 0,5 -> stable mais pas plus convergé
//      (0,24 % / 0,14 %). Un lambda mémorisé se rejoue par la vitesse (la correction de
//      position du sous-pas précédent est déjà dans v) : il compte deux fois.
//   3. collisions avec les formes du monde (sphère, capsule, plan, boîte alignée)
//      par projection hors de la surface à `thickness` (Müller 2007 §4.4, contrainte
//      d'inégalité) ; auto-collision par hachage spatial (NkSpatialHash, Teschner
//      2003), paires non voisines à moins de 2 x thickness, projetées à parts de masse.
//      La LISTE DES PAIRES candidates (rayon de recherche = contact 2r + MARGE) est
//      relue à chaque itération de chaque sous-pas et RECONSTRUITE seulement quand
//      elle peut être devenue fausse -- même schéma que les listes de voisines du SPH.
//      Le critère est EXACT (une borne, pas une estimation) : à chaque sous-pas, le
//      déplacement d_i de chaque particule depuis la dernière liste est comparé au
//      déplacement MOYEN de la nappe ; deux particules ne peuvent s'être rapprochées
//      de plus de 2 max_i |d_i - d_moyen|, donc la liste est valable tant que cette
//      quantité reste sous la marge. En chute libre tout bouge ensemble : zéro
//      reconstruction ; à l'impact, quelques-unes. Marge = max(2r, selfMarginK x vmax x
//      dt), vmax du pas précédent. Mesuré le 05/09 (Release, 32 x 4) : la traversée des
//      27 cellules à CHAQUE itération coûtait 50 ms sur 61 à 32 x 32 (solveur seul :
//      11-16 ms) ; une liste par PAS avec marge 2 vmax dt coûtait 0,9-3,4 s à 128 x 128
//      (rayon 84 mm pour 7,9 mm d'espacement : 350 voisines) ; une marge fixe 2r avec le
//      critère « dérive 2 vmax h » (estimé, pas mesuré) se reconstruisait 11 fois par
//      image à 256 x 256 (r = 3,8 mm) contre 1 à 32 x 32 -- O(v / r) en nombre.
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
// COLLIDERS EN MOUVEMENT (2026-09-05, vêtements sur mannequin) : l'appelant écrit dans
// `colliders` la pose de FIN de pas (les capsules du squelette animé à t + dt) ; le
// tissu garde dans `collidersPrev` la pose de début (copiée en fin de pas). Quand les
// deux listes ont la même taille et `params.colliderMotion`, chaque sous-pas s voit
// la forme INTERPOLÉE à (s + 1) / sous-pas (p0, p1, rayon), et le frottement lit la
// VITESSE du collider : le glissement tangentiel du sous-pas est mesuré RELATIVEMENT
// au point de contact du collider ((x - x_prev) - v_c h), sinon le frottement
// statique annulerait aussi le mouvement que le corps impose au tissu, et un bras qui
// bouge laisserait la manche derrière lui (témoin (h5) de test_garment.cpp,
// contre-épreuve colliderMotion = false). L'anti-tunnel vient de l'interpolation :
// une capsule qui parcourt 33 mm par image (2 m/s à 60 Hz) n'en parcourt que 2 par
// sous-pas, sous l'épaisseur + rayon ; sans interpolation elle saute par-dessus une
// nappe fine. Pas de balayage continu (CCD) : nommé, non fait.
//
// ÉPINGLES À CIBLE : une particule épinglée (masse inverse 0) peut recevoir une CIBLE
// par pas (SetPinTarget) -- la position d'un os animé. Elle y va LINÉAIREMENT sur les
// sous-pas (pas un saut au premier), et sa vitesse (cible - départ) / dt est vue par
// l'auto-collision et le frottement. C'est ce qui attache un vêtement à un squelette.
//
// Zéro STL. Aucune allocation par pas en régime établi.
// =============================================================================
#include "NKPhysics/NkPhysicsTypes.h"
#include "NKPhysics/NkSpatialHash.h"
#include "NKPhysics/NkBodySDF.h" // le corps vu comme un champ de distance (2026-09-05, lot 2)
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
				float32 bendCompliance = 1.f;	// sommets opposés (flexion, Provot) ; 1 m/N : la flexion CÈDE (table ci-dessus)
				float32 damping = 1.f;			// 1/s : v *= max(0, 1 - damping h)
				float32 friction = 0.2f;		// coefficient mu (statique = cinétique) des contacts, en position
				float32 thickness = 0.005f;		// rayon d'une particule (m) : collision et auto-collision
				// 16 x 2 : le budget mesuré avec la flexion à 1 m/N (table de l'en-tête) ; 32 passes par image.
				uint32 substeps = 16;
				uint32 iterations = 2;
				NkVec3f gravity = {0.f, -9.81f, 0.f};
				bool selfCollision = false; // hachage spatial, paires à moins de 2 x thickness
				// Résolution des paires à CHAQUE itération (vrai) ou une fois par sous-pas, après les
				// itérations (faux) : mesuré dans le lot budget, dit dans DECISIONS.
				bool selfEveryIteration = true;
				// HORLOGE fournie par l'appelant (secondes, monotone) : quand elle est là, chaque pas
				// remplit NkClothProfile. NKPhysics n'a pas d'horloge (pas de NKTime) : l'appelant
				// (la sonde de la démo) prête NkChrono. Nul = pas de profil, zéro coût.
				float64 (*clock)() = nullptr;
				// Marge de la liste de paires = max(2 x thickness, selfMarginK x vmax x dt) (en-tête).
				// Mesuré le 05/09 à 256 x 256 (table dans DECISIONS) ; 0 = marge 2r seule.
				float32 selfMarginK = 0.15f;
				bool collisions = true;		// formes du monde (colliders)
				// Colliders en mouvement (en-tête) : interpolation par sous-pas entre `collidersPrev` et
				// `colliders`, vitesse du collider dans le frottement. Faux = la forme de fin de pas est
				// appliquée dès le premier sous-pas et le frottement ignore son mouvement (contre-épreuve).
				bool colliderMotion = true;
				// Élagage : un collider dont la boîte (gonflée de rayon + épaisseur) ne croise pas celle
				// du tissu est sauté pour tout le sous-pas (un foulard ne teste pas les pieds). Compté.
				bool colliderCulling = true;
				// CHAMP DE DISTANCE du corps (NkCloth::bodySDF) : quand il est là, chaque particule est
				// projetée à `thickness` de l'ISOSURFACE, avec la normale = gradient. C'est ce qui fait
				// entrer les creux qu'une capsule ne décrit pas (mâchoire, aisselle, entrejambe) --
				// mesuré au lot 1 : capsules seules = 0,000 mm de pénétration MAIS 9 à 37 particules
				// sous la peau. Les capsules restent en SECOURS (hors grille, et pour la vitesse du
				// corps dans le frottement) ; Stats dit qui a résolu quoi.
				bool sdfCollision = true;
				// ZONE DE TRANSITION autour des ÉPINGLES. Une épingle est une contrainte DURE : sa
				// position est imposée par l'os, et le champ ne la pousse pas (masse inverse nulle).
				// Sa VOISINE, elle, est poussée à `thickness` de la peau -- et l'arête entre les deux
				// paie tout l'écart. Mesuré le 05/09 : la cape passait de 8,7 % d'étirement (capsules
				// seules) à 143 % dès que le champ poussait, alors qu'aucune particule ne traversait.
				// Ici le champ est PONDÉRÉ par la distance topologique à l'épingle la plus proche :
				// 0 sur l'épingle, 1 au-delà de `sdfPinBlendRings` anneaux d'arêtes.
				uint32 sdfPinBlendRings = 3;
				// Projection du champ de force sur la normale de la nappe (F_eff = n (n·F)) :
				// une voile ne prend le vent que de face. Faux = force brute par particule
				// (c'est ce que le témoin (e) mesure : atan(F / m g)).
				bool forceOnNormal = false;
				// MUTATION : faux = PBD pur, la compliance est IGNORÉE (dlambda = -C / (w_a + w_b)).
				// Le témoin (d) doit rougir. Jamais faux en production.
				bool xpbd = true;
		};

		// Profil du dernier pas, par phase (ms), si params.clock est fourni.
		struct NkClothProfile {
				float64 predict = 0, structural = 0, shear = 0, bend = 0, colliders = 0;
				float64 selfBuild = 0, selfSolve = 0, velocities = 0, measure = 0, total = 0;
		};

		// Ce que le dernier pas a mesuré — les témoins lisent ici.
		struct NkClothStats {
				uint32 particles = 0, pinned = 0;
				uint32 constraints = 0, structural = 0, shear = 0, bend = 0;
				float32 mass = 0.f;					// somme des masses (épinglées comprises)
				float32 maxStretch = 0.f;			// max |l - l0| / l0 sur les arêtes STRUCTURELLES
				uint32 maxStretchEdge = 0;			// indice de cette contrainte (Constraint())
				uint32 degenerateEdges = 0;			// structurelles de repos < épaisseur : hors statistique, comptées (géométrie effondrée)
				float32 meanStretch = 0.f;			// moyenne de |l - l0| / l0 (structurelles)
				float32 maxSpeed = 0.f;				// m/s
				float32 kineticEnergy = 0.f;		// J
				float32 potentialEnergy = 0.f;		// J, m g·(-x) (référence : origine)
				float32 maxPenetration = 0.f;		// m : max(0, -(distance signée au collider)) des CENTRES
				float32 minSelfDistance = 0.f;		// m : plus petite distance entre particules non voisines de la liste de paires (plancher = rayon de recherche)
				uint32 contacts = 0, selfContacts = 0; // projections faites au dernier sous-pas
				uint32 selfPairs = 0, selfBuilds = 0;  // paires candidates ; constructions de la liste dans le pas
				uint32 collidersIgnored = 0;		// formes d'un type non traité (dites, pas simulées)
				uint32 collidersCulled = 0;			// formes sautées par l'élagage au dernier sous-pas
				uint32 sdfContacts = 0;				// projections faites par le champ de distance au dernier sous-pas
				// m : max(0, thickness - distance signée) après le pas, sur les particules LIBRES
				// seulement. ⚠️ Une particule ÉPINGLÉE suit un os : elle est sous la peau par
				// construction (une épaule, une taille, un cou), et la compter faisait lire comme un
				// défaut du solveur ce qui est la définition d'une épingle -- mesuré le 05/09 : les
				// « 9 à 13 particules de foulard en permanence sous la peau » étaient ses 29 épingles.
				float32 maxSdfPenetration = 0.f;
				float32 maxPinError = 0.f;			// m : max |x - cible| des particules épinglées à cible (0 attendu)
				uint32 pinTargets = 0;				// particules épinglées qui ont une cible
				uint32 substeps = 0, iterations = 0;
				float32 dt = 0.f;
		};

		class NkCloth {
			public:
				NkClothParams params;
				NkVector<collision::NkShape> colliders; // formes MONDE : NK_SPHERE, NK_CAPSULE3D, NK_PLANE3D, NK_BOX3D (alignée) -- pose de FIN de pas
				// Pose de DÉBUT de pas des mêmes formes (même ordre, même taille), copiée depuis `colliders`
				// à la fin de chaque Step. Vide ou de taille différente = colliders immobiles sur le pas.
				NkVector<collision::NkShape> collidersPrev;
				const math::NkIForceField *forceField = nullptr; // vent : contrat NkIForceField, force en N par particule
				// Champ de distance du corps à la pose de FIN de pas (l'appelant le reconstruit).
				// Nul = collisions par les capsules seules, comme avant.
				const NkBodySDF *bodySDF = nullptr;
				// Champ à la pose de DÉBUT de pas. Quand les deux sont là, chaque sous-pas lit la
				// distance et le gradient INTERPOLÉS à sa fraction -- exactement ce que les capsules
				// font depuis le lot 1. Mesuré au lot 2 : avec un seul champ, figé pendant les 32
				// sous-pas pendant que le corps avance, il restait 1,7 mm (cape), 6,7 mm (foulard) et
				// 26,7 mm (jupe) de pénétration résiduelle -- ce n'était pas une affaire de résolution.
				const NkBodySDF *bodySDFPrev = nullptr;

				// ── Construction ─────────────────────────────────────────────
				void Clear();
				// Nappe nx x ny de particules ; particule (i, j) en origin + i du + j dv.
				// |du|, |dv| = espacement (les vecteurs portent la taille). Masse totale
				// répartie uniformément. Contraintes : structurelles (arêtes), cisaillement
				// (deux diagonales par quad), flexion (sommets à deux arêtes d'écart en i et
				// en j). Longueurs de repos = distances de cette construction.
				void BuildGrid(uint32 nx, uint32 ny, const NkVec3f &origin, const NkVec3f &du, const NkVec3f &dv,
							   float32 totalMass);
				// PANNEAU ajouté à un tissu existant (vêtements : plusieurs pièces dans UN tissu, cousues
				// par AddDistance entre leurs bords). Même construction que BuildGrid (structurelles,
				// cisaillement, flexion) mais sans Clear ; `wrapU` referme la grille en i (cylindre : jupe,
				// manche, tube). Les triangles du panneau sont ajoutés à la liste explicite (SetTriangles),
				// donc Triangles() / ComputeNormals() les voient. Rend l'indice de la première particule ;
				// la particule (i, j) du panneau est first + j * nx + i.
				uint32 AppendGrid(uint32 nx, uint32 ny, const NkVec3f &origin, const NkVec3f &du, const NkVec3f &dv,
								  float32 totalMass, bool wrapU = false);
				// Panneau dont chaque particule a SA position (anneau de rayon variable, plis) :
				// particule (i, j) = rows[j * nx + i]. Contraintes et triangles comme AppendGrid.
				uint32 AppendPanel(uint32 nx, uint32 ny, const NkVec3f *rows, float32 totalMass, bool wrapU = false);
				// Panneau À TROUS : mask[j * nx + i] = 0 -> pas de particule (emmanchure, encolure) ;
				// une contrainte ou un triangle n'existe que si toutes ses particules existent.
				// `indexOf` (optionnel, nx * ny entrées) reçoit l'indice de chaque case ou -1.
				// La masse totale est celle des particules PRÉSENTES.
				uint32 AppendPanelMasked(uint32 nx, uint32 ny, const NkVec3f *rows, const uint8 *mask, float32 totalMass,
										 bool wrapU, NkVector<int32> *indexOf = nullptr);
				uint32 AddParticle(const NkVec3f &p, float32 mass);
				enum Kind : uint8 { STRUCTURAL = 0, SHEAR = 1, BEND = 2 };
				// Longueur de repos = distance courante entre a et b.
				void AddDistance(uint32 a, uint32 b, Kind kind);
				void Pin(uint32 i, bool pinned = true);
				// Déplace une particule SANS toucher aux longueurs de repos (pli, pose).
				void SetPosition(uint32 i, const NkVec3f &p);
				// CIBLE d'une particule épinglée pour le prochain pas (en-tête : elle y va linéairement
				// sur les sous-pas, avec la vitesse correspondante). Épingle la particule si besoin.
				void SetPinTarget(uint32 i, const NkVec3f &target);
				void ClearPinTarget(uint32 i);
				// Triangles EXPLICITES (trois indices par triangle) : quand la liste est non vide,
				// Triangles() et ComputeNormals() l'utilisent à la place de la grille.
				void SetTriangles(const uint32 *indices, uint32 count);
				void AddTriangle(uint32 a, uint32 b, uint32 c);
				// Ajoute les formes MONDE des corps de `world` dont la couche croise `layerMask`
				// (les os d'un ragdoll : passer son `group`). Types non traités comptés.
				void AddCollidersFromWorld(const NkPhysicsWorld &world, uint32 layerMask = 0xFFFFFFFFu);

				// ── Simulation ───────────────────────────────────────────────
				void Step(float32 dt, float32 time = 0.f);
				const NkClothStats &Stats() const noexcept {
					return mStats;
				}
				const NkClothProfile &Profile() const noexcept {
					return mProfile;
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
				const float32 *Masses() const noexcept {
					return mMass.Data();
				}
				// Boîte englobante des particules (positions courantes) ; faux si vide.
				bool Bounds(NkVec3f &outMin, NkVec3f &outMax) const noexcept;
				const float32 *InvMasses() const noexcept {
					return mInvMass.Data();
				}
				uint32 ConstraintCount() const noexcept {
					return (uint32)mCA.Size();
				}
				// Lecture d'une contrainte : extrémités et longueur de repos (faux si hors bornes).
				bool Constraint(uint32 c, uint32 &a, uint32 &b, float32 &rest) const noexcept {
					if (c >= (uint32)mCA.Size())
						return false;
					a = mCA[c];
					b = mCB[c];
					rest = mRest[c];
					return true;
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
				// alpha = fraction du pas atteinte à la fin de ce sous-pas (épingles à cible)
				void Predict(float32 h, float32 time, float32 alpha, float32 invDt);
				void SolveDistances(float32 h, uint32 c0, uint32 c1); // contraintes [c0, c1)
				// Formes du sous-pas (interpolées) et vitesse de leurs deux points p0 / p1 (m/s) ; h pour
				// convertir la vitesse en déplacement du sous-pas dans le frottement.
				void SolveColliders(const collision::NkShape *S, uint32 nk, const NkVec3f *V0, const NkVec3f *V1,
									float32 h, float32 alpha);
				// distance et normale du corps à la fraction `alpha` du pas (interpolation des deux champs)
				bool SampleBody(const NkVec3f &p, float32 alpha, float32 &outDist, NkVec3f &outNormal) const;
				void PrepareColliderStep(float32 alpha, float32 invDt); // remplit mColStep / mColV0 / mColV1 / mColSkip
				void SolveSelf();
				void UpdateVelocities(float32 h);
				void Measure(float32 dt);
				bool Adjacent(uint32 a, uint32 b) const noexcept;

				// colonnes (SoA)
				NkVector<NkVec3f> mPos, mPrev, mVel, mNormal;
				NkVector<float32> mInvMass, mMass;
				NkVector<uint8> mContact;	// 1 si projeté contre un collider ce sous-pas
				NkVector<NkVec3f> mContactN; // normale du dernier contact
				// épingles à cible (en-tête) : cible du pas, position de départ du pas, 1 si une cible est posée
				NkVector<NkVec3f> mPinTarget, mPinStart;
				NkVector<uint8> mPinHas;
				// distance topologique (en anneaux d'arêtes) à l'épingle la plus proche, 255 = loin
				NkVector<uint8> mPinRing;
				bool mPinRingDirty = true;
				void BuildPinRings();
				// triangles explicites (SetTriangles / AppendGrid)
				NkVector<uint32> mTri;
				// colliders du sous-pas : formes interpolées, vitesses de p0 / p1, élagage
				NkVector<collision::NkShape> mColStep;
				NkVector<NkVec3f> mColV0, mColV1;
				NkVector<uint8> mColSkip;
				// contraintes de distance
				NkVector<uint32> mCA, mCB;
				NkVector<float32> mRest, mLambda;
				NkVector<uint8> mKind;
				// adjacence (CSR) construite depuis les contraintes : exclut les voisines de l'auto-collision
				NkVector<uint32> mAdjStart, mAdjIdx;
				bool mAdjDirty = true;
				void BuildAdjacency();
				NkSpatialHash mHash;
				// paires candidates à l'auto-collision, reconstruites quand la dérive dépasse la marge (en-tête)
				NkVector<uint32> mPairA, mPairB;
				NkVector<NkVec3f> mPairBase; // positions au moment de la dernière liste
				float32 mPairRadius = 0.f, mPairMargin = 0.f;
				void BuildSelfPairs(float32 margin);
				bool SelfPairsStale() const; // 2 max |d_i - d_moyen| >= marge
				uint32 mGridW = 0, mGridH = 0;
				NkClothStats mStats;
				NkClothProfile mProfile;
				uint32 mKindStart[4] = {0, 0, 0, 0}; // bornes des contraintes par type (groupées : BuildGrid), pour le profil
				bool mKindsGrouped = false;
		};

	} // namespace physics
} // namespace nkentseu
