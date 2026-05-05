//
// NkMutex.cpp
// =============================================================================
// Description :
//   Implémentation des méthodes non-inline des primitives de mutex.
//   Ce fichier contient les définitions séparées pour :
//   - Réduire les dépendances de compilation (inclusions système isolées)
//   - Minimiser la taille des objets compilés utilisant uniquement le header
//   - Permettre une compilation incrémentale optimisée
//
// Note architecturale :
//   Les méthodes de NkScopedLockMutex sont inline dans le header car :
//   - Elles sont critiques pour la performance (appelées fréquemment)
//   - Leur implémentation est triviale (délégation directe à NkMutex)
//   - L'inlining permet au compilateur d'optimiser l'acquisition/libération
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

// =====================================================================
// Inclusion de l'en-tête de déclaration
// =====================================================================
#include "NKThreading/NkMutex.h"

// =====================================================================
// Inclusions système conditionnelles (isolées dans le .cpp)
// =====================================================================
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

// =====================================================================
// Namespace : nkentseu::threading
// =====================================================================
namespace nkentseu {
    namespace threading {

        // =================================================================
        // IMPLÉMENTATION : NkMutex
        // =================================================================

        // -----------------------------------------------------------------
        // Constructeur : initialisation du mutex natif
        // -----------------------------------------------------------------
        NkMutex::NkMutex() noexcept
            : mInitialized(true)
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                InitializeSRWLock(&mMutex);
            #else
                const int result = pthread_mutex_init(&mMutex, nullptr);
                mInitialized = (result == 0);
            #endif
        }

        // -----------------------------------------------------------------
        // Destructeur : libération des ressources système
        // -----------------------------------------------------------------
        NkMutex::~NkMutex() noexcept
        {
            #if !defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mInitialized) {
                    (void)pthread_mutex_destroy(&mMutex);
                }
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : Lock (verrouillage bloquant)
        // -----------------------------------------------------------------
        void NkMutex::Lock() noexcept
        {
            if (!mInitialized) {
                return;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                AcquireSRWLockExclusive(&mMutex);
            #else
                (void)pthread_mutex_lock(&mMutex);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : TryLock (tentative non-bloquante)
        // -----------------------------------------------------------------
        nk_bool NkMutex::TryLock() noexcept
        {
            if (!mInitialized) {
                return false;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return (TryAcquireSRWLockExclusive(&mMutex) != FALSE);
            #else
                return (pthread_mutex_trylock(&mMutex) == 0);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : Unlock (déverrouillage)
        // -----------------------------------------------------------------
        void NkMutex::Unlock() noexcept
        {
            if (!mInitialized) {
                return;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                ReleaseSRWLockExclusive(&mMutex);
            #else
                (void)pthread_mutex_unlock(&mMutex);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : TryLockFor (tentative avec timeout)
        // -----------------------------------------------------------------
        nk_bool NkMutex::TryLockFor(nk_uint32 milliseconds) noexcept
        {
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

        // -----------------------------------------------------------------
        // Méthode : GetNativeHandle (accès au handle natif)
        // -----------------------------------------------------------------
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            SRWLOCK& NkMutex::GetNativeHandle() noexcept
            {
                return mMutex;
            }
        #else
            pthread_mutex_t& NkMutex::GetNativeHandle() noexcept
            {
                return mMutex;
            }
        #endif

        // -----------------------------------------------------------------
        // Méthode privée : GetMonotonicTimeMs
        // -----------------------------------------------------------------
        nk_uint64 NkMutex::GetMonotonicTimeMs() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return static_cast<nk_uint64>(GetTickCount64());
            #else
                timespec ts{};
                if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
                    return 0U;
                }
                return static_cast<nk_uint64>(ts.tv_sec) * 1000ULL +
                       static_cast<nk_uint64>(ts.tv_nsec / 1000000ULL);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode privée : YieldThread
        // -----------------------------------------------------------------
        void NkMutex::YieldThread() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                Sleep(0);
            #else
                (void)sched_yield();
            #endif
        }

        // =================================================================
        // IMPLÉMENTATION : NkRecursiveMutex
        // =================================================================

        // -----------------------------------------------------------------
        // Constructeur : initialisation du mutex réentrant natif
        // -----------------------------------------------------------------
        NkRecursiveMutex::NkRecursiveMutex() noexcept
            : mInitialized(true)
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                InitializeCriticalSection(&mMutex);
            #else
                pthread_mutexattr_t attr{};

                if (pthread_mutexattr_init(&attr) != 0) {
                    mInitialized = false;
                    return;
                }

                #if defined(PTHREAD_MUTEX_RECURSIVE)
                    constexpr int recursiveType = PTHREAD_MUTEX_RECURSIVE;
                #elif defined(PTHREAD_MUTEX_RECURSIVE_NP)
                    constexpr int recursiveType = PTHREAD_MUTEX_RECURSIVE_NP;
                #else
                    constexpr int recursiveType = PTHREAD_MUTEX_NORMAL;
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

        // -----------------------------------------------------------------
        // Destructeur : libération des ressources du mutex réentrant
        // -----------------------------------------------------------------
        NkRecursiveMutex::~NkRecursiveMutex() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mInitialized) {
                    DeleteCriticalSection(&mMutex);
                }
            #else
                if (mInitialized) {
                    (void)pthread_mutex_destroy(&mMutex);
                }
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : Lock (verrouillage réentrant)
        // -----------------------------------------------------------------
        void NkRecursiveMutex::Lock() noexcept
        {
            if (!mInitialized) {
                return;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                EnterCriticalSection(&mMutex);
            #else
                (void)pthread_mutex_lock(&mMutex);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : TryLock (tentative non-bloquante réentrante)
        // -----------------------------------------------------------------
        nk_bool NkRecursiveMutex::TryLock() noexcept
        {
            if (!mInitialized) {
                return false;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return (TryEnterCriticalSection(&mMutex) != FALSE);
            #else
                return (pthread_mutex_trylock(&mMutex) == 0);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : Unlock (déverrouillage réentrant)
        // -----------------------------------------------------------------
        void NkRecursiveMutex::Unlock() noexcept
        {
            if (!mInitialized) {
                return;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                LeaveCriticalSection(&mMutex);
            #else
                (void)pthread_mutex_unlock(&mMutex);
            #endif
        }

        // =================================================================
        // Note : NkScopedLockMutex implémentations inline dans le header
        // =================================================================
        // Les méthodes de NkScopedLockMutex sont définies inline dans NkMutex.h
        // pour les raisons suivantes :
        //
        // 1. Performance critique : acquisition/libération de lock est dans
        //    le chemin critique de nombreuses opérations concurrentes.
        //
        // 2. Implémentation triviale : délégation directe à NkMutex::Lock/Unlock,
        //    pas de logique complexe justifiant une séparation.
        //
        // 3. Optimisation compilateur : l'inlining permet l'élimination des
        //    appels de fonction et l'optimisation aggressive du code généré.

    }
}