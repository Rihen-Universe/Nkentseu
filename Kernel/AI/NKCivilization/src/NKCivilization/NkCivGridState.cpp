// =============================================================================
// NKCivilization/NkCivGridState.cpp — implémentation du substrat partagé (Phase 5).
// =============================================================================
#include "NKCivilization/NkCivGridState.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			NkCivGridState::NkCivGridState(uint32 size, uint32 goalState, const NkVector<uint32> &holes)
				: mSize(size ? size : 1), mGoal(goalState) {
				for (nk_size i = 0; i < holes.Size(); ++i)
					mHoles.PushBack(holes[i]);
			}

			bool NkCivGridState::IsHole(uint32 state) const {
				for (nk_size i = 0; i < mHoles.Size(); ++i)
					if (mHoles[i] == state)
						return true;
				return false;
			}

			uint32 NkCivGridState::NextState(uint32 state, uint32 action) const {
				uint32 x = state % mSize;
				uint32 y = state / mSize;
				switch (action) {
					case 0:
						if (y > 0)
							--y;
						break;
					case 1:
						if (y + 1 < mSize)
							++y;
						break;
					case 2:
						if (x > 0)
							--x;
						break;
					case 3:
						if (x + 1 < mSize)
							++x;
						break;
					default:
						break;
				}
				return y * mSize + x;
			}

			void NkCivGridState::AddResource(uint32 state, uint32 capacity, uint32 regenPerTick) {
				mResources.PushBack(state);
				mCapacity.PushBack(capacity);
				mStock.PushBack(capacity); // pleine au départ
				mRegenPerTick.PushBack(regenPerTick);
			}

			bool NkCivGridState::HasResource(uint32 state) const {
				for (nk_size i = 0; i < mResources.Size(); ++i)
					if (mResources[i] == state && mStock[i] > 0)
						return true;
				return false;
			}

			bool NkCivGridState::IsResourceCell(uint32 state) const {
				for (nk_size i = 0; i < mResources.Size(); ++i)
					if (mResources[i] == state)
						return true;
				return false;
			}

			uint32 NkCivGridState::Stock(uint32 state) const {
				for (nk_size i = 0; i < mResources.Size(); ++i)
					if (mResources[i] == state)
						return mStock[i];
				return 0;
			}

			bool NkCivGridState::ConsumeResource(uint32 state) {
				for (nk_size i = 0; i < mResources.Size(); ++i) {
					if (mResources[i] == state && mStock[i] > 0) {
						--mStock[i]; // prélève une unité (regénérable selon RegenerateTick)
						return true;
					}
				}
				return false;
			}

			void NkCivGridState::RegenerateTick() {
				for (nk_size i = 0; i < mResources.Size(); ++i) {
					if (mRegenPerTick[i] == 0)
						continue;
					uint32 next = mStock[i] + mRegenPerTick[i];
					mStock[i] = (next > mCapacity[i]) ? mCapacity[i] : next;
				}
			}

			nk_size NkCivGridState::ResourcesRemaining() const {
				nk_size n = 0;
				for (nk_size i = 0; i < mStock.Size(); ++i)
					n += mStock[i];
				return n;
			}

			void NkCivGridState::SetResourceParams(uint32 state, uint32 capacity, uint32 regenPerTick) {
				for (nk_size i = 0; i < mResources.Size(); ++i) {
					if (mResources[i] == state) {
						mCapacity[i] = capacity;
						mRegenPerTick[i] = regenPerTick;
						if (mStock[i] > capacity)
							mStock[i] = capacity;
						return;
					}
				}
			}

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
