#pragma once
// =============================================================================
// Noge/ECS/Systems/NkNavigationSystem.h — pont ECS -> NKNavigation (NavMesh/A*)
// =============================================================================
// Modèle identique à NkPhysicsSystem (pont ECS -> NKPhysics) :
//   - POSSÈDE un nav::NkNavMesh (comme NkPhysicsSystem possède un
//     physics::NkPhysicsWorld) : le NavMesh est une ressource PARTAGÉE d'un
//     niveau/scène, générée UNE FOIS via BuildNavMesh(), pas par entité.
//   - Pour chaque entité (NkTransform + NkNavAgentComponent), à chaque tick :
//       • si `needsRepath` : appelle nav::NkNavMesh::FindPath() (position
//         courante -> destination), remplit le tampon borné de waypoints du
//         composant, état -> Moving (succès) ou Failed (échec, cf.
//         NkNavAgentComponent::lastPathStatus).
//       • si état == Moving : avance le NkTransform vers le waypoint courant
//         à `moveSpeed`, passe au waypoint suivant une fois `arriveRadius`
//         atteint, état -> Arrived au dernier waypoint.
//
// Navigation IA CLASSIQUE/déterministe (NavMesh + A*) — À NE PAS CONFONDRE
// avec NkAgentSystem.h (pont vers NKAgent/NKRL, IA de gameplay par
// apprentissage, item G1.2 de Engine/Noge/ROADMAP.md). Les deux systèmes sont
// indépendants et peuvent coexister sur des entités différentes.
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/AI/NkNavAgentComponent.h"
#include "NKNavigation/NkNavMesh.h"

namespace nkentseu {

	class NkNavigationSystem final : public ecs::NkSystem {
		public:
			NkNavigationSystem() noexcept = default;

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkTransform>()
					.Writes<ecs::NkNavAgentComponent>()
					.InGroup(ecs::NkSystemGroup::Update)
					.Sequential() // le nav::NkNavMesh partagé n'est pas thread-safe
					.Named("NkNavigationSystem");
			}

			void Execute(ecs::NkWorld &world, float32 dt) noexcept override;

			// Génère (ou régénère) le NavMesh partagé — à appeler une fois par
			// niveau/scène avant que les agents ne se déplacent (même esprit que
			// peupler physics::NkPhysicsWorld de corps avant NkPhysicsSystem::Step()).
			[[nodiscard]] bool BuildNavMesh(const nav::NkNavMeshBuildDesc &desc) noexcept {
				return mNavMesh.Build(desc);
			}

			// Accès direct au NavMesh (requêtes, diagnostics, debug-draw...).
			[[nodiscard]] nav::NkNavMesh &NavMesh() noexcept {
				return mNavMesh;
			}

			[[nodiscard]] const nav::NkNavMesh &NavMesh() const noexcept {
				return mNavMesh;
			}

		private:
			// Un pas ECS pour une entité : replanifie si nécessaire puis avance le long du chemin.
			void StepEntity(ecs::NkNavAgentComponent &agent, ecs::NkTransform &tf, float32 dt) const noexcept;

			nav::NkNavMesh mNavMesh; // NavMesh PARTAGÉ, POSSÉDÉ (comme physics::NkPhysicsWorld de NkPhysicsSystem)
	};

} // namespace nkentseu
