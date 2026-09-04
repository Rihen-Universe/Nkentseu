// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// ============================================================
// FILE: NkLatch.cpp
// DESCRIPTION: Latch — compte à rebours à usage unique.
//
// Écrit le 2026-09-04 : la classe était déclarée et exportée depuis des
// mois, ce fichier disait « implementation is header-only » et ne
// définissait RIEN (0 symbole dans NKThreading.lib, mesuré par la suite
// de tests, qui ne se liait pas). Déclarer n'est pas livrer.
//
// Sémantique : CountDown() décrémente, jamais en dessous de zéro et ne
// remonte jamais ; Wait() bloque jusqu'à zéro (ou l'échéance) ; une fois à
// zéro, tous les Wait() présents et futurs reviennent immédiatement.
// Bâti sur NkMutex + NkConditionVariable, sans STL.
// ============================================================
#include "NKThreading/Synchronization/NkLatch.h"

namespace nkentseu {
	namespace threading {

		NkLatch::NkLatch(nk_uint32 initialCount) noexcept : mCount(initialCount), mMutex(), mCondVar() {
		}

		void NkLatch::CountDown(nk_uint32 count) noexcept {
			NkScopedLockMutex lock(mMutex);
			if (mCount == 0u)
				return; // déjà libéré : un CountDown de plus ne change rien
			mCount = (count >= mCount) ? 0u : (mCount - count);
			if (mCount == 0u)
				mCondVar.NotifyAll();
		}

		nk_bool NkLatch::Wait(nk_int32 timeoutMs) noexcept {
			NkScopedLockMutex lock(mMutex);
			if (timeoutMs < 0) {
				while (mCount > 0u)
					mCondVar.Wait(lock);
				return true;
			}
			const nk_uint64 deadline = NkConditionVariable::GetMonotonicTimeMs() + (nk_uint64)timeoutMs;
			while (mCount > 0u) {
				if (!mCondVar.WaitUntil(lock, deadline))
					return mCount == 0u; // échéance : on relit l'état plutôt que de supposer
			}
			return true;
		}

		nk_bool NkLatch::IsReady() const noexcept {
			NkScopedLockMutex lock(mMutex);
			return mCount == 0u;
		}

		nk_uint32 NkLatch::GetCount() const noexcept {
			NkScopedLockMutex lock(mMutex);
			return mCount;
		}

	} // namespace threading
} // namespace nkentseu
