#pragma once
// =============================================================================
// NkNavTypes.h — Types fondamentaux de NKNavigation (ZÉRO STL).
// Tout repose sur NKMath (vecteurs) + NKCore (types primitifs), comme
// NKCollision/NkColTypes.h. Aucune dépendance std:: — les conteneurs viennent
// de NKContainers, la mémoire de NKMemory.
// =============================================================================
#include "NKMath/NKMath.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nav {

		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;
		using nkentseu::uint8;
		using nkentseu::math::NkVec3f;

		// ── Obstacle ──────────────────────────────────────────────────────────
		// Boîte AABB world-space. Empreinte au sol = projection XZ ; hypothèse
		// v1 (documentée, honnête) : l'obstacle REPOSE sur le sol couvert par le
		// NavMesh (pas de plate-forme suspendue ni de tunnel en dessous) — donc
		// seul le test XZ (dilaté du rayon de l'agent) est appliqué au filtrage,
		// pas de test de recouvrement vertical fin. Suffisant pour un premier
		// jalon (obstacles = murs/caisses posés au sol) ; passage AU-DESSOUS
		// d'un obstacle suspendu (pont, arche) hors-scope de cette v1.
		struct NkNavObstacle {
				NkVec3f min{};
				NkVec3f max{};
		};

		// ── Triangle du NavMesh ───────────────────────────────────────────────
		// v0/v1/v2 = indices dans NkNavMesh::Vertex(i). neighbors[k] = triangle
		// adjacent partageant l'arête (v_k, v_{k+1 mod 3}), ou -1 (bord de mesh /
		// obstacle / hors zone walkable).
		struct NkNavTriangle {
				uint32 v0 = 0, v1 = 0, v2 = 0;
				NkVec3f centroid{};
				NkVec3f normal{0.f, 1.f, 0.f};
				int32 neighbors[3] = {-1, -1, -1};
		};

		// Résultat d'une requête de chemin — utile pour diagnostiquer un échec
		// (mesh vide / point hors mesh / îlots déconnectés par des obstacles).
		enum class NkPathStatus : uint8 {
			Success = 0,	   // chemin trouvé, waypoints remplis
			EmptyMesh,		   // NavMesh vide (Build() jamais appelé / zone entièrement non-walkable)
			StartOutOfMesh,	   // aucun triangle trouvé pour le point de départ
			GoalOutOfMesh,	   // aucun triangle trouvé pour le point d'arrivée
			Unreachable		   // start et goal existent mais aucun chemin dans le graphe d'adjacence
		};

	} // namespace nav
} // namespace nkentseu
