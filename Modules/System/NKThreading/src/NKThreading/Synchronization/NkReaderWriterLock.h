// ============================================================
// FILE: NkReaderWriterLock.h
// DESCRIPTION: Reader-Writer lock synchronization
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Reader-Writer lock optimisé pour les scénarios où il y a
// beaucoup plus de lectures que d'écritures. Plusieurs
// lecteurs peuvent acquérir le verrou simultanément, mais
// un seul écrivain peut l'avoir, exclusivement.
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_SYNCHRONIZATION_NKREADERWRITERLOCK_H_INCLUDED
#define NKENTSEU_THREADING_SYNCHRONIZATION_NKREADERWRITERLOCK_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkConditionVariable.h"

namespace nkentseu {
namespace threading {

    /**
     * @brief Reader-Writer lock synchronization
     * 
     * Permet plusieurs lecteurs simultanés, mais exclusion
     * mutuelle pour les écrivains. Optimal pour patterns
     * reader-heavy (ex: cache, config system).
     * 
     * Usage:
     * ```cpp
     * NkReaderWriterLock rwlock;
     * 
     * // Reader thread
     * {
     *     NkReadLock lock(rwlock);  // Plusieurs readers OK
     *     ReadData();
     * }
     * 
     * // Writer thread
     * {
     *     NkWriteLock lock(rwlock);  // Exclusif
     *     WriteData();
     * }
     * ```
     */
    class NkReaderWriterLock {
    public:
        NkReaderWriterLock() noexcept
            : mReaders(0)
            , mWriters(0)
            , mWritersWaiting(0)
        {
        }
        
        NkReaderWriterLock(const NkReaderWriterLock&) = delete;
        NkReaderWriterLock& operator=(const NkReaderWriterLock&) = delete;
        
        // == Reader lock interface ==
        
        /**
         * @brief Acquiert le verrou en lecture
         * 
         * Plusieurs threads peuvent détenir le verrou
         * en lecture simultanément, à condition qu'aucun
         * writer n'attend.
         */
        void LockRead() noexcept {
            NkScopedLock lock(mMutex);
            
            // Attendre si un writer attend (writer starvation prevention)
            while (mWriters > 0 || mWritersWaiting > 0) {
                mReadCondVar.Wait(lock);
            }
            
            mReaders++;
        }
        
        /**
         * @brief Essaie d'acquérir le verrou en lecture
         */
        nk_bool TryLockRead() noexcept {
            NkScopedLock lock(mMutex);
            
            if (mWriters > 0 || mWritersWaiting > 0) {
                return false;
            }
            
            mReaders++;
            return true;
        }
        
        /**
         * @brief Libère le verrou en lecture
         */
        void UnlockRead() noexcept {
            NkScopedLock lock(mMutex);
            
            NKENTSEU_ASSERT(mReaders > 0);
            mReaders--;
            
            if (mReaders == 0) {
                mWriteCondVar.NotifyOne();  // Réveille un writer attendant
            }
        }
        
        // == Writer lock interface ==
        
        /**
         * @brief Acquiert le verrou en écriture
         * 
         * Exclusif - aucun autres reader/writer ne peut
         * détenir le verrou.
         */
        void LockWrite() noexcept {
            NkScopedLock lock(mMutex);
            
            mWritersWaiting++;
            
            while (mReaders > 0 || mWriters > 0) {
                mWriteCondVar.Wait(lock);
            }
            
            mWritersWaiting--;
            mWriters++;
        }
        
        /**
         * @brief Essaie d'acquérir le verrou en écriture
         */
        nk_bool TryLockWrite() noexcept {
            NkScopedLock lock(mMutex);
            
            if (mReaders > 0 || mWriters > 0 || mWritersWaiting > 0) {
                return false;
            }
            
            mWriters++;
            return true;
        }
        
        /**
         * @brief Libère le verrou en écriture
         */
        void UnlockWrite() noexcept {
            NkScopedLock lock(mMutex);
            
            NKENTSEU_ASSERT(mWriters > 0);
            mWriters--;
            
            // Réveille tous les readers attendant
            mReadCondVar.NotifyAll();
            
            // Ou un writer si des readers ne sont pas arrivés
            if (mWritersWaiting > 0) {
                mWriteCondVar.NotifyOne();
            }
        }
        
    private:
        nk_uint32 mReaders;        // Nombre de readers actifs
        nk_uint32 mWriters;        // Nombre de writers actifs (0 ou 1)
        nk_uint32 mWritersWaiting; // Nombre de writers attendant
        
        mutable NkMutex mMutex;
        mutable NkConditionVariable mReadCondVar;
        mutable NkConditionVariable mWriteCondVar;
    };
    
    // == RAII Lock guards ==
    
    /**
     * @brief RAII guard pour read lock
     */
    class NkReadLock {
    public:
        explicit NkReadLock(NkReaderWriterLock& lock) noexcept
            : mLock(lock)
        {
            mLock.LockRead();
        }
        
        ~NkReadLock() {
            mLock.UnlockRead();
        }
        
        NkReadLock(const NkReadLock&) = delete;
        NkReadLock& operator=(const NkReadLock&) = delete;
        
    private:
        NkReaderWriterLock& mLock;
    };
    
    /**
     * @brief RAII guard pour write lock
     */
    class NkWriteLock {
    public:
        explicit NkWriteLock(NkReaderWriterLock& lock) noexcept
            : mLock(lock)
        {
            mLock.LockWrite();
        }
        
        ~NkWriteLock() {
            mLock.UnlockWrite();
        }
        
        NkWriteLock(const NkWriteLock&) = delete;
        NkWriteLock& operator=(const NkWriteLock&) = delete;
        
    private:
        NkReaderWriterLock& mLock;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_SYNCHRONIZATION_NKREADERWRITERLOCK_H_INCLUDED
