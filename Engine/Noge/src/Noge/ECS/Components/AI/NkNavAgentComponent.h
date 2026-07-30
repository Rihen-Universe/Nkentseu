#pragma once
// -----------------------------------------------------------------------------
// FICHIER: Noge/ECS/Components/AI/NkNavAgentComponent.h
// DESCRIPTION: Pont ECS -> NKNavigation (Kernel/Runtime/NKNavigation) — NavMesh
//              + pathfinding A*. Navigation IA CLASSIQUE/déterministe (à ne
//              pas confondre avec NkAgentComponent.h, pont vers NKAgent/NKRL,
//              qui est de l'IA de gameplay par apprentissage).
//
// Même philosophie que NkCollider3D (Noge/ECS/Components/Physics/NkPhysics.h) :
// le NavMesh PARTAGÉ (nav::NkNavMesh) est possédé par NkNavigationSystem (pas
// par ce composant), exactement comme physics::NkPhysicsWorld est possédé par
// NkPhysicsSystem. Ce composant ne porte QUE l'état PAR-AGENT : destination
// courante, chemin calculé (tampon borné, zéro allocation dynamique par tick),
// progression le long du chemin.
//
// NkNavigationSystem (Noge/ECS/Systems/NkNavigationSystem.h) consomme ce
// composant : replanifie (FindPath) quand `needsRepath` est vrai, puis avance
// le NkTransform de l'entité le long des waypoints à `moveSpeed`.
// -----------------------------------------------------------------------------

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace ecs {

		using namespace math;

		// État de haut niveau d'un agent de navigation (piloté par NkNavigationSystem).
		enum class NkNavAgentState : uint8 {
			Idle = 0, // pas de destination active
			Pathing,  // destination fixée, chemin pas encore (re)calculé ce tick
			Moving,	  // chemin valide, agent en déplacement le long des waypoints
			Arrived,  // dernier waypoint atteint (arriveRadius)
			Failed	  // FindPath a échoué (départ/arrivée hors NavMesh, ou îlots
					  // déconnectés par des obstacles) — voir lastPathStatus
					  // (nav::NkPathStatus, Kernel/Runtime/NKNavigation/NkNavTypes.h)
		};

		// =============================================================================
		// NkNavAgentComponent — destination + chemin A* + progression (état par-agent).
		// =============================================================================
		struct NkNavAgentComponent {
				static constexpr uint32 kMaxWaypoints = 64; // tampon borné (zéro allocation dynamique par tick)

				// ── Configuration ────────────────────────────────────────────────────
				float32 moveSpeed = 2.f;	  // unités/seconde le long du chemin
				float32 arriveRadius = 0.2f; // distance à un waypoint pour le considérer atteint
				float32 agentRadius = 0.3f;  // rayon de l'agent (info -- doit correspondre à celui utilisé
											  // pour dilater les obstacles côté NkNavMeshBuildDesc::agentRadius)

				// ── Requête ──────────────────────────────────────────────────────────
				NkVec3f destination{};
				bool hasDestination = false;
				bool needsRepath = false; // mis à true par SetDestination() ; consommé par NkNavigationSystem

				// ── Chemin calculé (tampon borné) ──────────────────────────────────────
				NkVec3f waypoints[kMaxWaypoints];
				uint32 waypointCount = 0;
				uint32 currentWaypoint = 0;

				// ── État runtime (piloté par NkNavigationSystem, lecture possible côté jeu) ──
				NkNavAgentState state = NkNavAgentState::Idle;
				uint8 lastPathStatus = 0; // nav::NkPathStatus brut (évite d'inclure NKNavigation ici)

				// Fixe une nouvelle destination et demande un replan au prochain tick
				// de NkNavigationSystem::Execute().
				void SetDestination(const NkVec3f &dest) noexcept {
					destination = dest;
					hasDestination = true;
					needsRepath = true;
					waypointCount = 0;
					currentWaypoint = 0;
					state = NkNavAgentState::Pathing;
				}

				void Stop() noexcept {
					hasDestination = false;
					needsRepath = false;
					waypointCount = 0;
					currentWaypoint = 0;
					state = NkNavAgentState::Idle;
				}

				[[nodiscard]] bool HasPath() const noexcept {
					return waypointCount > 0;
				}
		};
		NK_COMPONENT(NkNavAgentComponent)

	} // namespace ecs
} // namespace nkentseu
