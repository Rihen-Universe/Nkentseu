// ============================================================
// FILE: NkEvent.h
// DESCRIPTION: Win32-style Event synchronization primitive
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Primitive de synchronisation de style Win32 qui signale
// un événement. Peut être reseted manuellement ou
// automatiquement après que tous les threads attendant
// se réveillent.
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_SYNCHRONIZATION_NKEVENT_H_INCLUDED
#define NKENTSEU_THREADING_SYNCHRONIZATION_NKEVENT_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkConditionVariable.h"

namespace nkentseu {
namespace threading {

    /**
     * @brief Win32-style event synchronization
     * 
     * Primitive permettant à un thread de signaler un
     * événement et à d'autres d'attendre cet événement.
     * 
     * Peut être ManualReset (doit être remis à zéro manuellement)
     * ou AutoReset (se remet automatiquement après un signal).
     * 
     * Usage:
     * ```cpp
     * NkEvent event(false);  // AutoReset
     * 
     * // Thread 1
     * event.Wait();  // Attend l'événement
     * 
     * // Thread 2
     * event.Set();   // Signale l'événement, réveille tous les threads
     * ```
     */
    class NkEvent {
    public:
        /**
         * @brief Constructeur
         * @param manualReset Si true, doit être reseté manuellement
         * @param initialState État initial (signalé ou non)
         */
        explicit NkEvent(nk_bool manualReset = false, 
                        nk_bool initialState = false) noexcept
            : mManualReset(manualReset)
            , mSignaled(initialState)
        {
        }
        
        NkEvent(const NkEvent&) = delete;
        NkEvent& operator=(const NkEvent&) = delete;
        
        /**
         * @brief Signale l'événement (réveille tous les threads attendant)
         */
        void Set() noexcept {
            NkScopedLock lock(mMutex);
            mSignaled = true;
            mCondVar.NotifyAll();
        }
        
        /**
         * @brief Remet l'événement à zéro
         */
        void Reset() noexcept {
            NkScopedLock lock(mMutex);
            mSignaled = false;
        }
        
        /**
         * @brief Signale l'événement puis le remet à zéro
         * (pour AutoReset automatique)
         */
        void Pulse() noexcept {
            NkScopedLock lock(mMutex);
            mSignaled = true;
            mCondVar.NotifyAll();
            if (!mManualReset) {
                mSignaled = false;
            }
        }
        
        /**
         * @brief Attend l'événement
         * @param timeoutMs Timeout en millisecondes (-1 = infini)
         * @return true si l'événement est signalé, false si timeout
         */
        nk_bool Wait(nk_int32 timeoutMs = -1) noexcept {
            NkScopedLock lock(mMutex);
            
            if (mSignaled) {
                if (!mManualReset) {
                    mSignaled = false;
                }
                return true;
            }
            
            if (timeoutMs < 0) {
                // Infini
                while (!mSignaled) {
                    mCondVar.Wait(lock);
                }
            } else {
                // Avec timeout
                auto end = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs);
                while (!mSignaled) {
                    if (!mCondVar.WaitUntil(lock, end)) {
                        return false;  // Timeout
                    }
                }
            }
            
            if (!mManualReset) {
                mSignaled = false;
            }
            return true;
        }
        
        /**
         * @brief Vérifie si l'événement est signalé sans attendre
         */
        nk_bool IsSignaled() const noexcept {
            NkScopedLock lock(mMutex);
            return mSignaled;
        }
        
    private:
        nk_bool mManualReset;
        nk_bool mSignaled;
        mutable NkMutex mMutex;
        mutable NkConditionVariable mCondVar;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_SYNCHRONIZATION_NKEVENT_H_INCLUDED
