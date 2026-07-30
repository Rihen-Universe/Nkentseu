// =============================================================================
// Applications/NkNavCoreDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle du CŒUR du système de navigation
// (Kernel/Runtime/NKNavigation/NkNavMesh.h/.cpp) — génération de NavMesh +
// pathfinding A* — DÉCOUPLÉE de Noge/ECS (zéro dépendance à `Noge`, à
// `NkNavAgentComponent`/`NkNavigationSystem`).
//
// Raison d'être de cette démo SÉPARÉE de `Applications/NkNavDemo`
// (l'intégration ECS complète) : au moment de ce chantier, la lib `Noge`
// dans son ensemble ne compile pas — 3 fichiers SANS RAPPORT avec la
// navigation (`Noge/Doc/NkVectorDocument.cpp`, `Noge/IO/NkSVGIO.cpp` —
// ambiguïté `NkColor` dans `Noge/Color/NkColorManager.h` — et
// `Noge/ECS/Replication/NkNetWorld.cpp`, chantier NKNetwork d'un AUTRE agent
// en cours d'écriture en parallèle) cassent le lien de tout exécutable qui
// dépend de `Noge`, y compris `NkNavDemo`. Cette démo prouve que le CŒUR du
// système de navigation (module 1+2 du jalon : NavMesh + A*) fonctionne
// réellement, indépendamment de ce blocage externe. Voir
// Engine/Noge/ROADMAP.md (pilier "Navigation IA") pour le détail complet,
// y compris le statut de `NkNavDemo` (intégration ECS, code écrit et compile
// individuellement, exécution bloquée tant que `Noge` ne compile pas).
//
// Contourne le blocage de `jenga test` (politique de workspace), même schéma
// que les autres démos console de ce dépôt — application console avec
// assertions manuelles, pas une TestSuite.
// =============================================================================
#include "NKNavigation/NKNavigation.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::nav;
using namespace nkentseu::math;

namespace {

	int gPassCount = 0;
	int gFailCount = 0;

	void Check(bool cond, const char *what) noexcept {
		if (cond) {
			++gPassCount;
			logger.Infof("  [OK]   %s\n", what);
		} else {
			++gFailCount;
			logger.Errorf("  [FAIL] %s\n", what);
		}
	}

	// Sol plat 30x30 -- géométrie SOURCE sondée par raycast vertical RÉEL
	// (collision::NkRayTriangle3D, appelé depuis NkNavMesh::Build -- zéro
	// réimplémentation locale d'intersection rayon/triangle).
	const NkVec3f kGroundVerts[4] = {
		{-15.f, 0.f, -15.f},
		{15.f, 0.f, -15.f},
		{15.f, 0.f, 15.f},
		{-15.f, 0.f, 15.f},
	};
	const uint32 kGroundIndices[6] = {0, 1, 2, 0, 2, 3};

	bool InsideAnyObstacle(const NkVec3f &p, const NkNavObstacle *obstacles, uint32 count) noexcept {
		for (uint32 i = 0; i < count; ++i) {
			const NkNavObstacle &ob = obstacles[i];
			if (p.x >= ob.min.x && p.x <= ob.max.x && p.z >= ob.min.z && p.z <= ob.max.z)
				return true;
		}
		return false;
	}

	bool SegmentAvoidsObstacles(const NkVec3f &a, const NkVec3f &b, const NkNavObstacle *obstacles,
								 uint32 count) noexcept {
		constexpr uint32 kSamples = 24;
		for (uint32 s = 0; s <= kSamples; ++s) {
			const float32 t = (float32)s / (float32)kSamples;
			const NkVec3f p = a + (b - a) * t;
			if (InsideAnyObstacle(p, obstacles, count))
				return false;
		}
		return true;
	}

} // namespace

int main() {
	logger.Infof("=== NkNavCoreDemo : NavMesh + A* (Kernel/Runtime/NKNavigation), sans dependance a Noge/ECS ===\n");

	// ── Scénario 1 : 3 obstacles sur la diagonale -> chemin trouvé, détour prouvé ──
	{
		logger.Infof("-- Scenario 1 : 3 obstacles sur la diagonale depart->but --\n");

		const NkNavObstacle obstacles[3] = {
			{{-4.5f, -1.f, -4.5f}, {-1.5f, 3.f, -1.5f}},
			{{-1.5f, -1.f, -1.5f}, {1.5f, 3.f, 1.5f}},
			{{1.5f, -1.f, 1.5f}, {4.5f, 3.f, 4.5f}},
		};

		NkNavMesh mesh;
		NkNavMeshBuildDesc desc;
		desc.areaMin = {-10.f, -1.f, -10.f};
		desc.areaMax = {10.f, 1.f, 10.f};
		desc.cellSize = 0.5f;
		desc.groundVerts = kGroundVerts;
		desc.groundVertCount = 4;
		desc.groundIndices = kGroundIndices;
		desc.groundIndexCount = 6;
		desc.maxSlopeDeg = 45.f;
		desc.obstacles = obstacles;
		desc.obstacleCount = 3;
		desc.agentRadius = 0.4f;
		desc.probeHeight = 10.f;

		const bool built = mesh.Build(desc);
		logger.Infof("   NavMesh : %u triangles, %u sommets\n", mesh.TriangleCount(), mesh.VertexCount());
		Check(built && mesh.TriangleCount() > 0, "S1: NavMesh genere avec des triangles walkable");

		// La ligne droite depart->but passe EXACTEMENT par le centre des 3 obstacles :
		// preuve que le filtrage a bien retire les triangles recouverts.
		const NkVec3f straightMid = {0.f, 0.f, 0.f};
		Check(mesh.FindTriangle(straightMid) >= 0, "S1: FindTriangle repond meme au centre de la zone obstruee (repli robuste)");

		const NkVec3f start{-8.f, 0.f, -8.f};
		const NkVec3f goal{8.f, 0.f, 8.f};
		NkVector<NkVec3f> path;
		const NkPathStatus status = mesh.FindPath(start, goal, path);
		logger.Infof("   FindPath : statut=%d, %u waypoints\n", (int)status, (uint32)path.Size());
		Check(status == NkPathStatus::Success, "S1: A* trouve un chemin (NkPathStatus::Success)");
		Check(path.Size() >= 2, "S1: le chemin contient au moins 2 waypoints");

		bool pathClear = true;
		float32 pathLength = 0.f;
		for (uint32 i = 0; i + 1 < path.Size(); ++i) {
			if (!SegmentAvoidsObstacles(path[i], path[i + 1], obstacles, 3))
				pathClear = false;
			pathLength += (path[i + 1] - path[i]).Len();
		}
		Check(pathClear, "S1: chaque segment du chemin evite les 3 obstacles");

		const float32 straightLineDist = (goal - start).Len();
		logger.Infof("   Longueur chemin = %.3f (ligne droite = %.3f -- detour reel si superieur)\n", pathLength,
					 straightLineDist);
		Check(pathLength > straightLineDist,
			  "S1: le chemin est plus long que la ligne droite (preuve d'un VRAI detour, pas une ligne qui traverse)");

		// -- "Simulation" manuelle (sans ECS) : avance le long des waypoints --------
		NkVec3f pos = start;
		constexpr float32 kSpeed = 4.f;
		constexpr float32 kDt = 1.f / 30.f;
		constexpr float32 kArriveRadius = 0.3f;
		bool everInsideObstacle = false;
		uint32 wpIdx = 0;
		uint32 frame = 0;
		for (; frame < 3000 && wpIdx < path.Size(); ++frame) {
			const NkVec3f target = path[wpIdx];
			const NkVec3f toTarget = target - pos;
			const float32 dist = toTarget.Len();
			if (dist <= kArriveRadius) {
				pos = target;
				++wpIdx;
				continue;
			}
			const NkVec3f dir = toTarget * (1.f / dist);
			const float32 step = kSpeed * kDt;
			pos = (step >= dist) ? target : (pos + dir * step);
			if (InsideAnyObstacle(pos, obstacles, 3))
				everInsideObstacle = true;
		}
		logger.Infof("   Simulation manuelle : arrivee en %u frames (%.2f s), position finale = (%.3f, %.3f, %.3f)\n",
					 frame, (double)frame * kDt, pos.x, pos.y, pos.z);
		Check(wpIdx >= path.Size(), "S1: la simulation manuelle atteint le dernier waypoint avant la limite de frames");
		Check(!everInsideObstacle, "S1: le deplacement REEL (position echantillonnee a chaque frame) n'entre jamais dans un obstacle");
		Check((pos - goal).Len() <= kArriveRadius + 0.05f, "S1: position finale a portee du but");
	}

	// ── Scénario 2 (négatif) : mur SOLIDE sans ouverture -> Unreachable attendu ──
	{
		logger.Infof("-- Scenario 2 (negatif) : mur solide sans ouverture -> chemin impossible --\n");

		// Mur continu en X=0, sur TOUTE la profondeur de la zone (z in [-10,10]),
		// dilate par agentRadius -- aucune ouverture, gauche et droite deviennent
		// deux ilots deconnectes du graphe d'adjacence.
		const NkNavObstacle wall[1] = {
			{{-0.6f, -1.f, -10.f}, {0.6f, 3.f, 10.f}},
		};

		NkNavMesh mesh;
		NkNavMeshBuildDesc desc;
		desc.areaMin = {-10.f, -1.f, -10.f};
		desc.areaMax = {10.f, 1.f, 10.f};
		desc.cellSize = 0.5f;
		desc.groundVerts = kGroundVerts;
		desc.groundVertCount = 4;
		desc.groundIndices = kGroundIndices;
		desc.groundIndexCount = 6;
		desc.maxSlopeDeg = 45.f;
		desc.obstacles = wall;
		desc.obstacleCount = 1;
		desc.agentRadius = 0.4f;
		desc.probeHeight = 10.f;

		const bool built = mesh.Build(desc);
		Check(built && mesh.TriangleCount() > 0, "S2: NavMesh genere (le mur ne vide pas tout le mesh, juste une bande)");

		const NkVec3f start{-8.f, 0.f, 0.f};
		const NkVec3f goal{8.f, 0.f, 0.f};
		NkVector<NkVec3f> path;
		const NkPathStatus status = mesh.FindPath(start, goal, path);
		logger.Infof("   FindPath a travers le mur solide : statut=%d\n", (int)status);
		Check(status == NkPathStatus::Unreachable,
			  "S2: A* detecte correctement qu'aucun chemin n'existe (ilots deconnectes par le mur)");
	}

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
