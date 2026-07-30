// =============================================================================
// NkKeyDoorGridWorld.cpp — implémentation du monde clé-puis-porte (NKAI, Phase 4).
// =============================================================================
#include "NKRL/NkKeyDoorGridWorld.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			NkKeyDoorGridWorld::NkKeyDoorGridWorld(uint32 size, uint32 startCell, uint32 keyCell, uint32 doorCell,
												   const NkVector<uint32> &holes, float stepCost)
				: mSize(size ? size : 1), mStart(startCell), mKey(keyCell), mDoor(doorCell), mCur(startCell),
				  mHasKey(false), mStepCost(stepCost), mBlockedDoorHits(0), mHoles(holes) {
			}

			bool NkKeyDoorGridWorld::IsHole(uint32 cell) const {
				for (uint32 i = 0; i < mHoles.Size(); ++i)
					if (mHoles[i] == cell)
						return true;
				return false;
			}

			uint32 NkKeyDoorGridWorld::Reset() {
				mCur = mStart;
				mHasKey = false;
				mBlockedDoorHits = 0;
				return mCur; // sans clé : état = position
			}

			void NkKeyDoorGridWorld::Step(uint32 action, uint32 &nextState, float &reward, bool &done) {
				const uint32 N = mSize;
				uint32 x = mCur % N;
				uint32 y = mCur / N;

				// 0=haut 1=bas 2=gauche 3=droite ; sortir de la grille = rester sur place.
				switch (action) {
					case 0:
						if (y > 0)
							--y;
						break;
					case 1:
						if (y + 1 < N)
							++y;
						break;
					case 2:
						if (x > 0)
							--x;
						break;
					case 3:
						if (x + 1 < N)
							++x;
						break;
					default:
						break;
				}

				uint32 dest = y * N + x;

				// Porte fermée : entrer sur la porte SANS la clé = bloqué (on reste).
				if (dest == mDoor && !mHasKey) {
					++mBlockedDoorHits;
					dest = mCur;
				}

				mCur = dest;

				// Marcher sur la case-clé prend la clé automatiquement.
				if (mCur == mKey)
					mHasKey = true;

				nextState = (mHasKey ? N * N : 0u) + mCur;

				if (mCur == mDoor && mHasKey) {
					reward = 1.0f; // porte franchie : but final
					done = true;
				} else if (IsHole(mCur)) {
					reward = -1.0f;
					done = true;
				} else {
					reward = -mStepCost;
					done = false;
				}
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
