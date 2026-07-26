// =============================================================================
// Noge/ECS/Systems/NkAgentSystem.cpp — pont ECS -> NKAgent (IA de gameplay / RL)
// =============================================================================
#include "NkAgentSystem.h"

namespace nkentseu {

	using namespace ecs;
	using namespace ai;

	// -------------------------------------------------------------------------
	// StepEntity — un pas complet (Reset si besoin, puis Step) pour une entité.
	// -------------------------------------------------------------------------
	void NkAgentSystem::StepEntity(NkAgentComponent &ac) noexcept {
		if (!ac.agent || !ac.environment)
			return; // référence non branchée : rien à piloter (cf. commentaire du composant)

		// Premier tick de l'épisode (ou reprise après une fin d'épisode sans
		// auto-reset) : (re)initialise l'environnement.
		if (!ac.hasState) {
			ac.currentState = ac.environment->Reset();
			ac.hasState = true;
			ac.stepsInEpisode = 0;
		}

		// Perception -> décision -> action -> mémorisation, délégué à NkAgent
		// (aucune réimplémentation de la boucle RL ici, cf. NkAgent::Step).
		agent::NkAgentStepResult step = ac.agent->Step(*ac.environment, ac.currentState, ac.learn);

		ac.lastAction = step.action;
		ac.lastReward = step.reward;
		ac.lastDone = step.done;
		ac.currentState = step.nextRawState;
		++ac.stepsInEpisode;
		++ac.totalSteps;

		const bool hitStepCap = (ac.maxStepsPerEpisode != 0u) && (ac.stepsInEpisode >= ac.maxStepsPerEpisode);
		if (step.done || hitStepCap) {
			++ac.episodesCompleted;
			// But atteint = épisode terminé par le terminal `done` avec récompense
			// positive (+1 au but, cf. NkGridWorld) — pas juste la borne de pas.
			if (step.done && step.reward > 0.5f)
				++ac.episodesReachedGoal;

			if (ac.autoResetOnDone) {
				ac.currentState = ac.environment->Reset();
				ac.stepsInEpisode = 0;
				// hasState reste true : l'épisode suivant démarre au prochain tick.
			} else {
				ac.hasState = false; // attend un Reset() explicite (prochain tick le fera).
			}
		}
	}

	// -------------------------------------------------------------------------
	// Execute — un pas ECS par entité porteuse d'un NkAgentComponent valide.
	// -------------------------------------------------------------------------
	void NkAgentSystem::Execute(NkWorld &world, float32 /*dt*/) noexcept {
		world.Query<NkAgentComponent>().ForEach([](NkEntityId, NkAgentComponent &ac) { StepEntity(ac); });
	}

} // namespace nkentseu
