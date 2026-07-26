// =============================================================================
// NkNavMesh.cpp — génération de NavMesh (grille filtrée pente/obstacles) +
// pathfinding A* sur le graphe d'adjacence des triangles.
// =============================================================================
#include "NKNavigation/NkNavMesh.h"
#include "NKCollision/NkColTests.h" // collision::NkRayTriangle3D (raycast RÉEL, réutilisé — pas réimplémenté)
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
	namespace nav {

		using namespace math;

		namespace {

			// Marge de tolérance sur le test de pente (évite un rejet par erreur
			// d'arrondi flottant sur une pente exactement à la limite).
			constexpr float32 kSlopeEpsilon = 1e-4f;

			NK_FORCE_INLINE uint64 MakeEdgeKey(uint32 a, uint32 b) noexcept {
				const uint32 lo = (a < b) ? a : b;
				const uint32 hi = (a < b) ? b : a;
				return (static_cast<uint64>(lo) << 32) | static_cast<uint64>(hi);
			}

			// Propriétaire courant d'une arête pendant la construction du graphe
			// d'adjacence. triIndex < 0 = arête déjà consommée par 2 triangles.
			struct EdgeRef {
					int32 triIndex = -1;
					int32 slot = -1; // 0/1/2 : arête (v0,v1) / (v1,v2) / (v2,v0)
			};

			// Sonde la géométrie de sol source par un RAYCAST VERTICAL RÉEL
			// (collision::NkRayTriangle3D, NKCollision) — zéro réimplémentation
			// locale d'intersection rayon/triangle. Renvoie le point d'impact le
			// plus haut (surface la plus proche du sommet de la sonde).
			bool ProbeGround(float32 x, float32 z, float32 topY, float32 botY, const NkVec3f *verts,
							  const uint32 *indices, uint32 indexCount, NkVec3f &outPoint,
							  NkVec3f &outNormal) noexcept {
				if (topY <= botY)
					return false;

				collision::NkRay3D ray;
				ray.origin = {x, topY, z};
				ray.dir = {0.f, -1.f, 0.f};
				ray.maxT = topY - botY;

				bool found = false;
				float32 bestT = ray.maxT + 1.f;
				const uint32 triCount = indexCount / 3;
				for (uint32 t = 0; t < triCount; ++t) {
					const uint32 i0 = indices[t * 3 + 0];
					const uint32 i1 = indices[t * 3 + 1];
					const uint32 i2 = indices[t * 3 + 2];
					collision::NkRayHit3D hit;
					if (!collision::NkRayTriangle3D(ray, verts[i0], verts[i1], verts[i2], hit))
						continue;
					if (hit.t < bestT) {
						bestT = hit.t;
						outPoint = hit.point;
						outNormal = hit.normal;
						found = true;
					}
				}
				return found;
			}

		} // namespace

		void NkNavMesh::Clear() noexcept {
			mVertices.Clear();
			mTriangles.Clear();
		}

		bool NkNavMesh::Build(const NkNavMeshBuildDesc &desc) noexcept {
			Clear();

			if (desc.cellSize <= 0.f || !desc.groundVerts || desc.groundVertCount == 0 || !desc.groundIndices ||
				desc.groundIndexCount < 3)
				return false;

			const float32 width = desc.areaMax.x - desc.areaMin.x;
			const float32 depth = desc.areaMax.z - desc.areaMin.z;
			if (width <= 0.f || depth <= 0.f)
				return false;

			uint32 cols = static_cast<uint32>(NkCeil(width / desc.cellSize));
			uint32 rows = static_cast<uint32>(NkCeil(depth / desc.cellSize));
			if (cols < 1)
				cols = 1;
			if (rows < 1)
				rows = 1;
			const uint32 vertsPerRow = cols + 1;
			const uint32 vertCount = (rows + 1) * vertsPerRow;

			// ── 1. Sonde verticale (raycast NKCollision réel) sur la grille de sommets ──
			NkVector<uint8> valid;
			valid.Reserve(vertCount);
			mVertices.Reserve(vertCount);

			const float32 topY = desc.areaMax.y + desc.probeHeight;
			const float32 botY = desc.areaMin.y - desc.probeHeight;

			for (uint32 j = 0; j <= rows; ++j) {
				for (uint32 i = 0; i <= cols; ++i) {
					const float32 x = desc.areaMin.x + static_cast<float32>(i) * desc.cellSize;
					const float32 z = desc.areaMin.z + static_cast<float32>(j) * desc.cellSize;
					NkVec3f hitPoint{x, desc.areaMin.y, z};
					NkVec3f hitNormal{0.f, 1.f, 0.f};
					const bool hit = ProbeGround(x, z, topY, botY, desc.groundVerts, desc.groundIndices,
												  desc.groundIndexCount, hitPoint, hitNormal);
					mVertices.PushBack(hitPoint);
					valid.PushBack(hit ? uint8(1) : uint8(0));
				}
			}

			// ── 2. Triangulation de grille + filtres (pente puis obstacles) ──────────────
			const float32 maxSlopeRad = desc.maxSlopeDeg * (NK_PI_F / 180.f);
			const float32 cosLimit = NkCos(maxSlopeRad);

			const auto idx = [vertsPerRow](uint32 i, uint32 j) noexcept -> uint32 { return j * vertsPerRow + i; };

			const auto tryAddTri = [&](uint32 ia, uint32 ib, uint32 ic) noexcept {
				if (!valid[ia] || !valid[ib] || !valid[ic])
					return; // trou (aucun impact de sonde) sous au moins un sommet

				const NkVec3f &pa = mVertices[ia];
				const NkVec3f &pb = mVertices[ib];
				const NkVec3f &pc = mVertices[ic];

				NkVec3f normal = (pb - pa).Cross(pc - pa);
				const float32 lenSq = normal.LenSq();
				if (lenSq < 1e-12f)
					return; // triangle dégénéré (3 sommets alignés)
				normal = normal * (1.f / NkSqrt(lenSq));
				if (normal.y < 0.f)
					normal = normal * -1.f; // le sol est orienté vers le haut par construction

				if (normal.Dot(NkVec3f{0.f, 1.f, 0.f}) < cosLimit - kSlopeEpsilon)
					return; // pente > maxSlopeDeg

				const NkVec3f centroid = (pa + pb + pc) * (1.f / 3.f);

				// Obstacles : empreinte XZ dilatée du rayon de l'agent (v1 — voir
				// note de NkNavObstacle : hypothèse "obstacle posé au sol", pas de
				// test vertical fin).
				for (uint32 o = 0; o < desc.obstacleCount; ++o) {
					const NkNavObstacle &ob = desc.obstacles[o];
					const float32 minX = ob.min.x - desc.agentRadius;
					const float32 maxX = ob.max.x + desc.agentRadius;
					const float32 minZ = ob.min.z - desc.agentRadius;
					const float32 maxZ = ob.max.z + desc.agentRadius;
					if (centroid.x >= minX && centroid.x <= maxX && centroid.z >= minZ && centroid.z <= maxZ)
						return; // recouvert par un obstacle
				}

				NkNavTriangle tri;
				tri.v0 = ia;
				tri.v1 = ib;
				tri.v2 = ic;
				tri.centroid = centroid;
				tri.normal = normal;
				mTriangles.PushBack(tri);
			};

			for (uint32 j = 0; j < rows; ++j) {
				for (uint32 i = 0; i < cols; ++i) {
					const uint32 v00 = idx(i, j);
					const uint32 v10 = idx(i + 1, j);
					const uint32 v11 = idx(i + 1, j + 1);
					const uint32 v01 = idx(i, j + 1);
					tryAddTri(v00, v10, v11);
					tryAddTri(v00, v11, v01);
				}
			}

			// ── 3. Graphe d'adjacence (arêtes partagées) — table de hachage générale, ──
			// valable pour n'importe quelle triangulation (pas seulement la grille) ─────
			NkHashMap<uint64, EdgeRef> edgeOwner;
			const uint32 triCount = TriangleCount();
			for (uint32 t = 0; t < triCount; ++t) {
				NkNavTriangle &tri = mTriangles[t];
				const uint32 vs[3] = {tri.v0, tri.v1, tri.v2};
				for (int32 slot = 0; slot < 3; ++slot) {
					const uint32 a = vs[static_cast<uint32>(slot)];
					const uint32 b = vs[static_cast<uint32>((slot + 1) % 3)];
					const uint64 key = MakeEdgeKey(a, b);
					EdgeRef *existing = edgeOwner.Find(key);
					if (!existing) {
						EdgeRef ref;
						ref.triIndex = static_cast<int32>(t);
						ref.slot = slot;
						edgeOwner.Insert(key, ref);
					} else if (existing->triIndex >= 0) {
						NkNavTriangle &other = mTriangles[static_cast<uint32>(existing->triIndex)];
						tri.neighbors[slot] = existing->triIndex;
						other.neighbors[existing->slot] = static_cast<int32>(t);
						existing->triIndex = -1; // arête consommée par ses 2 triangles
					}
					// else : arête non-manifold (>2 triangles) -- ignorée, ne devrait pas
					// se produire sur une triangulation de grille régulière.
				}
			}

			return !mTriangles.IsEmpty();
		}

		bool NkNavMesh::ContainsXZ(const NkNavTriangle &tri, const NkVec3f &p) const noexcept {
			const NkVec3f &a = mVertices[tri.v0];
			const NkVec3f &b = mVertices[tri.v1];
			const NkVec3f &c = mVertices[tri.v2];

			const auto sign = [](const NkVec3f &p1, const NkVec3f &p2, const NkVec3f &p3) noexcept -> float32 {
				return (p1.x - p3.x) * (p2.z - p3.z) - (p2.x - p3.x) * (p1.z - p3.z);
			};

			const float32 d1 = sign(p, a, b);
			const float32 d2 = sign(p, b, c);
			const float32 d3 = sign(p, c, a);

			const bool hasNeg = (d1 < 0.f) || (d2 < 0.f) || (d3 < 0.f);
			const bool hasPos = (d1 > 0.f) || (d2 > 0.f) || (d3 > 0.f);
			return !(hasNeg && hasPos);
		}

		int32 NkNavMesh::FindTriangle(const NkVec3f &p) const noexcept {
			const uint32 n = TriangleCount();
			if (n == 0)
				return -1;

			for (uint32 t = 0; t < n; ++t) {
				if (ContainsXZ(mTriangles[t], p))
					return static_cast<int32>(t);
			}

			// Repli : centroïde le plus proche en XZ (départ/arrivée légèrement hors mesh).
			int32 best = -1;
			float32 bestDist = 1e30f;
			for (uint32 t = 0; t < n; ++t) {
				const NkVec3f &c = mTriangles[t].centroid;
				const float32 dx = c.x - p.x;
				const float32 dz = c.z - p.z;
				const float32 d = dx * dx + dz * dz;
				if (d < bestDist) {
					bestDist = d;
					best = static_cast<int32>(t);
				}
			}
			return best;
		}

		NkVec3f NkNavMesh::PortalMidpoint(uint32 triIndex, int32 neighborTri) const noexcept {
			const NkNavTriangle &tri = mTriangles[triIndex];
			const uint32 vs[3] = {tri.v0, tri.v1, tri.v2};
			for (int32 slot = 0; slot < 3; ++slot) {
				if (tri.neighbors[slot] == neighborTri) {
					const NkVec3f &a = mVertices[vs[static_cast<uint32>(slot)]];
					const NkVec3f &b = mVertices[vs[static_cast<uint32>((slot + 1) % 3)]];
					return (a + b) * 0.5f;
				}
			}
			return tri.centroid; // ne devrait pas arriver si neighborTri est bien un voisin
		}

		NkPathStatus NkNavMesh::FindPath(const NkVec3f &start, const NkVec3f &goal,
										  NkVector<NkVec3f> &outWaypoints) const noexcept {
			outWaypoints.Clear();
			if (mTriangles.IsEmpty())
				return NkPathStatus::EmptyMesh;

			const int32 startTri = FindTriangle(start);
			const int32 goalTri = FindTriangle(goal);
			if (startTri < 0)
				return NkPathStatus::StartOutOfMesh;
			if (goalTri < 0)
				return NkPathStatus::GoalOutOfMesh;

			if (startTri == goalTri) {
				outWaypoints.PushBack(start);
				outWaypoints.PushBack(goal);
				return NkPathStatus::Success;
			}

			const uint32 n = TriangleCount();
			NkVector<float32> gScore;
			NkVector<float32> fScore;
			NkVector<int32> cameFrom;
			NkVector<uint8> inOpen;
			NkVector<uint8> inClosed;
			gScore.Reserve(n);
			fScore.Reserve(n);
			cameFrom.Reserve(n);
			inOpen.Reserve(n);
			inClosed.Reserve(n);
			for (uint32 i = 0; i < n; ++i) {
				gScore.PushBack(1e30f);
				fScore.PushBack(1e30f);
				cameFrom.PushBack(-1);
				inOpen.PushBack(0);
				inClosed.PushBack(0);
			}

			const uint32 goalU = static_cast<uint32>(goalTri);
			const auto heuristic = [&](uint32 t) noexcept -> float32 {
				return (mTriangles[t].centroid - mTriangles[goalU].centroid).Len();
			};

			NkVector<uint32> openList;
			const uint32 startU = static_cast<uint32>(startTri);
			gScore[startU] = 0.f;
			fScore[startU] = heuristic(startU);
			inOpen[startU] = 1;
			openList.PushBack(startU);

			while (!openList.IsEmpty()) {
				// Liste ouverte linéaire (extraction du min par balayage) -- suffisant
				// pour un premier jalon ; une priority queue serait l'optimisation
				// naturelle pour des NavMesh de grande taille (non nécessaire ici).
				uint32 bestSlot = 0;
				float32 bestF = fScore[openList[0]];
				for (uint32 k = 1; k < openList.Size(); ++k) {
					const float32 f = fScore[openList[k]];
					if (f < bestF) {
						bestF = f;
						bestSlot = k;
					}
				}
				const uint32 current = openList[bestSlot];

				if (current == goalU) {
					NkVector<uint32> triPath; // ordre inverse : goal -> ... -> start
					int32 cur = static_cast<int32>(current);
					while (cur >= 0) {
						triPath.PushBack(static_cast<uint32>(cur));
						cur = cameFrom[static_cast<uint32>(cur)];
					}

					outWaypoints.PushBack(start);
					for (uint32 i = triPath.Size(); i >= 2; --i) {
						const uint32 a = triPath[i - 1];
						const uint32 b = triPath[i - 2];
						outWaypoints.PushBack(PortalMidpoint(a, static_cast<int32>(b)));
					}
					outWaypoints.PushBack(goal);
					return NkPathStatus::Success;
				}

				// Swap-pop : retire `current` de la liste ouverte (ordre non significatif).
				openList[bestSlot] = openList[openList.Size() - 1];
				openList.PopBack();
				inOpen[current] = 0;
				inClosed[current] = 1;

				const NkNavTriangle &tri = mTriangles[current];
				for (int32 s = 0; s < 3; ++s) {
					const int32 nb = tri.neighbors[s];
					if (nb < 0 || inClosed[static_cast<uint32>(nb)])
						continue;
					const uint32 nbU = static_cast<uint32>(nb);
					const float32 stepCost = (mTriangles[nbU].centroid - tri.centroid).Len();
					const float32 tentativeG = gScore[current] + stepCost;
					if (tentativeG < gScore[nbU]) {
						cameFrom[nbU] = static_cast<int32>(current);
						gScore[nbU] = tentativeG;
						fScore[nbU] = tentativeG + heuristic(nbU);
						if (!inOpen[nbU]) {
							inOpen[nbU] = 1;
							openList.PushBack(nbU);
						}
					}
				}
			}

			return NkPathStatus::Unreachable;
		}

	} // namespace nav
} // namespace nkentseu
