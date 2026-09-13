// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// ============================================================
// FILE: NkBarrier.cpp
// DESCRIPTION: Barrière réutilisable à générations (phases).
//
// Écrit le 2026-09-04 : déclarée et exportée depuis des mois, jamais
// définie (0 symbole dans NKThreading.lib). Déclarer n'est pas livrer.
//
// Sémantique : Wait() bloque jusqu'à ce que N threads soient arrivés ; le
// DERNIER arrivé libère tous les autres, ouvre la phase suivante et reçoit
// `true` (il est le meneur de la phase — un seul par phase) ; les autres
// reçoivent `false`. La barrière est immédiatement réutilisable : un thread
// rapide qui revient avant que les lents soient repartis attend la phase
// suivante, jamais la courante (c'est le rôle du numéro de phase).
// Reset() force la phase suivante et libère tout le monde.
// Bâti sur NkMutex + NkConditionVariable, sans STL.
// ============================================================
#include "NKThreading/Synchronization/NkBarrier.h"

namespace nkentseu {
	namespace threading {

		NkBarrier::NkBarrier(nk_uint32 numThreads) noexcept
			: mNumThreads(numThreads == 0u ? 1u : numThreads), mCurrentPhase(0u), mCount(0u), mMutex(), mCondVar() {
		}

		nk_bool NkBarrier::Wait() noexcept {
			NkScopedLockMutex lock(mMutex);
			const nk_uint32 phase = mCurrentPhase;
			++mCount;
			if (mCount >= mNumThreads) {
				mCount = 0u;
				++mCurrentPhase; // la phase change AVANT le réveil : les dormeurs la voient
				mCondVar.NotifyAll();
				return true; // le dernier arrivé mène la phase
			}
			while (phase == mCurrentPhase)
				mCondVar.Wait(lock);
			return false;
		}

		void NkBarrier::Reset() noexcept {
			NkScopedLockMutex lock(mMutex);
			mCount = 0u;
			++mCurrentPhase;
			mCondVar.NotifyAll();
		}

	} // namespace threading
} // namespace nkentseu
