// =============================================================================
// NkQLearning.cpp — implémentation de l'agent Q-learning tabulaire (NKAI, Phase 4).
// =============================================================================
#include "NKRL/NkQLearning.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			NkQLearning::NkQLearning(uint32 numStates, uint32 numActions, float alpha, float gamma, float epsilon,
									 uint32 seed)
				: mNumStates(numStates), mNumActions(numActions), mAlpha(alpha), mGamma(gamma), mEpsilon(epsilon),
				  mRng(seed ? seed : 1u) {
				mQ.Resize(numStates * numActions);
				for (uint32 i = 0; i < mQ.Size(); ++i)
					mQ[i] = 0.0f;
			}

			float NkQLearning::Rand01() {
				mRng = mRng * 1664525u + 1013904223u;
				return (float)((mRng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
			}

			uint32 NkQLearning::SelectGreedy(uint32 state) const {
				const float *row = &mQ[state * mNumActions];
				uint32 best = 0;
				float bv = row[0];
				for (uint32 a = 1; a < mNumActions; ++a)
					if (row[a] > bv) {
						bv = row[a];
						best = a;
					}
				return best;
			}

			uint32 NkQLearning::SelectAction(uint32 state) {
				if (Rand01() < mEpsilon) {
					// Exploration : action uniforme.
					uint32 a = (uint32)(Rand01() * (float)mNumActions);
					if (a >= mNumActions)
						a = mNumActions - 1;
					return a;
				}
				return SelectGreedy(state);
			}

			void NkQLearning::Update(uint32 s, uint32 a, float r, uint32 sNext, bool done) {
				float *q = &mQ[s * mNumActions + a];
				float target = r;
				if (!done) {
					// r + γ · max_a' Q(s',a')
					const float *row = &mQ[sNext * mNumActions];
					float best = row[0];
					for (uint32 k = 1; k < mNumActions; ++k)
						if (row[k] > best)
							best = row[k];
					target += mGamma * best;
				}
				*q += mAlpha * (target - *q); // pas de différence temporelle
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
