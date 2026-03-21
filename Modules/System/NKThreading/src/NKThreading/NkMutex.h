// ============================================================
// FILE: NkMutex.h
// DESCRIPTION: Standard OS mutex (production-ready)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.0.0
// ============================================================
// Mutex standard supportant:
// - Lock/Unlock/TryLock
// - Scoped locking (RAII)
// - Cross-platform (Windows/Linux/macOS)
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKMUTEX_H_INCLUDED
#define NKENTSEU_THREADING_NKMUTEX_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKThreading/NkThreadingExport.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <errno.h>
    #include <pthread.h>
    #include <sched.h>
    #include <time.h>
#endif

namespace nkentseu {
namespace threading {

    /**
     * @brief Standard OS mutex (pas re-entrant)
     * 
     * Utilise les primitives natives du système d'exploitation
     * pour une synchronisation robuste entre threads.
     * 
     * **Utiliser pour:** I/O, opérations longues, ressources partagées
     * **Ne pas utiliser pour:** Sections très courtes (préférer SpinLock)
     * 
     * @example
     * NkMutex mutex;
     * {
     *     NkScopedLock guard(mutex);
     *     // Section critique
     * }
     */
    class NKTHREADING_API NkMutex {
    public:
        // ==============================================
        // CONSTRUCTEUR/DESTRUCTEUR
        // ==============================================
        
        NkMutex() noexcept
            : mInitialized(true) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            InitializeSRWLock(&mMutex);
#else
            mInitialized = (pthread_mutex_init(&mMutex, nullptr) == 0);
#endif
        }
        
        ~NkMutex() {
#if !defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mInitialized) {
                pthread_mutex_destroy(&mMutex);
            }
#endif
        }
        
        // Non-copiable, non-movable
        NkMutex(const NkMutex&) = delete;
        NkMutex& operator=(const NkMutex&) = delete;
        
        // ==============================================
        // API LOCK/UNLOCK
        // ==============================================
        
        /**
         * @brief Verrouille le mutex (bloquant)
         */
        void Lock() noexcept {
            if (!mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            AcquireSRWLockExclusive(&mMutex);
#else
            (void)pthread_mutex_lock(&mMutex);
#endif
        }
        
        /**
         * @brief Essaie de verrouiller sans bloquer
         * @return true si verrouillé, false sinon
         */
        [[nodiscard]] nk_bool TryLock() noexcept {
            if (!mInitialized) {
                return false;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            return TryAcquireSRWLockExclusive(&mMutex) ? true : false;
#else
            return pthread_mutex_trylock(&mMutex) == 0;
#endif
        }
        
        /**
         * @brief Déverrouille le mutex
         */
        void Unlock() noexcept {
            if (!mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            ReleaseSRWLockExclusive(&mMutex);
#else
            (void)pthread_mutex_unlock(&mMutex);
#endif
        }
        
        /**
         * @brief Essaie de verrouiller avec timeout
         * @param milliseconds Temps d'attente maximum
         * @return true si verrouillé, false si timeout
         */
        [[nodiscard]] nk_bool TryLockFor(nk_uint32 milliseconds) noexcept {
            if (!mInitialized) {
                return false;
            }
            if (milliseconds == 0U) {
                return TryLock();
            }
            const nk_uint64 deadline = GetMonotonicTimeMs() + static_cast<nk_uint64>(milliseconds);
            do {
                if (TryLock()) {
                    return true;
                }
                YieldThread();
            } while (GetMonotonicTimeMs() < deadline);

            return false;
        }
        
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            SRWLOCK& Get() {
        #else
            pthread_mutex_t& Get() {
        #endif
            return mMutex;
        }

    private:
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

        static void YieldThread() noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            Sleep(0);
#else
            sched_yield();
#endif
        }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        SRWLOCK mMutex;
#else
        pthread_mutex_t mMutex;
#endif
        nk_bool mInitialized;
        
        // Pour l'accès au mutex natif dans les condition variables
        friend class NkConditionVariable;
    };
    
    // =======================================================
    // RECURSIVE MUTEX
    // =======================================================
    
    /**
     * @brief Recursive mutex (peut être verrouillé plusieurs fois par le même thread)
     * 
     * À utiliser seulement si vraiment nécessaire car:
     * - Plus lent qu'un mutex normal
     * - Moins prévisible pour les performances
     */
    class NKTHREADING_API NkRecursiveMutex {
    public:
        NkRecursiveMutex() noexcept
            : mInitialized(true) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            InitializeCriticalSection(&mMutex);
#else
            pthread_mutexattr_t attr{};
            if (pthread_mutexattr_init(&attr) != 0) {
                mInitialized = false;
                return;
            }

            #if defined(PTHREAD_MUTEX_RECURSIVE)
                const int recursiveType = PTHREAD_MUTEX_RECURSIVE;
            #elif defined(PTHREAD_MUTEX_RECURSIVE_NP)
                const int recursiveType = PTHREAD_MUTEX_RECURSIVE_NP;
            #else
                const int recursiveType = PTHREAD_MUTEX_NORMAL;
            #endif

            if (pthread_mutexattr_settype(&attr, recursiveType) != 0) {
                (void)pthread_mutexattr_destroy(&attr);
                mInitialized = false;
                return;
            }

            mInitialized = (pthread_mutex_init(&mMutex, &attr) == 0);
            (void)pthread_mutexattr_destroy(&attr);
#endif
        }
        
        ~NkRecursiveMutex() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mInitialized) {
                DeleteCriticalSection(&mMutex);
            }
#else
            if (mInitialized) {
                pthread_mutex_destroy(&mMutex);
            }
#endif
        }
        
        NkRecursiveMutex(const NkRecursiveMutex&) = delete;
        NkRecursiveMutex& operator=(const NkRecursiveMutex&) = delete;
        
        void Lock() noexcept {
            if (!mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            EnterCriticalSection(&mMutex);
#else
            (void)pthread_mutex_lock(&mMutex);
#endif
        }
        
        [[nodiscard]] nk_bool TryLock() noexcept {
            if (!mInitialized) {
                return false;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            return TryEnterCriticalSection(&mMutex) ? true : false;
#else
            return pthread_mutex_trylock(&mMutex) == 0;
#endif
        }
        
        void Unlock() noexcept {
            if (!mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            LeaveCriticalSection(&mMutex);
#else
            (void)pthread_mutex_unlock(&mMutex);
#endif
        }
        
    private:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        CRITICAL_SECTION mMutex;
#else
        pthread_mutex_t mMutex;
#endif
        nk_bool mInitialized;
    };
    
    // =======================================================
    // SCOPED LOCK PATTERN (RAII)
    // =======================================================
    
    /**
     * @brief Scoped mutex lock (RAII pattern)
     */
    class NKTHREADING_API NkScopedLock {
    public:
        explicit NkScopedLock(NkMutex& mutex) noexcept 
            : mMutex(&mutex) {
            mMutex->Lock();
        }
        
        ~NkScopedLock() {
            if (mMutex) {
                mMutex->Unlock();
            }
        }
        
        // Movable
        NkScopedLock(NkScopedLock&& other) noexcept 
            : mMutex(other.mMutex) {
            other.mMutex = nullptr;
        }
        
        NkScopedLock& operator=(NkScopedLock&& other) noexcept {
            if (this != &other) {
                if (mMutex) {
                    mMutex->Unlock();
                }
                mMutex = other.mMutex;
                other.mMutex = nullptr;
            }
            return *this;
        }
        
        // Non-copiable
        NkScopedLock(const NkScopedLock&) = delete;
        NkScopedLock& operator=(const NkScopedLock&) = delete;
        
        void Unlock() noexcept {
            if (mMutex) {
                mMutex->Unlock();
                mMutex = nullptr;
            }
        }

        [[nodiscard]] NkMutex* GetMutex() noexcept {
            return mMutex;
        }

        [[nodiscard]] const NkMutex* GetMutex() const noexcept {
            return mMutex;
        }
        
    private:
        NkMutex* mMutex;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKMUTEX_H_INCLUDED
