// =============================================================================
// NKAgent/NkAgentPerception.cpp — implémentation de la perception (NKAI, Phase 4).
// =============================================================================
#include "NKAgent/NkAgentPerception.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			NkAgentPerception::NkAgentPerception(const rl::NkGridWorld &world) : mSize(world.Size() ? world.Size() : 1) {
				mGoalX = world.GoalState() % mSize;
				mGoalY = world.GoalState() / mSize;
			}

			NkAgentPerception::NkAgentPerception(uint32 size, uint32 goalX, uint32 goalY)
				: mSize(size ? size : 1), mGoalX(goalX), mGoalY(goalY) {
			}

			void NkAgentPerception::Encode(uint32 rawState, NkVector<float> &outFeatures) const {
				const uint32 x = rawState % mSize;
				const uint32 y = rawState / mSize;
				const float denom = mSize > 1 ? (float)(mSize - 1) : 1.0f;

				outFeatures.Resize(4);
				outFeatures[0] = (float)x / denom;
				outFeatures[1] = (float)y / denom;
				outFeatures[2] = ((float)mGoalX - (float)x) / denom;
				outFeatures[3] = ((float)mGoalY - (float)y) / denom;
			}

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
