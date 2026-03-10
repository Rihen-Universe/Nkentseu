// ============================================================
// FILE: NkConditionVariable.h
// DESCRIPTION: Condition variable for thread synchronization
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.1.0
// ============================================================
// Primitive de synchronisation permettant aux threads
// d'attendre des conditions spécifiques.
// Implémentation native OS (Win32 / pthread).
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKCONDITIONVARIABLE_H_INCLUDED
#define NKENTSEU_THREADING_NKCONDITIONVARIABLE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThreadingExport.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <errno.h>
    #include <pthread.h>
    #include <time.h>
#endif

namespace nkentseu {
namespace threading {

    /**
     * @brief Condition variable pour synchronisation entre threads.
     */
    class NKTHREADING_API NkConditionVariable {
    public:
        NkConditionVariable() noexcept
            : mInitialized(true) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            InitializeConditionVariable(&mCondVar);
#else
            mInitialized = (pthread_cond_init(&mCondVar, nullptr) == 0);
#endif
        }

        ~NkConditionVariable() {
#if !defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mInitialized) {
                (void)pthread_cond_destroy(&mCondVar);
            }
#endif
        }

        NkConditionVariable(const NkConditionVariable&) = delete;
        NkConditionVariable& operator=(const NkConditionVariable&) = delete;

        /**
         * @brief Attend une notification (sans timeout).
         */
        void Wait(NkScopedLock& lock) noexcept {
            NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr || !mutex->mInitialized || !mInitialized) {
                return;
            }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
            (void)SleepConditionVariableSRW(&mCondVar, &mutex->mMutex, INFINITE, 0);
#else
            (void)pthread_cond_wait(&mCondVar, &mutex->mMutex);
#endif
        }

        /**
         * @brief Attend avec timeout relatif en millisecondes.
         * @return true si réveillé, false si timeout/erreur.
         */
        [[nodiscard]] nk_bool WaitFor(NkScopedLock& lock, nk_uint32 milliseconds) noexcept {
            NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr || !mutex->mInitialized || !mInitialized) {
                return false;
            }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
            const BOOL woke = SleepConditionVariableSRW(&mCondVar, &mutex->mMutex, milliseconds, 0);
            if (woke) {
                return true;
            }
            return GetLastError() != ERROR_TIMEOUT;
#else
            const timespec absDeadline = MakeAbsoluteTimeout(milliseconds);
            int result = 0;
            do {
                result = pthread_cond_timedwait(&mCondVar, &mutex->mMutex, &absDeadline);
            } while (result == EINTR);
            return result == 0;
#endif
        }

        /**
         * @brief Attend jusqu'à ce que le prédicat soit vrai.
         */
        template<typename Predicate>
        void WaitUntil(NkScopedLock& lock, Predicate&& predicate) noexcept {
            while (!predicate()) {
                Wait(lock);
            }
        }

        /**
         * @brief Attend jusqu'à une échéance monotonic en ms.
         * @return true si réveillé avant deadline, false sinon.
         */
        [[nodiscard]] nk_bool WaitUntil(NkScopedLock& lock, nk_uint64 deadlineMs) noexcept {
            nk_uint64 now = GetMonotonicTimeMs();
            while (now < deadlineMs) {
                nk_uint64 remaining = deadlineMs - now;
                nk_uint32 waitMs = (remaining > 0xFFFFFFFFULL)
                                     ? 0xFFFFFFFFu
                                     : static_cast<nk_uint32>(remaining);
                if (waitMs == 0) {
                    waitMs = 1;
                }

                if (WaitFor(lock, waitMs)) {
                    return true;
                }

                now = GetMonotonicTimeMs();
            }
            return false;
        }

        /**
         * @brief Attend jusqu'à deadline/predicate.
         */
        template<typename Predicate>
        [[nodiscard]] nk_bool WaitUntil(
            NkScopedLock& lock,
            nk_uint64 deadlineMs,
            Predicate&& predicate) noexcept {
            while (!predicate()) {
                if (!WaitUntil(lock, deadlineMs)) {
                    return false;
                }
            }
            return true;
        }

        /**
         * @brief Notifie un thread.
         */
        void NotifyOne() noexcept {
            if (!mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            WakeConditionVariable(&mCondVar);
#else
            (void)pthread_cond_signal(&mCondVar);
#endif
        }

        /**
         * @brief Notifie tous les threads.
         */
        void NotifyAll() noexcept {
            if (!mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            WakeAllConditionVariable(&mCondVar);
#else
            (void)pthread_cond_broadcast(&mCondVar);
#endif
        }

        /**
         * @brief Horloge monotonic en millisecondes.
         */
        static nk_uint64 GetMonotonicTimeMs() noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            return static_cast<nk_uint64>(GetTickCount64());
#else
            timespec ts{};
            if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
                return 0;
            }
            return static_cast<nk_uint64>(ts.tv_sec) * 1000ULL +
                   static_cast<nk_uint64>(ts.tv_nsec / 1000000ULL);
#endif
        }

    private:
#if !defined(NKENTSEU_PLATFORM_WINDOWS)
        static timespec MakeAbsoluteTimeout(nk_uint32 milliseconds) noexcept {
            timespec deadline{};
            if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
                deadline.tv_sec = 0;
                deadline.tv_nsec = 0;
                return deadline;
            }

            deadline.tv_sec += static_cast<time_t>(milliseconds / 1000U);
            deadline.tv_nsec += static_cast<long>(milliseconds % 1000U) * 1000000L;
            if (deadline.tv_nsec >= 1000000000L) {
                deadline.tv_sec += 1;
                deadline.tv_nsec -= 1000000000L;
            }
            return deadline;
        }
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        CONDITION_VARIABLE mCondVar;
#else
        pthread_cond_t mCondVar;
#endif
        nk_bool mInitialized;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKCONDITIONVARIABLE_H_INCLUDED
