// ============================================================
// FILE: NkLatch.h
// DESCRIPTION: CountDown latch synchronization primitive
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Primitive CountDown latch. N threads peuvent incrémenter
// un compteur et attendre qu'il atteigne zéro. Utile pour
// synchroniser l'initialisation de N tâches avant de
// commencer le travail principal.
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_SYNCHRONIZATION_NKLATCH_H_INCLUDED
#define NKENTSEU_THREADING_SYNCHRONIZATION_NKLATCH_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkConditionVariable.h"

namespace nkentseu {
namespace threading {

    /**
     * @brief CountDown latch synchronization primitive
     * 
     * Synchronise threads basé sur un compteur décrémençant.
     * Les threads peuvent appeler CountDown() pour signaler
     * qu'ils ont terminé, et d'autres peuvent Wait() sur le
     * compteur qui arrive à zéro.
     * 
     * Contrairement à Barrier, Latch est à usage unique - elle
     * ne peut pas être reseted.
     * 
     * Usage:
     * ```cpp
     * NkLatch latch(4);  // 4 tasks à compléter
     * 
     * // Task thread 1-4
     * DoTask();
     * latch.CountDown();  // Signale que la task est finie
     * 
     * // Main thread
     * latch.Wait();  // Attend que tous les CountDown() soient appelés
     * ContinueAfterAllTasksComplete();
     * ```
     */
    class NkLatch {
    public:
        /**
         * @brief Constructeur
         * @param initialCount Nombre initial de compte
         */
        explicit NkLatch(nk_uint32 initialCount) noexcept
            : mCount(initialCount)
        {
            NKENTSEU_ASSERT(initialCount > 0);
        }
        
        NkLatch(const NkLatch&) = delete;
        NkLatch& operator=(const NkLatch&) = delete;
        
        /**
         * @brief Décrémente le latch de 1
         * 
         * Si le compteur arrive à zéro, réveille tous les
         * threads attendant.
         */
        void CountDown(nk_uint32 count = 1) noexcept {
            if (count == 0) {
                return;
            }

            {
                NkScopedLock lock(mMutex);
                
                NKENTSEU_ASSERT(mCount >= count);
                if (count >= mCount) {
                    mCount = 0;
                } else {
                    mCount -= count;
                }
                
                if (mCount == 0) {
                    mCondVar.NotifyAll();
                }
            }
        }
        
        /**
         * @brief Attend que le latch arrive à zéro
         * @param timeoutMs Timeout en millisecondes (-1 = infini)
         * @return true si atteint zéro, false si timeout
         */
        nk_bool Wait(nk_int32 timeoutMs = -1) noexcept {
            NkScopedLock lock(mMutex);
            
            if (mCount == 0) {
                return true;
            }
            
            if (timeoutMs < 0) {
                // Infini
                while (mCount > 0) {
                    mCondVar.Wait(lock);
                }
                return true;
            } else {
                // Avec timeout
                const nk_uint64 deadline =
                    NkConditionVariable::GetMonotonicTimeMs() + static_cast<nk_uint64>(timeoutMs);
                while (mCount > 0) {
                    if (!mCondVar.WaitUntil(lock, deadline)) {
                        return false;  // Timeout
                    }
                }
                return true;
            }
        }
        
        /**
         * @brief Vérifie la valeur courante du compteur
         */
        nk_uint32 GetCount() const noexcept {
            NkScopedLock lock(mMutex);
            return mCount;
        }
        
        /**
         * @brief Vérifie si le latch est arrivé à zéro
         */
        nk_bool IsReady() const noexcept {
            NkScopedLock lock(mMutex);
            return mCount == 0;
        }
        
    private:
        nk_uint32 mCount;
        mutable NkMutex mMutex;
        mutable NkConditionVariable mCondVar;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_SYNCHRONIZATION_NKLATCH_H_INCLUDED
