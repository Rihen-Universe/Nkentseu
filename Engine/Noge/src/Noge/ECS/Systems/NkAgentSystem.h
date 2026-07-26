#pragma once
// =============================================================================
// Noge/ECS/Systems/NkAgentSystem.h — pont ECS -> NKAgent (IA de gameplay / RL)
// =============================================================================
// Modèle identique à NkPhysicsSystem (pont ECS -> NKPhysics) :
//   - NE POSSÈDE PAS les ressources IA (contrairement à NkPhysicsSystem qui
//     possède le physics::NkPhysicsWorld) : agent::NkAgent et rl::NkGridWorld
//     restent possédés par l'appelant (démo/jeu), le composant NkAgentComponent
//     n'en garde que des pointeurs (cf. commentaire de ce fichier).
//   - Pour chaque entité porteuse d'un NkAgentComponent valide (agent +
//     environment non nuls), appelle UNE FOIS PAR TICK :
//       • Reset() de l'environnement si c'est le premier tick de l'épisode
//         (hasState == false), sinon
//       • agent::NkAgent::Step() — perception -> décision (ε-greedy si
//         `learn`, gloutonne sinon) -> action sur l'environnement -> mémorisation.
//   - Gère la fin d'épisode (terminal `done` OU `maxStepsPerEpisode` atteint) :
//     comptabilise `episodesCompleted`/`episodesReachedGoal`, puis relance un
//     nouvel épisode (Reset()) si `autoResetOnDone`.
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/AI/NkAgentComponent.h" // NkAgentComponent

namespace nkentseu {

	class NkAgentSystem final : public ecs::NkSystem {
		public:
			NkAgentSystem() noexcept = default;

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkAgentComponent>()
					.InGroup(ecs::NkSystemGroup::Update)
					.Sequential() // les instances NKAgent/NKRL référencées ne sont pas thread-safe
					.Named("NkAgentSystem");
			}

			void Execute(ecs::NkWorld &world, float32 dt) noexcept override;

		private:
			// Un pas ECS pour une entité : perçoit/décide/agit/mémorise via NkAgent,
			// puis gère la clôture d'épisode (terminal ou borne de pas atteinte).
			static void StepEntity(ecs::NkAgentComponent &ac) noexcept;
	};

} // namespace nkentseu
