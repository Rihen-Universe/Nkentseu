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
#include "NKThreading/NkThreadingExport.h"

#include <mutex>
#include <thread>
#include <chrono>

namespace nkentseu {
namespace threading {

    /**
     * @brief Standard OS mutex (pas re-entrant)
     * 
     * Utilise la classe std::mutex du système d'exploitation
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
        
        NkMutex() noexcept = default;
        ~NkMutex() = default;
        
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
            mMutex.lock();
        }
        
        /**
         * @brief Essaie de verrouiller sans bloquer
         * @return true si verrouillé, false sinon
         */
        [[nodiscard]] nk_bool TryLock() noexcept {
            return mMutex.try_lock();
        }
        
        /**
         * @brief Déverrouille le mutex
         */
        void Unlock() noexcept {
            mMutex.unlock();
        }
        
        /**
         * @brief Essaie de verrouiller avec timeout
         * @param milliseconds Temps d'attente maximum
         * @return true si verrouillé, false si timeout
         */
        [[nodiscard]] nk_bool TryLockFor(nk_uint32 milliseconds) noexcept {
            const auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(milliseconds);

            while (std::chrono::steady_clock::now() < deadline) {
                if (mMutex.try_lock()) {
                    return true;
                }
                std::this_thread::yield();
            }
            return false;
        }
        
    private:
        std::mutex mMutex;
        
        // Pour l'accès au std::mutex dans les condition variables
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
        NkRecursiveMutex() noexcept = default;
        ~NkRecursiveMutex() = default;
        
        NkRecursiveMutex(const NkRecursiveMutex&) = delete;
        NkRecursiveMutex& operator=(const NkRecursiveMutex&) = delete;
        
        void Lock() noexcept {
            mMutex.lock();
        }
        
        [[nodiscard]] nk_bool TryLock() noexcept {
            return mMutex.try_lock();
        }
        
        void Unlock() noexcept {
            mMutex.unlock();
        }
        
    private:
        std::recursive_mutex mMutex;
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
            if (mMutex) {
                mMutex->Unlock();
            }
            mMutex = other.mMutex;
            other.mMutex = nullptr;
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
