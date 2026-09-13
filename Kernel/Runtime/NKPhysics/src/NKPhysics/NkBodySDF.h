#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkBodySDF.h — CHAMP DE DISTANCE SIGNÉ d'un corps skinné (2026-09-05, lot 2).
// C'est la vraie réponse à Rodolf : « sans que le tissu n'entre dans les mesh ».
// Les capsules tiennent le contact (0,000 mm mesuré) mais ne décrivent pas une
// section qui n'est pas un disque (mâchoire, aisselle, entrejambe, fesses) : le
// lot 1 a mesuré 9 à 37 particules sous la peau. Ici le tissu voit LE MAILLAGE.
//
// CONSTRUCTION, par image, en trois temps (Zhao, « A fast sweeping method for
// Eikonal equations », Math. Comp. 2005 ; bande étroite comme Bridson,
// « Fluid Simulation for Computer Graphics », §5) :
//   1. BANDE : chaque triangle est rastérisé dans les cellules de sa boîte
//      élargie de `band` cellules ; pour chacune, distance EXACTE point-triangle
//      (régions de Voronoï, Ericson §5.1.5) et on garde la plus petite ;
//   2. SIGNE, dans la bande : produit scalaire de (p - point le plus proche) par
//      la PSEUDONORMALE PONDÉRÉE PAR LES ANGLES du triangle le plus proche
//      (Bærentzen & Aanæs 2005 : c'est la seule normale qui donne le bon signe
//      quand le point le plus proche tombe sur une arête ou un sommet ; la
//      normale de face s'y trompe). ⚠️ Le signe par PARITÉ a été REFUTÉ le
//      2026-09-05 : sur XBot et YBot (Mixamo, coques ouvertes qui se recouvrent)
//      le contrôle positif ne reconnaissait que 47,4 % / 48,8 % des points
//      pourtant dedans par construction — contre 100 % sur CesiumMan.
//   3. PROPAGATION : balayage rapide (8 passes, les 8 sens de parcours) qui
//      étend |d| hors de la bande, en transportant le signe de la cellule
//      d'origine. Hors grille : la distance rendue est celle du bord (positive),
//      donc « dehors » — l'appelant garde ses capsules en secours.
//
// LECTURE : `Sample` interpole trilinéairement, `Gradient` dérive par différences
// centrées (la normale de contact), `Project` pousse un point à l'isosurface + une
// épaisseur. Coût mesuré : voir DECISIONS (bloc « champ de distance signé »).
//
// CE QUE ÇA NE FAIT PAS : le nombre d'enroulement généralisé (Jacobson, Kavan,
// Sorkine-Hornung, « Robust Inside-Outside Segmentation using Generalized Winding
// Numbers », SIGGRAPH 2013) donnerait un signe juste même sur un maillage
// franchement non étanche, au prix d'une somme sur TOUS les triangles par cellule.
// Nommé, non fait : la pseudonormale est mesurée d'abord, et c'est le contrôle
// positif de `Calibrate` qui dit si elle suffit sur un corps donné.
//
// Zéro STL.
// =============================================================================
#include "NKPhysics/NkPhysicsTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace physics {

		// D'OÙ VIENT LE SIGNE. La pseudonormale suppose une surface fermée et orientée ; mesuré le
		// 2026-09-05, elle rend 100 % sur CesiumMan mais **24,5 %** sur XBot et le centre du corps
		// « dehors » -- ces corps Mixamo sont deux coques ouvertes imbriquées. Le NOMBRE
		// D'ENROULEMENT GÉNÉRALISÉ (Jacobson, Kavan, Sorkine-Hornung, « Robust Inside-Outside
		// Segmentation using Generalized Winding Numbers », SIGGRAPH 2013) ne suppose RIEN : il somme
		// l'angle solide signé de chaque triangle vu du point (Van Oosterom & Strackee 1983), et un
		// point à l'intérieur de la coque externe rend ~1 même si une coque interne traverse.
		// Son prix : O(triangles) PAR POINT. D'où la grille de signe SÉPARÉE et grossière
		// (`signResolution`) -- le signe est une propriété topologique, il varie lentement ; la
		// distance, elle, garde la finesse de la grille principale.
		enum class NkSDFSign : uint8 {
			NK_PSEUDONORMAL = 0, // rapide, exige une surface fermée
			// ⚠️ 🔴 MESURÉ LE 2026-09-05 : CETTE IMPLÉMENTATION NE MARCHE PAS ENCORE — ne pas l'activer
			// sans refaire la mesure. Sur CesiumMan (corps fermé, où la pseudonormale rend 100 %),
			// le contrôle positif tombe à 60,6 % (grille de signe 12), 49,6 % (16) et 31,5 % (24) :
			// il EMPIRE quand la résolution monte, ce qui désigne une erreur systématique et non une
			// limite d'échantillonnage. L'orientation a été écartée (le même critère sur |w| ne change
			// rien : 49,6 %). Sur XBot, 18,6 % et le centre du corps toujours « dehors ». Coût mesuré :
			// 0,4 s (CesiumMan, 4 672 triangles) à 4,1 s (XBot, 49 112) par construction, à 16³ de
			// grille de signe -- O(résolution³ x triangles), sans la hiérarchie de Barnes-Hut que
			// Jacobson décrit (approximation dipolaire par grappes), qui est le vrai lot.
			NK_WINDING = 1,		 // Jacobson 2013 — ÉCRIT, MESURÉ, ROUGE : voir ci-dessus
		};

		// ── Primitives partagées (le champ ET la requête par particule les utilisent) ────────
		// Point le plus proche d'un triangle et RÉGION touchée (Ericson, « Real-Time Collision
		// Detection », §5.1.5) : 0 = face, 1..3 = arêtes ab / bc / ca, 4..6 = sommets a / b / c.
		NkVec3f NkClosestOnTriangle(const NkVec3f &p, const NkVec3f &a, const NkVec3f &b, const NkVec3f &c,
									uint32 &outRegion) noexcept;
		// Poids de la pseudonormale selon la région (Bærentzen & Aanæs 2005) : une face décide seule,
		// une arête se partage entre deux faces, un sommet pèse son angle.
		float32 NkTriangleCornerWeight(const NkVec3f &a, const NkVec3f &b, const NkVec3f &c, uint32 region) noexcept;

		struct NkBodySDFParams {
				NkSDFSign sign = NkSDFSign::NK_PSEUDONORMAL;
				uint32 signResolution = 16; // cellules sur le plus grand côté, pour la grille de SIGNE
				float32 windingThreshold = 0.5f; // w > seuil = dedans (Jacobson : 0,5 sépare les deux)
				uint32 resolution = 64; // cellules sur le PLUS GRAND côté de la boîte (si targetCellSize = 0)
				// CE QUI COMPTE POUR LE TISSU, c'est la taille de cellule -- pas le nombre de cellules :
				// une cellule de 26 mm fait « respirer » l'isosurface d'une image à l'autre et étire un
				// tissu de 6 mm. Quand `targetCellSize` > 0, c'est ELLE qui fixe la grille, et
				// `maxCells` borne la dépense (au-delà, la cellule est agrandie jusqu'à tenir).
				float32 targetCellSize = 0.f;
				uint32 maxCells = 250000u;
				float32 margin = 0.08f; // m ajoutés autour du corps (le tissu vit dehors)
				uint32 band = 2;		// cellules de part et d'autre de chaque triangle (bande exacte)
				bool sweep = true;		// propager hors de la bande (faux = seule la bande est juste)
				// BOÎTE IMPOSÉE : quand elle est donnée, la grille couvre CETTE boîte (plus la marge)
				// au lieu du corps entier, et les triangles qui ne la croisent pas ne sont même pas
				// rastérisés. C'est le levier des deux problèmes à la fois -- un vêtement n'a besoin
				// que de sa zone : le foulard du cou, la jupe du bassin et des cuisses. À nombre de
				// cellules égal, le volume plus petit donne une cellule BIEN plus fine (mesuré au lot
				// précédent : 26 mm sur le corps entier faisait « respirer » l'isosurface d'une image
				// à l'autre et étirait le tissu), et la bande coûte moins puisqu'elle voit moins de
				// triangles.
				bool useBounds = false;
				NkVec3f boundsMin{}, boundsMax{};
		};

		// Ce que la construction a mesuré (les témoins lisent ici).
		struct NkBodySDFStats {
				uint32 nx = 0, ny = 0, nz = 0, cells = 0;
				uint32 bandCells = 0; // cellules touchées par la bande exacte
				float32 cellSize = 0.f;
				float32 minValue = 0.f, maxValue = 0.f;
				uint32 negativeCells = 0; // cellules dites dedans
				uint32 skippedTriangles = 0; // triangles hors de la grille, jamais rastérisés (boîte imposée)
		};

		class NkBodySDF {
			public:
				NkBodySDFParams params;

				// Reconstruit le champ pour cette pose. `verts` MONDE, `indices` = 3 par triangle.
				bool Build(const NkVec3f *verts, uint32 vertCount, const uint32 *indices, uint32 triCount);
				bool Valid() const noexcept {
					return mStats.cells > 0;
				}
				const NkBodySDFStats &Stats() const noexcept {
					return mStats;
				}

				// Distance signée en p (négatif = dedans). Hors grille : distance au bord, positive.
				float32 Sample(const NkVec3f &p) const noexcept;
				// Gradient unitaire (la normale sortante) ; {0,1,0} si le champ est plat.
				NkVec3f Gradient(const NkVec3f &p) const noexcept;
				bool Inside(const NkVec3f &p) const noexcept {
					return Sample(p) < 0.f;
				}
				// Si p est à moins de `offset` de la surface, le pousse à `offset` le long du
				// gradient et rend vrai (avec la normale utilisée).
				bool Project(NkVec3f &p, float32 offset, NkVec3f *outNormal = nullptr) const noexcept;
				// La boîte du champ (l'appelant élague ses requêtes avec).
				const NkVec3f &Min() const noexcept {
					return mMin;
				}
				const NkVec3f &Max() const noexcept {
					return mMax;
				}

				// ── CALIBRATION : un instrument neuf se vérifie AVANT de servir à juger ────
				// Trois épreuves sur le corps qui vient d'être construit, rendues en pourcentage
				// de reconnaissance ; `outNeg` = points lointains dits dedans (0 attendu).
				//   positif A : le centre de chaque triangle rentré de `depth` sous sa face ;
				//   positif B : le barycentre du corps (dedans par construction sur un humanoïde) ;
				//   négatif  : des points à 3 x la taille du corps.
				// Rend le taux du positif A (0..1). Un champ dont ce taux n'est pas ~1 ne juge rien.
				float32 Calibrate(const NkVec3f *verts, const uint32 *indices, uint32 triCount, float32 depth,
								  uint32 *outPositiveA = nullptr, uint32 *outPositiveTotal = nullptr,
								  uint32 *outNeg = nullptr, uint32 *outNegTotal = nullptr, bool *outCenterInside = nullptr) const;

				// MUTATION (témoin) : inverse le signe du champ. Tout ce qui juge doit rougir.
				void MutateFlipSign();
				// Nombre d'enroulement généralisé en p (Jacobson 2013) : ~1 dedans, ~0 dehors.
				// O(triangleCount) -- instrument, pas fonction par image.
				static float32 WindingNumber(const NkVec3f &p, const NkVec3f *verts, const uint32 *indices,
											 uint32 triCount) noexcept;
				// Coût de la dernière grille de signe (ms), 0 si mode pseudonormale.
				float32 SignGridMs() const noexcept {
					return mSignMs;
				}

			private:
				NkVector<float32> mD;	  // distance signée par cellule
				NkVector<uint8> mKnown;	  // 1 = cellule de la bande (valeur exacte)
				NkVec3f mMin{}, mMax{};
				float32 mCell = 0.f, mInvCell = 0.f;
				uint32 mNX = 0, mNY = 0, mNZ = 0;
				NkBodySDFStats mStats;
				float32 mSignMs = 0.f;
				uint32 Index(uint32 i, uint32 j, uint32 k) const noexcept {
					return (k * mNY + j) * mNX + i;
				}
				float32 At(int32 i, int32 j, int32 k) const noexcept;
		};

		// ── LA DISTANCE EXACTE, PAR PARTICULE, SANS GRILLE DE CHAMP ──────────────────────────
		// Mesuré le 2026-09-05 : le poste dominant du champ est la RASTÉRISATION de sa bande (chaque
		// triangle visite les cellules de sa boîte élargie), et ni la résolution ni la boîte du
		// vêtement ne l'ont fait bouger -- 22 à 29 ms par image. Et une grille pleine ne peut pas être
		// fine autour d'un objet creux : la cape gardait une cellule de 15 mm là où son tissu fait
		// 6 mm d'épaisseur. Ici il n'y a plus de champ du tout : les TRIANGLES sont rangés une fois
		// dans une grille uniforme (leur taille, pas celle du tissu), et chaque particule demande la
		// distance EXACTE au plus proche -- aucune interpolation, aucune cellule, aucune isosurface
		// qui « respire » d'une image à l'autre.
		//
		// Ce qu'on perd par rapport au champ : la distance loin de la surface (la requête a un rayon
		// maximal) et le gradient lissé. Ce qu'on gagne : l'exactitude là où le tissu est, et le coût
		// qui suit le nombre de PARTICULES au lieu du nombre de triangles x cellules.
		class NkBodyProximity {
			public:
				// `cellSize` <= 0 : déduite de la taille moyenne d'un triangle (le bon choix par défaut).
				bool Build(const NkVec3f *verts, uint32 vertCount, const uint32 *indices, uint32 triCount,
						   float32 cellSize = 0.f);
				bool Valid() const noexcept {
					return mTriCount > 0;
				}
				// Distance SIGNÉE au corps et normale, si un triangle est à moins de `maxDist`.
				// Faux si rien n'est assez proche (l'appelant garde alors ses capsules).
				bool Query(const NkVec3f &p, float32 maxDist, float32 &outDist, NkVec3f &outNormal) const noexcept;
				uint32 TriangleCount() const noexcept {
					return mTriCount;
				}
				float32 CellSize() const noexcept {
					return mCell;
				}
				uint32 CellCount() const noexcept {
					return mNX * mNY * mNZ;
				}
				// Triangles rangés (somme des insertions) : le coût de la construction se lit dessus.
				uint32 InsertionCount() const noexcept {
					return (uint32)mCellTri.Size();
				}
				// Boîte du corps : l'appelant rejette d'un test ce qui est loin (une cape pend
				// derrière : la plupart de ses particules ne sont jamais près du corps).
				const NkVec3f &Min() const noexcept {
					return mMin;
				}
				const NkVec3f &Max() const noexcept {
					return mMax;
				}
				// Requêtes servies depuis la dernière construction (le coût se lit dessus).
				uint32 QueryCount() const noexcept {
					return mQueries;
				}
				uint32 TriangleTests() const noexcept {
					return mTriTests;
				}

			private:
				const NkVec3f *mVerts = nullptr;
				const uint32 *mIdx = nullptr;
				uint32 mTriCount = 0;
				NkVec3f mMin{}, mMax{};
				float32 mCell = 0.f, mInvCell = 0.f;
				uint32 mNX = 0, mNY = 0, mNZ = 0;
				NkVector<uint32> mCellStart, mCellTri;
				mutable uint32 mQueries = 0, mTriTests = 0;
		};

	} // namespace physics
} // namespace nkentseu
