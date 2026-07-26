// =============================================================================
// NKEmbodied/NkEmbodiedBody.cpp — implémentation du corps simulé (NKAI, Phase 6).
// =============================================================================
#include "NKEmbodied/NkEmbodiedBody.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			NkEmbodiedBody::NkEmbodiedBody(uint32 size, uint32 startState, uint32 goalState,
											const NkVector<uint32> &holes, float stepCost)
				: mWorld(size, startState, goalState, holes, stepCost), mCurState(startState), mLastReward(0.0f),
				  mDone(false), mReachedGoal(false), mTicks(0) {
			}

			uint32 NkEmbodiedBody::Reset() {
				mCurState = mWorld.Reset();
				mLastReward = 0.0f;
				mDone = false;
				mReachedGoal = false;
				mTicks = 0;
				return mCurState;
			}

			void NkEmbodiedBody::ApplyAction(uint32 action) {
				if (mDone)
					return; // corps arrêté : un actionneur ne réanime pas un corps terminé (Reset() explicite requis)

				uint32 nextState;
				float reward;
				bool done;
				mWorld.Step(action, nextState, reward, done);

				mCurState = nextState;
				mLastReward = reward;
				mDone = done;
				mReachedGoal = done && reward > 0.5f; // +1 = but atteint (cf. rl::NkGridWorld)
				++mTicks;
			}

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
