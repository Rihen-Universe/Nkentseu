#pragma once
// =============================================================================
// NkNavMesh.h — NavMesh triangulé (génération + requêtes) + pathfinding A*.
// =============================================================================
// Génération (Build) : échantillonne une grille régulière sur `areaMin..areaMax`
// (XZ) par pas de `cellSize`, sonde la géométrie de sol source (soupe de
// triangles non-ownante) par un RAYCAST VERTICAL RÉEL
// (collision::NkRayTriangle3D, NKCollision — zéro réimplémentation locale
// d'intersection rayon/triangle), rejette les cellules non walkable (pente >
// `maxSlopeDeg`, ou aucun point d'impact = trou/hors géométrie), rejette les
// triangles recouverts par un obstacle (empreinte AABB dilatée du rayon de
// l'agent), puis construit le graphe d'adjacence (triangles voisins = arête
// partagée) via une table de hachage d'arêtes — général, valable pour
// n'importe quelle triangulation (pas seulement la grille régulière).
//
// Pathfinding (FindPath) : A* sur le graphe de triangles (coût = distance
// euclidienne centroïde à centroïde, heuristique = distance au centroïde du
// triangle d'arrivée). Les waypoints renvoyés sont : point de départ exact,
// puis le MILIEU de chaque arête traversée (portail entre deux triangles
// successifs), puis point d'arrivée exact — PAS de lissage "funnel"
// (string-pulling) : le chemin est topologiquement correct et évite les
// obstacles, mais peut zigzaguer légèrement au lieu de couper au plus court
// à l'intérieur d'un couloir large. Documenté honnêtement comme limitation
// v1 (cf. Engine/Noge/ROADMAP.md, pilier Navigation IA).
// =============================================================================
#include "NKNavigation/NkNavTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nav {

		// Description d'une génération de NavMesh.
		struct NkNavMeshBuildDesc {
				// Zone couverte (coin min/max, XZ = étendue, Y = plage de sondage).
				NkVec3f areaMin{-10.f, -1.f, -10.f};
				NkVec3f areaMax{10.f, 1.f, 10.f};
				float32 cellSize = 1.f;

				// Géométrie de sol source (soupe de triangles), NON-ownante — durée
				// de vie gérée par l'appelant (même convention que collision::NkShape).
				const NkVec3f *groundVerts = nullptr;
				uint32 groundVertCount = 0;
				const uint32 *groundIndices = nullptr; // triplets (3 par triangle)
				uint32 groundIndexCount = 0;

				// Filtre de pente (degrés) : normale du triangle candidat vs +Y.
				float32 maxSlopeDeg = 45.f;

				// Obstacles à exclure (empreinte XZ), NON-ownant.
				const NkNavObstacle *obstacles = nullptr;
				uint32 obstacleCount = 0;
				float32 agentRadius = 0.f; // dilatation XZ appliquée à CHAQUE obstacle

				// Demi-hauteur de la sonde verticale (départ = areaMax.y + probeHeight,
				// arrivée = areaMin.y - probeHeight) — doit couvrir la géométrie de sol.
				float32 probeHeight = 50.f;
		};

		class NkNavMesh {
			public:
				// Génère le NavMesh. Retourne false si la zone ne produit aucun
				// triangle walkable (mesh vide résultant, pas une erreur fatale —
				// TriangleCount() == 0 ensuite). `desc` n'est PAS conservée après
				// l'appel (les buffers source peuvent être libérés par l'appelant).
				bool Build(const NkNavMeshBuildDesc &desc) noexcept;

				void Clear() noexcept;

				[[nodiscard]] uint32 TriangleCount() const noexcept {
					return static_cast<uint32>(mTriangles.Size());
				}

				[[nodiscard]] uint32 VertexCount() const noexcept {
					return static_cast<uint32>(mVertices.Size());
				}

				[[nodiscard]] const NkNavTriangle &Triangle(uint32 i) const noexcept {
					return mTriangles[i];
				}

				[[nodiscard]] const NkVec3f &Vertex(uint32 i) const noexcept {
					return mVertices[i];
				}

				[[nodiscard]] bool IsEmpty() const noexcept {
					return mTriangles.IsEmpty();
				}

				// Triangle contenant `p` (test point-in-triangle sur la projection XZ) ;
				// à défaut (point hors mesh), le triangle dont le centroïde est le plus
				// proche de `p` en XZ (repli robuste pour un départ/arrivée légèrement
				// hors NavMesh). Retourne -1 si le mesh est vide.
				[[nodiscard]] int32 FindTriangle(const NkVec3f &p) const noexcept;

				// A* entre `start` et `goal`. Remplit `outWaypoints` (Clear() implicite
				// en tête) si Success. Voir le statut retourné pour diagnostiquer un
				// échec (mesh vide / point hors mesh / îlots déconnectés).
				[[nodiscard]] NkPathStatus FindPath(const NkVec3f &start, const NkVec3f &goal,
													NkVector<NkVec3f> &outWaypoints) const noexcept;

			private:
				NkVector<NkVec3f> mVertices;
				NkVector<NkNavTriangle> mTriangles;

				// Point-in-triangle (XZ) — retourne aussi une distance approx. (0 si dedans).
				[[nodiscard]] bool ContainsXZ(const NkNavTriangle &tri, const NkVec3f &p) const noexcept;

				// Milieu de l'arête partagée entre `tri` (triangle courant) et son
				// voisin d'indice `neighborTri` (retrouve le bon slot via neighbors[]).
				[[nodiscard]] NkVec3f PortalMidpoint(uint32 triIndex, int32 neighborTri) const noexcept;
		};

	} // namespace nav
} // namespace nkentseu
