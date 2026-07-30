// =============================================================================
// Noge/ECS/Systems/NkNavigationSystem.cpp — pont ECS -> NKNavigation (NavMesh/A*)
// =============================================================================
#include "NkNavigationSystem.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

	using namespace ecs;
	using namespace math;

	// -------------------------------------------------------------------------
	// Un pas ECS pour une entité : replanifie si nécessaire puis avance le long
	// du chemin déjà calculé.
	// -------------------------------------------------------------------------
	void NkNavigationSystem::StepEntity(NkNavAgentComponent &agent, NkTransform &tf, float32 dt) const noexcept {
		if (!agent.hasDestination)
			return;

		// -- Replanification : FindPath délègue ENTIÈREMENT à nav::NkNavMesh (A*
		// sur le graphe de triangles) -- zéro réimplémentation locale. -----------
		if (agent.needsRepath) {
			NkVector<NkVec3f> path;
			const nav::NkPathStatus status = mNavMesh.FindPath(tf.localPosition, agent.destination, path);
			agent.lastPathStatus = static_cast<uint8>(status);
			agent.needsRepath = false;

			if (status == nav::NkPathStatus::Success && !path.IsEmpty()) {
				uint32 count = static_cast<uint32>(path.Size());
				if (count > NkNavAgentComponent::kMaxWaypoints)
					count = NkNavAgentComponent::kMaxWaypoints; // tronqué -- voir kMaxWaypoints
				for (uint32 i = 0; i < count; ++i)
					agent.waypoints[i] = path[i];
				agent.waypointCount = count;
				agent.currentWaypoint = 0;
				agent.state = NkNavAgentState::Moving;
			} else {
				agent.waypointCount = 0;
				agent.currentWaypoint = 0;
				agent.state = NkNavAgentState::Failed;
			}
		}

		if (agent.state != NkNavAgentState::Moving)
			return;

		if (agent.currentWaypoint >= agent.waypointCount) {
			// Fin du tampon de waypoints. Le chemin A* peut avoir été TRONQUÉ par
			// kMaxWaypoints (bug réel débusqué par la première exécution de
			// NkNavDemo : chemin de 82 waypoints, tampon de 64 -> l'agent
			// déclarait "Arrived" à ~5 unités du but). On ne déclare Arrived que
			// si la destination est RÉELLEMENT à portée ; sinon on REPLANIFIE
			// depuis la position courante (le chemin restant raccourcit à chaque
			// replan -> convergence garantie vers la destination).
			const float32 remaining = (agent.destination - tf.localPosition).Len();
			if (remaining <= agent.arriveRadius) {
				agent.state = NkNavAgentState::Arrived;
			} else {
				agent.needsRepath = true;
				agent.state = NkNavAgentState::Pathing;
			}
			return;
		}

		// -- Avance vers le waypoint courant (mouvement 3D -- suit la hauteur du
		// NavMesh, pas de projection XZ) -----------------------------------------
		const NkVec3f target = agent.waypoints[agent.currentWaypoint];
		const NkVec3f toTarget = target - tf.localPosition;
		const float32 dist = toTarget.Len();

		if (dist <= agent.arriveRadius) {
			tf.SetLocalPosition(target);
			++agent.currentWaypoint;
			// La clôture (Arrived vs replan si chemin tronqué) est décidée par le
			// bloc "fin du tampon" ci-dessus, au tick suivant -- une seule source
			// de vérité pour la fin de chemin.
			return;
		}

		const NkVec3f dir = toTarget * (1.f / dist);
		const float32 step = agent.moveSpeed * dt;
		if (step >= dist)
			tf.SetLocalPosition(target);
		else
			tf.SetLocalPosition(tf.localPosition + dir * step);
	}

	// -------------------------------------------------------------------------
	// Execute — un tick pour toutes les entités porteuses d'un NkNavAgentComponent.
	// -------------------------------------------------------------------------
	void NkNavigationSystem::Execute(NkWorld &world, float32 dt) noexcept {
		world.Query<NkNavAgentComponent, NkTransform>().ForEach(
			[&](NkEntityId, NkNavAgentComponent &agent, NkTransform &tf) { StepEntity(agent, tf, dt); });
	}

} // namespace nkentseu
