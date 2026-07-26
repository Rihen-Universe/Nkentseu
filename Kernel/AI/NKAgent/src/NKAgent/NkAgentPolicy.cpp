// =============================================================================
// NKAgent/NkAgentPolicy.cpp — implémentation de la politique de décision (NKAI, Phase 4).
// =============================================================================
#include "NKAgent/NkAgentPolicy.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			NkAgentPolicy::NkAgentPolicy(uint32 numStates, uint32 numActions, float alpha, float gamma, float epsilon,
										 uint32 seed)
				: mQLearning(numStates, numActions, alpha, gamma, epsilon, seed) {
			}

			uint32 NkAgentPolicy::SelectAction(uint32 rawState) {
				return mQLearning.SelectAction(rawState);
			}

			uint32 NkAgentPolicy::SelectGreedy(uint32 rawState) const {
				return mQLearning.SelectGreedy(rawState);
			}

			void NkAgentPolicy::Update(uint32 s, uint32 a, float r, uint32 sNext, bool done) {
				mQLearning.Update(s, a, r, sNext, done);
			}

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
