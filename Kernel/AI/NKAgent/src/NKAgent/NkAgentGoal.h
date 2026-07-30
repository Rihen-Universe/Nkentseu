// =============================================================================
// NKAgent/NkAgentGoal.h — un BUT de l'agent (le futur) (NKAI, Phase 4, Jalon 3).
//
// Un but = un état-cible (prédicat le plus simple sur l'état : « être dans CET
// état précis » — suffisant pour un environnement discret comme rl::NkEnvironment)
// + une priorité (utilisée par l'appelant pour ordonner ses propres décompositions,
// la pile NkGoalStack elle-même reste LIFO, cf. NkGoalStack.h) + un budget de pas
// optionnel (détection d'un but devenu INATTEIGNABLE : au-delà de `maxSteps` pas
// sans succès, le but est jugé en échec — remplace toute notion de replanification
// complexe par un abandon honnête et mesurable). Namespace : nkentseu::ai::agent.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			// Statut de vie d'un but, du point de vue de la pile qui le détient.
			enum class NkGoalStatus : uint32 {
				Active = 0,	 // en cours de poursuite (sommet de la pile)
				Achieved = 1, // état-cible atteint
				Failed = 2	 // budget de pas épuisé sans l'atteindre (jugé inatteignable)
			};

			struct NkAgentGoal {
					uint32 targetState = 0;	 // état-cible dans l'espace d'états de l'environnement
					float priority = 0.0f;	 // plus haut = plus important (informatif pour l'appelant)
					uint32 maxSteps = 0;		 // budget de pas avant échec (0 = illimité)
					uint32 stepsElapsed = 0;	 // pas passés à poursuivre CE but depuis qu'il est actif
					NkGoalStatus status = NkGoalStatus::Active;
			};

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
