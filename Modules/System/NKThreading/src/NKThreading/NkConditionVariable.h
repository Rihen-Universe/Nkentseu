// ============================================================
// FILE: NkConditionVariable.h
// DESCRIPTION: Condition variable for thread synchronization
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.0.0
// ============================================================
// Primitive de synchronisation permettant aux threads
// d'attendre des conditions spécifiques.
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKCONDITIONVARIABLE_H_INCLUDED
#define NKENTSEU_THREADING_NKCONDITIONVARIABLE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThreadingExport.h"

#include <condition_variable>
#include <chrono>
#include <type_traits>

namespace nkentseu {
namespace threading {

    /**
     * @brief Condition variable pour synchronisation entre threads
     * 
     * Permet à un thread d'attendre qu'une condition soit satisfaite
     * avant de continuer l'exécution.
     * 
     * @pattern
     * ```cpp
     * NkMutex mutex;
     * NkConditionVariable cv;
     * bool ready = false;
     * 
    * // Thread 1: Signal la condition
     * {
     *     NkScopedLock lock(mutex);
     *     ready = true;
     * }
     * cv.NotifyOne();
     * 
     * // Thread 2: Attend la condition
     * {
     *     NkScopedLock lock(mutex);
     *     while (!ready) {
     *         cv.Wait(lock);
     *     }
     * }
     * ```
     */
    class NKTHREADING_API NkConditionVariable {
    public:
        // ==============================================
        // CONSTRUCTEUR/DESTRUCTEUR
        // ==============================================
        
        NkConditionVariable() noexcept = default;
        ~NkConditionVariable() = default;
        
        // Non-copiable, non-movable
        NkConditionVariable(const NkConditionVariable&) = delete;
        NkConditionVariable& operator=(const NkConditionVariable&) = delete;
        
        // ==============================================
        // API WAIT/NOTIFY
        // ==============================================
        
        /**
         * @brief Attend la notification sur la condition
         * 
         * Le lock est automatiquement relâché pendant l'attente
         * et re-acquis à la notification.
         */
        void Wait(NkScopedLock& lock) noexcept {
            NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr) {
                return;
            }

            std::unique_lock<std::mutex> nativeLock(mutex->mMutex, std::adopt_lock);
            mCondVar.wait(nativeLock);
            nativeLock.release();
        }
        
        /**
         * @brief Attend avec timeout
         * @param lock Scoped lock sur le mutex
         * @param milliseconds Temps d'attente max
         * @return true si notifiée, false si timeout
         */
        [[nodiscard]] nk_bool WaitFor(NkScopedLock& lock,
                                      nk_uint32 milliseconds) noexcept {
            NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr) {
                return false;
            }

            std::unique_lock<std::mutex> nativeLock(mutex->mMutex, std::adopt_lock);
            const std::cv_status status = mCondVar.wait_for(
                nativeLock,
                std::chrono::milliseconds(milliseconds)
            );
            nativeLock.release();
            return status != std::cv_status::timeout;
        }
        
        /**
         * @brief Attend avec prédicat (re-check automatique)
         * @param lock Scoped lock
         * @param predicate Fonction retournant true si condition satisfaite
         */
        template<typename Predicate,
                 typename = std::enable_if_t<std::is_invocable_r_v<bool, Predicate&>>>
        void WaitUntil(NkScopedLock& lock, Predicate&& predicate) noexcept {
            while (!predicate()) {
                Wait(lock);
            }
        }

        /**
         * @brief Attend jusqu'à une échéance absolue
         * @param lock Scoped lock
         * @param deadline Time point d'expiration
         * @return true si notifiée, false si timeout
         */
        template<typename Clock, typename Duration>
        [[nodiscard]] nk_bool WaitUntil(
            NkScopedLock& lock,
            const std::chrono::time_point<Clock, Duration>& deadline
        ) noexcept {
            NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr) {
                return false;
            }

            std::unique_lock<std::mutex> nativeLock(mutex->mMutex, std::adopt_lock);
            const std::cv_status status = mCondVar.wait_until(nativeLock, deadline);
            nativeLock.release();
            return status != std::cv_status::timeout;
        }

        /**
         * @brief Attend jusqu'à une échéance avec prédicat
         * @param lock Scoped lock
         * @param deadline Time point d'expiration
         * @param predicate Condition de sortie
         * @return true si prédicat satisfait, false si timeout
         */
        template<typename Clock, typename Duration, typename Predicate>
        [[nodiscard]] nk_bool WaitUntil(
            NkScopedLock& lock,
            const std::chrono::time_point<Clock, Duration>& deadline,
            Predicate&& predicate
        ) noexcept {
            while (!predicate()) {
                if (!WaitUntil(lock, deadline)) {
                    return false;
                }
            }
            return true;
        }
        
        /**
         * @brief Notifie UN thread en attente
         */
        void NotifyOne() noexcept {
            mCondVar.notify_one();
        }
        
        /**
         * @brief Notifie TOUS les threads en attente
         */
        void NotifyAll() noexcept {
            mCondVar.notify_all();
        }
        
    private:
        std::condition_variable mCondVar;
        
        friend class NkScopedLock;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKCONDITIONVARIABLE_H_INCLUDED
