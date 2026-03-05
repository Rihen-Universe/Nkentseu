// ============================================================
// FILE: NkThreadPool.h
// DESCRIPTION: Work-stealing thread pool (production-ready)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.0.0
// ============================================================
// Thread pool haute-performance avec:
// - Work-stealing queue (load-balancing automatique)
// - Minimal contention (lock-free where possible)
// - Affinity-aware task scheduling
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKTHREADPOOL_H_INCLUDED
#define NKENTSEU_THREADING_NKTHREADPOOL_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKThreading/NkThreadingExport.h"
#include "NKThreading/NkMutex.h"
#include "NKContainers/Sequential/NkVector.h"

#include <functional>
#include <queue>
#include <memory>

namespace nkentseu {
namespace threading {

    /**
     * @brief Task pour le thread pool
     * 
     * Une tâche est juste une fonction sans arguments
     * qui sera exécutée par le thread pool.
     */
    using Task = std::function<void()>;
    
    // Forward declaration
    class NkThreadPoolImpl;
    
    /**
     * @brief Work-stealing thread pool (Production-ready)
     * 
     * Pool de threads optimisé pour:
     * - Tâches courtes (< 1ms typically)
     * - Travail parallèle intensif
     * - Load-balancing automatique
     * - Zero-copy (move sémantique)
     * 
     * @features
     * - **Work Stealing:** Threads idle prennent du travail des autres
     * - **Lock-free:** Queues atomiques quand possible
     * - **Affinity:** Tâches restent sur même CPU quando possible
     * - **Graceful Shutdown:** Les tâches restantes sont exécutées
     * 
     * @example
     * ```cpp
     * NkThreadPool pool(4);  // 4 worker threads
     * 
     * // Enqueue une tâche
     * pool.Enqueue([]() { printf("Task\n"); });
     * 
     * // Attendre la fin
     * pool.Join();
     * pool.Shutdown();
     * ```
     */
    class NKTHREADING_API NkThreadPool {
    public:
        // ==============================================
        // CONSTRUCTEUR/DESTRUCTEUR
        // ==============================================
        
        /**
         * @brief Crée un pool de N worker threads
         * @param numWorkers Nombre de threads (0 = nombre de CPU cores)
         */
        explicit NkThreadPool(nk_uint32 numWorkers = 0) noexcept;
        
        /**
         * @brief Destructeur (appelle Shutdown automatiquement)
         */
        ~NkThreadPool() noexcept;
        
        // Non-copiable, non-movable
        NkThreadPool(const NkThreadPool&) = delete;
        NkThreadPool& operator=(const NkThreadPool&) = delete;
        
        // ==============================================
        // API WORK SUBMISSION
        // ==============================================
        
        /**
         * @brief Enqueue une tâche dans la queue de travail
         * @param task Fonction à exécuter
         */
        void Enqueue(Task task) noexcept;
        
        /**
         * @brief Enqueue avec priorité (high/normal/low)
         * @param task Tâche
         * @param priority -2 (low) ... 0 (normal) ... +2 (high)
         */
        void EnqueuePriority(Task task, nk_int32 priority = 0) noexcept;
        
        /**
         * @brief Enqueue avec affinité CPU
         * @param task Tâche
         * @param cpuCore CPU core préféré
         */
        void EnqueueAffinity(Task task, nk_uint32 cpuCore) noexcept;
        
        /**
         * @brief Enqueue une tâche parallèle (range-based)
         * @param count Nombre d'itérations
         * @param func Fonction(index) pour chaque itération
         * @param grainSize Taille de grain (combien d'items par tâche)
         * 
         * @example
         * pool.ParallelFor(1000, [](size_t i) {
         *     data[i] *= 2;
         * }, 100);  // 10 tâches de 100 items chacune
         */
        template<typename Func>
        void ParallelFor(nk_size count, Func&& func, 
                        nk_size grainSize = 1) noexcept {
            // Implementation: diviser le travail en chunks
            // et enqueue chaque chunk comme tâche
            for (nk_size i = 0; i < count; i += grainSize) {
                nk_size end = (i + grainSize < count) ? i + grainSize : count;
                Enqueue([&func, i, end]() {
                    for (nk_size idx = i; idx < end; ++idx) {
                        func(idx);
                    }
                });
            }
        }
        
        // ==============================================
        // API QUERY/CONTROL
        // ==============================================
        
        /**
         * @brief Attend que toutes les tâches soient terminées
         */
        void Join() noexcept;
        
        /**
         * @brief Shutdown graceful (termine tâches, puis arrête)
         */
        void Shutdown() noexcept;
        
        /**
         * @brief Nombre de worker threads
         */
        [[nodiscard]] nk_uint32 GetNumWorkers() const noexcept;
        
        /**
         * @brief Nombre approximatif de tâches en queue
         */
        [[nodiscard]] nk_size GetQueueSize() const noexcept;
        
        /**
         * @brief Nombre de tâches complétées
         */
        [[nodiscard]] nk_uint64 GetTasksCompleted() const noexcept;
        
        /**
         * @brief Obtient le pool global (singleton)
         */
        static NkThreadPool& GetGlobal() noexcept;
        
    private:
        std::unique_ptr<NkThreadPoolImpl> mImpl;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKTHREADPOOL_H_INCLUDED