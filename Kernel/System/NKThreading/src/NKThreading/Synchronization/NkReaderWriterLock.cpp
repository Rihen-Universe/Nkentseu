// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// ============================================================
// FILE: NkReaderWriterLock.cpp
// DESCRIPTION: Verrou lecteurs/écrivain, préférence à l'écrivain.
//
// Écrit le 2026-09-04 : la QUATRIÈME primitive de Synchronization/ déclarée,
// exportée et jamais définie (0 symbole dans NKThreading.lib) — découverte
// en reliant la suite après avoir écrit les trois premières. Déclarer
// n'est pas livrer.
//
// Sémantique : N lecteurs simultanés OU un écrivain. Préférence à
// l'écrivain : dès qu'un écrivain attend, les nouveaux lecteurs patientent
// (pas de famine des écrivains) ; les lecteurs déjà entrés finissent.
// TryLock* n'attend jamais. NkReadLock / NkWriteLock : gardes RAII.
// Bâti sur NkMutex + deux NkConditionVariable, sans STL.
// ============================================================
#include "NKThreading/Synchronization/NkReaderWriterLock.h"

namespace nkentseu {
	namespace threading {

		NkReaderWriterLock::NkReaderWriterLock() noexcept
			: mReaders(0u), mWriters(0u), mWritersWaiting(0u), mMutex(), mReadCondVar(), mWriteCondVar() {
		}

		void NkReaderWriterLock::LockRead() noexcept {
			NkScopedLockMutex lock(mMutex);
			while (mWriters > 0u || mWritersWaiting > 0u)
				mReadCondVar.Wait(lock);
			++mReaders;
		}

		nk_bool NkReaderWriterLock::TryLockRead() noexcept {
			NkScopedLockMutex lock(mMutex);
			if (mWriters > 0u || mWritersWaiting > 0u)
				return false;
			++mReaders;
			return true;
		}

		void NkReaderWriterLock::UnlockRead() noexcept {
			NkScopedLockMutex lock(mMutex);
			if (mReaders == 0u)
				return; // déverrouillage sans verrou : on ne casse pas l'invariant
			--mReaders;
			if (mReaders == 0u && mWritersWaiting > 0u)
				mWriteCondVar.NotifyOne(); // le dernier lecteur sorti réveille UN écrivain
		}

		void NkReaderWriterLock::LockWrite() noexcept {
			NkScopedLockMutex lock(mMutex);
			++mWritersWaiting;
			while (mReaders > 0u || mWriters > 0u)
				mWriteCondVar.Wait(lock);
			--mWritersWaiting;
			mWriters = 1u;
		}

		nk_bool NkReaderWriterLock::TryLockWrite() noexcept {
			NkScopedLockMutex lock(mMutex);
			if (mReaders > 0u || mWriters > 0u)
				return false;
			mWriters = 1u;
			return true;
		}

		void NkReaderWriterLock::UnlockWrite() noexcept {
			NkScopedLockMutex lock(mMutex);
			if (mWriters == 0u)
				return;
			mWriters = 0u;
			if (mWritersWaiting > 0u)
				mWriteCondVar.NotifyOne(); // préférence à l'écrivain suivant…
			else
				mReadCondVar.NotifyAll(); // …sinon tous les lecteurs entrent
		}

		NkReadLock::NkReadLock(NkReaderWriterLock &lock) noexcept : mLock(lock) {
			mLock.LockRead();
		}

		NkReadLock::~NkReadLock() noexcept {
			mLock.UnlockRead();
		}

		NkWriteLock::NkWriteLock(NkReaderWriterLock &lock) noexcept : mLock(lock) {
			mLock.LockWrite();
		}

		NkWriteLock::~NkWriteLock() noexcept {
			mLock.UnlockWrite();
		}

	} // namespace threading
} // namespace nkentseu
