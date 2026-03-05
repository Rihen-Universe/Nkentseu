// ============================================================
// FILE: NkBarrier.h
// DESCRIPTION: Barrier synchronization primitive
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Synchronise N threads sur un point de rendez-vous.
// Tous les threads qui appellent Wait() attendent jusqu'à
// ce que N threads aient appelé Wait().
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_SYNCHRONIZATION_NKBARRIER_H_INCLUDED
#define NKENTSEU_THREADING_SYNCHRONIZATION_NKBARRIER_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkConditionVariable.h"

namespace nkentseu {
namespace threading {

    /**
     * @brief Barrier synchronization primitive
     * 
     * Force N threads à attendre jusqu'à ce que tous
     * aient atteint le point de synchronisation.
     * 
     * Usage:
     * ```cpp
     * NkBarrier barrier(4);  // 4 threads
     * 
     * // Thread 1-4
     * DoWork();
     * barrier.Wait();  // All wait here until 4 threads arrive
     * ContinueWork();
     * ```
     */
    class NkBarrier {
    public:
        /**
         * @brief Constructeur
         * @param numThreads Nombre de threads à synchroniser
         */
        explicit NkBarrier(nk_uint32 numThreads) noexcept
            : mNumThreads(numThreads)
            , mCurrentPhase(0)
            , mCount(0)
        {
            NKENTSEU_ASSERT(numThreads > 0);
        }
        
        NkBarrier(const NkBarrier&) = delete;
        NkBarrier& operator=(const NkBarrier&) = delete;
        
        /**
         * @brief Attend que tous les threads arrivent au barrier
         * 
         * Retourne true pour le dernier thread qui arrive,
         * false pour les autres. Cela permet une action spéciale
         * après la synchronisation.
         */
        nk_bool Wait() noexcept {
            NkScopedLock lock(mMutex);
            
            nk_uint32 phase = mCurrentPhase;
            mCount++;
            
            if (mCount >= mNumThreads) {
                // Dernier thread
                mCount = 0;
                mCurrentPhase++;
                mCondVar.NotifyAll();
                return true;
            }
            
            // Attendre les autres
            while (mCurrentPhase == phase) {
                mCondVar.Wait(lock);
            }
            
            return false;
        }
        
        /**
         * @brief Reset le barrier pour une nouvelle itération
         */
        void Reset() noexcept {
            NkScopedLock lock(mMutex);
            mCount = 0;
            mCurrentPhase = 0;
        }
        
    private:
        nk_uint32 mNumThreads;
        nk_uint32 mCurrentPhase;
        nk_uint32 mCount;
        mutable NkMutex mMutex;
        mutable NkConditionVariable mCondVar;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_SYNCHRONIZATION_NKBARRIER_H_INCLUDED
