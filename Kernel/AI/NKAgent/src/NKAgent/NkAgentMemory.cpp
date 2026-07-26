// =============================================================================
// NKAgent/NkAgentMemory.cpp — implémentation de la mémoire d'événements (NKAI, Phase 4).
// Jalon 2 : oubli pondéré par importance (évince le moins important, pas le
// plus ancien), le souvenir le plus récent étant toujours accepté.
// =============================================================================
#include "NKAgent/NkAgentMemory.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			NkAgentMemory::NkAgentMemory(nk_size capacity) : mCount(0) {
				const nk_size cap = capacity ? capacity : 1;
				mBuffer.Resize(cap);
				mImportance.Resize(cap);
				mOrder.Reserve(cap);
			}

			void NkAgentMemory::Push(const NkAgentTransition &t, float importance) {
				if (importance < 0.0f)
					importance = -importance;

				const nk_size cap = mBuffer.Size();
				nk_size slot;
				if (mCount < cap) {
					// Mémoire pas encore pleine : occupe le prochain slot libre.
					slot = mCount;
					++mCount;
				} else {
					// Mémoire pleine : évince le souvenir de MOINDRE importance.
					// mOrder est trié du plus ancien au plus récent, donc le premier
					// minimum rencontré est aussi le plus ancien (départage naturel).
					nk_size victimPos = 0;
					float victimImp = mImportance[mOrder[0]];
					for (nk_size i = 1; i < mOrder.Size(); ++i) {
						const float imp = mImportance[mOrder[i]];
						if (imp < victimImp) {
							victimImp = imp;
							victimPos = i;
						}
					}
					slot = mOrder[victimPos];
					// Retire la victime de l'ordre chronologique (décalage à gauche).
					for (nk_size i = victimPos + 1; i < mOrder.Size(); ++i)
						mOrder[i - 1] = mOrder[i];
					mOrder.PopBack();
				}

				mBuffer[slot] = t;
				mImportance[slot] = importance;
				mOrder.PushBack(slot); // le nouveau souvenir est le plus récent
			}

			const NkAgentTransition &NkAgentMemory::At(nk_size indexFromOldest) const {
				return mBuffer[mOrder[indexFromOldest]];
			}

			float NkAgentMemory::ImportanceAt(nk_size indexFromOldest) const {
				return mImportance[mOrder[indexFromOldest]];
			}

			const NkAgentTransition &NkAgentMemory::Last() const {
				return mBuffer[mOrder[mCount - 1]];
			}

			float NkAgentMemory::MinImportance() const {
				if (mCount == 0)
					return 0.0f;
				float v = mImportance[mOrder[0]];
				for (nk_size i = 1; i < mCount; ++i)
					if (mImportance[mOrder[i]] < v)
						v = mImportance[mOrder[i]];
				return v;
			}

			float NkAgentMemory::MaxImportance() const {
				if (mCount == 0)
					return 0.0f;
				float v = mImportance[mOrder[0]];
				for (nk_size i = 1; i < mCount; ++i)
					if (mImportance[mOrder[i]] > v)
						v = mImportance[mOrder[i]];
				return v;
			}

			void NkAgentMemory::Clear() {
				mOrder.Clear();
				mCount = 0;
			}

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
