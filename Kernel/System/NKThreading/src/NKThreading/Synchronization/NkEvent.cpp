// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// ============================================================
// FILE: NkEvent.cpp
// DESCRIPTION: Événement manuel / automatique, sans réveil perdu.
//
// Écrit le 2026-09-04 : déclaré et exporté depuis des mois, jamais défini
// (0 symbole dans NKThreading.lib). Déclarer n'est pas livrer.
//
// Sémantique :
//   Set()   : l'état devient signalé et le RESTE — un Set() avant Wait()
//             ne se perd pas. Manuel : tous les attendants partent ;
//             automatique : UN attendant part et consomme le signal.
//   Reset() : l'état redevient non signalé.
//   Pulse() : transitoire — libère les attendants PRÉSENTS (tous en manuel,
//             un en automatique) et ne laisse aucun état derrière lui ; sans
//             attendant, il ne fait rien (IsSignaled() reste faux).
//   Wait()  : bloque jusqu'au signal, à un Pulse, ou à l'échéance.
// Bâti sur NkMutex + NkConditionVariable, sans STL.
// ============================================================
#include "NKThreading/Synchronization/NkEvent.h"

namespace nkentseu {
	namespace threading {

		NkEvent::NkEvent(nk_bool manualReset, nk_bool initialState) noexcept
			: mManualReset(manualReset), mSignaled(initialState), mWaiters(0u), mPulseGeneration(0u), mMutex(),
			  mCondVar() {
		}

		void NkEvent::Set() noexcept {
			NkScopedLockMutex lock(mMutex);
			mSignaled = true;
			if (mManualReset)
				mCondVar.NotifyAll();
			else
				mCondVar.NotifyOne();
		}

		void NkEvent::Reset() noexcept {
			NkScopedLockMutex lock(mMutex);
			mSignaled = false;
		}

		void NkEvent::Pulse() noexcept {
			NkScopedLockMutex lock(mMutex);
			if (mWaiters == 0u)
				return; // transitoire : personne n'attend, rien ne reste
			if (mManualReset) {
				++mPulseGeneration; // tous les attendants de cette génération partent
				mCondVar.NotifyAll();
			} else {
				mSignaled = true; // UN attendant consommera ce signal
				mCondVar.NotifyOne();
			}
		}

		nk_bool NkEvent::Wait(nk_int32 timeoutMs) noexcept {
			NkScopedLockMutex lock(mMutex);
			++mWaiters;
			const nk_uint64 generation = mPulseGeneration;
			if (timeoutMs < 0) {
				while (!mSignaled && mPulseGeneration == generation)
					mCondVar.Wait(lock);
			} else {
				const nk_uint64 deadline = NkConditionVariable::GetMonotonicTimeMs() + (nk_uint64)timeoutMs;
				while (!mSignaled && mPulseGeneration == generation) {
					if (!mCondVar.WaitUntil(lock, deadline))
						break; // échéance : l'état est relu ci-dessous, pas supposé
				}
			}
			--mWaiters;
			const nk_bool pulsed = (mPulseGeneration != generation);
			if (mSignaled) {
				if (!mManualReset)
					mSignaled = false; // automatique : ce réveil consomme le signal
				return true;
			}
			return pulsed;
		}

		nk_bool NkEvent::IsSignaled() const noexcept {
			NkScopedLockMutex lock(mMutex);
			return mSignaled;
		}

	} // namespace threading
} // namespace nkentseu
