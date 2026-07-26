// =============================================================================
// Applications/NkNavDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle du premier système de navigation IA CLASSIQUE/
// déterministe de Noge (Engine/Noge/ROADMAP.md, pilier "Navigation IA
// (NavMesh/Pathfinding)") : génère un NavMesh RÉEL depuis une géométrie de
// sol (sondage par raycast NKCollision::NkRayTriangle3D,
// Kernel/Runtime/NKNavigation/NkNavMesh.cpp), calcule un chemin A* entre deux
// points en évitant 3 obstacles, puis pilote une entité ECS
// (NkNavAgentComponent + NkNavigationSystem, Engine/Noge/src/Noge/ECS/
// {Components,Systems}/.../NkNav*) le long de ce chemin sur plusieurs
// centaines de ticks — vérifie par assertions qu'elle atteint la destination
// SANS jamais traverser un obstacle (ni dans le chemin PLANIFIÉ, ni dans le
// déplacement RÉEL tick par tick).
//
// À NE PAS CONFONDRE avec Applications/NkAgentEcsDemo (NkAgentComponent/
// NkAgentSystem -> NKAgent/NKRL, IA de gameplay PAR APPRENTISSAGE) : ici,
// navigation classique/déterministe (NavMesh + A*), zéro apprentissage.
//
// Contourne le blocage de `jenga test` (politique de workspace), même schéma
// que Applications/NkEditableMeshDemo/NkAgentEcsDemo/NkLocomotionDemo —
// application console avec assertions manuelles, pas une TestSuite.
// =============================================================================
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Systems/NkNavigationSystem.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::ecs;

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

	// Sol plat 30x30 (largement plus grand que la zone de NavMesh -10..10) --
	// géométrie SOURCE sondée par raycast vertical RÉEL
	// (collision::NkRayTriangle3D via NkNavMesh::Build -- pas une réimplémentation).
	const NkVec3f kGroundVerts[4] = {
		{-15.f, 0.f, -15.f},
		{15.f, 0.f, -15.f},
		{15.f, 0.f, 15.f},
		{-15.f, 0.f, 15.f},
	};
	const uint32 kGroundIndices[6] = {0, 1, 2, 0, 2, 3};

	// 3 obstacles disposés SUR la diagonale start->goal : une trajectoire en
	// ligne droite les traverserait tous les 3 -- le NavMesh doit forcer un détour.
	const nav::NkNavObstacle kObstacles[3] = {
		{{-4.5f, -1.f, -4.5f}, {-1.5f, 3.f, -1.5f}},
		{{-1.5f, -1.f, -1.5f}, {1.5f, 3.f, 1.5f}},
		{{1.5f, -1.f, 1.5f}, {4.5f, 3.f, 4.5f}},
	};

	// Point p (test XZ, obstacle STRICT non-dilaté) à l'intérieur d'un obstacle
	// -- preuve que le déplacement RÉEL n'entre jamais dans son volume.
	bool InsideAnyObstacle(const NkVec3f &p) noexcept {
		for (const nav::NkNavObstacle &ob : kObstacles) {
			if (p.x >= ob.min.x && p.x <= ob.max.x && p.z >= ob.min.z && p.z <= ob.max.z)
				return true;
		}
		return false;
	}

} // namespace

int main() {
	logger.Infof("=== NkNavDemo : NavMesh + A* (NKNavigation) pilote une entite ECS Noge (NkNavigationSystem) ===\n");

	// ── 1. NavMesh : génération RÉELLE (raycast NKCollision + filtre pente/obstacles) ──
	NkNavigationSystem navSystem;

	nav::NkNavMeshBuildDesc desc;
	desc.areaMin = {-10.f, -1.f, -10.f};
	desc.areaMax = {10.f, 1.f, 10.f};
	desc.cellSize = 0.5f;
	desc.groundVerts = kGroundVerts;
	desc.groundVertCount = 4;
	desc.groundIndices = kGroundIndices;
	desc.groundIndexCount = 6;
	desc.maxSlopeDeg = 45.f;
	desc.obstacles = kObstacles;
	desc.obstacleCount = 3;
	desc.agentRadius = 0.4f;
	desc.probeHeight = 10.f;

	const bool built = navSystem.BuildNavMesh(desc);
	logger.Infof("-- NavMesh construit : %u triangles, %u sommets --\n", navSystem.NavMesh().TriangleCount(),
				 navSystem.NavMesh().VertexCount());
	Check(built && navSystem.NavMesh().TriangleCount() > 0,
		  "NavMesh genere avec des triangles walkable (pente/obstacles filtres)");

	// ── 2. Entité ECS : NkTransform (départ) + NkNavAgentComponent (destination) ──
	NkWorld world;
	const NkVec3f start{-8.f, 0.f, -8.f};
	const NkVec3f goal{8.f, 0.f, 8.f};

	// NOTE : NkEntityBuilder::With(T&&) a un bug de résolution de surcharge
	// pré-existant pour les lvalues nommées (cf. NkLocomotionDemo/main.cpp,
	// même contournement) -- World::Add<T>(id, value) avec argument de
	// template EXPLICITE, code NKECS core hors-scope de ce chantier.
	NkEntityId entity = world.CreateEntity();
	NkTransform tf;
	tf.localPosition = start;
	world.Add<NkTransform>(entity, tf);

	NkNavAgentComponent agent;
	agent.moveSpeed = 4.f;
	agent.arriveRadius = 0.3f;
	agent.agentRadius = 0.4f;
	world.Add<NkNavAgentComponent>(entity, agent);

	NkNavAgentComponent *ac = world.Get<NkNavAgentComponent>(entity);
	ac->SetDestination(goal);
	Check(ac->needsRepath && ac->hasDestination, "destination fixee sur l'agent, replan demande (SetDestination)");

	// ── 3. Simulation : NkNavigationSystem::Execute() tick par tick ───────────────
	constexpr float32 kDt = 1.f / 30.f;
	constexpr uint32 kMaxFrames = 1800; // 60 s a 30 fps -- large marge

	bool everMoving = false;
	bool obstacleHitDuringMovement = false;
	uint32 framesToArrive = 0;

	for (uint32 f = 0; f < kMaxFrames; ++f) {
		navSystem.Execute(world, kDt);

		const NkNavAgentComponent *acNow = world.Get<NkNavAgentComponent>(entity);
		const NkTransform *tfNow = world.Get<NkTransform>(entity);

		if (acNow->state == NkNavAgentState::Moving)
			everMoving = true;

		if (InsideAnyObstacle(tfNow->localPosition))
			obstacleHitDuringMovement = true;

		if (acNow->state == NkNavAgentState::Arrived) {
			framesToArrive = f + 1;
			break;
		}
		if (acNow->state == NkNavAgentState::Failed) {
			logger.Errorf("-- FindPath a echoue (lastPathStatus=%d) --\n", (int)acNow->lastPathStatus);
			break;
		}
	}

	const NkNavAgentComponent *acFinal = world.Get<NkNavAgentComponent>(entity);
	const NkTransform *tfFinal = world.Get<NkTransform>(entity);

	logger.Infof("-- Chemin calcule : %u waypoints, statut=%d --\n", acFinal->waypointCount,
				 (int)acFinal->lastPathStatus);
	Check(acFinal->lastPathStatus == (uint8)nav::NkPathStatus::Success, "A* a trouve un chemin (NkPathStatus::Success)");
	Check(acFinal->waypointCount >= 2, "le chemin contient au moins 2 waypoints (depart+arrivee, plus si detour)");
	Check(everMoving, "l'agent est passe par l'etat Moving (deplacement reellement pilote par NkNavigationSystem)");

	// -- Le chemin PLANIFIÉ (waypoints A*) évite les obstacles, segment par segment --
	bool plannedPathClear = true;
	for (uint32 i = 0; i + 1 < acFinal->waypointCount; ++i) {
		const NkVec3f a = acFinal->waypoints[i];
		const NkVec3f b = acFinal->waypoints[i + 1];
		constexpr uint32 kSamples = 20;
		for (uint32 s = 0; s <= kSamples; ++s) {
			const float32 t = (float32)s / (float32)kSamples;
			const NkVec3f p = a + (b - a) * t;
			if (InsideAnyObstacle(p)) {
				plannedPathClear = false;
				break;
			}
		}
	}
	Check(plannedPathClear, "le chemin PLANIFIE (waypoints A*) n'entre jamais dans un obstacle");
	Check(!obstacleHitDuringMovement, "le deplacement REEL (position ECS a chaque tick) n'entre jamais dans un obstacle");
	Check(acFinal->state == NkNavAgentState::Arrived, "l'agent a atteint l'etat Arrived avant la limite de frames");

	if (framesToArrive > 0)
		logger.Infof("-- Arrivee en %u frames (%.2f s a %d fps) --\n", framesToArrive, (double)framesToArrive * kDt,
					 (int)(1.f / kDt));

	const float32 distToGoal = (tfFinal->localPosition - goal).Len();
	logger.Infof("-- Position finale = (%.3f, %.3f, %.3f) -- distance au but = %.4f --\n", tfFinal->localPosition.x,
				 tfFinal->localPosition.y, tfFinal->localPosition.z, distToGoal);
	Check(distToGoal <= acFinal->arriveRadius + 0.05f, "position finale a portee du but (arriveRadius)");

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
