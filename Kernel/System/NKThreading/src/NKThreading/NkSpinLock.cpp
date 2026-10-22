// ============================================================
// FILE: NkSpinLock.cpp
// DESCRIPTION: Implementation (mostly inline in header)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.0.0
// ============================================================

#include "NKThreading/NkSpinLock.h"

namespace nkentseu {
	namespace memory {
		// NkSpinLock: Implémentation entièrement dans le header (inline)
		// Pas de code supplémentaire nécessaire
		//
		// (22/10/2026) FAUX : LockSlow() etait DECLARE (NkSpinLock.h) et appele par Lock() des la premiere
		// contention, mais jamais DEFINI nulle part. Tout programme qui verrouillait un memory::NkSpinLock
		// echouait a l'edition de liens (« undefined reference to NkSpinLock::LockSlow() » ; releve par le
		// banc de l'en-tete parapluie NKThreading.h, et par l'essai des DLL du 21/10). La voici, telle que
		// l'en-tete la decrit : attendre en LECTURE (peu de trafic de cache) avec l'instruction PAUSE, puis
		// ceder le processeur au systeme apres 32 tours, et retenter l'echange atomique quand le verrou
		// parait libre. Banc : NKThreading/tests/test_entete_parapluie.cpp (P3, P5).
		void NkSpinLock::LockSlow() noexcept {
			nk_uint32 tours = 0;
			for (;;) {
				while (mLocked.Load(NkMemoryOrder::NK_RELAXED)) {
					if (++tours < 32u) {
						PauseInstruction();
					} else {
						ThreadYield();
						tours = 0;
					}
				}
				if (!mLocked.Exchange(true, NkMemoryOrder::NK_ACQUIRE)) {
					return;
				}
			}
		}
	} // namespace memory
} // namespace nkentseu
