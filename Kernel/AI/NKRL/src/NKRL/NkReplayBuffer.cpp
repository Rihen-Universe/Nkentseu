// =============================================================================
// NkReplayBuffer.cpp — implémentation de la mémoire de rejeu du DQN (NKAI, Phase 4).
// =============================================================================
#include "NKRL/NkReplayBuffer.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			NkReplayBuffer::NkReplayBuffer(nk_size capacity, uint32 seed)
				: mCount(0), mNextSlot(0), mRng(seed ? seed : 1u) {
				const nk_size cap = capacity ? capacity : 1;
				mBuffer.Resize(cap);
			}

			float NkReplayBuffer::Rand01() {
				mRng = mRng * 1664525u + 1013904223u;
				return (float)((mRng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
			}

			void NkReplayBuffer::Push(uint32 state, uint32 action, float reward, uint32 nextState, bool done) {
				const nk_size cap = mBuffer.Size();
				NkReplayTransition &slot = mBuffer[mNextSlot];
				slot.state = state;
				slot.action = action;
				slot.reward = reward;
				slot.nextState = nextState;
				slot.done = done;

				mNextSlot = (mNextSlot + 1) % cap;
				if (mCount < cap)
					++mCount;
			}

			void NkReplayBuffer::Sample(nk_size batchSize, NkVector<nk_size> &outIndices) {
				outIndices.Clear();
				if (mCount == 0)
					return;
				for (nk_size i = 0; i < batchSize; ++i) {
					nk_size idx = (nk_size)(Rand01() * (float)mCount);
					if (idx >= mCount)
						idx = mCount - 1;
					outIndices.PushBack(idx);
				}
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
