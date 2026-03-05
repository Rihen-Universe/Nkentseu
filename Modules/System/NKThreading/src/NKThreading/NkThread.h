// ============================================================
// FILE: NkThread.h
// DESCRIPTION: Thread wrapper (production-ready)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.0.0
// ============================================================
// Wrapper C++ autour des threads OS (std::thread)
// avec support:
// - Affinity (pinning CPU)
// - Priorité
// - Naming (pour debugger)
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKTHREAD_H_INCLUDED
#define NKENTSEU_THREADING_NKTHREAD_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKThreading/NkThreadingExport.h"

#include <thread>
#include <functional>
#include <memory>

namespace nkentseu {
namespace threading {

    /**
     * @brief Thread wrapper (High-level API)
     * 
     * Wrapper autour std::thread avec features additionnelles:
     * - Nommage (pour debugger)
     * - CPU affinity
     * - Priorité
     * - Join automatique au destruction
     * 
     * @example
     * NkThread thread([]() {
     *     printf("Thread running\n");
     * });
     * thread.SetName("MyWorker");
     * thread.SetAffinity(0);  // Pin à CPU 0
     * thread.Join();
     */
    class NKTHREADING_API NkThread {
    public:
        using ThreadFunc = std::function<void()>;
        
        // ==============================================
        // CONSTRUCTEUR/DESTRUCTEUR
        // ==============================================
        
        /**
         * @brief Crée un thread (ne le démarre pas encore)
         */
        NkThread() noexcept = default;
        
        /**
         * @brief Crée et démarre un thread avec une fonction
         */
        template<typename Func>
        explicit NkThread(Func&& func)
            : mThread(std::forward<Func>(func)) {}
        
        /**
         * @brief Destructeur (detach si pas joined)
         */
        ~NkThread() {
            if (mThread.joinable()) {
                mThread.detach();
            }
        }
        
        // Movable
        NkThread(NkThread&& other) noexcept = default;
        NkThread& operator=(NkThread&& other) noexcept = default;
        
        // Non-copiable
        NkThread(const NkThread&) = delete;
        NkThread& operator=(const NkThread&) = delete;
        
        // ==============================================
        // API THREAD
        // ==============================================
        
        /**
         * @brief Attend la fin du thread
         */
        void Join() noexcept {
            if (mThread.joinable()) {
                mThread.join();
            }
        }
        
        /**
         * @brief Detache le thread (run-to-completion)
         */
        void Detach() noexcept {
            if (mThread.joinable()) {
                mThread.detach();
            }
        }
        
        /**
         * @brief Vérifie si le thread est joinable
         */
        [[nodiscard]] nk_bool Joinable() const noexcept {
            return mThread.joinable();
        }
        
        /**
         * @brief Obtient l'ID du thread
         */
        [[nodiscard]] std::thread::id GetID() const noexcept {
            return mThread.get_id();
        }
        
        /**
         * @brief Nomme le thread (pour debugger)
         */
        void SetName(const nk_char* name) noexcept;
        
        /**
         * @brief Assigne le thread à un CPU spécifique
         * @param cpuCore Index du core CPU (0-based)
         */
        void SetAffinity(nk_uint32 cpuCore) noexcept;
        
        /**
         * @brief Définit la priorité du thread
         * @param priority -2 (low) ... 0 (normal) ... +2 (high)
         */
        void SetPriority(nk_int32 priority) noexcept;
        
        /**
         * @brief Obtient le CPU core sur lequel tourne ce thread
         */
        [[nodiscard]] nk_uint32 GetCurrentCore() const noexcept;
        
    private:
        std::thread mThread;
    };
    
    // =======================================================
    // THREAD POOL (TODO dans NkThreadPool.h)
    // =======================================================

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKTHREAD_H_INCLUDED