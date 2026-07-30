// =============================================================================
// NKAgent/NkAgentPolicy.h — politique de décision de l'agent (NKAI, Phase 4).
//
// N'APPREND RIEN de nouveau : ENVELOPPE l'agent Q-learning tabulaire de NKRL
// (rl::NkQLearning), déjà livré et prouvé (NKRLTest : 7.9% -> 100% de réussite).
// NkAgent COMPOSE dessus au lieu de réimplémenter l'apprentissage par
// renforcement. Cette couche existe comme point d'extension futur (Jalon 4 :
// combiner la politique apprise avec du raisonnement NKInfer et des traits de
// personnalité) sans toucher à NKRL. Namespace : nkentseu::ai::agent.
// =============================================================================
#pragma once

#include "NKRL/NkQLearning.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			class NkAgentPolicy {
				public:
					NkAgentPolicy(uint32 numStates, uint32 numActions, float alpha = 0.1f, float gamma = 0.99f,
								  float epsilon = 0.1f, uint32 seed = 1u);

					// Action ε-greedy (exploration), pour l'entraînement.
					uint32 SelectAction(uint32 rawState);
					// Action gloutonne (argmax Q), pour l'exploitation/évaluation.
					uint32 SelectGreedy(uint32 rawState) const;

					// Mise à jour TD après une transition (s,a,r,s',done) — déléguée à NKRL.
					void Update(uint32 s, uint32 a, float r, uint32 sNext, bool done);

					void SetEpsilon(float e) {
						mQLearning.SetEpsilon(e);
					}

					float Epsilon() const {
						return mQLearning.Epsilon();
					}

					// Accès à la brique NKRL sous-jacente (inspection, tests, extension future).
					const rl::NkQLearning &QLearning() const {
						return mQLearning;
					}

					rl::NkQLearning &QLearning() {
						return mQLearning;
					}

				private:
					rl::NkQLearning mQLearning;
			};

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
