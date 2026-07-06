// =============================================================================
// NkGridWorld.cpp — implémentation du monde-grille (NKAI, Phase 4).
// =============================================================================
#include "NKRL/NkGridWorld.h"

namespace nkentseu {
    namespace ai {
        namespace rl {

            NkGridWorld::NkGridWorld(uint32 size, uint32 startState, uint32 goalState,
                                     const NkVector<uint32>& holes, float stepCost)
                : mSize(size ? size : 1), mStart(startState), mGoal(goalState),
                  mCur(startState), mStepCost(stepCost), mHoles(holes) {}

            bool NkGridWorld::IsHole(uint32 state) const {
                for (uint32 i = 0; i < mHoles.Size(); ++i)
                    if (mHoles[i] == state) return true;
                return false;
            }

            uint32 NkGridWorld::Reset() {
                mCur = mStart;
                return mCur;
            }

            void NkGridWorld::Step(uint32 action, uint32& nextState, float& reward, bool& done) {
                const uint32 N = mSize;
                uint32 x = mCur % N;
                uint32 y = mCur / N;

                // 0=haut 1=bas 2=gauche 3=droite ; sortir = rester sur place.
                switch (action) {
                    case 0: if (y > 0)     --y; break;
                    case 1: if (y + 1 < N) ++y; break;
                    case 2: if (x > 0)     --x; break;
                    case 3: if (x + 1 < N) ++x; break;
                    default: break;
                }

                mCur = y * N + x;
                nextState = mCur;

                if (mCur == mGoal)      { reward =  1.0f; done = true; }
                else if (IsHole(mCur))  { reward = -1.0f; done = true; }
                else                    { reward = -mStepCost; done = false; }
            }

        } // namespace rl
    } // namespace ai
} // namespace nkentseu
